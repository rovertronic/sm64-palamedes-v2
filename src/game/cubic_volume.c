#include "cubic_volume.h"
#include "dma_model_streaming.h"
#include "engine/math_util.h"

#include "sm64.h"
#include "area.h"
#include "audio/external.h"
#include "behavior_actions.h"
#include "behavior_data.h"
#include "camera.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game_init.h"
#include "interaction.h"
#include "level_table.h"
#include "level_update.h"
#include "main.h"
#include "mario.h"
#include "mario_actions_airborne.h"
#include "mario_actions_automatic.h"
#include "mario_actions_cutscene.h"
#include "mario_actions_moving.h"
#include "mario_actions_object.h"
#include "mario_actions_stationary.h"
#include "mario_actions_submerged.h"
#include "mario_misc.h"
#include "mario_step.h"
#include "memory.h"
#include "object_fields.h"
#include "object_helpers.h"
#include "object_list_processor.h"
#include "print.h"
#include "save_file.h"
#include "sound_init.h"
#include "rumble_init.h"

CubicVolume sCubicVolumeList[CUBIC_VOLUME_MAX];
int sCubicVolumeCount = 0;

void cubic_volume_reset(void) {
    dma_model_reset();
    sCubicVolumeCount = 0;
}

CubicVolume * cubic_volume_add(void) {
    if (sCubicVolumeCount < CUBIC_VOLUME_MAX) {
        sCubicVolumeCount++;
    }
    return &sCubicVolumeList[sCubicVolumeCount-1];
}

s32 cubic_volume_point_inside_volume(Vec3f p, CubicVolume * v) {
    Vec3f rp;
    vec3f_diff(rp,p,v->pos);

    Mat4 rotMat;
    Quat invR;
    quat_inverse(invR,v->rot);
    mtxf_from_quat(invR,rotMat);

    Vec3f rpr;
    linear_mtxf_mul_vec3(rotMat, rpr, rp);

    // Debug Visualization

    /*
    Mat4 debugMat;
    mtxf_from_quat(v->rot,debugMat);
    vec3f_copy(debugMat[3],v->pos);
    mtxf_scale_vec3f(debugMat, debugMat, v->scale);
    debugTransform * dt = debug_mtxf(debugMat);
    dt->flags |= WSVD_CUBE;
    */

    for (int i = 0; i < 3; i++) {
        if (rpr[i] > (v->scale[i]*100.0f)) {return FALSE;}
        if (rpr[i] < (v->scale[i]*-100.0f)) {return FALSE;}
    }

    return TRUE;
}

CubicVolume * cubic_volume_check_one(Vec3f pos, int cubicVolumeType) {
    for (int i = 0; i < sCubicVolumeCount; i++) {
        CubicVolume * v = &sCubicVolumeList[i];
        if (v->type == cubicVolumeType) {
            if (cubic_volume_point_inside_volume(pos,v)) {
                return v;
            }
        }
    }
    return NULL;
}

void cubic_volume_check_all(Vec3f pos, int cubicVolumeType, void (* iterateVolume)(CubicVolume *)) {
    for (int i = 0; i < sCubicVolumeCount; i++) {
        CubicVolume * v = &sCubicVolumeList[i];
        if (v->type == cubicVolumeType) {
            if (cubic_volume_point_inside_volume(pos,v)) {
                iterateVolume(v);
            }
        }
    }
}