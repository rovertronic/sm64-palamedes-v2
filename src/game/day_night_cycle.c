#include "day_night_cycle.h"
#include <PR/ultratypes.h>
#include <PR/gbi.h>
#include "actors/group0.h"
#include "rendering_graph_node.h"
#include "frame_lerp.h"
#include "memory.h"
#include "camera.h"
#include "object_list_processor.h"
#include "emutest.h"
#include "src/engine/graph_node.h"
#include "debug_tracker.h"

// This day/night system assumes sun and moon are 180 degrees apart, location at equator

#define DAY_NIGHT_SUNRISE_TIME 8 // Hour that the sun comes up
#define DAY_ADVANCE_PER_FRAME 0.0001f // Amount of day passed per frame


/* 1.0f = 24 HOURS */
f32 sDayProgress = 0.0f;
Vec3f sSunDirection = {-127.f, 0.f , .0f};
Vec3f sDayLightColor = {1.0f,1.0f,1.0f};

f32 sDayDiffuseGradientList[][4] = {
    // Time, LIGHT R, G, B,

    /* bumper */
    {-1.00f,   0.0f, 0.0f, 0.0},

    /* morning, pinkish*/
    {0.00f,   0.0f, 0.0f, 0.0f},
    {0.05f,   1.0f, 0.1f, 0.5f},

    /* day, fullbright */
    {0.10f,   1.0f, 1.0f, 1.0f},
    {0.40f,   1.0f, 1.0f, 1.0f},

    /* evening, orange*/
    {0.43f,   1.0f, 0.5f, 0.0f},
    {0.46f,   1.0f, 0.3f, 0.0f},
    {0.50f,   0.0f, 0.0f, 0.0f},

    /* night, subtle moon*/
    {0.55f,   0.4f, 0.4f, 0.4f},
    {0.95f,   0.4f, 0.4f, 0.4f},

    /* warp around to start*/
    {1.00f,   0.0f, 0.0f, 0.0f},

    /* bumper */
    {2.00f,   0.0f, 0.0f, 0.0f},
};

f32 sDayAmbientGradientList[][4] = {
    // Time, AMB R G B

    /* bumper */
    {-1.00f,  .2f, .2f, .4f},

    /* morning, pinkish*/
    {0.00f,   .2f, .2f, .4f},
    {0.05f,   .5f, .5f, .5f},

    /* day, fullbright */
    {0.10f,   .5f, .5f, .5f},
    {0.40f,   .5f, .5f, .5f},

    /* evening, orange*/
    {0.43f,   .5f, .5f, .5f},
    {0.46f,   .5f, .5f, .5f},
    {0.50f,   .2f, .2f, .4f},

    /* night, subtle moon*/
    {0.55f,   .2f, .2f, .4f},
    {0.95f,   .2f, .2f, .4f},

    /* warp around to start*/
    {1.00f,   .2f, .2f, .4f},

    /* bumper */
    {2.00f,   .2f, .2f, .4f},
};

Lights1 sLightInfo = gdSPDefLights1(
    0x7F, 0x7F, 0x7F, //amb
    255, 100, 0, //color
    0x00, 0x00, 0x00
);

void day_night_set_light(void) {
    Gfx * dl = alloc_display_list(sizeof(Gfx) * 4);
    Gfx * dlh = dl;

    Lights1* curLight = (Lights1*)alloc_display_list(sizeof(Lights1));
    bcopy(&sLightInfo, curLight, sizeof(Lights1));

    curLight->l->l.dir[0] = (s8)(sSunDirection[0]);
    curLight->l->l.dir[1] = (s8)(sSunDirection[1]);
    curLight->l->l.dir[2] = (s8)(sSunDirection[2]);

    curLight->l->l.col[0] = (u8)(day_night_get_color_channel(0,sDayDiffuseGradientList));
    curLight->l->l.col[1] = (u8)(day_night_get_color_channel(1,sDayDiffuseGradientList));
    curLight->l->l.col[2] = (u8)(day_night_get_color_channel(2,sDayDiffuseGradientList));

    curLight->l->l.colc[0] = (u8)(day_night_get_color_channel(0,sDayDiffuseGradientList));
    curLight->l->l.colc[1] = (u8)(day_night_get_color_channel(1,sDayDiffuseGradientList));
    curLight->l->l.colc[2] = (u8)(day_night_get_color_channel(2,sDayDiffuseGradientList));

    curLight->a.l.col[0] = (u8)(day_night_get_color_channel(0,sDayAmbientGradientList));
    curLight->a.l.col[1] = (u8)(day_night_get_color_channel(1,sDayAmbientGradientList));
    curLight->a.l.col[2] = (u8)(day_night_get_color_channel(2,sDayAmbientGradientList));

    curLight->a.l.colc[0] = (u8)(day_night_get_color_channel(0,sDayAmbientGradientList));
    curLight->a.l.colc[1] = (u8)(day_night_get_color_channel(1,sDayAmbientGradientList));
    curLight->a.l.colc[2] = (u8)(day_night_get_color_channel(2,sDayAmbientGradientList));

    gSPSetLights1(dlh++, (*curLight));
    gSPEndDisplayList(dlh++);

    geo_append_display_list(dl, LAYER_OPAQUE);
    geo_append_display_list(dl, LAYER_TRANSPARENT_DECAL);
    geo_append_display_list(dl, LAYER_TRANSPARENT);
    geo_append_display_list(dl, LAYER_ALPHA);
}

Gfx *geo_day_night_light(s32 callContext, UNUSED struct GraphNode *node, UNUSED Mat4 *mtx) {
    if (callContext == GEO_CONTEXT_RENDER) {
        day_night_set_light();
    }
    return NULL;
}

#include "src/engine/math_util.h"

void day_night_restore_object_light(void) {
    Gfx * dl = alloc_display_list(sizeof(Gfx) * 4);
    Gfx * dlh = dl;

    Lights1* curLight = (Lights1*)alloc_display_list(sizeof(Lights1));
    bcopy(&sLightInfo, curLight, sizeof(Lights1));

#ifdef WORLDSPACE_LIGHTING
    curLight->l->l.dir[0] = (s8)(globalLightDirection[0]);
    curLight->l->l.dir[1] = (s8)(globalLightDirection[1]);
    curLight->l->l.dir[2] = (s8)(globalLightDirection[2]);
#else
    Vec3f transformedLightDirection;
    linear_mtxf_transpose_mul_vec3f(gCameraTransform, transformedLightDirection, globalLightDirection);
    curLight->l->l.dir[0] = (s8)(transformedLightDirection[0]);
    curLight->l->l.dir[1] = (s8)(transformedLightDirection[1]);
    curLight->l->l.dir[2] = (s8)(transformedLightDirection[2]);
#endif

    gSPSetLights1(dlh++, (*curLight));
    gSPEndDisplayList(dlh++);

    geo_append_display_list(dl, LAYER_OPAQUE);
}

u8 day_night_get_color_channel(int colorIndex,f32 list[][4]) {
    int listItem = 0;
    while(sDayProgress >= list[listItem][0]) {
        listItem++;
    }
    f32 curGrad =  list[listItem-1][0];
    f32 nextGrad = list[listItem][0];

    f32 range = nextGrad - curGrad;
    f32 alpha = (sDayProgress - curGrad) / range;

    f32 channel = approach_f32_asymptotic(
    list[listItem-1][colorIndex+1],
    list[listItem][colorIndex+1],
    alpha);

    return channel * 255.0f;
}

u8 day_night_get_hour(void) {
    return (DAY_NIGHT_SUNRISE_TIME + ((int)(sDayProgress*24.0f))) % 12;
}

s32 day_night_get_day_or_night(void) {
    return (sDayProgress >= .5f);
}

u32 debughour = 0;

void day_night_logic(void) {
    f32 celestialBodyProgress = ((sDayProgress*2.0f) - (int)((sDayProgress*2.0f)));
    f32 daySine = M_PI*celestialBodyProgress;
    sSunDirection[0] = cosf(daySine);
    sSunDirection[1] = sinf(daySine);
    sSunDirection[2] = 0.0f;

    vec3f_normalize(sSunDirection);
    vec3_scale_dest(sSunDirection,sSunDirection,127.f);

    sDayProgress += DAY_ADVANCE_PER_FRAME;
    sDayProgress = sDayProgress - (int)sDayProgress;

    debughour = day_night_get_hour();
    debug_u32(&debughour, "Hour");
}
