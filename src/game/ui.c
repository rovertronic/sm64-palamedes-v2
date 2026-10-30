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

Mat4 sUiMatStack[5];
int sUiMatStackIndex = 0;

uiTrans sUiTransList[UI_TRANS_COUNT];
uiObject sUiObjectList[UI_OBJECT_COUNT];
char sUiCharBuffer[200];
int sUiCurrLayer = 0;

uiid gUiidScreen;

uiid sUiidPauseTrans;
uiDestroySignal sPauseDestroySignal;

uiid sUiidHudTrans[2];
uiid sUiidHudText[2];
f32 sHudEmphasisTimer[2];
f32 sHudMarioMoveTimer;

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

uiid ui_create_transform(s8 parentId) {
    s8 myId = 0;
    while (sUiTransList[myId].initialized == TRUE) {
        myId++;
    }

    uiTrans * self = &sUiTransList[myId];
    uiTrans * parent = NULL;

    self->initialized = TRUE;
    self->layer = 0;
    self->parent = parentId;
    self->getout = FALSE;
    self->transition = 0.0f;
    self->alpha = 0;
    self->destroySignal = NULL;
    for (int i = 0; i < 3; i++) {
        self->color[i] = 255;
    }

    self->transitionFunction[0] = ui_trans_transition_fade_in;
    self->transitionFunction[1] = ui_trans_transition_fade_out;

    vec3f_set(self->pos,0,0,0);
    vec3f_set(self->rot,0,0,0);
    vec3f_set(self->posLerp,0,0,0);
    vec3f_set(self->rotLerp,0,0,0);

    self->childlist = UI_NONE;
    self->next = UI_NONE;
    self->prev = UI_NONE;

    self->objlist = UI_NONE;

    self->base = (parentId == UI_NONE);

    if (parentId != UI_NONE) {
        parent = &sUiTransList[parentId];

        if (parent->childlist == UI_NONE) {
            parent->childlist = myId;
        } else {
            // Navigate parent transform child list
            uiid siblingId = parent->childlist;
            uiid siblingPrevId;
            uiTrans * sibling = &sUiTransList[siblingId];
            while(siblingId != UI_NONE){
                sibling = &sUiTransList[siblingId];
                siblingPrevId = siblingId;
                siblingId = sibling->next;
            }
            self->prev = siblingPrevId;
            sibling->next = myId;
        }
    }
    return myId;
}

uiid ui_create_object(s8 parentTransId) {
    s8 myId = 0;
    while (sUiObjectList[myId].initialized == TRUE) {
        myId++;
    }
    uiObject * self = &sUiObjectList[myId];
    uiTrans * parent = NULL;

    self->initialized = TRUE;
    self->uiObjectSibling = UI_NONE;
    self->printOrigin = PRINT_ORIGIN_LEFT;
    self->alpha = 255;

    if (parentTransId != UI_NONE) {
        self->parentTrans = parentTransId;

        parent = &sUiTransList[parentTransId];

        if (parent->objlist == -1) {
            parent->objlist = myId;
        } else {
            // Navigate parent transform list
            uiObject * sibling = &sUiObjectList[parent->objlist];
            while(sibling){
                if (sibling->uiObjectSibling == UI_NONE) {
                    sibling->uiObjectSibling = myId;
                    sibling = NULL;
                } else {
                    sibling = &sUiObjectList[sibling->uiObjectSibling];
                }
            }
        }
    }
    return myId;
}

void ui_set_destroy_signal(uiid myId, uiDestroySignal * destroySignal) {
    uiTrans * self = &sUiTransList[myId];
    (*destroySignal) = FALSE;
    self->destroySignal = destroySignal;
}

s32 ui_is_not_transitioning(uiid myId) {
    return (ui_trans_ptr(myId) && ui_trans_ptr(myId)->transition == 1.0f);
}

void ui_destroy_trans(uiid myId) {
    uiTrans * self = &sUiTransList[myId];

    if (!self->getout) {
        return;
    }
    self->getout = FALSE;

    // Remove all objects I own
    uiid objId = self->objlist;
    while(objId != UI_NONE) {
        uiObject * del = ui_object_ptr(objId);
        del->initialized = FALSE;
        del->text = TEXT_DEBUG_DELETED;
        objId = del->uiObjectSibling;
    }
    self->objlist = UI_NONE;

    // Delete children if I have any
    if (self->childlist != UI_NONE) {
        ui_destroy_trans(self->childlist);
    }

    // Remove self from child list
    if (self->prev == UI_NONE) {
        sUiTransList[self->parent].childlist = self->next;
    } else {
        sUiTransList[self->prev].next = self->next;
    }
    if (self->next != UI_NONE) {
        ui_trans_ptr(self->next)->prev = self->prev;
    }
    self->initialized = FALSE;

    self->parent = UI_NONE;
    self->childlist = UI_NONE;
    self->next = UI_NONE;
    self->prev = UI_NONE;

    if (self->destroySignal) {
        (*self->destroySignal) = TRUE;
    }
}

f32 ui_trans_get_parents_alpha(uiid transParent) {
    uiTrans * self = ui_trans_ptr(transParent);
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

uiid ui_create_text(s8 parentTransId, s16 textId) {
    s8 myId = ui_create_object(parentTransId);
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_TEXT;
    self->text = textId;
    self->x2 = 0;
    self->printFont = FONT_SM64DS;

    return myId;
}

uiid ui_create_slice(s8 parentTransId, nineSliceParams * p, s16 x1, s16 y1, s16 x2, s16 y2) {
    s8 myId = ui_create_object(parentTransId);
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_SLICE;
    
    self->x1 = x1;
    self->y1 = y1;
    self->x2 = x2;
    self->y2 = y2;

    self->ptr = p;

    return myId;
}

uiid ui_create_rectangle(s8 parentTransId, s16 x1, s16 y1, s16 x2, s16 y2) {
    s8 myId = ui_create_object(parentTransId);
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_BOX;
    
    self->x1 = x1;
    self->y1 = y1;
    self->x2 = x2;
    self->y2 = y2;

    return myId;
}

uiid ui_create_btn(s8 parentTransId, s16 textId) {
    s8 myId = ui_create_object(parentTransId);
    uiObject * self = &sUiObjectList[myId];

    self->type = UI_CLASS_BUTTON;
    self->text = textId;
    self->printFont = FONT_SM64DS;

    return myId;
}

void ui_trans_begin_remove(s8 myId) {
    uiTrans * self = &sUiTransList[myId];
    //self->layer = 1;
    self->getout = TRUE;
    self->transition = 0.0f;
}

void ui_set_trans_pos(s8 myId, Vec3f pos) {
    uiTrans * self = &sUiTransList[myId];
    vec3f_copy(self->pos,pos);
}

void ui_set_trans_xy(s8 myId, s16 x, s16 y) {
    uiTrans * self = &sUiTransList[myId];
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
    uiTrans * self = &sUiTransList[myId];
    self->color[0] = r;
    self->color[1] = g;
    self->color[2] = b;
}

void ui_set_transition_instant(s8 myId) {
    uiTrans * self = &sUiTransList[myId];
    self->alpha = 255;
    self->transition = 1.0f;
}

void ui_set_transition_slide(uiid myId, Vec3f start, Vec3f end) {
    uiTrans * self = &sUiTransList[myId];
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
    uiTrans * self = &sUiTransList[myId];
    self->alpha = self->transition*255.0f;

    self->transition += .1f;
}

void ui_trans_transition_fade_out(uiid myId) {
    uiTrans * self = &sUiTransList[myId];
    self->alpha = (1.0f - self->transition)*255.0f;

    self->transition += .1f;
}

void ui_trans_transition_instant(uiid myId) {
    uiTrans * self = &sUiTransList[myId];
    self->alpha = 255.0f;
    self->transition = 1.0f;
}

void ui_trans_transition_page_rip_out(uiid myId) {
    uiTrans * self = &sUiTransList[myId];
    self->alpha = 255.0f;

    self->pos[1] -= self->transition*20.0f;
    self->rot[2] -= 0x200;

    self->transition += 0.06f;
}

void ui_trans_transition_slide_in(uiid myId) {
    uiTrans * self = &sUiTransList[myId];
    for (int i = 0; i < 3; i++) {
        self->pos[i] = approach_f32_asymptotic(self->slideStart[i],self->slideEnd[i],smoothstep2(self->transition));
    }
    self->transition += .05f;
}

void ui_trans_transition_slide_out(uiid myId) {
    uiTrans * self = &sUiTransList[myId];
    for (int i = 0; i < 3; i++) {
        self->pos[i] = approach_f32_asymptotic(self->slideEnd[i],self->slideStart[i],smoothstep2(self->transition));
    }
    self->transition += .05f;
}

// TECHNICAL FUNCTIONS

uiObject * ui_object_ptr(s8 myId) {
    if (myId == UI_NONE) {
        return NULL;
    }
    return &sUiObjectList[myId];
}

uiTrans * ui_trans_ptr(s8 myId) {
    if (myId == UI_NONE) {
        return NULL;
    }
    return &sUiTransList[myId];
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

int sUiDebugRecursion = 0;
int sUiDebugY = 0;

void ui_process_ui_object(uiObject * self) {
    char * str;
    int x;
    int y;
    uiTrans * tp = ui_trans_ptr(self->parentTrans); 
    f32 parentAlpha = ui_trans_get_parents_alpha(self->parentTrans);

    if (tp->alpha == 0 || self->alpha == 0) {return;} // Skip render if 0 alpha
    f32 mixAlpha = ((parentAlpha) * (self->alpha/255.f))*255.f;
    gDPSetEnvColor(gDisplayListHead++, tp->color[0], tp->color[1], tp->color[2], mixAlpha);

    switch(self->type) {
        case UI_CLASS_TEXT:
            utf8_init_print();
            utf8_set_font(self->printFont);

            sprintf(sUiCharBuffer,get_text(self->text),self->printInt[0],self->printInt[1]);
            str = sUiCharBuffer;

            utf8_size(str,&x,&y);
            self->textXsize = x;

            if (self->x2 != 0) {
                str = utf8_autonewline(str,self->x2);
            }
            utf8_print(str,0,0);
            break;
        case UI_CLASS_SLICE:
            init_slice_render(self->ptr);
            render_slice(self->x1,self->y1, self->x2, self->y2);
            break;
        case UI_CLASS_BUTTON:
            sprintf(sUiCharBuffer,get_text(self->text),self->printInt[0],self->printInt[1]);
            str = sUiCharBuffer;

            utf8_size(str,&x,&y);
            self->textXsize = x;

            int xoffset = 0;
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

void ui_transform_process_children(s8 transId) {
    sUiDebugY++;
    uiTrans * child;

    while(transId != UI_NONE) {
        child = &sUiTransList[transId];

        if (child->initialized == FALSE) {
            return;
        }

        frameLerpPosUi(child->pos,child->posLerp);
        frameLerpPosUi(child->rot,child->rotLerp);
        Vec3s rot; vec3f_to_vec3s(rot,child->rotLerp);
        Mat4 transform; mtxf_rotate_zxy_and_translate(transform,child->posLerp,rot);
        ui_mtx_inc(transform);

        // Only render child's objects if on same layer
        if (child->layer == sUiCurrLayer) {
            uiObject * object = &sUiObjectList[child->objlist];
            if (child->objlist == UI_NONE) {
                object = NULL;
            }
            while(object) {
                ui_process_ui_object(object);
                if (object->uiObjectSibling != UI_NONE) {
                    object = &sUiObjectList[object->uiObjectSibling];
                } else {
                    object = NULL;
                }
            }
        }

        if (child->childlist != UI_NONE) {
            sUiDebugRecursion ++;
            ui_transform_process_children(child->childlist);
            sUiDebugRecursion --;
        }
        ui_mtx_pop();

        transId = child->next;
    }
}

void ui_render(void) {
    sUiDebugRecursion = 0;
    sUiDebugY = 0;

    ui_init_mtx_stack();
    for (int l = 0; l < 2; l++) {
        sUiCurrLayer = l;
        for (int i = 0; i < UI_TRANS_COUNT; i++) {
            uiTrans * self = &sUiTransList[i];
            if (self->initialized && self->base == TRUE) {
                //I am a root
                ui_transform_process_children(i);
            }
        }
    }

    int activemat = 0;
    for (int i = 0; i < UI_OBJECT_COUNT; i++) {
        uiObject * self = &sUiObjectList[i];
        if (self->initialized) {
            activemat++;
        }
    }

    //print_text_fmt_int(20, 100, "O %d", activemat);

    activemat = 0;
    for (int i = 0; i < UI_TRANS_COUNT; i++) {
        uiTrans * self = &sUiTransList[i];
        if (self->initialized) {
            activemat++;
        }
    }
    //print_text_fmt_int(20, 120, "T %d", activemat);
}

void ui_logic(void) {
    for (int i = 0; i < UI_TRANS_COUNT; i++) {
        uiTrans * self = &sUiTransList[i];
        if (self->initialized) {
            if (self->getout) {
                self->transitionFunction[1](i);
            } else {
                self->transitionFunction[0](i);
            }

            if (self->transition >= 1.0f) {
                self->transition = 1.0f;
                if (self->getout) {
                    ui_destroy_trans(i);
                }
            }
        }
    }

    if ((gMarioState->action & ACT_GROUP_MASK) == ACT_GROUP_STATIONARY) {
        sHudMarioMoveTimer -= .01f;
        sHudMarioMoveTimer = MAX(sHudMarioMoveTimer,0.0f);
    } else {
        sHudMarioMoveTimer = 1.0f;
    }

    // Hud
    for (int i = 0; i < 2; i++) {
        if (sUiidHudText[i] != UI_NONE ) {
            u8 size = ui_object_ptr(sUiidHudText[i])->textXsize;
            f32 emphasis = CLAMP(sHudEmphasisTimer[i],0.0f,1.0f);
            emphasis = smoothstep2(emphasis);
            ui_trans_ptr(sUiidHudTrans[i])->pos[0] = ((-size)+(emphasis*(f32)(22+size)));

            f32 bounce = sinf((CLAMP(sHudEmphasisTimer[i],3.0f,3.5f)-1.0f)*M_PI*4.0f)*3.0f;
            ui_trans_ptr(sUiidHudTrans[i])->pos[1] = 209-(20*i)+bounce;

            if (sHudMarioMoveTimer == 0.0f) {
                sHudEmphasisTimer[i] += .05f;
                sHudEmphasisTimer[i] = MIN(sHudEmphasisTimer[i],3.0f);
            } else {
                sHudEmphasisTimer[i] -= .05f;
                sHudEmphasisTimer[i] = MAX(sHudEmphasisTimer[i],0.0f);
            }
            if (sHudEmphasisTimer[i] == 0.0f) {
                ui_trans_ptr(sUiidHudTrans[i])->alpha = 0;
            } else {
                ui_trans_ptr(sUiidHudTrans[i])->alpha = 255;
            }
        }
    }
    if (ui_object_ptr(sUiidHudText[0])->printInt[0] != gMarioState->numCoins) {
        ui_object_ptr(sUiidHudText[0])->printInt[0] = gMarioState->numCoins;
        ui_trans_ptr(sUiidHudTrans[0])->posLerp[0] = 22;
        ui_trans_ptr(sUiidHudTrans[0])->pos[0] = 22;
        sHudEmphasisTimer[0] = 3.25f;
    }
    s16 starct = neo_save_collectible_count(gCurrNeoSaveFile->stars,STAR_MISSION_COUNT);
    if (ui_object_ptr(sUiidHudText[1])->printInt[0] != starct) {
        ui_object_ptr(sUiidHudText[1])->printInt[0] = starct;
        ui_trans_ptr(sUiidHudTrans[1])->posLerp[0] = 22;
        ui_trans_ptr(sUiidHudTrans[1])->pos[0] = 22;
        sHudEmphasisTimer[1] = 3.25f;
    }
}

void ui_init(void) {
    gUiidScreen = ui_create_transform(UI_NONE);
    Vec3f screenCorner = {-160.0f,-120.0f,-120.0f};
    ui_set_trans_pos(gUiidScreen,screenCorner);

    sUiidHudTrans[0] = ui_create_transform(gUiidScreen);
    sUiidHudTrans[1] = ui_create_transform(gUiidScreen);
    ui_set_trans_xy(sUiidHudTrans[0],22,189);
    ui_set_trans_xy(sUiidHudTrans[1],22,209);

    sUiidHudText[0] = ui_create_text(sUiidHudTrans[0],TEXT_COINS);
    sUiidHudText[1] = ui_create_text(sUiidHudTrans[1],TEXT_STARS);
    ui_object_ptr(sUiidHudText[0])->printFont = FONT_PINBALL;
    ui_object_ptr(sUiidHudText[1])->printFont = FONT_PINBALL;

    sHudEmphasisTimer[0] = 0.0f;
    sHudEmphasisTimer[1] = 0.0f;
    sHudMarioMoveTimer = 0.0f;
}

// Pause screen

void ui_pause_create(void) {
    sUiidPauseTrans = ui_create_transform(gUiidScreen);
    uiid pauseRectangle = ui_create_rectangle(sUiidPauseTrans,0,240,320,0);
    ui_set_trans_color(sUiidPauseTrans,0,0,0);
    ui_object_ptr(pauseRectangle)->alpha = 100;

    uiid transPauseText = ui_create_transform(sUiidPauseTrans);
    ui_create_text(transPauseText,TEXT_PAUSE);

    ui_set_destroy_signal(sUiidPauseTrans,&sPauseDestroySignal);
}

void ui_pause_destroy(void) {
    ui_trans_begin_remove(sUiidPauseTrans);
    sUiidPauseTrans = UI_NONE;
}

// Return 1 to unpause, Return 2 for exit course.
s32 ui_pause_logic(void) {
    if (sPauseDestroySignal) {
        return 1;
    }
    if (ui_is_not_transitioning(sUiidPauseTrans)) {
        if (gPlayer1Controller->buttonPressed & START_BUTTON) {
            ui_pause_destroy();
        }
    }
    return 0;
}