#include "dma_model_streaming.h"
#include "level_update.h"
#include "cubic_volume.h"

// Dma model streaming system assumes

Gfx * sDmaModelTable[10] = {
    // Contains: ROM Addresses to Display Lists
    // Check: bin/dma_model.inc.c
};

Gfx * sDmaLoadedDlTable[10];

void dma_model_load(UNUSED int id) {
    // Add model loading task to thread
    // Use sDmaModelTable[id]
    // Thread: runs dma_read, once complete, set sDmaLoadedDlTable[ id ] to the allocated display list pointer
}

void dma_model_unload(UNUSED int id) {
    // Add model freeing task to thread
    // Frees data at sDmaLoadedDlTable[ id ] and sets it to null
}

void dma_model_load_when_mario_in_region(CubicVolume * cv) {
    dma_model_load(cv->param);
}

void dma_model_check_volumes(void) {
    cubic_volume_check_all(gMarioState->pos,VOLUME_TYPE_DMA_MODEL,dma_model_load_when_mario_in_region);
    // Run dma_model_unload() on any volumes mario is not currently in
}

Gfx *geo_dma_model(UNUSED s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    UNUSED struct GraphNodeGenerated * nodeGen = (struct GraphNodeGenerated *)node;
    UNUSED struct GraphNodeDisplayList * dl = (struct GraphNodeDisplayList *)node->next;

    // Render sDmaLoadedDlTable[ nodeGen->paramter ] when not NULL.
    // Utilize gSPSegment to avoid having to manually shift all segmented addresses to virtual addresses.

    return NULL;
}