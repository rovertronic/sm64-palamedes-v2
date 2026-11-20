#include <PR/ultratypes.h>

#include "sm64.h"
#include "game/ingame_menu.h"
#include "graph_node.h"
#include "behavior_script.h"
#include "behavior_data.h"
#include "game/memory.h"
#include "game/object_helpers.h"
#include "game/macro_special_objects.h"
#include "surface_collision.h"
#include "math_util.h"
#include "game/mario.h"
#include "game/object_list_processor.h"
#include "surface_load.h"
#include "game/puppyprint.h"
#include "game/debug.h"
#include "game/camera.h"
#include "game/dma_model_streaming.h"
#include "game/level_update.h"
#include "game/main.h"
#include <PR/os_internal_reg.h>

#include "config.h"

/**
 * Partitions for course and object surfaces. The arrays represent
 * the 16x16 cells that each level is split into.
 */
SpatialPartitionCell gStaticSurfacePartition[NUM_CELLS][NUM_CELLS];
SpatialPartitionCell gDynamicSurfacePartition[NUM_CELLS][NUM_CELLS];
struct CellCoords {
    u8 z;
    u8 x;
    u8 partition;
};
struct CellCoords sCellsUsed[NUM_CELLS];
u16 sNumCellsUsed;
u8 sClearAllCells;

/**
 * Pools of data that can contain either surface nodes or surfaces.
 * The static surface pool is resized to be exactly the amount of memory needed for the level geometry.
 * The dynamic surface pool is set at a fixed length and cleared every frame.
 */
void *gCurrStaticSurfacePool;
void *gDynamicSurfacePool;

/**
 * The end of the data currently allocated to the surface pools.
 */
void *gCurrStaticSurfacePoolEnd;
void *gDynamicSurfacePoolEnd;

/**
 * The amount of data currently allocated to static surfaces.
 */
u32 gTotalStaticSurfaceData;

/**
 * Allocate the part of the surface node pool to contain a surface node.
 */
static struct SurfaceNode *alloc_surface_node(u32 dynamic) {
    struct SurfaceNode **poolEnd = (struct SurfaceNode **)(dynamic ? &gDynamicSurfacePoolEnd : &gCurrStaticSurfacePoolEnd);

    struct SurfaceNode *node = *poolEnd;
    (*poolEnd)++;
    gSurfaceNodesAllocated++;

    node->next = NULL;

    return node;
}

/**
 * Allocate the part of the surface pool to contain a surface and
 * initialize the surface.
 */
static struct Surface *alloc_surface(u32 dynamic) {
    struct Surface **poolEnd = (struct Surface **)(dynamic ? &gDynamicSurfacePoolEnd : &gCurrStaticSurfacePoolEnd);
    
    struct Surface *surface = *poolEnd;
    (*poolEnd)++;
    gSurfacesAllocated++;

    surface->type = SURFACE_DEFAULT;
    surface->flags = SURFACE_FLAGS_NONE;
    surface->object = NULL;

    return surface;
}

/**
 * Add a surface to the correct cell list of surfaces.
 * @param dynamic Determines whether the surface is static or dynamic
 * @param cellX The X position of the cell in which the surface resides
 * @param cellZ The Z position of the cell in which the surface resides
 * @param surface The surface to add
 */
static void link_surface_to_cell(struct SurfaceNode *newNode, s32 dynamic,
                                s32 cellX, s32 cellZ, struct Surface *surface, f32 normalY) {
    struct SurfaceNode **list;
    s32 priority;
    s32 sortDir = 1; // highest to lowest, then insertion order (water and floors)
    s32 listIndex;

    if (SURFACE_IS_NEW_WATER(surface->type)) {
        listIndex = SPATIAL_PARTITION_WATER;
    } else if (normalY > NORMAL_FLOOR_THRESHOLD) {
        listIndex = SPATIAL_PARTITION_FLOORS;
    } else if (normalY < NORMAL_CEIL_THRESHOLD) {
        listIndex = SPATIAL_PARTITION_CEILS;
        sortDir = -1; // lowest to highest, then insertion order
    } else {
        listIndex = SPATIAL_PARTITION_WALLS;
        sortDir = 0; // insertion order
    }

    s32 surfacePriority = surface->upperY * sortDir;

    newNode->surface = surface;
    newNode->next = NULL;

    if (dynamic) {
        list = &gDynamicSurfacePartition[cellZ][cellX][listIndex];
        if (sNumCellsUsed >= sizeof(sCellsUsed) / sizeof(struct CellCoords)) {
            sClearAllCells = TRUE;
        } else {
            if (*list == NULL) {
                sCellsUsed[sNumCellsUsed].z = cellZ;
                sCellsUsed[sNumCellsUsed].x = cellX;
                sCellsUsed[sNumCellsUsed].partition = listIndex;
                sNumCellsUsed++;
            }
        }
    } else {
        list = &gStaticSurfacePartition[cellZ][cellX][listIndex];
    }

    if (*list == NULL) {
        *list = newNode;
        return;
    }

    struct SurfaceNode *curNode = *list;

    // Check if surface should be placed at the beginning of the list.
    priority = curNode->surface->upperY * sortDir;
    if (surfacePriority > priority) {
        *list = newNode;
        newNode->next = curNode;
        return;
    }

    // Loop until we find the appropriate place for the surface in the list.
    while (curNode->next != NULL) {
        priority = curNode->next->surface->upperY * sortDir;

        if (surfacePriority > priority) {
            break;
        }

        curNode = curNode->next;
    }

    newNode->next = curNode->next;
    curNode->next = newNode;
}

static void add_surface_to_cell(s32 dynamic, s32 cellX, s32 cellZ, struct Surface *surface, f32 normalY) {
    link_surface_to_cell(alloc_surface_node(dynamic), dynamic, cellX, cellZ, surface, normalY);
}

/**
 * Every level is split into CELL_SIZE * CELL_SIZE cells of surfaces (to limit computing
 * time). This function determines the lower cell for a given x/z position.
 * @param coord The coordinate to test
 */
static s32 lower_cell_index(s32 coord) {
    // Move from range [-LEVEL_BOUNDARY_MAX, LEVEL_BOUNDARY_MAX) to [0, 2 * LEVEL_BOUNDARY_MAX)
    coord += LEVEL_BOUNDARY_MAX;
    if (coord < 0) {
        coord = 0;
    }

    // [0, NUM_CELLS)
    s32 index = coord / CELL_SIZE;

    // Potentially > NUM_CELLS - 1, but since the upper index is <= NUM_CELLS - 1, not exploitable
    return MAX(0, index);
}

/**
 * Every level is split into CELL_SIZE * CELL_SIZE cells of surfaces (to limit computing
 * time). This function determines the upper cell for a given x/z position.
 * @param coord The coordinate to test
 */
static s32 upper_cell_index(s32 coord) {
    // Move from range [-LEVEL_BOUNDARY_MAX, LEVEL_BOUNDARY_MAX) to [0, 2 * LEVEL_BOUNDARY_MAX)
    coord += LEVEL_BOUNDARY_MAX;
    if (coord < 0) {
        coord = 0;
    }

    // [0, NUM_CELLS)
    s32 index = coord / CELL_SIZE;

    // Potentially < 0, but since lower index is >= 0, not exploitable
    return MIN((NUM_CELLS - 1), index);
}

/**
 * Every level is split into 16x16 cells, this takes a surface, finds
 * the appropriate cells (with a buffer), and adds the surface to those
 * cells.
 * @param surface The surface to check
 * @param dynamic Boolean determining whether the surface is static or dynamic
 */
static void add_surface(struct Surface *surface, s32 dynamic) {
    s32 cellZ, cellX;
    s32 minX, maxX, minZ, maxZ;
    f32 normalY = get_surface_normal(surface).y;

    min_max_3i(surface->vertex1[0], surface->vertex2[0], surface->vertex3[0], &minX, &maxX);
    min_max_3i(surface->vertex1[2], surface->vertex2[2], surface->vertex3[2], &minZ, &maxZ);

    s32 minCellX = lower_cell_index(minX);
    s32 maxCellX = upper_cell_index(maxX);
    s32 minCellZ = lower_cell_index(minZ);
    s32 maxCellZ = upper_cell_index(maxZ);

    for (cellZ = minCellZ; cellZ <= maxCellZ; cellZ++) {
        for (cellX = minCellX; cellX <= maxCellX; cellX++) {
            add_surface_to_cell(dynamic, cellX, cellZ, surface, normalY);
        }
    }
}

/**
 * Initializes a Surface struct using the given vertex data
 * @param vertexData The raw data containing vertex positions
 * @param vertexIndices Helper which tells positions in vertexData to start reading vertices
 * @param dynamic If the surface belongs to an object or not
 */
static struct Surface *read_surface_data(TerrainData *vertexData, TerrainData **vertexIndices, u32 dynamic) {
    Vec3f v[3];
    Vec3t offset;
    s16 min, max;

    vec3_scale_dest(offset, (*vertexIndices), 3);

    vec3s_copy(v[0], (vertexData + offset[0]));
    vec3s_copy(v[1], (vertexData + offset[1]));
    vec3s_copy(v[2], (vertexData + offset[2]));

#ifdef ENABLE_VANILLA_LEVEL_SPECIFIC_CHECKS
    // Fast64 removes degenerate triangles from custom levels during export.
    Vec3f n;
    find_vector_perpendicular_to_plane(n, v[0], v[1], v[2]);
    if (vec3_sumsq(n) < NEAR_ZERO) {
        return NULL;
    }
#endif

    struct Surface *surface = alloc_surface(dynamic);

    vec3s_copy(surface->vertex1, v[0]);
    vec3s_copy(surface->vertex2, v[1]);
    vec3s_copy(surface->vertex3, v[2]);

    min_max_3s(v[0][1], v[1][1], v[2][1], &min, &max);
    surface->lowerY = (min - SURFACE_VERTICAL_BUFFER);
    surface->upperY = (max + SURFACE_VERTICAL_BUFFER);

    return surface;
}

#ifndef ALL_SURFACES_HAVE_FORCE
/**
 * Returns whether a surface has exertion/moves Mario
 * based on the surface type.
 */
static s32 surface_has_force(s32 surfaceType) {
    s32 hasForce = FALSE;

    switch (surfaceType) {
        case SURFACE_0004: // Unused
        case SURFACE_FLOWING_WATER:
        case SURFACE_DEEP_MOVING_QUICKSAND:
        case SURFACE_SHALLOW_MOVING_QUICKSAND:
        case SURFACE_MOVING_QUICKSAND:
        case SURFACE_HORIZONTAL_WIND:
        case SURFACE_INSTANT_MOVING_QUICKSAND:
            hasForce = TRUE;
            break;

        default:
            break;
    }
    return hasForce;
}
#endif

/**
 * Returns whether a surface should have the
 * SURFACE_FLAG_NO_CAM_COLLISION flag.
 */
static s32 surf_has_no_cam_collision(s32 surfaceType) {
    s32 flags = SURFACE_FLAGS_NONE;

    switch (surfaceType) {
        case SURFACE_NO_CAM_COLLISION:
        case SURFACE_NO_CAM_COLLISION_77: // Unused
        case SURFACE_NO_CAM_COL_VERY_SLIPPERY:
        case SURFACE_SWITCH:
            flags = SURFACE_FLAG_NO_CAM_COLLISION;
            break;

        default:
            break;
    }

    return flags;
}

/* A model owns its persistent surfaces and nodes in one worker allocation. */
struct StreamedCollision {
    struct StreamedCollisionInfo info;
    struct Surface *surfaces;
    struct SurfaceNode *nodes;
    SpatialPartitionCell *cells;
    s32 activeSlot;
};

static struct StreamedCollision *volatile sStreamedCollisions[DMA_MODEL_COUNT];
static struct StreamedCollision *sRenderedStreamedCollisions[DMA_MODEL_COUNT];
static volatile OSId sStreamedRenderThread = -1;

static void streamed_surface_cells(const struct Surface *surface,
                                  s32 *minX, s32 *maxX, s32 *minZ, s32 *maxZ) {
    s32 low, high;
    min_max_3i(surface->vertex1[0], surface->vertex2[0], surface->vertex3[0], &low, &high);
    *minX = lower_cell_index(low);
    *maxX = upper_cell_index(high);
    min_max_3i(surface->vertex1[2], surface->vertex2[2], surface->vertex3[2], &low, &high);
    *minZ = lower_cell_index(low);
    *maxZ = upper_cell_index(high);
}

/* Parse directly from the export, with no fixed-size vertex scratch buffer.
 * Every count and triangle index is checked before reading it. Objects and
 * environmental boxes have a different lifecycle and are intentionally rejected. */
static s32 streamed_collision_parse(const Collision *data, const Collision *end,
    const Vec3f offset, struct Surface *surfaces, u32 capacity, struct StreamedCollisionInfo *info) {
    const Collision *vertices = NULL;
    s32 vertexCount = 0;
    u32 surfaceCount = 0, nodeCount = 0;
    s32 chunkMinX = NUM_CELLS, chunkMaxX = -1;
    s32 chunkMinZ = NUM_CELLS, chunkMaxZ = -1;

    while (data < end) {
        u32 command = (u32) *data++;
        s32 count, stride, i, j;
        if (sizeof(Collision) == sizeof(s16)) {
            command &= 0xFFFF; /* Preserve packed materials in signed exports. */
        }
        if (command == TERRAIN_LOAD_END) {
            if (vertices == NULL) {
                return FALSE;
            }
            info->surfaces = surfaceCount;
            info->nodes = nodeCount;
            info->minCellX = chunkMinX;
            info->maxCellX = chunkMaxX;
            info->minCellZ = chunkMinZ;
            info->maxCellZ = chunkMaxZ;
            u32 cells = surfaceCount == 0 ? 0
                : (chunkMaxX - chunkMinX + 1) * (chunkMaxZ - chunkMinZ + 1);
            u64 bytes = ALIGN16((u64) ALIGN16(sizeof(struct StreamedCollision))
                + ALIGN16((u64) surfaceCount * sizeof(struct Surface))
                + ALIGN16((u64) nodeCount * sizeof(struct SurfaceNode))
                + (u64) cells * sizeof(SpatialPartitionCell));
            if (bytes > 0xFFFFFFFFULL) {
                return FALSE;
            }
            info->bytes = (u32) bytes;
            return TRUE;
        }
        if (command == TERRAIN_LOAD_CONTINUE) {
            continue;
        }
        if (command == TERRAIN_LOAD_VERTICES) {
            if (data == end) {
                return FALSE;
            }
            vertexCount = *data++;
            if (vertexCount < 0 || vertexCount > (end - data) / 3) {
                return FALSE;
            }
            vertices = data;
            data += 3 * vertexCount;
            continue;
        }
        if (!(TERRAIN_LOAD_IS_SURFACE_TYPE_LOW(command)
              || TERRAIN_LOAD_IS_SURFACE_TYPE_HIGH(command)) || vertices == NULL || data == end) {
            return FALSE;
        }
        count = *data++;
#ifdef ALL_SURFACES_HAVE_FORCE
        stride = 4;
#else
        stride = 3 + surface_has_force(command & SURFACE_MASK_TYPE);
#endif
        if (count < 0 || count > (end - data) / stride) {
            return FALSE;
        }
        for (i = 0; i < count; i++, data += stride) {
            struct Surface surface = { 0 };
            Vec3f v[3], normal;
            s32 minX, maxX, minZ, maxZ;
            s16 low, high;
            for (j = 0; j < 3; j++) {
                s32 index = data[j];
                if (index < 0 || index >= vertexCount) {
                    return FALSE;
                }
                vec3s_copy(v[j], vertices + 3 * index);
                vec3_add(v[j], offset);
                /* Prevent translated vertices wrapping in the stored format. */
                if (sizeof(Collision) == sizeof(s16)
                    && (v[j][0] < S16_MIN || v[j][0] > S16_MAX
                        || v[j][1] < S16_MIN || v[j][1] > S16_MAX
                        || v[j][2] < S16_MIN || v[j][2] > S16_MAX)) {
                    return FALSE;
                }
            }
            find_vector_perpendicular_to_plane(normal, v[0], v[1], v[2]);
            if (vec3_sumsq(normal) < NEAR_ZERO) {
                continue;
            }
            vec3s_copy(surface.vertex1, v[0]);
            vec3s_copy(surface.vertex2, v[1]);
            vec3s_copy(surface.vertex3, v[2]);
            min_max_3s(v[0][1], v[1][1], v[2][1], &low, &high);
            surface.lowerY = MAX(low - SURFACE_VERTICAL_BUFFER, S16_MIN);
            surface.upperY = MIN(high + SURFACE_VERTICAL_BUFFER, S16_MAX);
            surface.type = command & SURFACE_MASK_TYPE;
            surface.material = command >> 12;
            surface.flags = surf_has_no_cam_collision(surface.type);
            streamed_surface_cells(&surface, &minX, &maxX, &minZ, &maxZ);
            if (minX > maxX || minZ > maxZ) {
                continue;
            }
            if (surfaces != NULL) {
                if (surfaceCount >= capacity) {
                    return FALSE;
                }
                surfaces[surfaceCount] = surface;
            }
            surfaceCount++;
            u32 nodesForSurface = (maxX - minX + 1) * (maxZ - minZ + 1);
            if (nodeCount > 0xFFFFFFFFU - nodesForSurface) {
                return FALSE;
            }
            nodeCount += nodesForSurface;
            chunkMinX = MIN(chunkMinX, minX);
            chunkMaxX = MAX(chunkMaxX, maxX);
            chunkMinZ = MIN(chunkMinZ, minZ);
            chunkMaxZ = MAX(chunkMaxZ, maxZ);
        }
    }
    return FALSE; /* Truncated input, including a missing COL_END(). */
}

s32 streamed_collision_measure(const Collision *data, const Collision *end,
                               const Vec3f offset, struct StreamedCollisionInfo *info) {
    return data != NULL && data < end
        && streamed_collision_parse(data, end, offset, NULL, 0, info);
}

/* Stable bottom-up mergesort: O(n log n), no recursion or temporary pool.
 * Restore export order first because nodes are initially prepended to cells. */
static struct SurfaceNode *streamed_sort_nodes(struct SurfaceNode *head, s32 sortDir) {
    struct SurfaceNode *reversed = NULL;
    u32 width = 1;
    while (head != NULL) {
        struct SurfaceNode *next = head->next;
        head->next = reversed;
        reversed = head;
        head = next;
    }
    head = reversed;
    if (sortDir == 0) {
        return head;
    }
    while (TRUE) {
        struct SurfaceNode *left = head, *tail = NULL;
        u32 merges = 0;
        head = NULL;
        while (left != NULL) {
            struct SurfaceNode *right = left;
            u32 leftCount = 0, rightCount = width, i;
            merges++;
            for (i = 0; i < width && right != NULL; i++) {
                leftCount++;
                right = right->next;
            }
            while (leftCount != 0 || (rightCount != 0 && right != NULL)) {
                struct SurfaceNode *node;
                if (leftCount != 0 && (rightCount == 0 || right == NULL
                    || left->surface->upperY * sortDir >= right->surface->upperY * sortDir)) {
                    node = left;
                    left = left->next;
                    leftCount--;
                } else {
                    node = right;
                    right = right->next;
                    rightCount--;
                }
                if (tail == NULL) {
                    head = node;
                } else {
                    tail->next = node;
                }
                tail = node;
            }
            left = right;
        }
        if (tail != NULL) {
            tail->next = NULL;
        }
        if (merges <= 1) {
            return head;
        }
        width *= 2;
    }
}

struct StreamedCollision *streamed_collision_build(void *memory,
    const struct StreamedCollisionInfo *info, const Collision *data, const Collision *end,
    const Vec3f offset) {
    struct StreamedCollision *collision = memory;
    struct StreamedCollisionInfo actual;
    struct SurfaceNode *node;
    u32 i, cellCount;
    s32 width = info->maxCellX - info->minCellX + 1;
    collision->surfaces = (struct Surface *) ((u8 *) memory + ALIGN16(sizeof(*collision)));
    collision->nodes = (struct SurfaceNode *) ((u8 *) collision->surfaces
                                             + ALIGN16(info->surfaces * sizeof(struct Surface)));
    collision->cells = (SpatialPartitionCell *) ((u8 *) collision->nodes
                                                + ALIGN16(info->nodes * sizeof(struct SurfaceNode)));
    if (!streamed_collision_parse(data, end, offset, collision->surfaces, info->surfaces, &actual)
        || actual.surfaces != info->surfaces || actual.nodes != info->nodes
        || actual.bytes != info->bytes || actual.minCellX != info->minCellX
        || actual.maxCellX != info->maxCellX || actual.minCellZ != info->minCellZ
        || actual.maxCellZ != info->maxCellZ) {
        return NULL;
    }
    collision->info = actual;
    collision->activeSlot = -1;
    cellCount = info->surfaces == 0 ? 0 : width * (info->maxCellZ - info->minCellZ + 1);
    bzero(collision->cells, cellCount * sizeof(SpatialPartitionCell));
    node = collision->nodes;
    for (i = 0; i < info->surfaces; i++) {
        struct Surface *surface = &collision->surfaces[i];
        s32 minX, maxX, minZ, maxZ, x, z, partition;
        f32 normalY = get_surface_normal(surface).y;
        if (SURFACE_IS_NEW_WATER(surface->type)) {
            partition = SPATIAL_PARTITION_WATER;
        } else if (normalY > NORMAL_FLOOR_THRESHOLD) {
            partition = SPATIAL_PARTITION_FLOORS;
        } else if (normalY < NORMAL_CEIL_THRESHOLD) {
            partition = SPATIAL_PARTITION_CEILS;
        } else {
            partition = SPATIAL_PARTITION_WALLS;
        }
        streamed_surface_cells(surface, &minX, &maxX, &minZ, &maxZ);
        for (z = minZ; z <= maxZ; z++) {
            for (x = minX; x <= maxX; x++) {
                struct SurfaceNode **head = &collision->cells[
                    (z - info->minCellZ) * width + x - info->minCellX][partition];
                node->surface = surface;
                node->next = *head;
                *head = node++;
            }
        }
    }
    for (i = 0; i < cellCount; i++) {
        s32 partition;
        for (partition = 0; partition < NUM_SPATIAL_PARTITIONS; partition++) {
            s32 sortDir = partition == SPATIAL_PARTITION_WALLS ? 0
                : partition == SPATIAL_PARTITION_CEILS ? -1 : 1;
            collision->cells[i][partition] = streamed_sort_nodes(collision->cells[i][partition], sortDir);
        }
    }
    return collision;
}

/* Publication only: the prepared cells/nodes are immutable from now on. */
s32 streamed_collision_attach(struct StreamedCollision *collision) {
    s32 slot;
    if (collision == NULL || collision->activeSlot >= 0) {
        return TRUE;
    }
    for (slot = 0; slot < DMA_MODEL_COUNT; slot++) {
        if (sStreamedCollisions[slot] == NULL) {
            collision->activeSlot = slot;
            sStreamedCollisions[slot] = collision;
            gNumStaticSurfaces += collision->info.surfaces;
            gSurfacesAllocated += collision->info.surfaces;
            gNumStaticSurfaceNodes += collision->info.nodes;
            gSurfaceNodesAllocated += collision->info.nodes;
            gTotalStaticSurfaceData += collision->info.bytes;
            return TRUE;
        }
    }
    return FALSE;
}

/* A frame uses one stable set for collision queries and debug vertex counts.
 * Main-thread simulation continues to see the current published chunks. */
void streamed_collision_render_begin(void) {
    s32 slot;
    for (slot = 0; slot < DMA_MODEL_COUNT; slot++) {
        sRenderedStreamedCollisions[slot] = sStreamedCollisions[slot];
    }
    sStreamedRenderThread = osGetThreadId(NULL);
}

void streamed_collision_render_end(void) {
    sStreamedRenderThread = -1;
}

struct SurfaceNode *streamed_collision_next_cell(u32 *cursor, s32 cellX, s32 cellZ, s32 partition) {
    struct StreamedCollision *volatile *chunks = sStreamedRenderThread == osGetThreadId(NULL)
        ? sRenderedStreamedCollisions : sStreamedCollisions;
    while (*cursor < DMA_MODEL_COUNT) {
        struct StreamedCollision *collision = chunks[(*cursor)++];
        if (collision != NULL) {
            const struct StreamedCollisionInfo *info = &collision->info;
            if (cellX >= info->minCellX && cellX <= info->maxCellX
                && cellZ >= info->minCellZ && cellZ <= info->maxCellZ) {
                s32 width = info->maxCellX - info->minCellX + 1;
                struct SurfaceNode *node = collision->cells[
                    (cellZ - info->minCellZ) * width + cellX - info->minCellX][partition];
                if (node != NULL) {
                    return node;
                }
            }
        }
    }
    return NULL;
}

static s32 streamed_collision_owns(const struct StreamedCollision *collision, const struct Surface *surface) {
    uintptr_t address = (uintptr_t) surface;
    return address >= (uintptr_t) collision->surfaces
        && address < (uintptr_t) (collision->surfaces + collision->info.surfaces);
}

/* Retired allocations remain valid for the old render snapshot. Clear cached
 * pointers on removal and once again when that snapshot has finished. */
static void streamed_collision_forget(const struct StreamedCollision *collision) {
    s32 i;
    if (gMarioState != NULL) {
        if (streamed_collision_owns(collision, gMarioState->floor)) {
            gMarioState->floor = NULL;
            gMarioState->floorHeight = FLOOR_LOWER_LIMIT;
        }
        if (streamed_collision_owns(collision, gMarioState->ceil)) {
            gMarioState->ceil = NULL;
            gMarioState->ceilHeight = CELL_HEIGHT_LIMIT;
        }
        if (streamed_collision_owns(collision, gMarioState->wall)) {
            gMarioState->wall = NULL;
        }
    }
    for (i = 0; i < OBJECT_POOL_CAPACITY; i++) {
        if (streamed_collision_owns(collision, gObjectPool[i].oFloor)) {
            gObjectPool[i].oFloor = NULL;
            gObjectPool[i].oFloorHeight = FLOOR_LOWER_LIMIT;
        }
    }
    if (streamed_collision_owns(collision, sMarioGeometry.currFloor)) {
        sMarioGeometry.currFloor = NULL;
        sMarioGeometry.currFloorHeight = FLOOR_LOWER_LIMIT;
        sMarioGeometry.currFloorType = SURFACE_DEFAULT;
    }
    if (streamed_collision_owns(collision, sMarioGeometry.prevFloor)) {
        sMarioGeometry.prevFloor = NULL;
        sMarioGeometry.prevFloorHeight = FLOOR_LOWER_LIMIT;
        sMarioGeometry.prevFloorType = SURFACE_DEFAULT;
    }
    if (streamed_collision_owns(collision, sMarioGeometry.currCeil)) {
        sMarioGeometry.currCeil = NULL;
        sMarioGeometry.currCeilHeight = CELL_HEIGHT_LIMIT;
        sMarioGeometry.currCeilType = SURFACE_DEFAULT;
    }
    if (streamed_collision_owns(collision, sMarioGeometry.prevCeil)) {
        sMarioGeometry.prevCeil = NULL;
        sMarioGeometry.prevCeilHeight = CELL_HEIGHT_LIMIT;
        sMarioGeometry.prevCeilType = SURFACE_DEFAULT;
    }
}

void streamed_collision_detach(struct StreamedCollision *collision) {
    if (collision == NULL || collision->activeSlot < 0) {
        return;
    }
    sStreamedCollisions[collision->activeSlot] = NULL;
    collision->activeSlot = -1;
    /* Simulation must stop using old cached floors immediately. A preempted
     * renderer may still hold them, so the allocation is retained separately. */
    streamed_collision_forget(collision);
    gNumStaticSurfaces -= collision->info.surfaces;
    gSurfacesAllocated -= collision->info.surfaces;
    gNumStaticSurfaceNodes -= collision->info.nodes;
    gSurfaceNodesAllocated -= collision->info.nodes;
    gTotalStaticSurfaceData -= collision->info.bytes;
}

void streamed_collision_release(struct StreamedCollision *collision) {
    if (collision != NULL) {
        /* Clear any camera caches restored by the old render snapshot. */
        streamed_collision_forget(collision);
    }
}

void streamed_collision_reset(void) {
    s32 slot;
    for (slot = 0; slot < DMA_MODEL_COUNT; slot++) {
        struct StreamedCollision *collision = sStreamedCollisions[slot];
        streamed_collision_detach(collision);
    }
}

/** Load the area terrain's surfaces for a given surface type. */
static void load_static_surfaces(TerrainData **data, TerrainData *vertexData, s32 surfaceType, UNUSED RoomData **surfaceRooms) {
    s32 i;
    struct Surface *surface;

    s32 type = surfaceType & SURFACE_MASK_TYPE;
    s32 material = (surfaceType >> 12);

#ifndef ALL_SURFACES_HAVE_FORCE
    s16 hasForce = surface_has_force(type);
#endif
    s32 flags = surf_has_no_cam_collision(type);

    s32 numSurfaces = *(*data)++;

    for (i = 0; i < numSurfaces; i++) {
        surface = read_surface_data(vertexData, data, FALSE);
        if (surface != NULL) {
            surface->type = type;
            surface->material = material;
            surface->flags = flags;

            add_surface(surface, FALSE);
        }

#ifdef ALL_SURFACES_HAVE_FORCE
        *data += 4;
#else
        *data += 3;
        if (hasForce) {
            (*data)++;
        }
#endif
    }
}

/**
 * Read the data for vertices for reference by triangles.
 */
static TerrainData *read_vertex_data(TerrainData **data) {
    s32 numVertices = *(*data)++;

    TerrainData *vertexData = *data;
    *data += 3 * numVertices;

    return vertexData;
}

/**
 * Loads in special environmental regions, such as water, poison gas, and JRB fog.
 */
static void load_environmental_regions(TerrainData **data) {
    s32 i;

    gEnvironmentRegions = *data;
    s32 numRegions = *(*data)++;

    for (i = 0; i < numRegions; i++) {
        *data += 5;
        gEnvironmentLevels[i] = *(*data)++;
    }
}

/**
 * Allocate the dynamic surface pool for object collision.
 */
void alloc_surface_pools(void) {
    gDynamicSurfacePool = main_pool_alloc(DYNAMIC_SURFACE_POOL_SIZE, MEMORY_POOL_LEFT);
    gDynamicSurfacePoolEnd = gDynamicSurfacePool;

    gCCMEnteredSlide = FALSE;
    reset_red_coins_collected();
}

#ifdef NO_SEGMENTED_MEMORY
/**
 * Get the size of the terrain data, to get the correct size when copying later.
 */
u32 get_area_terrain_size(TerrainData *data) {
    TerrainData *startPos = data;
    s32 end = FALSE;
    TerrainData terrainLoadType;
    s32 numVertices;
    s32 numRegions;
    s32 numSurfaces;
#ifndef ALL_SURFACES_HAVE_FORCE
    TerrainData hasForce;
#endif

    while (!end) {
        terrainLoadType = *data++;

        switch (terrainLoadType) {
            case TERRAIN_LOAD_VERTICES:
                numVertices = *data++;
                data += 3 * numVertices;
                break;

            case TERRAIN_LOAD_OBJECTS:
                data += get_special_objects_size(data);
                break;

            case TERRAIN_LOAD_ENVIRONMENT:
                numRegions = *data++;
                data += 6 * numRegions;
                break;

            case TERRAIN_LOAD_CONTINUE:
                continue;

            case TERRAIN_LOAD_END:
                end = TRUE;
                break;

            default:
                numSurfaces = *data++;
#ifdef ALL_SURFACES_HAVE_FORCE
                data += 4 * numSurfaces;
#else
                hasForce = surface_has_force(terrainLoadType);
                data += (3 + hasForce) * numSurfaces;
#endif
                break;
        }
    }

    return data - startPos;
}
#endif


/**
 * Process the level file, loading in vertices, surfaces, some objects, and environmental
 * boxes (water, gas, JRB fog).
 */
void load_area_terrain(s32 index, TerrainData *data, RoomData *surfaceRooms, s16 *macroObjects) {
    u32 mask = __osDisableInt();
    PUPPYPRINT_GET_SNAPSHOT();
    s32 terrainLoadType;
    TerrainData *vertexData = NULL;
    u32 surfacePoolData;

    /* Retire the previous area's private chunks before replacing base terrain;
     * any old render snapshot keeps its worker allocations alive. */
    dma_model_reset();
    streamed_collision_reset();

    // Initialize the data for this.
    gEnvironmentRegions = NULL;
    gSurfaceNodesAllocated = 0;
    gSurfacesAllocated = 0;
    bzero(&sCellsUsed, sizeof(sCellsUsed));
    sNumCellsUsed = 0;
    sClearAllCells = TRUE;

    // Clear the static (level) surface partitions for new use.
    bzero(gStaticSurfacePartition, sizeof(gStaticSurfacePartition));
    gTotalStaticSurfaceData = 0;

    // Initialise a new surface pool for this block of static surface data
    gCurrStaticSurfacePool = main_pool_alloc(main_pool_available() - 0x10, MEMORY_POOL_LEFT);
    gCurrStaticSurfacePoolEnd = gCurrStaticSurfacePool;

    // A while loop iterating through each section of the level data. Sections of data
    // are prefixed by a terrain "type." This type is reused for surfaces as the surface
    // type.
    while (TRUE) {
        terrainLoadType = *data++;

        if (TERRAIN_LOAD_IS_SURFACE_TYPE_LOW(terrainLoadType)) {
            load_static_surfaces(&data, vertexData, terrainLoadType, &surfaceRooms);
        } else if (terrainLoadType == TERRAIN_LOAD_VERTICES) {
            vertexData = read_vertex_data(&data);
        } else if (terrainLoadType == TERRAIN_LOAD_OBJECTS) {
            spawn_special_objects(index, &data);
        } else if (terrainLoadType == TERRAIN_LOAD_ENVIRONMENT) {
            load_environmental_regions(&data);
        } else if (terrainLoadType == TERRAIN_LOAD_CONTINUE) {
            continue;
        } else if (terrainLoadType == TERRAIN_LOAD_END) {
            break;
        } else if (TERRAIN_LOAD_IS_SURFACE_TYPE_HIGH(terrainLoadType)) {
            load_static_surfaces(&data, vertexData, terrainLoadType, &surfaceRooms);
            continue;
        }
    }

    if (macroObjects != NULL && *macroObjects != -1) {
        // If the first macro object presetID is within the range [0, 29].
        // Generally an early spawning method, every object is in BBH (the first level).
        if (0 <= *macroObjects && *macroObjects < 30) {
            spawn_macro_objects_hardcoded(index, macroObjects);
        }
        // A more general version that can spawn more objects.
        else {
            spawn_macro_objects(index, macroObjects);
        }
    }

    surfacePoolData = (uintptr_t)gCurrStaticSurfacePoolEnd - (uintptr_t)gCurrStaticSurfacePool;
    gTotalStaticSurfaceData += surfacePoolData;
    main_pool_realloc(gCurrStaticSurfacePool, surfacePoolData);

    gNumStaticSurfaceNodes = gSurfaceNodesAllocated;
    gNumStaticSurfaces = gSurfacesAllocated;
    profiler_collision_update(first);
    __osRestoreInt(mask);
}

/**
 * If not in time stop, clear the surface partitions.
 */
void clear_dynamic_surfaces(void) {
    u32 mask = __osDisableInt();
    PUPPYPRINT_GET_SNAPSHOT();
    if (!(gTimeStopState & TIME_STOP_ACTIVE)) {
        clear_dynamic_surface_references();

        gSurfacesAllocated = gNumStaticSurfaces;
        gSurfaceNodesAllocated = gNumStaticSurfaceNodes;
        gDynamicSurfacePoolEnd = gDynamicSurfacePool;
        if (sClearAllCells) {
            bzero(gDynamicSurfacePartition, sizeof(gDynamicSurfacePartition));
        } else {
            for (u32 i = 0; i < sNumCellsUsed; i++) {
                gDynamicSurfacePartition[sCellsUsed[i].z][sCellsUsed[i].x][sCellsUsed[i].partition] = NULL;
            }
        }
        sNumCellsUsed = 0;
        sClearAllCells = FALSE;
    }
    profiler_collision_update(first);
    __osRestoreInt(mask);
}

/**
 * Applies an object's transformation to the object's vertices.
 */
void transform_object_vertices(TerrainData **data, TerrainData *vertexData) {
    register s32 numVertices = *(*data)++;
    register TerrainData *vertices = *data;

    Mat4 transform;
    mtxf_object(transform,o);

    // Go through all vertices, rotating and translating them to transform the object.
    Vec3f pos;
    while (numVertices--) {
        vec3s_to_vec3f(pos, vertices);
        vertices += 3;

        //! No bounds check on vertex data
        linear_mtxf_mul_vec3_and_translate(transform, vertexData, pos);

        vertexData += 3;
    }

    *data = vertices;
}

/**
 * Load in the surfaces for the o. This includes setting the flags, exertion, and room.
 */
void load_object_surfaces(TerrainData **data, TerrainData *vertexData, u32 dynamic) {
    u32 mask = __osDisableInt();

    s32 i;

    s32 surfaceType = *(*data)++;
    s32 numSurfaces = *(*data)++;

    s32 type = surfaceType & SURFACE_MASK_TYPE;
    s32 material = (surfaceType >> 12);

#ifndef ALL_SURFACES_HAVE_FORCE
    TerrainData hasForce = surface_has_force(type);
#endif

    s32 flags = surf_has_no_cam_collision(type) | (dynamic ? SURFACE_FLAG_DYNAMIC : 0);

    for (i = 0; i < numSurfaces; i++) {
        struct Surface *surface = read_surface_data(vertexData, data, dynamic);

        if (surface != NULL) {
            surface->object = o;
            surface->type = type;
            surface->material = material;

            surface->flags |= flags;
            add_surface(surface, dynamic);
        }

#ifdef ALL_SURFACES_HAVE_FORCE
        *data += 4;
#else
        if (hasForce) {
            *data += 4;
        } else {
            *data += 3;
        }
#endif
    }
    __osRestoreInt(mask);
}

#ifdef AUTO_COLLISION_DISTANCE
static void get_optimal_coll_dist(struct Object *obj) {
    register f32 thisVertDist, maxDist = 0.0f;
    Vec3f v;
    TerrainData *collisionData = o->collisionData;
    obj->oFlags |= OBJ_FLAG_DONT_CALC_COLL_DIST;
    collisionData++;
    register u32 vertsLeft = *(collisionData)++;
    while (vertsLeft) {
        vec3_prod(v, collisionData, obj->header.gfx.scale);
        thisVertDist = vec3_sumsq(v);
        if (thisVertDist > maxDist) maxDist = thisVertDist;
        collisionData += 3;
        vertsLeft--;
    }
    obj->oCollisionDistance = (sqrtf(maxDist) + 100.0f);
}
#endif

static TerrainData sVertexData[600];

/**
 * Transform an object's vertices, reload them, and render the object.
 */
void load_object_collision_model(void) {
    PUPPYPRINT_GET_SNAPSHOT();
    TerrainData *collisionData = o->collisionData;

    Vec3f dist;
    vec3_diff(dist, &o->oPosVec, &gMarioObject->oPosVec);
    f32 sqrLateralDist = sqr(dist[0]) + sqr(dist[2]);

    f32 verticalMarioDiff = gMarioObject->oPosY - o->oPosY;

#ifdef AUTO_COLLISION_DISTANCE
    if (!(o->oFlags & OBJ_FLAG_DONT_CALC_COLL_DIST)) {
        get_optimal_coll_dist(o);
    }
#endif

    // If the object collision is supposed to be loaded more than the
    // drawing distance, extend the drawing range.
    if (o->oCollisionDistance > o->oDrawingDistance) {
        o->oDrawingDistance = o->oCollisionDistance;
    }
    
    s32 inColRadius = (
           (sqrLateralDist < sqr(o->oCollisionDistance))
        && (verticalMarioDiff > 0 || verticalMarioDiff > -o->oCollisionDistance)
        && (verticalMarioDiff < 0 || verticalMarioDiff < o->oCollisionDistance + 2000.f)
    );

    // Update if no Time Stop, in range, and in the current room.
    if (
        !(gTimeStopState & TIME_STOP_ACTIVE)
        && inColRadius
        && !(o->activeFlags & ACTIVE_FLAG_IN_DIFFERENT_ROOM)
    ) {
        collisionData++;
        transform_object_vertices(&collisionData, sVertexData);

        // TERRAIN_LOAD_CONTINUE acts as an "end" to the terrain data.
        while (*collisionData != TERRAIN_LOAD_CONTINUE) {
            load_object_surfaces(&collisionData, sVertexData, TRUE);
        }
    }

    f32 marioDist = o->oDistanceToMario;

    // On an object's first frame, the distance is set to 19000.0f.
    // If the distance hasn't been updated, update it now.
    if (marioDist == 19000.0f) {
        marioDist = dist_between_objects(o, gMarioObject);
    }

    COND_BIT((marioDist < o->oDrawingDistance), o->header.gfx.node.flags, GRAPH_RENDER_ACTIVE);
    profiler_collision_update(first);
}

/**
 * Transform an object's vertices and add them to the static surface pool.
 */
void load_object_static_model(void) {
    u32 mask = __osDisableInt();
    PUPPYPRINT_GET_SNAPSHOT();
    TerrainData *collisionData = o->collisionData;
    u32 surfacePoolData;

    // Initialise a new surface pool for this block of surface data
    gCurrStaticSurfacePool = main_pool_alloc(main_pool_available() - 0x10, MEMORY_POOL_LEFT);
    gCurrStaticSurfacePoolEnd = gCurrStaticSurfacePool;
    gSurfaceNodesAllocated = gNumStaticSurfaceNodes;
    gSurfacesAllocated = gNumStaticSurfaces;

    collisionData++;
    transform_object_vertices(&collisionData, sVertexData);

    // TERRAIN_LOAD_CONTINUE acts as an "end" to the terrain data.
    while (*collisionData != TERRAIN_LOAD_CONTINUE) {
        load_object_surfaces(&collisionData, sVertexData, FALSE);
    }

    surfacePoolData = (uintptr_t)gCurrStaticSurfacePoolEnd - (uintptr_t)gCurrStaticSurfacePool;
    gTotalStaticSurfaceData += surfacePoolData;
    main_pool_realloc(gCurrStaticSurfacePool, surfacePoolData);

    gNumStaticSurfaceNodes = gSurfaceNodesAllocated;
    gNumStaticSurfaces = gSurfacesAllocated;
    profiler_collision_update(first);

    __osRestoreInt(mask);
}

void transform_dynamic_vertices_custom(TerrainData **data, TerrainData *vertexData, void (*func)(Vec3col,Vec3col)) {
    register s32 numVertices = *(*data)++;
    register TerrainData *vertices = *data;

    while (numVertices--) {
        func(vertexData,vertices);

        vertices += 3;
        vertexData += 3;
    }

    *data = vertices;
}

void load_dynamic_surfaces_custom(TerrainData **data, TerrainData *vertexData, u32 dynamic) {
    s32 i;

    s32 surfaceType = *(*data)++;
    s32 numSurfaces = *(*data)++;

    s32 type = surfaceType & SURFACE_MASK_TYPE;
    s32 material = (surfaceType >> 12);

#ifndef ALL_SURFACES_HAVE_FORCE
    TerrainData hasForce = surface_has_force(surfaceType);
#endif

    for (i = 0; i < numSurfaces; i++) {
        struct Surface *surface = read_surface_data(vertexData, data, dynamic);

        if (surface != NULL) {
            surface->type = type;
            surface->material = material;

            add_surface(surface, dynamic);
        }

#ifdef ALL_SURFACES_HAVE_FORCE
        *data += 4;
#else
        if (hasForce) {
            *data += 4;
        } else {
            *data += 3;
        }
#endif
    }
}

void load_collision_custom_transform(Collision * col, void (*func)(Vec3col,Vec3col)) {
    TerrainData *collisionData = col;

    collisionData++;
    transform_dynamic_vertices_custom(&collisionData, sVertexData, func);
    while (*collisionData != TERRAIN_LOAD_CONTINUE) {
        load_dynamic_surfaces_custom(&collisionData, sVertexData, TRUE);
    }
}
