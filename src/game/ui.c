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
#include "segment2.h"
#include "main.h"
#include <string.h>

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

#define UI_MEMORY_BLOCK_SIZE 8
#define UI_MEMORY_BLOCK_COUNT (1000 / UI_MEMORY_BLOCK_SIZE)
static u8 sUiMemoryPool[1000] ALIGNED8;
static u8 sUiMemoryBlocks[UI_MEMORY_BLOCK_COUNT];

static void * ui_alloc(u32 size) {
    if (size == 0 || size > sizeof(sUiMemoryPool)) {
        return NULL;
    }
    u32 blocks = (size + UI_MEMORY_BLOCK_SIZE - 1) / UI_MEMORY_BLOCK_SIZE;
    u32 run = 0;
    for (u32 i = 0; i < UI_MEMORY_BLOCK_COUNT; i++) {
        run = sUiMemoryBlocks[i] ? 0 : run + 1;
        if (run == blocks) {
            u32 start = i + 1 - blocks;
            sUiMemoryBlocks[start] = blocks;
            for (u32 j = start + 1; j <= i; j++) {
                sUiMemoryBlocks[j] = 0xFF;
            }
            u8 * data = &sUiMemoryPool[start * UI_MEMORY_BLOCK_SIZE];
            for (u32 j = 0; j < blocks * UI_MEMORY_BLOCK_SIZE; j++) {
                data[j] = 0;
            }
            return data;
        }
    }
    return NULL;
}

static void ui_free(void * data) {
    if (data == NULL) {
        return;
    }
    u32 start = ((u8 *) data - sUiMemoryPool) / UI_MEMORY_BLOCK_SIZE;
    u32 blocks = sUiMemoryBlocks[start];
    for (u32 i = start; i < start + blocks; i++) {
        sUiMemoryBlocks[i] = 0;
    }
}

static void * ui_realloc(void * data, u32 size) {
    if (data == NULL) {
        return ui_alloc(size);
    }
    if (size == 0) {
        ui_free(data);
        return NULL;
    }
    if (size > sizeof(sUiMemoryPool)) {
        return NULL;
    }

    u32 start = ((u8 *) data - sUiMemoryPool) / UI_MEMORY_BLOCK_SIZE;
    u32 oldBlocks = sUiMemoryBlocks[start];
    u32 blocks = (size + UI_MEMORY_BLOCK_SIZE - 1) / UI_MEMORY_BLOCK_SIZE;
    if (blocks <= oldBlocks) {
        sUiMemoryBlocks[start] = blocks;
        for (u32 i = start + blocks; i < start + oldBlocks; i++) {
            sUiMemoryBlocks[i] = 0;
        }
        return data;
    }

    u32 end = start + oldBlocks;
    while (end < start + blocks && end < UI_MEMORY_BLOCK_COUNT && sUiMemoryBlocks[end] == 0) {
        end++;
    }
    if (end == start + blocks) {
        for (u32 i = start + oldBlocks; i < end; i++) {
            sUiMemoryBlocks[i] = 0xFF;
        }
        sUiMemoryBlocks[start] = blocks;
        return data;
    }

    void * resized = ui_alloc(size);
    if (resized == NULL) {
        return NULL;
    }
    memcpy(resized, data, oldBlocks * UI_MEMORY_BLOCK_SIZE);
    ui_free(data);
    return resized;
}

static uiid ui_create_object(s8 parentId, u8 type, u32 dataSize) {
    uiid myId = 0;
    while (myId < UI_OBJECT_COUNT && sUiObjectList[myId].initialized) {
        myId++;
    }
    if (myId == UI_OBJECT_COUNT) {
        return UI_NONE;
    }

    void * data = ui_alloc(dataSize);
    if (data == NULL) {
        return UI_NONE;
    }

    uiObject * self = &sUiObjectList[myId];
    *self = (uiObject) {0};
    self->initialized = TRUE;
    self->type = type;
    self->data = data;
    self->parent = parentId;
    self->base = (parentId == UI_NONE);
    self->childlist = UI_NONE;
    self->next = UI_NONE;
    self->prev = UI_NONE;
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
    uiid myId = ui_create_object(parentId, UI_CLASS_TRANSFORM, sizeof(uiTransformData));
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

    if (self->type == UI_CLASS_TEXT_RAW) {
        uiTextRawData * data = (uiTextRawData *) self->data;
        if (data->buffer != NULL) {
            ui_free(*data->buffer);
            *data->buffer = NULL;
        }
    }
    ui_free(self->data);
    self->data = NULL;
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

uiid ui_create_image(s8 parentId, u8 * texture) {
    s8 myId = ui_create_object(parentId, UI_CLASS_IMAGE, sizeof(uiImageData));
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];
    uiImageData * data = (uiImageData *) self->data;

    data->texture = texture;

    return myId;
}

uiid ui_create_text(s8 parentId, s16 textId) {
    s8 myId = ui_create_object(parentId, UI_CLASS_TEXT, sizeof(uiTextData));
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    uiTextData * data = (uiTextData *) self->data;
    data->text = textId;
    data->x2 = 0;
    data->printFont = FONT_SM64DS;

    return myId;
}

uiid ui_create_text_raw(s8 parentId, char * text) {
    s8 myId = ui_create_object(parentId, UI_CLASS_TEXT_RAW, sizeof(uiTextRawData));
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    uiTextRawData * data = (uiTextRawData *) self->data;
    data->printFont = FONT_SM64DS;
    data->ptr = text;

    return myId;
}

// The pointer slot must outlive the object. Resizing updates that slot, and
// destroying the object frees the buffer and clears the slot.
static uiid ui_create_text_buffer(s8 parentId, char ** buffer) {
    char * text = ui_alloc(1);
    if (text == NULL) {
        return UI_NONE;
    }
    uiid myId = ui_create_text_raw(parentId, NULL);
    if (myId == UI_NONE) {
        ui_free(text);
        return UI_NONE;
    }
    *buffer = text;
    uiTextRawData * data = (uiTextRawData *) ui_object_ptr(myId)->data;
    data->buffer = buffer;
    return myId;
}

uiid ui_create_slice(s8 parentId, nineSliceParams * p, s16 x1, s16 y1, s16 x2, s16 y2) {
    s8 myId = ui_create_object(parentId, UI_CLASS_SLICE, sizeof(uiSliceData));
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    uiSliceData * data = (uiSliceData *) self->data;
    
    data->x1 = x1;
    data->y1 = y1;
    data->x2 = x2;
    data->y2 = y2;

    data->ptr = p;

    return myId;
}

uiid ui_create_rectangle(s8 parentId, s16 x1, s16 y1, s16 x2, s16 y2) {
    s8 myId = ui_create_object(parentId, UI_CLASS_BOX, sizeof(uiBoxData));
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    uiBoxData * data = (uiBoxData *) self->data;
    
    data->x1 = x1;
    data->y1 = y1;
    data->x2 = x2;
    data->y2 = y2;

    return myId;
}

uiid ui_create_btn(s8 parentId, s16 textId) {
    s8 myId = ui_create_object(parentId, UI_CLASS_BUTTON, sizeof(uiButtonData));
    if (myId == UI_NONE) {
        return UI_NONE;
    }
    uiObject * self = &sUiObjectList[myId];

    uiButtonData * data = (uiButtonData *) self->data;
    data->text = textId;
    data->printFont = FONT_SM64DS;

    return myId;
}

uiid ui_create_text_quick(s8 parentId, char * text, s16 x, s16 y, u8 print_origin) {
    uiid textId = ui_create_text_raw(parentId, text);
    ui_set_trans_xy(textId,x,y);

    if (textId == UI_NONE) {
        return UI_NONE;
    }
    ((uiTextRawData *) ui_object_ptr(textId)->data)->printOrigin = print_origin;

    return textId;
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
    switch (self->type) {
        case UI_CLASS_TEXT:
            ((uiTextData *) self->data)->text = textId;
            break;
        case UI_CLASS_BUTTON:
            ((uiButtonData *) self->data)->text = textId;
            break;
    }
}

void ui_set_object_color(s8 myId, u8 r, u8 g, u8 b) {
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

    f32 mixAlpha = ui_trans_get_parents_alpha(self->parent) * self->alpha;
    if (mixAlpha == 0.0f) {
        return;
    }
    u8 r = self->color[0];
    u8 g = self->color[1];
    u8 b = self->color[2];
    gDPSetEnvColor(gDisplayListHead++, r, g, b, mixAlpha);

    switch(self->type) {
        case UI_CLASS_TEXT: {
            uiTextData * data = (uiTextData *) self->data;
            utf8_init_print();
            utf8_set_font(data->printFont);

            str = get_text(data->text);

            utf8_size(str,&x,&y);
            data->textXsize = x;

            xoffset = 0;
            switch(data->printOrigin) {
                case PRINT_ORIGIN_LEFT:
                    xoffset = x/2;
                    break;
                case PRINT_ORIGIN_RIGHT:
                    xoffset = -x/2;
                    break;
            }

            if (data->x2 != 0) {
                str = utf8_autonewline(str,data->x2);
            }
            utf8_print(str,(-x/2)+xoffset,0);
            break;
        }
        case UI_CLASS_TEXT_RAW: {
            uiTextRawData * data = (uiTextRawData *) self->data;
            utf8_init_print();
            utf8_set_font(data->printFont);

            str = data->buffer != NULL ? *data->buffer : data->ptr;

            utf8_size(str,&x,&y);
            data->textXsize = x;

            xoffset = 0;
            switch(data->printOrigin) {
                case PRINT_ORIGIN_LEFT:
                    xoffset = x/2;
                    break;
                case PRINT_ORIGIN_RIGHT:
                    xoffset = -x/2;
                    break;
            }
            
            utf8_print(str,(-x/2)+xoffset,0);
            break;
        }
        case UI_CLASS_SLICE: {
            uiSliceData * data = (uiSliceData *) self->data;
            init_slice_render(data->ptr);
            render_slice(data->x1,data->y1, data->x2, data->y2);
            break;
        }
        case UI_CLASS_BUTTON: {
            uiButtonData * data = (uiButtonData *) self->data;
            str = get_text(data->text);

            utf8_size(str,&x,&y);
            data->textXsize = x;

            xoffset = 0;
            switch(data->printOrigin) {
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
            utf8_set_font(data->printFont);
            utf8_print(str,(-x/2)+xoffset,0);
            break;
        }
        case UI_CLASS_BOX: {
            uiBoxData * data = (uiBoxData *) self->data;
            Vtx * boxVerts = alloc_display_list(4 * sizeof(Vtx));

            make_vertex(boxVerts, 0, data->x1, data->y1,         0,   0,0,   255,255,255,255);
            make_vertex(boxVerts, 1, data->x2, data->y1,         0,   0,0,   255,255,255,255);
            make_vertex(boxVerts, 2, data->x1, data->y2,         0,   0,0,   255,255,255,255);
            make_vertex(boxVerts, 3, data->x2, data->y2,         0,   0,0,   255,255,255,255);

            gSPDisplayList(gDisplayListHead++,dl_ui_draw_text_bg_box);
            gSPVertex(gDisplayListHead++,boxVerts,4,0);
            gSP2Triangles(gDisplayListHead++, 1, 0, 2, 0, 3, 1, 2, 0);
            break;
        }
        case UI_CLASS_IMAGE: {
            uiImageData * data = (uiImageData *) self->data;
            render_rgba16_texture( 0, 0, data->texture );
            break;
        }
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
        ((uiTextRawData *) ui_object_ptr(sUiidHudText[0])->data)->printFont = FONT_PINBALL;
        ((uiTextRawData *) ui_object_ptr(sUiidHudText[1])->data)->printFont = FONT_PINBALL;

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
                u8 size = ((uiTextRawData *) ui_object_ptr(sUiidHudText[i])->data)->textXsize;
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

uiid sUiidPauseTabs[MAX_TAB_COUNT];
s8 sUiPauseTabIds[MAX_TAB_COUNT];
u8 sUiPauseTabCount = 0;

uiid sUiidPauseCursor = UI_NONE;

char * sUiPauseStringBuffers[SAVE_FILE_OPTION_COUNT];

s8 sUiPauseTabIndex = 0;
s8 sUiPauseTabIndexOld = -1;

uiid sUiidPauseTabContent = UI_NONE;
uiDestroySignal sUiPauseTabContentDestroySignal = TRUE;


s8 sUiPauseIndex2 = -1;

// OPTIONS
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
    for (int i = 0; i < SAVE_FILE_OPTION_COUNT; i++) {
        uiid textId = ui_create_text_buffer(sUiidPauseTabContent, &sUiPauseStringBuffers[i]);
        if (textId == UI_NONE) {
            continue;
        }
        uiObject * text = ui_object_ptr(textId);
        text->alpha = 0;
        text->transition = 0.0f;
        text->transitionFunction[0] = ui_trans_transition_fade_in;
        text->transitionFunction[1] = ui_trans_transition_fade_out;
        ui_set_trans_xy(textId, 40, 150 - i * 20);
        ((uiTextRawData *) ui_object_ptr(textId)->data)->printOrigin = PRINT_ORIGIN_LEFT;
    }
}

static void ui_pause_option_format(int index, const char * format, const char * start, const char * end) {
    if (sUiPauseStringBuffers[index] == NULL) {
        return;
    }
    u32 size = strlen(format) + strlen(start) + strlen(end) + 1;
    char * buffer = ui_realloc(sUiPauseStringBuffers[index], size);
    if (buffer == NULL) {
        return;
    }
    sUiPauseStringBuffers[index] = buffer;
    sprintf(buffer, format, start, end);
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

    ui_pause_option_format(0, get_text(TEXT_WIDESCREEN), ui_pause_option_start(0), ui_pause_option_bool_end(0));
    ui_pause_option_format(1, get_text(TEXT_LANGUAGE),   ui_pause_option_start(1), ui_pause_option_generic_end(1));

    // Apply
    gLanguage = gCurrNeoSaveFile->options[1];
    gConfig.widescreen = gCurrNeoSaveFile->options[0];
}

void ui_pause_progression_create(void) {
    uiid level_name = ui_create_text(sUiidPauseTabContent, TEXT_STAR_GROUP_0);
    ui_set_trans_xy(level_name, 40,140);
    uiid level_stars = ui_create_text_quick(sUiidPauseTabContent, "★★@0@★★@@★★★", 40, 120, PRINT_ORIGIN_LEFT);
    ((uiTextRawData *) ui_object_ptr(level_stars)->data)->printFont = FONT_PINBALL;
}

void ui_pause_progression_update(void) {

}


void (*sUiPauseTabFunctions[])(void) = {
    /* Tab 0 */ ui_pause_progression_create, ui_pause_progression_update,
    /* Tab 1 */ ui_pause_options_create, ui_pause_options_update,
};

void ui_pause_tab_create(int text, int tab_id) {
    sUiidPauseTabs[sUiPauseTabCount] = ui_create_btn(sUiidPauseTrans, text);
    uiObject * tab = ui_object_ptr(sUiidPauseTabs[sUiPauseTabCount]);
    tab->alpha = 0;
    tab->transition = 0.0f;
    tab->transitionFunction[0] = ui_trans_transition_fade_in;
    tab->transitionFunction[1] = ui_trans_transition_fade_out;
    ((uiButtonData *) ui_object_ptr(sUiidPauseTabs[sUiPauseTabCount])->data)->printOrigin = PRINT_ORIGIN_CENTER;
    sUiPauseTabIds[sUiPauseTabCount] = tab_id;
    sUiPauseTabCount++;
}

void ui_pause_tab_content_create() {
    sUiidPauseTabContent = ui_create_transform(sUiidPauseTrans);
    ui_set_destroy_signal(sUiidPauseTabContent, &sUiPauseTabContentDestroySignal);

    sUiPauseTabFunctions[(sUiPauseTabIndex * 2)+0]();
    sUiPauseTabFunctions[(sUiPauseTabIndex * 2)+1]();
}

void ui_pause_create(void) {
    sUiPauseTabIndex = 0;
    sUiPauseTabIndexOld = 0;

    sUiPauseTabContentDestroySignal = TRUE;

    sUiidPauseTrans = ui_create_transform(gUiidScreen);
    uiid pauseRectangle = ui_create_rectangle(sUiidPauseTrans,0,240,320,0);
    ui_set_object_color(pauseRectangle,0,0,0);
    ui_object_ptr(pauseRectangle)->alpha = 100;

    //uiid transPauseText = ui_create_transform(sUiidPauseTrans);
    //ui_create_text(transPauseText,TEXT_PAUSE);

    ui_create_text_quick(sUiidPauseTrans, "Paused...", 160, 210, PRINT_ORIGIN_CENTER);

    ui_set_destroy_signal(sUiidPauseTrans,&sPauseDestroySignal);
    ui_hud_destroy();

    sUiPauseTabCount = 0;
    ui_pause_tab_create(TEXT_PROGRESS, 0);
    ui_pause_tab_create(TEXT_SETTINGS, 1);

    int xtotalhalf = ((sUiPauseTabCount-1) * 80) / 2;
    for (int i = 0; i < sUiPauseTabCount; i++) {
        ui_set_trans_xy( sUiidPauseTabs[i], 160 + (i * 80) - xtotalhalf, 180 );
        //xoffset += ((uiButtonData *) ui_object_ptr(sUiidPauseTabs[i])->data)->textXsize;

        //int x, y;
        //utf8_size( get_text( ((uiButtonData *) ui_object_ptr(sUiidPauseTabs[i])->data)->text ), &x, &y );
        //xoffset += x;

        ui_set_object_color(sUiidPauseTabs[i],50,50,50);
    }

    sUiidPauseCursor = ui_create_image(sUiidPauseTrans, pin_rgba16);
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
                ui_object_ptr(sUiidPauseTabs[i])->color[0] = 255;
            } else {
                ui_object_ptr(sUiidPauseTabs[i])->color[0] = 50;
            }
        }

        if ((sUiPauseTabIndexOld != sUiPauseTabIndex) && (ui_is_not_transitioning(sUiidPauseTabContent))) {
            sUiPauseTabIndexOld = sUiPauseTabIndex;
            ui_trans_begin_remove(sUiidPauseTabContent);
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
                sUiPauseTabFunctions[(sUiPauseTabIndex * 2)+1]();
            }
        }
    }
    return 0;
}