#include <PR/ultratypes.h>
#include <PR/gbi.h>
#include "actors/group0.h"
#include "rendering_graph_node.h"
#include "frame_lerp.h"
#include "memory.h"
#include "camera.h"
#include "object_list_processor.h"
#include "emutest.h"
#include "engine/surface_collision.h"
#include "neo_shadow.h"
#include "worldspace_visual_debug.h"
#include "segment2.h"
#include "level_update.h"
#include "level_region.h"
#include "special_shadow.h"

static const Vtx vertex_neo_shadow[] = {
    {{{    -1,      0,     -1}, 0, { -2048,  -2048}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     1,      0,     -1}, 0, {  2048,  -2048}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    -1,      0,      1}, 0, { -2048,   2048}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     1,      0,      1}, 0, {  2048,   2048}, {0xff, 0xff, 0xff, 0xff}}},
};

Gfx dl_neo_shadow_mesh[] = {
    gsSPVertex(vertex_neo_shadow, 4, 0),
    gsSP2Triangles( 0,  2,  1, 0x0,  1,  2,  3, 0x0),
    gsSPEndDisplayList(),
};

Gfx dl_neo_shadow_end[] = {
    gsSPSetGeometryMode(G_LIGHTING | G_CULL_BACK),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPEndDisplayList(),  
};

#include "engine/math_util.h"

neoShadow sNeoShadowList[MAX_NEO_SHADOWS];
Gfx * sNeoShadowTextures[NEOSHADOW_RENDER_PASS_CT] = {
    [NEOSHADOW_TYPE_CICRLE] = dl_shadow_circle,
    [NEOSHADOW_TYPE_SQUARE] = dl_shadow_square,
    [NEOSHADOW_TYPE_COIN] = dl_shadow_coin,
    [NEOSHADOW_TYPE_MARIO] = dl_shadow_special,
};

s32 neoshadow_dim_with_distance(f32 distFromFloor) {
    u8 solidity = 180;
    if (solidity < 121) {
        return solidity;
    } else if (distFromFloor <= 0.0f) {
        return solidity;
    } else if (distFromFloor >= 600.0f) {
        return 120;
    } else {
        return (((120 - solidity) * distFromFloor) / 600.0f) + (f32) solidity;
    }
}

f32 neoshadow_scale_with_distance(f32 distFromFloor) {
    f32 initial = 1.0f;
    if (distFromFloor <= 0.0f) {
        return initial;
    } else if (distFromFloor >= 600.0f) {
        return initial * 0.5f;
    } else {
        return initial * (1.0f - ((distFromFloor * 0.5f) / 600.0f));
    }
}

void neoshadow_cast(neoShadow * s, struct Object * obj) {
    struct Surface * floor;
    f32 h = find_floor(obj->oPosX,obj->oPosY+10.0f,obj->oPosZ,&floor);
    if (floor) {
        s->pos[0] = obj->oPosX;
        s->pos[1] = h;
        s->pos[2] = obj->oPosZ;

        surface_normal_to_vec3f(s->normal, floor);

        f32 baseScale = (f32)s->baseScale;
        f32 scale = neoshadow_scale_with_distance(obj->oPosY - h) * baseScale;

        s->scale[0] = scale * obj->header.gfx.scale[0];
        s->scale[1] = scale * obj->header.gfx.scale[2];
    }
}

void neoshadow_obj_create(struct Object * obj, s16 scale, u8 type) {
    int i = 0;
    while (sNeoShadowList[i].initialized) {
        i++;
    }
    if (i >= MAX_NEO_SHADOWS) {
        return;
    }
    sNeoShadowList[i].owner = obj;
    neoshadow_cast(&sNeoShadowList[i],obj);
    vec3f_copy(sNeoShadowList[i].posLerp,sNeoShadowList[i].pos);
    sNeoShadowList[i].baseScale = scale/2;
    sNeoShadowList[i].type = type;
    sNeoShadowList[i].yaw = 0;
    sNeoShadowList[i].overridden = FALSE;
    sNeoShadowList[i].initialized = TRUE;
    obj->shadow = &sNeoShadowList[i];
}

void neoshadow_reset(void) {
    for (int i = 0; i < MAX_NEO_SHADOWS; i++) {
        sNeoShadowList[i].initialized = FALSE;
    }
}

int neoshadow_count(void) {
    int ct = 0;
    for (int i = 0; i < MAX_NEO_SHADOWS; i++) {
        if (sNeoShadowList[i].initialized) {
            ct++;
        }
    }
    return ct;
}

void neoshadow_render(void) {
    Gfx * dlh = alloc_display_list(sizeof(Gfx)* ((10*neoshadow_count())+10) );
    Gfx * dl = dlh;
    Gfx * shadowMesh = dl_neo_shadow_mesh;

    for (int j = 0; j < NEOSHADOW_RENDER_PASS_CT; j++) {
        gSPDisplayList(dlh++,sNeoShadowTextures[j]);
        if (j == NEOSHADOW_TYPE_MARIO) {
            shadowMesh = dl_shadow_special_tri;
        }
        for (int i = 0; i < MAX_NEO_SHADOWS; i++) {
            neoShadow * s = &sNeoShadowList[i];
            if (s->type == j && s->initialized && (!(s->owner->header.gfx.node.flags & GRAPH_RENDER_INVISIBLE)) && s->scale[0] > 0.0f) {
                frameLerpPos(s->pos,s->posLerp);
                s->scaleLerp[0] = frameLerpFloat(s->scale[0],s->scaleLerp[0]);
                s->scaleLerp[1] = frameLerpFloat(s->scale[1],s->scaleLerp[1]);
                s->yawLerp = frameLerpShort(s->yaw,s->yawLerp);

                f32 ydist = s->owner->oPosY-s->pos[1];
                Mat4 shadowMat;
                Vec3f scalev = {s->scaleLerp[0],s->scaleLerp[0],s->scaleLerp[1]};
                mtxf_shadow(shadowMat,s->normal,s->posLerp,scalev,s->yawLerp);

                Mtx *mtx = alloc_display_list(sizeof(*mtx));
                mtxf_to_mtx(mtx, shadowMat);

                gDPSetEnvColor(dlh++,0,0,0,neoshadow_dim_with_distance(ydist));
                gSPMatrix(dlh++, VIRTUAL_TO_PHYSICAL(mtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
                gSPDisplayList(dlh++,shadowMesh);
            }
        }
    }
    gSPDisplayList(dlh++,dl_neo_shadow_end);
    gSPEndDisplayList(dlh++);

    geo_append_display_list(dl, LAYER_TRANSPARENT_DECAL);
}

void neoshadow_logic(void) {
    for (int i = 0; i < MAX_NEO_SHADOWS; i++) {
        neoShadow * s = &sNeoShadowList[i];
        if (s->initialized && !s->overridden) {
            struct Object * obj = s->owner;
            if (obj->regionFlags > 0 && (!(obj->regionFlags & gRegionFlags))) {
                // If object not in loaded region, unload shadow
                s->initialized = FALSE;
                obj->shadow = NULL;
                continue;
            }

            f32 dsqrd = sqr(gMarioState->pos[0] - s->pos[0]) + sqr(gMarioState->pos[1] - s->pos[1]) + sqr(gMarioState->pos[2] - s->pos[2]);
            if (dsqrd <= 4000000.0f) {
                neoshadow_cast(s,obj);
            } else {
                if (s->ownerLastPos[0] != obj->oPosX || s->ownerLastPos[1] != obj->oPosZ) {
                    s->ownerLastPos[0] = obj->oPosX;
                    s->ownerLastPos[1] = obj->oPosZ;

                    neoshadow_cast(s,obj);
                }
            }
        }
    }
}