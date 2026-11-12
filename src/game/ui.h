#include "sm64.h"
#include "utf8_print.h"

#ifndef UI_H
#define UI_H

typedef s8 uiid;
typedef u8 uiDestroySignal;

uiid ui_create_transform(s8 parentId);
uiid ui_create_text(s8 parentId, s16 textId);
uiid ui_create_text_raw(s8 parentId, char * text);
uiid ui_create_slice(s8 parentId, nineSliceParams * p, s16 x1, s16 y1, s16 x2, s16 y2);
uiid ui_create_btn(s8 parentId, s16 textId);
uiid ui_create_rectangle(s8 parentId, s16 x1, s16 y1, s16 x2, s16 y2);

uiid ui_create_text_quick(s8 parentId, s8 * textObject, char * text, s16 x, s16 y, u8 print_origin);

s32 ui_is_not_transitioning(uiid myId);
void ui_set_destroy_signal(uiid myId, uiDestroySignal * destroySignal);
void ui_destroy_trans(uiid idPtr);

void ui_set_trans_pos(s8 myId, Vec3f pos);
void ui_set_trans_xy(s8 myId, s16 x, s16 y);
void ui_set_text(s8 myId, s16 textId);
void ui_set_object_color(s8 myId, u8 r, u8 g, u8 b);

void ui_set_transition_instant(s8 myId);
void ui_set_transition_slide(uiid myId, Vec3f start, Vec3f end);

void ui_trans_begin_remove(s8 myId);

void ui_trans_transition_fade_in(uiid myId);
void ui_trans_transition_fade_out(uiid myId);
void ui_trans_transition_instant(uiid myId);
void ui_trans_transition_page_rip_out(uiid myId);
void ui_trans_transition_slide_in(uiid myId);
void ui_trans_transition_slide_out(uiid myId);

extern uiid gUiidScreen;

void ui_render(void);
void ui_logic(void);
void ui_init(void);

void ui_pause_create(void);
void ui_pause_destroy(void);
s32 ui_pause_logic(void);

void ui_hud_create(void);
void ui_hud_update(void);
void ui_hud_destroy(void);

s32 ui_get_input(int dir);

#define UI_NONE -1
#define UI_OBJECT_COUNT 70

enum {
    UI_CLASS_TEXT,
    UI_CLASS_SLICE,
    UI_CLASS_BUTTON,
    UI_CLASS_IMAGE,
    UI_CLASS_BOX,
    UI_CLASS_TEXT_RAW,
    UI_CLASS_TRANSFORM,
};

enum {
    PRINT_ORIGIN_LEFT,
    PRINT_ORIGIN_CENTER,
    PRINT_ORIGIN_RIGHT,
};

typedef struct {
    u8 reserved;
} uiTransformData;

typedef struct {
    u8 * texture;
} uiImageData;

typedef struct {
    u16 text;
    u8 textXsize;
    u8 printParam;
    u8 printOrigin;
    u8 printFont;
    s16 x2;
} uiTextData;

typedef struct {
    char * ptr;
    u8 textXsize;
    u8 printParam;
    u8 printOrigin;
    u8 printFont;
} uiTextRawData;

typedef struct {
    u16 text;
    u8 textXsize;
    u8 printParam;
    u8 printOrigin;
    u8 printFont;
} uiButtonData;

typedef struct {
    nineSliceParams * ptr;
    s16 x1;
    s16 y1;
    s16 x2;
    s16 y2;
} uiSliceData;

typedef struct {
    s16 x1;
    s16 y1;
    s16 x2;
    s16 y2;
} uiBoxData;

typedef struct {
    u8 initialized:1;
    u8 base:1;
    u8 getout:1;
    u8 layer:1;

    uiid parent;
    uiid childlist;
    uiid next;
    uiid prev;

    u8 type;

    Vec3f pos;
    Vec3f posLerp;
    Vec3f slideStart;
    Vec3f slideEnd;
    Vec3f rot;
    Vec3f rotLerp;

    void (* transitionFunction[2])(uiid self);
    uiDestroySignal * destroySignal;
    f32 transition;
    u8 alpha;
    u8 color[3];

    void * data;
} uiObject;

uiObject * ui_object_ptr(s8 myId);

#endif
