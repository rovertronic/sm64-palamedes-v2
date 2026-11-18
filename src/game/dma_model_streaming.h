#include "types.h"

#ifndef DMA_MODEL_STREAMING_H
#define DMA_MODEL_STREAMING_H

void dma_model_load(int id);
void dma_model_unload(int id);
void dma_model_check_volumes(void);
Gfx *geo_dma_model(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx);

#endif