#ifndef DMA_MODEL_STREAMING_H
#define DMA_MODEL_STREAMING_H

#include "types.h"

#define DMA_MODEL_COUNT 10

/* Persistent surfaces, private cells and nodes, separate from the raw asset
 * pool and the engine's dynamic collision buffer. */
#define DMA_MODEL_COLLISION_POOL_SIZE 0x40000

struct GraphNode;

struct DmaModelEntry {
    const u8 *romStart;
    const u8 *romEnd;
    const u8 *segmentStart; /* Linked start within shared segment 0x0F. */
    const Gfx *displayList; /* Linked segment-0x0F address, not a RAM pointer. */
    const Collision *collision; /* Optional symbol from the same export folder. */
    Vec3f collisionOffset; /* Fixed world placement; match the geo translation. */
};

#define DMA_MODEL(name, rootDisplayList) \
    { _dmaModel_##name##RomStart, _dmaModel_##name##RomEnd, \
      _dmaModel_##name##Start, rootDisplayList, NULL, { 0, 0, 0 } }

#define DMA_MODEL_WITH_COLLISION(name, rootDisplayList, collisionSymbol) \
    { _dmaModel_##name##RomStart, _dmaModel_##name##RomEnd, \
      _dmaModel_##name##Start, rootDisplayList, collisionSymbol, { 0, 0, 0 } }

#define DMA_MODEL_WITH_COLLISION_AT(name, rootDisplayList, collisionSymbol, x, y, z) \
    { _dmaModel_##name##RomStart, _dmaModel_##name##RomEnd, \
      _dmaModel_##name##Start, rootDisplayList, collisionSymbol, { x, y, z } }

enum DmaModelStatus {
    DMA_MODEL_STATUS_UNLOADED,
    DMA_MODEL_STATUS_LOADING,
    DMA_MODEL_STATUS_READY,
    DMA_MODEL_STATUS_NO_MEMORY,
    DMA_MODEL_STATUS_BAD_DATA,
    DMA_MODEL_STATUS_DMA,                 /* 5: DMA submitted or being retried. */
    DMA_MODEL_STATUS_BUILDING_COLLISION,  /* 6: worker converting collision. */
    DMA_MODEL_STATUS_WAITING_FOR_INSTALL  /* 7: prepared; game must publish it. */
};

extern const struct DmaModelEntry sDmaModelTable[DMA_MODEL_COUNT];
extern Gfx * volatile sDmaLoadedDlTable[DMA_MODEL_COUNT];
extern volatile enum DmaModelStatus sDmaModelStatus[DMA_MODEL_COUNT];

void dma_model_streaming_init(void);
void dma_model_load(int id);
void dma_model_unload(int id);
void dma_model_reset(void);
void dma_model_check_volumes(void);
void dma_model_frame_submitted(void);
void dma_model_update(void);
void dma_model_collision_render_begin(void);
void dma_model_collision_render_end(void);
Gfx *geo_dma_model(s32 callContext, struct GraphNode *node, Mat4 *mtx);

#endif
