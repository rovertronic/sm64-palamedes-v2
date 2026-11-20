#include <ultra64.h>

#include "dma_model_streaming.h"
#include "cubic_volume.h"
#include "engine/graph_node.h"
#include "engine/surface_load.h"
#include "level_update.h"
#include "main.h"
#include "memory.h"
#include "object_list_processor.h"
#include "segment_symbols.h"
#include "assets/models/streaming_assets.h"
#include "debug_tracker.h"

#define DMA_MODEL_POOL_SIZE 1000000
#define DMA_MODEL_SEGMENT 0x0F

/* The build discovers Fast64 exports and generates each model's ROM range.
 * All models share segment 0x0F; exports require no streaming-specific edits. */
const struct DmaModelEntry sDmaModelTable[DMA_MODEL_COUNT] = {
    [0] = DMA_MODEL_WITH_COLLISION_AT(test, test_test_mesh, test_collision, 3089, 1634, 6871),
    [1] = DMA_MODEL(cruelty, cruelty_curelty_mesh),
};

ALIGNED16 u8 sDmaModelMemoryPool[DMA_MODEL_POOL_SIZE];
Gfx * volatile sDmaLoadedDlTable[DMA_MODEL_COUNT];
volatile enum DmaModelStatus sDmaModelStatus[DMA_MODEL_COUNT];
static ALIGNED16 u8 sDmaCollisionMemoryPool[DMA_MODEL_COLLISION_POOL_SIZE];

static OSThread sAssetStreamingThread;
static ALIGNED16 u8 sAssetStreamingStack[THREAD11_STACK];
static OSMesgQueue sAssetStreamingQueue;
static OSMesg sAssetStreamingMessages[1];
static volatile u32 sWantedMask;
static volatile u32 sBuildingMask;
static volatile u32 sSubmittedMask;
/* A prepared asset stays owned until collision is unpublished and the old
 * render snapshot is released. The worker cannot recycle it before that ack. */
static volatile u32 sReadyMask;
static volatile u32 sCollisionReaders;
static volatile u32 sRetiringMask;
static u32 sFailedMask;
static Gfx *sPreparedDl[DMA_MODEL_COUNT];
static struct StreamedCollision *sPreparedCollision[DMA_MODEL_COUNT];
static s32 sInitialized;
/* Only the worker modifies the allocator. Retired blocks remain allocated
 * while either a CPU-built list or a submitted RCP task references them. */
static u32 sModelOffset[DMA_MODEL_COUNT];
static u32 sModelSize[DMA_MODEL_COUNT];
static u32 sCollisionOffset[DMA_MODEL_COUNT];
static u32 sCollisionSize[DMA_MODEL_COUNT];
static struct StreamedCollisionInfo sCollisionInfo[DMA_MODEL_COUNT];
static u32 sVolumeMask;
static f32 sModelPoolUsedPercent;
static f32 sCollisionPoolUsedPercent;
static s32 sDebugPreparedMask;
static s32 sDebugCollisionReaders;

static void dma_model_debug_pool_usage(void) {
    u32 modelBytes = 0, collisionBytes = 0;
    int id;
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    /* Count every reserved allocation, including pending loads and retired
     * blocks still waiting for collision/graphics references to be released. */
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        modelBytes += sModelSize[id];
        collisionBytes += sCollisionSize[id];
    }
    sDebugPreparedMask = sReadyMask;
    sDebugCollisionReaders = sCollisionReaders;
    osSetIntMask(mask);
    sModelPoolUsedPercent = 100.0f * modelBytes / sizeof(sDmaModelMemoryPool);
    sCollisionPoolUsedPercent = 100.0f * collisionBytes / sizeof(sDmaCollisionMemoryPool);
    debug_f32(&sModelPoolUsedPercent, "Model pool used *");
    debug_f32(&sCollisionPoolUsedPercent, "Collision pool used *");
    debug_s32(&sDebugPreparedMask, "Stream prepared");
    debug_s32(&sDebugCollisionReaders, "Stream CPU readers");
}

static void dma_model_wake(void) {
    if (sInitialized) {
        /* The queue is a wakeup, not a list of requests. A full queue already
         * guarantees that the latest desired state will be processed. */
        osSendMesg(&sAssetStreamingQueue, (OSMesg) 1, OS_MESG_NOBLOCK);
    }
}

static s32 dma_model_valid(int id) {
    return id >= 0 && id < DMA_MODEL_COUNT && sDmaModelTable[id].romStart != NULL;
}

void dma_model_load(int id) {
    OSIntMask mask;
    if (!dma_model_valid(id)) {
        return;
    }
    mask = osSetIntMask(OS_IM_NONE);
    sWantedMask |= 1U << id;
    osSetIntMask(mask);
    dma_model_wake();
}

void dma_model_unload(int id) {
    OSIntMask mask;
    if (!dma_model_valid(id)) {
        return;
    }
    mask = osSetIntMask(OS_IM_NONE);
    sWantedMask &= ~(1U << id);
    /* Hide immediately; the worker frees it after existing draws finish. */
    sDmaLoadedDlTable[id] = NULL;
    osSetIntMask(mask);
    dma_model_wake();
}

/* Only acknowledge memory retirement after the old CPU render snapshot is
 * finished. References are cleared once more because that frame could have
 * restored a cached floor after the game removed the chunk. */
static void dma_model_release_retired(void) {
    int id;
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        u32 bit = 1U << id;
        if (sRetiringMask & bit) {
            streamed_collision_release(sPreparedCollision[id]);
            sPreparedCollision[id] = NULL;
            sPreparedDl[id] = NULL;
            sReadyMask &= ~bit;
            sRetiringMask &= ~bit;
            sDmaModelStatus[id] = DMA_MODEL_STATUS_UNLOADED;
        }
    }
}

void dma_model_reset(void) {
    int id;
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    sWantedMask = 0;
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        sDmaLoadedDlTable[id] = NULL;
    }
    osSetIntMask(mask);
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        u32 bit = 1U << id;
        if ((sReadyMask & bit) && !(sRetiringMask & bit)) {
            streamed_collision_detach(sPreparedCollision[id]);
            sRetiringMask |= bit;
        }
    }
    if (sCollisionReaders == 0) {
        dma_model_release_retired();
    }
    dma_model_wake();
}

/* First fit without moving live models or depending on the level's heap. */
static s32 dma_stream_allocate(u32 size, u32 poolSize, const u32 *offsets, const u32 *sizes) {
    u32 offset = 0;
    int id;
    while (size <= poolSize && offset <= poolSize - size) {
        for (id = 0; id < DMA_MODEL_COUNT; id++) {
            if (sizes[id] != 0 && offset < offsets[id] + sizes[id]
                && offset + size > offsets[id]) {
                offset = offsets[id] + sizes[id];
                break;
            }
        }
        if (id == DMA_MODEL_COUNT) {
            return offset;
        }
    }
    return -1;
}

static void thread11_asset_streaming(UNUSED void *arg) {
    int id;
    while (TRUE) {
        osRecvMesg(&sAssetStreamingQueue, NULL, OS_MESG_BLOCK);
        /* Collision memory is returned only after a game-thread detach.
         * Display data additionally waits for CPU-built/submitted RCP lists. */
        for (id = 0; id < DMA_MODEL_COUNT; id++) {
            u32 bit = 1U << id;
            OSIntMask mask = osSetIntMask(OS_IM_NONE);
            /* A detach can be followed by a new request before this worker
             * runs. Reclaim the acknowledged collision block in either case. */
            if (!(sReadyMask & bit)) {
                sCollisionSize[id] = 0;
                sPreparedCollision[id] = NULL;
                if ((!(sWantedMask & bit) || (sFailedMask & bit))
                    && !((sBuildingMask | sSubmittedMask) & bit)) {
                    sModelSize[id] = 0;
                    sCollisionInfo[id].bytes = 0;
                }
            }
            if (!(sWantedMask & bit)) {
                sDmaLoadedDlTable[id] = NULL;
                sFailedMask &= ~bit;
                if (!(sReadyMask & bit)) {
                    sDmaModelStatus[id] = DMA_MODEL_STATUS_UNLOADED;
                }
            }
            osSetIntMask(mask);
        }
        for (id = 0; id < DMA_MODEL_COUNT; id++) {
            const struct DmaModelEntry *entry = &sDmaModelTable[id];
            struct StreamedCollision *collision = NULL;
            u32 bit = 1U << id;
            u32 size, dlOffset, collisionOffset = 0;
            u8 *base;
            s32 offset;
            OSIntMask mask;
            if (!(sWantedMask & bit) || ((sReadyMask | sFailedMask) & bit)
                || !dma_model_valid(id)) {
                continue;
            }
            sDmaModelStatus[id] = DMA_MODEL_STATUS_LOADING;
            size = (uintptr_t) entry->romEnd - (uintptr_t) entry->romStart;
            dlOffset = (uintptr_t) entry->displayList - (uintptr_t) entry->segmentStart;
            if (size == 0 || (size & 15) != 0 || dlOffset > size - sizeof(Gfx)
                || ((uintptr_t) entry->displayList >> 24) != DMA_MODEL_SEGMENT) {
                goto bad_data;
            }
            if (entry->collision != NULL) {
                collisionOffset = (uintptr_t) entry->collision - (uintptr_t) entry->segmentStart;
                if (collisionOffset >= size || collisionOffset % sizeof(Collision) != 0) {
                    goto bad_data;
                }
            }
            if (sModelSize[id] == 0) {
                offset = dma_stream_allocate(size, sizeof(sDmaModelMemoryPool), sModelOffset, sModelSize);
                if (offset < 0) {
                    sDmaModelStatus[id] = DMA_MODEL_STATUS_NO_MEMORY;
                    continue;
                }
                sModelOffset[id] = offset;
                sModelSize[id] = size;
                sCollisionInfo[id].bytes = 0;
                sDmaModelStatus[id] = DMA_MODEL_STATUS_DMA;
                dma_read(sDmaModelMemoryPool + offset, entry->romStart, entry->romEnd);
            }
            base = sDmaModelMemoryPool + sModelOffset[id];
            if (entry->collision != NULL) {
                const Collision *data = (const Collision *) (base + collisionOffset);
                const Collision *end = (const Collision *) (base + size);
                sDmaModelStatus[id] = DMA_MODEL_STATUS_BUILDING_COLLISION;
                if (sCollisionInfo[id].bytes == 0
                    && !streamed_collision_measure(data, end, entry->collisionOffset, &sCollisionInfo[id])) {
                    goto bad_data;
                }
                offset = dma_stream_allocate(sCollisionInfo[id].bytes,
                    sizeof(sDmaCollisionMemoryPool), sCollisionOffset, sCollisionSize);
                if (offset < 0) {
                    sDmaModelStatus[id] = DMA_MODEL_STATUS_NO_MEMORY;
                    continue;
                }
                sCollisionOffset[id] = offset;
                sCollisionSize[id] = sCollisionInfo[id].bytes;
                collision = streamed_collision_build(sDmaCollisionMemoryPool + offset,
                    &sCollisionInfo[id], data, end, entry->collisionOffset);
                if (collision == NULL) {
                    sCollisionSize[id] = 0;
                    goto bad_data;
                }
            }
            mask = osSetIntMask(OS_IM_NONE);
            if (sWantedMask & bit) {
                sPreparedDl[id] = (Gfx *) (base + dlOffset);
                sPreparedCollision[id] = collision;
                sReadyMask |= bit;
                sDmaModelStatus[id] = DMA_MODEL_STATUS_WAITING_FOR_INSTALL;
                /* The game thread installs collision before exposing the DL. */
            } else {
                sCollisionSize[id] = 0;
                if (!((sBuildingMask | sSubmittedMask) & bit)) {
                    sModelSize[id] = 0;
                    sCollisionInfo[id].bytes = 0;
                }
            }
            osSetIntMask(mask);
            continue;

bad_data:
            sFailedMask |= bit;
            sDmaModelStatus[id] = DMA_MODEL_STATUS_BAD_DATA;
        }
    }
}

void dma_model_streaming_init(void) {
    if (sInitialized) {
        return;
    }
    osCreateMesgQueue(&sAssetStreamingQueue, sAssetStreamingMessages, 1);
    osCreateThread(&sAssetStreamingThread, THREAD_11_ASSET_STREAMING,
                   thread11_asset_streaming, NULL,
                   sAssetStreamingStack + sizeof(sAssetStreamingStack), 1);
    sInitialized = TRUE;
    osStartThread(&sAssetStreamingThread);
}

/* Called after receiving the previous task's DP-completion message and before
 * submitting the newly built task. Preserve both generations until then. */
void dma_model_frame_submitted(void) {
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    sSubmittedMask = sBuildingMask;
    sBuildingMask = 0;
    osSetIntMask(mask);
    dma_model_wake();
}

/* Render queries keep a fixed set of private chunks for the whole frame.
 * Publishing new chunks never waits for this reader; removing old chunks keeps
 * their allocations alive until this scope ends. */
void dma_model_collision_render_begin(void) {
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    streamed_collision_render_begin();
    sCollisionReaders++;
    osSetIntMask(mask);
}

void dma_model_collision_render_end(void) {
    s32 retired;
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    streamed_collision_render_end();
    sCollisionReaders--;
    retired = sRetiringMask != 0 && sCollisionReaders == 0;
    if (retired) {
        /* This only clears a bounded number of object/camera references. There
         * is no triangle traversal, list sorting, allocation or blocking here.
         * Keep the game from preempting the final cache cleanup/acknowledgment. */
        dma_model_release_retired();
    }
    osSetIntMask(mask);
    if (retired) {
        dma_model_wake();
    }
}

/* All geometry/index work is already complete on the worker. Each activation
 * publishes one pointer; deactivation removes it and retires the old snapshot.
 * No render rendezvous or writes to shared base-terrain lists are necessary. */
void dma_model_update(void) {
    int id;
    dma_model_debug_pool_usage();
    if (gTimeStopState & TIME_STOP_ACTIVE) {
        return;
    }
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        u32 bit = 1U << id;
        if ((sReadyMask & bit) && !(sWantedMask & bit) && !(sRetiringMask & bit)) {
            streamed_collision_detach(sPreparedCollision[id]);
            sDmaLoadedDlTable[id] = NULL;
            sRetiringMask |= bit;
            sDmaModelStatus[id] = DMA_MODEL_STATUS_UNLOADED;
        }
    }
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        u32 bit = 1U << id;
        if ((sReadyMask & sWantedMask & bit) && sDmaLoadedDlTable[id] == NULL) {
            if (streamed_collision_attach(sPreparedCollision[id])) {
                sRetiringMask &= ~bit;
                sDmaLoadedDlTable[id] = sPreparedDl[id];
                sDmaModelStatus[id] = DMA_MODEL_STATUS_READY;
            } else {
                sDmaModelStatus[id] = DMA_MODEL_STATUS_NO_MEMORY;
            }
        }
    }
    if (sCollisionReaders == 0) {
        dma_model_release_retired();
    }
    dma_model_debug_pool_usage();
    dma_model_wake();
}

static void dma_model_load_when_mario_in_region(CubicVolume *cv) {
    if (dma_model_valid(cv->param)) {
        sVolumeMask |= 1U << cv->param;
    }
}

void dma_model_check_volumes(void) {
    int id;
    sVolumeMask = 0;
    if (gMarioState != NULL) {
        cubic_volume_check_all(gMarioState->pos, VOLUME_TYPE_DMA_MODEL,
                               dma_model_load_when_mario_in_region);
    }
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        if (sVolumeMask & (1U << id)) {
            dma_model_load(id);
        } else {
            dma_model_unload(id);
        }
    }
}

Gfx *geo_dma_model(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated *self = (struct GraphNodeGenerated *) node;
    struct GraphNodeDisplayList *next = (struct GraphNodeDisplayList *) node->next;
    Gfx *model, *wrapper;
    u8 *base;
    int id = self->parameter;
    OSIntMask mask;

    /* Use GEO_ASM(id, geo_dma_model), GEO_DISPLAY_LIST(layer, NULL). */
    if (node->next == node || node->next == NULL
        || node->next->type != GRAPH_NODE_TYPE_DISPLAY_LIST) {
        return NULL;
    }
    if (callContext != GEO_CONTEXT_RENDER) {
        next->displayList = NULL;
        return NULL;
    }
    next->displayList = NULL;
    if (!dma_model_valid(id)) {
        return NULL;
    }
    mask = osSetIntMask(OS_IM_NONE);
    model = sDmaLoadedDlTable[id];
    if (model != NULL) {
        sBuildingMask |= 1U << id;
    }
    osSetIntMask(mask);
    if (model == NULL) {
        return NULL;
    }
    wrapper = alloc_display_list(4 * sizeof(Gfx));
    if (wrapper == NULL) {
        return NULL;
    }
    base = (u8 *) model - ((uintptr_t) sDmaModelTable[id].displayList
                         - (uintptr_t) sDmaModelTable[id].segmentStart);
    /* Internal pointers retain their offsets in the shared linked segment.
     * Bias its RSP base so they resolve into this model's allocated block.
     * Keep the full 32-bit subtraction: it can wrap before the RSP adds
     * the pointer's offset back, yielding the correct physical RAM address. */
    gSPSegment(&wrapper[0], DMA_MODEL_SEGMENT,
               (u32) VIRTUAL_TO_PHYSICAL(base)
                   - ((u32) (uintptr_t) sDmaModelTable[id].segmentStart & 0x00FFFFFF));
    gSPDisplayList(&wrapper[1], sDmaModelTable[id].displayList);
    gSPSegment(&wrapper[2], DMA_MODEL_SEGMENT,
               VIRTUAL_TO_PHYSICAL(get_segment_base_addr(DMA_MODEL_SEGMENT)));
    gSPEndDisplayList(&wrapper[3]);
    next->displayList = (Gfx *) VIRTUAL_TO_PHYSICAL(wrapper);
    return NULL;
}
