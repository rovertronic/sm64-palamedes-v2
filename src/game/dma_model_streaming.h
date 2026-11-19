#ifndef DMA_MODEL_STREAMING_H
#define DMA_MODEL_STREAMING_H

#include "types.h"

#define DMA_MODEL_COUNT 10

struct GraphNode;

struct DmaModelEntry {
    const u8 *romStart;
    const u8 *romEnd;
    const u8 *segmentStart; /* Linked start within shared segment 0x0F. */
    const Gfx *displayList; /* Linked segment-0x0F address, not a RAM pointer. */
};

#define DMA_MODEL(name, rootDisplayList) \
    { _dmaModel_##name##RomStart, _dmaModel_##name##RomEnd, \
      _dmaModel_##name##Start, rootDisplayList }

extern const struct DmaModelEntry sDmaModelTable[DMA_MODEL_COUNT];
extern Gfx * volatile sDmaLoadedDlTable[DMA_MODEL_COUNT];

void dma_model_streaming_init(void);
void dma_model_load(int id);
void dma_model_unload(int id);
void dma_model_reset(void);
void dma_model_check_volumes(void);
void dma_model_frame_submitted(void);
Gfx *geo_dma_model(s32 callContext, struct GraphNode *node, Mat4 *mtx);

#endif
