#include <ultra64.h>

#include "dma_model_streaming.h"
#include "cubic_volume.h"
#include "engine/graph_node.h"
#include "level_update.h"
#include "main.h"
#include "memory.h"
#include "segment_symbols.h"
#include "assets/models/streaming_assets.h"

#define DMA_MODEL_POOL_SIZE 100000
#define DMA_MODEL_SEGMENT 0x0F

/* The build discovers Fast64 exports and generates each model's ROM range.
 * All models share segment 0x0F; exports require no streaming-specific edits. */
const struct DmaModelEntry sDmaModelTable[DMA_MODEL_COUNT] = {
    [0] = DMA_MODEL(test, test_test_mesh),
    [1] = DMA_MODEL(cruelty, cruelty_curelty_mesh),
};

ALIGNED16 u8 sDmaModelMemoryPool[DMA_MODEL_POOL_SIZE];
Gfx * volatile sDmaLoadedDlTable[DMA_MODEL_COUNT];

static OSThread sAssetStreamingThread;
static ALIGNED16 u8 sAssetStreamingStack[THREAD11_STACK];
static OSMesgQueue sAssetStreamingQueue;
static OSMesg sAssetStreamingMessages[1];
static volatile u32 sWantedMask;
static volatile u32 sBuildingMask;
static volatile u32 sSubmittedMask;
static s32 sInitialized;
/* Only the worker modifies the allocator. Retired blocks remain allocated
 * while either a CPU-built list or a submitted RCP task references them. */
static u32 sModelOffset[DMA_MODEL_COUNT];
static u32 sModelSize[DMA_MODEL_COUNT];
static u32 sVolumeMask;

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

void dma_model_reset(void) {
    int id;
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    sWantedMask = 0;
    for (id = 0; id < DMA_MODEL_COUNT; id++) {
        sDmaLoadedDlTable[id] = NULL;
    }
    osSetIntMask(mask);
    dma_model_wake();
}

/* First fit without moving live models or depending on the level's heap. */
static s32 dma_model_allocate(u32 size) {
    u32 offset = 0;
    int id;
    while (offset + size <= sizeof(sDmaModelMemoryPool)) {
        for (id = 0; id < DMA_MODEL_COUNT; id++) {
            if (sModelSize[id] != 0 && offset < sModelOffset[id] + sModelSize[id]
                && offset + size > sModelOffset[id]) {
                offset = sModelOffset[id] + sModelSize[id];
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
        /* Reclaim first, so outgoing models make room for incoming ones. */
        for (id = 0; id < DMA_MODEL_COUNT; id++) {
            u32 bit = 1U << id;
            OSIntMask mask = osSetIntMask(OS_IM_NONE);
            if (!(sWantedMask & bit)) {
                sDmaLoadedDlTable[id] = NULL;
                if (!((sBuildingMask | sSubmittedMask) & bit)) {
                    sModelSize[id] = 0;
                }
            }
            osSetIntMask(mask);
        }
        for (id = 0; id < DMA_MODEL_COUNT; id++) {
            const struct DmaModelEntry *entry = &sDmaModelTable[id];
            u32 bit = 1U << id;
            u32 size, dlOffset;
            s32 offset;
            OSIntMask mask;
            if (!(sWantedMask & bit) || sDmaLoadedDlTable[id] != NULL
                || !dma_model_valid(id)) {
                continue;
            }
            size = (uintptr_t) entry->romEnd - (uintptr_t) entry->romStart;
            dlOffset = (uintptr_t) entry->displayList - (uintptr_t) entry->segmentStart;
            if (size == 0 || size > sizeof(sDmaModelMemoryPool)
                || (size & 15) != 0 || dlOffset >= size
                || ((uintptr_t) entry->displayList >> 24) != DMA_MODEL_SEGMENT) {
                continue;
            }
            if (sModelSize[id] == 0) {
                offset = dma_model_allocate(size);
                if (offset < 0) {
                    /* Retry when a later frame releases retired blocks. */
                    continue;
                }
                sModelOffset[id] = offset;
                sModelSize[id] = size;
                /* dma_read has a private completion queue and invalidates
                 * the destination cache. Publish only after it completes. */
                dma_read(sDmaModelMemoryPool + offset, entry->romStart, entry->romEnd);
            }
            mask = osSetIntMask(OS_IM_NONE);
            if (sWantedMask & bit) {
                sDmaLoadedDlTable[id] = (Gfx *) (sDmaModelMemoryPool + sModelOffset[id] + dlOffset);
            } else {
                /* A cancelled DMA was never published or rendered. */
                sModelSize[id] = 0;
            }
            osSetIntMask(mask);
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
