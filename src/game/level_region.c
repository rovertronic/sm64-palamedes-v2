#include "sm64.h"
#include "game_init.h"
#include "level_region.h"
#include "src/engine/graph_node.h"
#include "cubic_volume.h"
#include "level_update.h"
#include "engine/math_util.h"

regionObject sRegionObjectList[MAX_REGION_OBJECTS];
int sRegionObjectCount = 0;

u32 gRegionFlags = 0;
u32 sPrevRegionFlags = 0;
regionObject * sRegionObject = NULL;
Bool8 gRegionDisableRender = FALSE;

u8 gRegionRedCoins[33];

void region_flag_volume(CubicVolume * v) {
    gRegionFlags |= (1 << v->param);
}

void region_flag_object(CubicVolume * v) {
    sRegionObject->regionFlags |= (1 << v->param);
}

void region_init_object(Vec3f pos, s16 angle, u32 bparams, u16 modelId, BehaviorScript * behavior, EventData * event) {
    sRegionObject = &sRegionObjectList[sRegionObjectCount];

    vec3f_copy(sRegionObject->pos,pos);
    sRegionObject->angle = angle;
    sRegionObject->bparams = bparams;
    sRegionObject->modelId = modelId;
    sRegionObject->behavior = behavior;
    sRegionObject->event = event;
    cubic_volume_check_all(pos,VOLUME_TYPE_REGION,region_flag_object);

    sRegionObjectCount++;
}

void region_logic(void) {
    gRegionFlags = 0;
    cubic_volume_check_all(gMarioState->pos,VOLUME_TYPE_REGION,region_flag_volume);

    if (sPrevRegionFlags != gRegionFlags) {
        // Spawn region objects
        for (int i = 0; i < sRegionObjectCount; i++) {
            regionObject * ro = &sRegionObjectList[i];

            if ((ro->regionFlags & gRegionFlags)&&(!(ro->regionFlags & sPrevRegionFlags))) {
                struct Object * spawnedObj = spawn_object(gMarioState->marioObj,ro->modelId,ro->behavior);
                vec3f_copy(&spawnedObj->oPosVec,ro->pos);
                spawnedObj->oMoveAngleYaw = ro->angle * 182;
                spawnedObj->oFaceAngleYaw = ro->angle * 182;

                spawnedObj->oFaceAnglePitch = 0;
                spawnedObj->oMoveAnglePitch = 0;
                spawnedObj->oFaceAngleRoll = 0;
                spawnedObj->oMoveAngleRoll = 0;

                spawnedObj->regionFlags = ro->regionFlags;

                for (int i = 0; i < 32; i++) {
                    if (spawnedObj->regionFlags & (1 << i)) {
                        spawnedObj->primaryRegionId = i;
                        break;
                    }
                }

                spawnedObj->oBehParams = ro->bparams;
                spawnedObj->oBehParams2ndByte = GET_BPARAM2(ro->bparams);
            }
        }

        // Set local region red coin counter back to 0
        for (int i = 0; i < 32; i++) {
            // If leaving region i
            if ( (sPrevRegionFlags & (1 << i)) && (!(gRegionFlags & (1 << i))) ) {
                gRegionRedCoins[i] = 0;
            }
        }


        sPrevRegionFlags = gRegionFlags;
    }

    debug_u32(&gRegionFlags,"Region flags");
}

Gfx *geo_region(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated * self = (struct GraphNodeGenerated *)node;
    if (callContext == GEO_CONTEXT_RENDER) {
        if (gRegionFlags & (1 << self->parameter)) {
            gRegionDisableRender = FALSE;
        } else {
            gRegionDisableRender = TRUE;
        }
    }
    return NULL;
}

Gfx *geo_region_revert(s32 callContext, UNUSED struct GraphNode *node, UNUSED Mat4 *mtx) {
    if (callContext == GEO_CONTEXT_RENDER) {
        gRegionDisableRender = FALSE;
    }
    return NULL;
}

Gfx *geo_region_lod(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated * self = (struct GraphNodeGenerated *)node;
    struct GraphNodeDisplayList * dl = (struct GraphNodeDisplayList *)node->next;

    if (callContext == GEO_CONTEXT_AREA_LOAD) {
        self->dlCopy = dl->displayList;
    }
    if (callContext == GEO_CONTEXT_RENDER) {
        if (gRegionFlags & (1 << self->parameter)) {
            dl->displayList = NULL;
        } else {
            dl->displayList = self->dlCopy;
        }
    }
    return NULL;
}