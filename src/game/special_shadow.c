#include <PR/ultratypes.h>
#include <PR/gbi.h>
#include <PR/mbi.h>

u8 special_shadow_texture_rgba16[4096];

Gfx dl_shadow_special[] = {
    gsDPPipeSync(),
    gsSPClearGeometryMode(G_LIGHTING | G_CULL_BACK),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),

    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_MODULATEIFADEA2, G_CC_MODULATEIFADEA2),
    gsDPLoadTextureBlock(special_shadow_texture_rgba16, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, (G_TX_WRAP | G_TX_MIRROR), (G_TX_WRAP | G_TX_MIRROR), 6, 6, G_TX_NOLOD, G_TX_NOLOD),
    gsSPEndDisplayList(),
};

static const Vtx vertex_special_shadow[] = {
    {{{    -1,      0,     -1}, 0, { (-2048+2048)/2,  (-2048+2048)/2}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     1,      0,     -1}, 0, { ( 2048+2048)/2,  (-2048+2048)/2}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    -1,      0,      1}, 0, { (-2048+2048)/2,  ( 2048+2048)/2}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     1,      0,      1}, 0, { ( 2048+2048)/2,  ( 2048+2048)/2}, {0xff, 0xff, 0xff, 0xff}}},
};

Gfx dl_shadow_special_tri[] = {
    gsSPVertex(vertex_special_shadow, 4, 0),
    gsSP2Triangles( 0,  2,  1, 0x0,  1,  2,  3, 0x0),
    gsDPPipeSync(),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsSPSetGeometryMode(G_LIGHTING | G_CULL_BACK),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPEndDisplayList(),
};

#include "types.h"
#include "engine/math_util.h"
#include "special_shadow.h"
#include "segment2.h"
#include "game_init.h"
#include "ingame_menu.h"
#include "actors/group0.h"
#include "level_update.h"
#include "object_list_processor.h"
#include "config.h"
#include "config/config_world.h"

SpecialShadowPart gSpecialShadowPartList[SSP_COUNT]={
    [SSP_HEAD].dl = shadow_head_shadow_head_mesh,
    [SSP_LEFT_FOOT].dl = shadow_foot_2_shadow_foot_2_mesh,
    [SSP_RIGHT_FOOT].dl = shadow_foot_2_shadow_foot_2_mesh,
    [SSP_LEFT_HAND].dl = shadow_hand_shadow_hand_mesh,
    [SSP_RIGHT_HAND].dl = shadow_hand_shadow_hand_mesh,

    [SSP_LEG_1_R].dl = shadow_limb_shadow_limb_mesh,
    [SSP_LEG_2_R].dl = shadow_limb_shadow_limb_mesh,
    [SSP_LEG_1_L].dl = shadow_limb_shadow_limb_mesh,
    [SSP_LEG_2_L].dl = shadow_limb_shadow_limb_mesh,

    [SSP_ARM_1_R].dl = shadow_limb_shadow_limb_mesh,
    [SSP_ARM_2_R].dl = shadow_limb_shadow_limb_mesh,
    [SSP_ARM_1_L].dl = shadow_limb_shadow_limb_mesh,
    [SSP_ARM_2_L].dl = shadow_limb_shadow_limb_mesh,

    [SSP_TORSO].dl = shadow_torso_shadow_torso_mesh,
};

Vp gTextureViewport = { {
    { 128, 128, 511, 0 },
    { 128, 128, 511, 0 },
} };

#define SHADOW_VIEWPORT_SCALE 200.0f/WORLD_SCALE
void special_shadow_generate(void) {
    if (gMarioObject == NULL) {
        return;
    }

    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
    if (matrix == NULL) {
        return;
    }
    create_dl_identity_matrix();
    guOrtho(matrix, -SHADOW_VIEWPORT_SCALE, SHADOW_VIEWPORT_SCALE, -SHADOW_VIEWPORT_SCALE, SHADOW_VIEWPORT_SCALE, -SHADOW_VIEWPORT_SCALE, SHADOW_VIEWPORT_SCALE, 1.0f);
    // Should produce G_RDPHALF_1 in Fast3D
    gSPPerspNormalize(gDisplayListHead++, 0xFFFF);
    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);

    Mtx *marioScaleMtx = (Mtx *) alloc_display_list(sizeof(Mtx));
    guScale(marioScaleMtx,4.f,4.f,4.f);
    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(marioScaleMtx), G_MTX_MUL | G_MTX_NOPUSH);

    // Set AUX framebuffer
    gDPPipeSync(gDisplayListHead++);
    gDPSetColorImage(gDisplayListHead++, G_IM_FMT_I, G_IM_SIZ_8b, 64, special_shadow_texture_rgba16);
    gSPViewport(gDisplayListHead++,VIRTUAL_TO_PHYSICAL(&gTextureViewport));

    // Clear AUX framebuffer
    gDPPipeSync(gDisplayListHead++);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
    gDPSetFillColor(gDisplayListHead++, 0);
    gDPFillRectangle(gDisplayListHead++, 0, 0, 63, 63);
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);

    for (int i = 0; i < SSP_COUNT; i ++) {
        // Render Part
        Mtx *partMtx = (Mtx *) alloc_display_list(sizeof(Mtx));
        mtxf_to_mtx(partMtx,gSpecialShadowPartList[i].transform);
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(partMtx),  G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);

        gDPPipeSync(gDisplayListHead++);
        gSPDisplayList(gDisplayListHead++, gSpecialShadowPartList[i].dl);

        gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
    }

    // Revert
    gDPPipeSync(gDisplayListHead++);
    gDPSetColorImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, gPhysicalFramebuffers[sRenderingFramebuffer]);

    //for (int x = 0; x < 64; x++) {
    //    for (int y = 0; y < 64; y++) {
    //        if (x == 0 || x == 63 || y == 0 || y == 63) {
    //            special_shadow_texture_rgba16[(y*32)+x] = 0xFF;
    //        }
    //    }
    //}
}

Mat4 shadowPartUpright;

void special_shadow_update_part(int index, Mat4 partTransform) {
    Vec3f from = {0.0f,0.0f,1.0f};
    Vec3f to = {0.0f,1.0f,0.0f};
    Quat uprightRot;
    quat_fromto(uprightRot,from,to);
    mtxf_from_quat(uprightRot,shadowPartUpright);

    Mat4 finalTransform;
    mtxf_copy(finalTransform,partTransform);
    finalTransform[3][0] -= gMarioState->marioObj->header.gfx.posLerp[0];
    finalTransform[3][1] -= gMarioState->marioObj->header.gfx.posLerp[1];
    finalTransform[3][2] -= gMarioState->marioObj->header.gfx.posLerp[2];

    finalTransform[3][0] *= .5f;
    finalTransform[3][1] *= .5f;
    finalTransform[3][2] *= .5f;

    Mat4 finalTransform2;
    mtxf_mul(finalTransform2,finalTransform,shadowPartUpright);

    mtxf_copy(gSpecialShadowPartList[index].transform,finalTransform2);
}
