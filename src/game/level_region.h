#include "types.h"
#include "event_dialog.h"

#ifndef LEVEL_REGION_H
#define LEVEL_REGION_H

typedef struct {
    Vec3f pos;
    BehaviorScript * behavior;
    u32 bparams;
    EventData * event;
    u32 regionFlags;
    u16 modelId;
    u16 angle;
} regionObject;

#define MAX_REGION_OBJECTS 1000

enum regionIds {
    REGION_COUNT
};

extern u32 gRegionFlags;
extern Bool8 gRegionDisableRender;
extern u8 gRegionRedCoins[33];

void region_logic(void);
void region_init_object(Vec3f pos, s16 angle, u32 bparams, u16 modelId, BehaviorScript * behavior, EventData * event);
Gfx *geo_region(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx);
Gfx *geo_region_lod(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx);
Gfx *geo_region_revert(s32 callContext, UNUSED struct GraphNode *node, UNUSED Mat4 *mtx);

#endif