#include "types.h"

#ifndef UTF8_PRINT_H
#define UTF8_PRINT_H

enum {
    PRINT_RGBA16_32x32,
    PRINT_RGBA16_128x16,
    PRINT_IA4_512x16,
};

enum {
    UTF8_SPACE,

    UTF8_EXCLAMATION,
    UTF8_APOSTROPHE,
    UTF8_OPEN_BRACKET,
    UTF8_CLOSE_BRACKET,
    UTF8_PERCENTAGE,
    UTF8_PLUS,
    UTF8_COMMA,
    UTF8_DASH,
    UTF8_PERIOD,
    UTF8_SLASH,

    UTF8_0,
    UTF8_1,
    UTF8_2,
    UTF8_3,
    UTF8_4,
    UTF8_5,
    UTF8_6,
    UTF8_7,
    UTF8_8,
    UTF8_9,

    UTF8_COLON,
    UTF8_QUESTION,

    UTF8_UPPERCASE_A,
    UTF8_UPPERCASE_B,
    UTF8_UPPERCASE_C,
    UTF8_UPPERCASE_D,
    UTF8_UPPERCASE_E,
    UTF8_UPPERCASE_F,
    UTF8_UPPERCASE_G,
    UTF8_UPPERCASE_H,
    UTF8_UPPERCASE_I,
    UTF8_UPPERCASE_J,
    UTF8_UPPERCASE_K,
    UTF8_UPPERCASE_L,
    UTF8_UPPERCASE_M,
    UTF8_UPPERCASE_N,
    UTF8_UPPERCASE_O,
    UTF8_UPPERCASE_P,
    UTF8_UPPERCASE_Q,
    UTF8_UPPERCASE_R,
    UTF8_UPPERCASE_S,
    UTF8_UPPERCASE_T,
    UTF8_UPPERCASE_U,
    UTF8_UPPERCASE_V,
    UTF8_UPPERCASE_W,
    UTF8_UPPERCASE_X,
    UTF8_UPPERCASE_Y,
    UTF8_UPPERCASE_Z,

    UTF8_LOWERCASE_A,
    UTF8_LOWERCASE_B,
    UTF8_LOWERCASE_C,
    UTF8_LOWERCASE_D,
    UTF8_LOWERCASE_E,
    UTF8_LOWERCASE_F,
    UTF8_LOWERCASE_G,
    UTF8_LOWERCASE_H,
    UTF8_LOWERCASE_I,
    UTF8_LOWERCASE_J,
    UTF8_LOWERCASE_K,
    UTF8_LOWERCASE_L,
    UTF8_LOWERCASE_M,
    UTF8_LOWERCASE_N,
    UTF8_LOWERCASE_O,
    UTF8_LOWERCASE_P,
    UTF8_LOWERCASE_Q,
    UTF8_LOWERCASE_R,
    UTF8_LOWERCASE_S,
    UTF8_LOWERCASE_T,
    UTF8_LOWERCASE_U,
    UTF8_LOWERCASE_V,
    UTF8_LOWERCASE_W,
    UTF8_LOWERCASE_X,
    UTF8_LOWERCASE_Y,
    UTF8_LOWERCASE_Z,

    UTF8_ESZETT,
    UTF8_LOWERCASE_UMLAUT_A,
    UTF8_LOWERCASE_UMLAUT_O,
    UTF8_LOWERCASE_UMLAUT_U,

    UTF8_LEFT,
    UTF8_RIGHT,

    UTF8_L_BUTTON,
    UTF8_R_BUTTON,

    UTF8_SMILE,
    UTF8_ANGER,

    UTF8_COUNT
};

#define LOWERCASE_UMLAUT_X 236
#define LOWERCASE_UMLAUT_SIZE 3

#define PRINT_Y_OFFSET 16

enum {
    FONT_SM64DS,
    FONT_PINBALL,
};

typedef struct {
    u32 utf8code;
    Texture * tex;
    u16 xUv;
    u8 size;
    u16 xUvSecondary;
    u8 sizeSecondary;
} fontChar;

typedef struct {
    int charCount;
    int spacing;
    int printParam;
    fontChar * charArray;
} fontInfo;

void utf8_initialize_table(void);
void utf8_print(char * str, int x, int y);
void utf8_init_print(void);
void utf8_set_font(int fontID);
char * utf8_autonewline(char * str, int maxX);
void utf8_size(char * str, int * x, int * y);

enum {
    SLICE_TYPE_4,
    SLICE_TYPE_9
};

typedef struct {
    Texture * texture;
    Texture * centerTexture;
    u8 xDivide1;
    u8 xDivide2;
    u8 sliceType;
} nineSliceParams;

extern nineSliceParams gNotepadSliceParams;
extern nineSliceParams gNotepadRipSliceParams;
extern nineSliceParams gStickySliceParams;
extern nineSliceParams gDefaultSliceParams;

void init_slice_render(nineSliceParams * params);
void render_slice(int x1, int y1, int x2, int y2);

void render_rgba16_texture(int x, int y, Texture * tex);

#endif