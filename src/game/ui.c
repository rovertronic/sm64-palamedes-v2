#include "sm64.h"
#include "game_init.h"
#include "ui.h"
#include "event_dialog.h"
#include "ingame_menu.h"
#include "utf8_print.h"
#include <PR/gbi.h>
#include "engine/math_util.h"
#include "frame_lerp.h"
#include "print.h"
#include "data/text_enums.h"
#include "text_load.h"
#include "level_update.h"
#include "geo_misc.h"
#include "neo_save.h"

const Gfx dl_ui_draw_text_bg_box[] = {
    gsDPPipeSync(),
    gsSPClearGeometryMode(G_LIGHTING),
    gsDPSetCombineMode(G_CC_FADE, G_CC_FADE),
    gsDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF2),
    gsSPEndDisplayList(),
};

Mat4 sUiMatStack[UI_OBJECT_COUNT + 1];
int sUiMatStackIndex = 0;

uiObject sUiObjectList[UI_OBJECT_COUNT];
char sUiCharBuffer[200];
int sUiCurrLayer = 0;

uiid gUiidScreen;

uiid sUiidPauseTrans;
uiDestroySignal sPauseDestroySignal;

void ui_mtx_inc(Mat4 mat) {
    mtxf_mul(sUiMatStack[sUiMatStackIndex],mat,sUiMatStack[sUiMatStackIndex-1]);

    Mtx *mtx = (Mtx *) alloc_display_list(sizeof(Mtx));
    mtxf_to_mtx(mtx,sUiMatStack[sUiMatStackIndex]);
    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

    sUiMatStackIndex++;
}

void ui_mtx_pop(void) {
    sUiMatStackIndex--;
}

static uiid ui_create_object(s8 parentId) {
    uiid myId = 0;
    while (myId < UI_OBJECT_COUNT && sUiObjectList[myId].initialized) {
        myId++;
    }
    if (myId == UI_OBJECT_COUNT) {
        return UI_NONE;
    }

    uiObject * self = &sUiObjectList[myId];
    *self = (uiObject) {0};
    self->initialized = TRUE;
    self->type = UI_CLASS_TRANSFORM;
    self->parent = parentId;
    self->base = (parentId == UI_NONE);
    self->childlist = UI_NONE;
    self->next = UI_NONE;
    self->prev = UI_NONE;
    self->printOrigin = PRINT_ORIGIN_LEFT;
    self->alpha = 255;
    self->transition = 1.0f;
    for (int i = 0; i < 3; i++) {
        self->color[i] = 255;
    }

    if (parentId != UI_NONE) {
        uiObject * parent = ui_object_ptr(parentId);
        if (parent->childlist == UI_NONE) {
            parent->childlist = myId;
        } else {
            uiid siblingId = parent->childlist;
            while (sUiObjectList[siblingId].next != UI_NONE) {
                siblingId = sUiObjectList[siblingId].next;
            }
            self->prev = siblingId;
            sUiObjectList[siblingId].next = myId;
        }
    }
    return myId;
}

uiid ui_create_transform(s8 parentId) {
    uiid myId = ui_create_object(parentId);
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = ui_object_ptr(myId);
    self->alpha = 0;
    self->transition = 0.0f;
    self->transitionFunction[0] = ui_trans_transition_fade_in;
    self->transitionFunction[1] = ui_trans_transition_fade_out;
    return myId;
}

void ui_set_destroy_signal(uiid myId, uiDestroySignal * destroySignal) {
    uiObject * self = &sUiObjectList[myId];
    (*destroySignal) = FALSE;
    self->destroySignal = destroySignal;
}

s32 ui_is_not_transitioning(uiid myId) {
    return (ui_object_ptr(myId) && ui_object_ptr(myId)->transition == 1.0f);
}

static void ui_destroy_object(uiid myId) {
    uiObject * self = ui_object_ptr(myId);

    // Destroy every descendant, including children that are not transitioning out.
    while (self->childlist != UI_NONE) {
        ui_destroy_object(self->childlist);
    }

    if (self->prev != UI_NONE) {
        sUiObjectList[self->prev].next = self->next;
    } else if (self->parent != UI_NONE) {
        sUiObjectList[self->parent].childlist = self->next;
    }
    if (self->next != UI_NONE) {
        sUiObjectList[self->next].prev = self->prev;
    }

    self->initialized = FALSE;
    self->getout = FALSE;
    self->parent = UI_NONE;
    self->next = UI_NONE;
    self->prev = UI_NONE;

    if (self->destroySignal) {
        *self->destroySignal = TRUE;
    }
}

void ui_destroy_trans(uiid myId) {
    uiObject * self = ui_object_ptr(myId);
    if (self && self->initialized && self->getout) {
        ui_destroy_object(myId);
    }
}

f32 ui_trans_get_parents_alpha(uiid transParent) {
    uiObject * self = ui_object_ptr(transParent);
    if (self) {
        f32 myAlpha = (self->alpha/255.f);
        if (self->parent != UI_NONE) {
            f32 parentAlpha = ui_trans_get_parents_alpha(self->parent);
            myAlpha *= parentAlpha;
        }
        return myAlpha;
    } else {
        return 1.0f;
    }
}

uiid ui_create_text(s8 parentId, s16 textId) {
    s8 myId = ui_create_object(parentId);
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_TEXT;
    self->text = textId;
    self->x2 = 0;
    self->printFont = FONT_SM64DS;

    return myId;
}

uiid ui_create_text_raw(s8 parentId, char * text) {
    s8 myId = ui_create_object(parentId);
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_TEXT_RAW;
    self->x2 = 0;
    self->printFont = FONT_SM64DS;
    self->ptr = text;

    return myId;
}

uiid ui_create_slice(s8 parentId, nineSliceParams * p, s16 x1, s16 y1, s16 x2, s16 y2) {
    s8 myId = ui_create_object(parentId);
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_SLICE;
    
    self->x1 = x1;
    self->y1 = y1;
    self->x2 = x2;
    self->y2 = y2;

    self->ptr = p;

    return myId;
}

uiid ui_create_rectangle(s8 parentId, s16 x1, s16 y1, s16 x2, s16 y2) {
    s8 myId = ui_create_object(parentId);
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_BOX;
    
    self->x1 = x1;
    self->y1 = y1;
    self->x2 = x2;
    self->y2 = y2;

    return myId;
}

uiid ui_create_btn(s8 parentId, s16 textId) {
    s8 myId = ui_create_object(parentId);
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_BUTTON;
    self->text = textId;
    self->printFont = FONT_SM64DS;

    return myId;
}

uiid ui_create_text_quick(s8 parentId, s8 * textObject, char * text, s16 x, s16 y, u8 print_origin) {
    uiid textTransform = ui_create_transform(parentId);
    if (textTransform == UI_NONE) {
        return UI_NONE;
    }
    ui_set_trans_xy(textTransform,x,y);

    uiid textId = ui_create_text_raw(textTransform, text);
    if (textId == UI_NONE) {
        ui_destroy_object(textTransform);
        return UI_NONE;
    }
    ui_object_ptr(textId)->printOrigin = print_origin;

    if (textObject != NULL) {
        *textObject = textId;
    }
    return textTransform;
}

void ui_trans_begin_remove(s8 myId) {
    uiObject * self = &sUiObjectList[myId];
    //self->layer = 1;
    self->getout = TRUE;
    self->transition = 0.0f;
}

void ui_set_trans_pos(s8 myId, Vec3f pos) {
    uiObject * self = &sUiObjectList[myId];
    vec3f_copy(self->pos,pos);
}

void ui_set_trans_xy(s8 myId, s16 x, s16 y) {
    uiObject * self = &sUiObjectList[myId];
    self->pos[0] = (f32)x;
    self->pos[1] = (f32)y;
    self->pos[2] = 0.0f;
    vec3f_copy(self->posLerp,self->pos);
}

void ui_set_text(s8 myId, s16 textId) {
    uiObject * self = &sUiObjectList[myId];
    self->text = textId;
}

void ui_set_trans_color(s8 myId, u8 r, u8 g, u8 b) {
    uiObject * self = &sUiObjectList[myId];
    self->color[0] = r;
    self->color[1] = g;
    self->color[2] = b;
}

void ui_set_transition_instant(s8 myId) {
    uiObject * self = &sUiObjectList[myId];
    self->alpha = 255;
    self->transition = 1.0f;
}

void ui_set_transition_slide(uiid myId, Vec3f start, Vec3f end) {
    uiObject * self = &sUiObjectList[myId];
    self->alpha = 255;
    self->transitionFunction[0] = ui_trans_transition_slide_in;
    self->transitionFunction[1] = ui_trans_transition_slide_out;
    vec3f_copy(self->pos,start);
    vec3f_copy(self->posLerp,start);
    vec3f_copy(self->slideStart,start);
    vec3f_copy(self->slideEnd,end);

}

// TRANSFORM TRANSITIONS

void ui_trans_transition_fade_in(uiid myId) {
    uiObject * self = &sUiObjectList[myId];
    self->alpha = self->transition*255.0f;

    self->transition += .1f;
}

void ui_trans_transition_fade_out(uiid myId) {
    uiObject * self = &sUiObjectList[myId];
    self->alpha = (1.0f - self->transition)*255.0f;

    self->transition += .1f;
}

void ui_trans_transition_instant(uiid myId) {
    uiObject * self = &sUiObjectList[myId];
    self->alpha = 255.0f;
    self->transition = 1.0f;
}

void ui_trans_transition_page_rip_out(uiid myId) {
    uiObject * self = &sUiObjectList[myId];
    self->alpha = 255.0f;

    self->pos[1] -= self->transition*20.0f;
    self->rot[2] -= 0x200;

    self->transition += 0.06f;
}

void ui_trans_transition_slide_in(uiid myId) {
    uiObject * self = &sUiObjectList[myId];
    for (int i = 0; i < 3; i++) {
        self->pos[i] = approach_f32_asymptotic(self->slideStart[i],self->slideEnd[i],smoothstep2(self->transition));
    }
    self->transition += .05f;
}

void ui_trans_transition_slide_out(uiid myId) {
    uiObject * self = &sUiObjectList[myId];
    for (int i = 0; i < 3; i++) {
        self->pos[i] = approach_f32_asymptotic(self->slideEnd[i],self->slideStart[i],smoothstep2(self->transition));
    }
    self->transition += .05f;
}

// TECHNICAL FUNCTIONS

uiObject * ui_object_ptr(s8 myId) {
    if (myId < 0 || myId >= UI_OBJECT_COUNT) {
        return NULL;
    }
    return &sUiObjectList[myId];
}

void ui_init_mtx_stack(void) {
    Mtx *matrix = (Mtx *) alloc_display_list(sizeof(Mtx));
    create_dl_identity_matrix();
    //guOrtho(matrix, 0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -10.0f, 10.0f, 1.0f);

    u16 perspNorm;
    guPerspective(matrix, &perspNorm, 90.0f, 4.0f/3.0f, 10.0f, 1000.0f, 1.0f);
    gSPPerspNormalize(gDisplayListHead++, perspNorm);
    gSPPerspNormalize(gDisplayListHead++, 0xFFFF);
    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(matrix), G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);


    mtxf_identity(sUiMatStack[0]);
    sUiMatStackIndex = 1;
}

void ui_mat4_screenspace(Mat4 mat) {
    mtxf_identity(mat);
    mat[3][0] = -160.0f;
    mat[3][1] = -90.0f;
    mat[3][2] = -90.0f;
}

void ui_process_ui_object(uiObject * self) {
    char * str;
    int x;
    int y;
    int xoffset;
    if (self->type == UI_CLASS_TRANSFORM) {
        return;
    }

    uiObject * parent = ui_object_ptr(self->parent);
    f32 mixAlpha = ui_trans_get_parents_alpha(self->parent) * self->alpha;
    if (mixAlpha == 0.0f) {
        return;
    }
    u8 r = self->color[0];
    u8 g = self->color[1];
    u8 b = self->color[2];
    if (parent) {
        r = r * parent->color[0] / 255;
        g = g * parent->color[1] / 255;
        b = b * parent->color[2] / 255;
    }
    gDPSetEnvColor(gDisplayListHead++, r, g, b, mixAlpha);

    switch(self->type) {
        case UI_CLASS_TEXT:
            utf8_init_print();
            utf8_set_font(self->printFont);

            str = get_text(self->text);

            utf8_size(str,&x,&y);
            self->textXsize = x;

            xoffset = 0;
            switch(self->printOrigin) {
                case PRINT_ORIGIN_LEFT:
                    xoffset = x/2;
                    break;
                case PRINT_ORIGIN_RIGHT:
                    xoffset = -x/2;
                    break;
            }

            if (self->x2 != 0) {
                str = utf8_autonewline(str,self->x2);
            }
            utf8_print(str,(-x/2)+xoffset,0);
            break;
        case UI_CLASS_TEXT_RAW:
            utf8_init_print();
            utf8_set_font(self->printFont);

            str = self->ptr;

            utf8_size(str,&x,&y);
            self->textXsize = x;

            xoffset = 0;
            switch(self->printOrigin) {
                case PRINT_ORIGIN_LEFT:
                    xoffset = x/2;
                    break;
                case PRINT_ORIGIN_RIGHT:
                    xoffset = -x/2;
                    break;
            }
            
            utf8_print(str,(-x/2)+xoffset,0);
            break;
        case UI_CLASS_SLICE:
            init_slice_render(self->ptr);
            render_slice(self->x1,self->y1, self->x2, self->y2);
            break;
        case UI_CLASS_BUTTON:
            str = get_text(self->text);

            utf8_size(str,&x,&y);
            self->textXsize = x;

            xoffset = 0;
            switch(self->printOrigin) {
                case PRINT_ORIGIN_LEFT:
                    xoffset = x/2;
                    break;
                case PRINT_ORIGIN_RIGHT:
                    xoffset = -x/2;
                    break;
            }

            init_slice_render(&gStickySliceParams);
            render_slice((-x/2)-10+xoffset,22,(x/2)+10+xoffset,-10);

            utf8_init_print();
            utf8_set_font(self->printFont);
            utf8_print(str,(-x/2)+xoffset,0);
            break;
        case UI_CLASS_BOX:;
            Vtx * boxVerts = alloc_display_list(4 * sizeof(Vtx));

            make_vertex(boxVerts, 0, self->x1, self->y1,         0,   0,0,   255,255,255,255);
            make_vertex(boxVerts, 1, self->x2, self->y1,         0,   0,0,   255,255,255,255);
            make_vertex(boxVerts, 2, self->x1, self->y2,         0,   0,0,   255,255,255,255);
            make_vertex(boxVerts, 3, self->x2, self->y2,         0,   0,0,   255,255,255,255);

            gSPDisplayList(gDisplayListHead++,dl_ui_draw_text_bg_box);
            gSPVertex(gDisplayListHead++,boxVerts,4,0);
            gSP2Triangles(gDisplayListHead++, 1, 0, 2, 0, 3, 1, 2, 0);
            break;
    }
}

void ui_object_process_children(uiid objectId, u8 parentLayer) {
    while (objectId != UI_NONE) {
        uiObject * self = ui_object_ptr(objectId);
        if (!self->initialized) {
            return;
        }

        frameLerpPosUi(self->pos,self->posLerp);
        frameLerpPosUi(self->rot,self->rotLerp);
        Vec3s rot; vec3f_to_vec3s(rot,self->rotLerp);
        Mat4 transform; mtxf_rotate_zxy_and_translate(transform,self->posLerp,rot);
        ui_mtx_inc(transform);

        // A foreground parent carries its descendants onto the same layer.
        u8 layer = self->layer | parentLayer;
        if (layer == sUiCurrLayer) {
            ui_process_ui_object(self);
        }
        if (self->childlist != UI_NONE) {
            ui_object_process_children(self->childlist, layer);
        }
        ui_mtx_pop();
        objectId = self->next;
    }
}

void ui_render(void) {
    ui_init_mtx_stack();
    for (int l = 0; l < 2; l++) {
        sUiCurrLayer = l;
        for (int i = 0; i < UI_OBJECT_COUNT; i++) {
            uiObject * self = &sUiObjectList[i];
            if (self->initialized && self->base) {
                ui_object_process_children(i, 0);
            }
        }
    }
}

u8 sUiStickHoldTimer[4];

s32 ui_get_input(int dir) {
    if (sUiStickHoldTimer[dir] == 1 || (sUiStickHoldTimer[dir] > 20 &&  (sUiStickHoldTimer[dir] % 4 == 0 )) ) {return TRUE;}
    return FALSE;
}

void ui_logic(void) {

    for (int i = 0; i < 4; i++) {
        sUiStickHoldTimer[i] ++;
    }
    if (gPlayer1Controller->rawStickX < 60) {
        sUiStickHoldTimer[0] = 0;
    }
    if (gPlayer1Controller->rawStickX > -60) {
        sUiStickHoldTimer[1] = 0;
    }
    if (gPlayer1Controller->rawStickY < 60) {
        sUiStickHoldTimer[2] = 0;
    }
    if (gPlayer1Controller->rawStickY > -60) {
        sUiStickHoldTimer[3] = 0;
    }

    for (int i = 0; i < UI_OBJECT_COUNT; i++) {
        uiObject * self = &sUiObjectList[i];
        if (self->initialized) {
            void (* transitionFunction)(uiid) = self->transitionFunction[self->getout];
            if (transitionFunction) {
                transitionFunction(i);
            } else {
                self->transition = 1.0f;
            }

            if (self->transition >= 1.0f) {
                self->transition = 1.0f;
                if (self->getout) {
                    ui_destroy_trans(i);
                }
            }
        }
    }

    ui_hud_update();
}

void ui_init(void) {
    gUiidScreen = ui_create_transform(UI_NONE);
    Vec3f screenCorner = {-160.0f,-120.0f,-120.0f};
    ui_set_trans_pos(gUiidScreen,screenCorner);
}

// HUD
uiid sUiidHudTrans[2] = {UI_NONE, UI_NONE};
uiid sUiidHudText[2];
f32 sHudEmphasisTimer[2];
f32 sHudMarioMoveTimer;
char sCoinCharBuffer[6];
char sStarCharBuffer[6];

void ui_hud_create(void) {
    if (sUiidHudTrans[0] == UI_NONE) {
        sUiidHudTrans[0] = ui_create_transform(gUiidScreen);
        sUiidHudTrans[1] = ui_create_transform(gUiidScreen);
        ui_set_trans_xy(sUiidHudTrans[0],22,209);
        ui_set_trans_xy(sUiidHudTrans[1],22,189);

        sUiidHudText[0] = ui_create_text_raw(sUiidHudTrans[0],sCoinCharBuffer);
        sUiidHudText[1] = ui_create_text_raw(sUiidHudTrans[1],sStarCharBuffer);
        ui_object_ptr(sUiidHudText[0])->printFont = FONT_PINBALL;
        ui_object_ptr(sUiidHudText[1])->printFont = FONT_PINBALL;

        sHudEmphasisTimer[0] = 0.0f;
        sHudEmphasisTimer[1] = 0.0f;

        sHudMarioMoveTimer = 0.0f;
    }
}

void ui_hud_destroy(void) {
    if (sUiidHudTrans[0] != UI_NONE) {
        ui_trans_begin_remove(sUiidHudTrans[0]);
        ui_trans_begin_remove(sUiidHudTrans[1]);
        sUiidHudTrans[0] = UI_NONE;
        sUiidHudTrans[1] = UI_NONE;
    }
}

void ui_hud_update(void) {
    if (sUiidHudTrans[0] != UI_NONE) {

        if ((gMarioState->action & ACT_GROUP_MASK) == ACT_GROUP_STATIONARY) {
            sHudMarioMoveTimer -= .01f;
            sHudMarioMoveTimer = MAX(sHudMarioMoveTimer,0.0f);
        } else {
            sHudMarioMoveTimer = 1.0f;
        }

        for (int i = 0; i < 2; i++) {
            if (sUiidHudText[i] != UI_NONE ) {
                u8 size = ui_object_ptr(sUiidHudText[i])->textXsize;
                f32 emphasis = CLAMP(sHudEmphasisTimer[i],0.0f,1.0f);
                emphasis = smoothstep2(emphasis);
                ui_object_ptr(sUiidHudTrans[i])->pos[0] = ((-size)+(emphasis*(f32)(22+size)));

                f32 bounce = sinf((CLAMP(sHudEmphasisTimer[i],3.0f,3.5f)-1.0f)*M_PI*4.0f)*3.0f;
                ui_object_ptr(sUiidHudTrans[i])->pos[1] = 209-(20*i)+bounce;

                if (sHudMarioMoveTimer == 0.0f) {
                    sHudEmphasisTimer[i] += .05f;
                    sHudEmphasisTimer[i] = MIN(sHudEmphasisTimer[i],3.0f);
                } else {
                    sHudEmphasisTimer[i] -= .05f;
                    sHudEmphasisTimer[i] = MAX(sHudEmphasisTimer[i],0.0f);
                }

                ui_object_ptr(sUiidHudTrans[i])->alpha = emphasis * 255.0f;
            }
        }

        s16 starct = neo_save_collectible_count(gCurrNeoSaveFile->stars,STAR_MISSION_COUNT);

        sprintf(sCoinCharBuffer, "$X%d", gMarioState->numCoins );
        sprintf(sStarCharBuffer, "★X%d", starct );

        static int oldCoins;
        static int oldStars;

        if (oldCoins != gMarioState->numCoins) {
            ui_object_ptr(sUiidHudTrans[0])->posLerp[0] = 22;
            ui_object_ptr(sUiidHudTrans[0])->pos[0] = 22;
            sHudEmphasisTimer[0] = 3.25f;
        }
        
        if (oldStars != starct) {
            ui_object_ptr(sUiidHudTrans[1])->posLerp[0] = 22;
            ui_object_ptr(sUiidHudTrans[1])->pos[0] = 22;
            sHudEmphasisTimer[1] = 3.25f;
        }
        
        oldCoins = gMarioState->numCoins;
        oldStars = starct;
    }
}

// Pause screen

#define MAX_TAB_COUNT 10

uiid sUiPauseTabs[MAX_TAB_COUNT];
uiid sUiPauseTabTransforms[MAX_TAB_COUNT];
uiid sUiPauseTabIds[MAX_TAB_COUNT];
u8 sUiPauseTabCount = 0;

char sUiPauseStringBuffers[10][40];

s8 sUiPauseTabIndex = 0;
s8 sUiPauseTabIndexOld = -1;

uiid sUiPauseTabContent = UI_NONE;
uiDestroySignal sUiPauseTabContentDestroySignal = TRUE;


s8 sUiPauseIndex2 = -1;


u8 sUiOptionMaxList[] = {
    2,
    2,
};

char * ui_pause_option_start(int index) {
    if (sUiPauseIndex2 == index) {
        return "@Y@← ";
    } else {
        return "";
    }
}

char * ui_pause_option_bool_end(int index) {
    if (sUiPauseIndex2 == index) {
        if (gCurrNeoSaveFile->options[index]) {
            return " Enabled →";
        } else {
            return " Disabled →";
        }
    } else {
        if (gCurrNeoSaveFile->options[index]) {
            return " @G@Enabled";
        } else {
            return " @R@Disabled";
        }
    }
}

char * ui_pause_option_generic_end(int index) {
    if (sUiPauseIndex2 == index) {
        return " →";
    } else {
        return "";
    }
}

void ui_pause_options_create() {
    ui_create_text_quick(sUiPauseTabContent, NULL, sUiPauseStringBuffers[0], 40, 150, PRINT_ORIGIN_LEFT );
    ui_create_text_quick(sUiPauseTabContent, NULL, sUiPauseStringBuffers[1], 40, 130, PRINT_ORIGIN_LEFT );
}

void ui_pause_options_update() {
    if (sUiPauseIndex2 > -1) {
        if (ui_get_input(0)) {
            neo_save_option_set(sUiPauseIndex2,1,sUiOptionMaxList[sUiPauseIndex2]);
        }
        if (ui_get_input(1)) {
            neo_save_option_set(sUiPauseIndex2,-1,sUiOptionMaxList[sUiPauseIndex2]);
        }

        if (ui_get_input(2)) {
            sUiPauseIndex2 --;
        }
        if (ui_get_input(3)) {
            sUiPauseIndex2 ++;
        }

        sUiPauseIndex2 = (sUiPauseIndex2 + SAVE_FILE_OPTION_COUNT) % SAVE_FILE_OPTION_COUNT;

        if (gPlayer1Controller->buttonPressed & A_BUTTON) {
            sUiPauseIndex2 = -1;
        }
    }

    sprintf(sUiPauseStringBuffers[0], get_text(TEXT_WIDESCREEN), ui_pause_option_start(0), ui_pause_option_bool_end(0));
    sprintf(sUiPauseStringBuffers[1], get_text(TEXT_LANGUAGE),   ui_pause_option_start(1), ui_pause_option_generic_end(1));

    gLanguage = gCurrNeoSaveFile->options[1];
}



void ui_pause_tab_create(int text, int tab_id) {
    sUiPauseTabTransforms[sUiPauseTabCount] = ui_create_transform(sUiidPauseTrans);
    sUiPauseTabs[sUiPauseTabCount] = ui_create_btn(sUiPauseTabTransforms[sUiPauseTabCount], text);
    ui_object_ptr(sUiPauseTabs[sUiPauseTabCount])->printOrigin = PRINT_ORIGIN_CENTER;
    sUiPauseTabIds[sUiPauseTabCount] = tab_id;
    sUiPauseTabCount++;
}

void ui_pause_tab_content_create() {
    sUiPauseTabContent = ui_create_transform(sUiidPauseTrans);
    ui_set_destroy_signal(sUiPauseTabContent, &sUiPauseTabContentDestroySignal);

    ui_pause_options_create();
    ui_pause_options_update();
}

void ui_pause_create(void) {
    sUiPauseTabIndex = 0;
    sUiPauseTabIndexOld = 0;

    sUiPauseTabContentDestroySignal = TRUE;

    sUiidPauseTrans = ui_create_transform(gUiidScreen);
    uiid pauseRectangle = ui_create_rectangle(sUiidPauseTrans,0,240,320,0);
    ui_set_trans_color(sUiidPauseTrans,0,0,0);
    ui_object_ptr(pauseRectangle)->alpha = 100;

    //uiid transPauseText = ui_create_transform(sUiidPauseTrans);
    //ui_create_text(transPauseText,TEXT_PAUSE);

    ui_create_text_quick(sUiidPauseTrans, NULL, "Paused...", 160, 210, PRINT_ORIGIN_CENTER);

    ui_set_destroy_signal(sUiidPauseTrans,&sPauseDestroySignal);
    ui_hud_destroy();

    sUiPauseTabCount = 0;
    ui_pause_tab_create(TEXT_PROGRESS, 0);
    ui_pause_tab_create(TEXT_SETTINGS, 1);

    int xtotalhalf = ((sUiPauseTabCount-1) * 80) / 2;
    for (int i = 0; i < sUiPauseTabCount; i++) {
        ui_set_trans_xy( sUiPauseTabTransforms[i], 160 + (i * 80) - xtotalhalf, 180 );
        //xoffset += ui_object_ptr(sUiPauseTabs[i])->textXsize;

        //int x, y;
        //utf8_size( get_text( ui_object_ptr(sUiPauseTabs[i])->text ), &x, &y );
        //xoffset += x;

        ui_object_ptr(sUiPauseTabTransforms[i])->color[0] = 50;
        ui_object_ptr(sUiPauseTabTransforms[i])->color[1] = 50;
        ui_object_ptr(sUiPauseTabTransforms[i])->color[2] = 50;
    }
}

void ui_pause_destroy(void) {
    ui_trans_begin_remove(sUiidPauseTrans);
    sUiidPauseTrans = UI_NONE;

    ui_hud_create();
}

// Return 1 to unpause, Return 2 for exit course.
s32 ui_pause_logic(void) {
    if (sPauseDestroySignal) {
        return 1;
    }
    if (ui_is_not_transitioning(sUiidPauseTrans)) {

        if (sUiPauseIndex2 == -1) {
            if (ui_get_input(0)) {
                sUiPauseTabIndex++;
            }
            if (ui_get_input(1)) {
                sUiPauseTabIndex--;
            }
        }
        
        sUiPauseTabIndex = ((sUiPauseTabCount + sUiPauseTabIndex) % sUiPauseTabCount);

        for (int i = 0; i < sUiPauseTabCount; i++) {
            if (i == sUiPauseTabIndex) {
                ui_object_ptr(sUiPauseTabTransforms[i])->color[0] = 255;
            } else {
                ui_object_ptr(sUiPauseTabTransforms[i])->color[0] = 50;
            }
        }

        if ((sUiPauseTabIndexOld != sUiPauseTabIndex) && (ui_is_not_transitioning(sUiPauseTabContent))) {
            sUiPauseTabIndexOld = sUiPauseTabIndex;
            ui_trans_begin_remove(sUiPauseTabContent);
        }

        if (sUiPauseTabContentDestroySignal) {
            ui_pause_tab_content_create();
        }

        if (sUiPauseIndex2 == -1) {
            if (gPlayer1Controller->buttonPressed & A_BUTTON) {
                sUiPauseIndex2 = 0;
            }

            if (gPlayer1Controller->buttonPressed & START_BUTTON) {
                ui_pause_destroy();
            }
        } else {
            if (!sUiPauseTabContentDestroySignal) {
                ui_pause_options_update();
            }
        }
    }
    return 0;
}