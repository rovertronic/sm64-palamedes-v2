#include <PR/ultratypes.h>
#include <PR/gbi.h>

#include "utf8_print.h"
#include "types.h"
#include "segment2.h"
#include "geo_misc.h"
#include "game_init.h"
#include "text_load.h"
#include "event_dialog.h"
#include "emutest.h"

//0-2 top, 3-5 bottom
u8 print_textcolor[6];
u8 print_italics = 0;

fontInfo * sPrintFont = NULL;
Texture * sCurrPrintTexture = NULL;

#include "utf8_font.inc.c"

fontInfo sFontInfoArray[] = {
    [FONT_SM64DS] = {
        .charCount = sizeof(sCharListSM64DS) / sizeof(fontChar),
        .spacing = 1,
        .printParam = PRINT_IA4_512x16,
        .charArray = sCharListSM64DS,
    },
    [FONT_PINBALL] = {
        .charCount = sizeof(sCharListPinball) / sizeof(fontChar),
        .spacing = -2,
        .printParam = PRINT_RGBA16_128x16,
        .charArray = sCharListPinball,
    },
};


nineSliceParams testSliceParams = {
    .texture = nine_slice_sample_rgba16,
    .centerTexture = NULL,
    .xDivide1 = 16,
    .sliceType = SLICE_TYPE_4,
};

nineSliceParams gStickySliceParams = {
    .texture = nine_slice_stickynote_rgba16,
    .centerTexture = NULL,
    .xDivide1 = 19,
    .sliceType = SLICE_TYPE_4,
};

nineSliceParams gNotepadSliceParams = {
    .texture = nine_slice_notepad_rgba16,
    .centerTexture = NULL,
    .xDivide1 = 8,
    .xDivide2 = 24,
    .sliceType = SLICE_TYPE_9,
};

nineSliceParams gNotepadRipSliceParams = {
    .texture = nine_slice_notepad_rip_rgba16,
    .centerTexture = NULL,
    .xDivide1 = 8,
    .xDivide2 = 24,
    .sliceType = SLICE_TYPE_9,
};

nineSliceParams gDefaultSliceParams = {
    .texture = nine_slice_default_rgba16,
    .centerTexture = NULL,
    .xDivide1 = 16,
    .sliceType = SLICE_TYPE_4,
};

void utf8_initialize_table(void) {
    for (int j = 0; j < 2; j++){
        sPrintFont = &sFontInfoArray[j];
        for (int i = 0; i < UTF8_COUNT; i++) {
            fontChar * curChar = &sPrintFont->charArray[i];
            fontChar * nextChar = &sPrintFont->charArray[i+1];

            if (curChar->size == 0) {
                curChar->size = (nextChar->xUv-curChar->xUv)-1;
            }
        }
    }
}

void set_print_texture(int printTextureParam, Texture * tex) {
    sCurrPrintTexture = tex;

    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING);
    if (gEmulator & (EMU_CONSOLE|EMU_ARES)) {
        gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
    } else {
        gDPSetTextureFilter(gDisplayListHead++, G_TF_POINT);
    }

    gDPSetTextureFilter(gDisplayListHead++, G_TF_POINT);
    gDPSetCombineMode(gDisplayListHead++, G_CC_FADEA, G_CC_FADEA);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gDPPipeSync(gDisplayListHead++);
    gSPTexture(gDisplayListHead++,65535, 65535, 0, 0, 1);
    switch(printTextureParam) {
        case PRINT_RGBA16_32x32:
            gDPSetTextureImage(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 1, tex);
            gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 0, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
            gDPLoadBlock(gDisplayListHead++,7, 0, 0, 1023, 256);
            gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, 0, 0, G_TX_CLAMP | G_TX_NOMIRROR, 5, 0, G_TX_CLAMP | G_TX_NOMIRROR, 5, 0);
            gDPSetTileSize(gDisplayListHead++,0, 0, 0, 124, 124);
            break;
        case PRINT_RGBA16_128x16:
            gDPSetTextureImage(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 1, tex);
            gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 0, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
            gDPLoadBlock(gDisplayListHead++,7, 0, 0, 2047, 64);
            gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 0, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 7, 0);
            gDPSetTileSize(gDisplayListHead++,0, 0, 0, 508, 60);
            break;
        case PRINT_IA4_512x16:
            gDPSetTextureImage(gDisplayListHead++,G_IM_FMT_IA, G_IM_SIZ_16b, 1, tex);
            gDPSetTile(gDisplayListHead++,G_IM_FMT_IA, G_IM_SIZ_16b, 0, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
            gDPLoadBlock(gDisplayListHead++,7, 0, 0, 2047, 64);
            gDPSetTile(gDisplayListHead++,G_IM_FMT_IA, G_IM_SIZ_4b, 32, 0, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 9, 0);
            gDPSetTileSize(gDisplayListHead++,0, 0, 0, 2044, 60);
            break;
    }
}

fontChar * get_fontchar_from_utf8_codepoint(u32 codepoint) {
    // Binary search
    int low = 0;
    int high = sPrintFont->charCount-1;

    while (low <= high) {
        int mid = low + (high - low)/2;

        u32 searchedCodepoint = sPrintFont->charArray[mid].utf8code;
        if (searchedCodepoint == codepoint) {
            return &sPrintFont->charArray[mid];
        } else if (codepoint < searchedCodepoint) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }

    }

    return NULL;
}

void render_fontchar(fontChar * fc ,int x, int y) {
    if (sCurrPrintTexture != fc->tex) {
        set_print_texture(sPrintFont->printParam,fc->tex);
        gDPSetCombineMode(gDisplayListHead++, G_CC_UI_TEXT, G_CC_UI_TEXT);
    }

    Vtx * charVerts = alloc_display_list(4 * sizeof(Vtx));

    u16 xUv = (fc->xUv)*32;
    u16 size = (fc->size+1)*32;

    u16 oUv = 0; //Uv offset

    make_vertex(charVerts, 0, x,          y,                  oUv, oUv+xUv,      16*32, print_textcolor[3], print_textcolor[4], print_textcolor[5], 255);
    make_vertex(charVerts, 1, x+fc->size+1, y,                  oUv, oUv+xUv+size, 16*32, print_textcolor[3], print_textcolor[4], print_textcolor[5], 255);
    make_vertex(charVerts, 2, print_italics+x,          y+16, oUv, oUv+xUv,      0,     print_textcolor[0], print_textcolor[1], print_textcolor[2], 255);
    make_vertex(charVerts, 3, print_italics+x+fc->size+1, y+16, oUv, oUv+xUv+size, 0,     print_textcolor[0], print_textcolor[1], print_textcolor[2], 255);

    gSPVertex(gDisplayListHead++,charVerts,4,0);
    gSP2Triangles(gDisplayListHead++, 0, 1, 2, 0, 1, 3, 2, 0);

    if (fc->xUvSecondary > 0) {
        charVerts = alloc_display_list(4 * sizeof(Vtx));
        xUv = fc->xUvSecondary*32;
        size = fc->sizeSecondary*32;

        make_vertex(charVerts, 0, x,          y,                  0, xUv,      16*32, print_textcolor[3], print_textcolor[4], print_textcolor[5], 255);
        make_vertex(charVerts, 1, x+fc->size, y,                  0, xUv+size, 16*32, print_textcolor[3], print_textcolor[4], print_textcolor[5], 255);
        make_vertex(charVerts, 2, print_italics+x,          y+16, 0, xUv,      0,     print_textcolor[0], print_textcolor[1], print_textcolor[2], 255);
        make_vertex(charVerts, 3, print_italics+x+fc->size, y+16, 0, xUv+size, 0,     print_textcolor[0], print_textcolor[1], print_textcolor[2], 255);

        gSPVertex(gDisplayListHead++,charVerts,4,0);
        gSP2Triangles(gDisplayListHead++, 0, 1, 2, 0, 1, 3, 2, 0);
    }
}

//returns byte size
s8 utf8_to_codepoint(const char *s, uint32_t *codepoint) {
    unsigned char c = s[0];
    if (c < 0x80) {
        *codepoint = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        *codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        *codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        *codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                     ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    return -1;
}

void utf8_print(char * str, int x, int y) {
    for (int i = 0; i < 6; i++) {
        print_textcolor[i] = 255;
    }
    print_italics = 0;

    int charIndex = 0;
    int printX = 0;
    int printY = 0;
    char * printHead = &str[charIndex];

    while((*printHead) != '\0') {

        if ((*printHead) == '\n') {
            printX = 0;
            printY -= PRINT_Y_OFFSET;

            charIndex++;
            printHead = &str[charIndex];
            continue;
        }

        if ((*printHead) == '@') {
            for (int i = 0; i < 6; i++) {
                print_textcolor[i] = 255;
            }
            print_italics = 0;

            printHead = &str[++charIndex];
            while ((*printHead) != '@') {
                switch (*printHead) {
                    case 'G':
                        print_textcolor[0] = 50;
                        print_textcolor[1] = 255;
                        print_textcolor[2] = 50;
                        print_textcolor[3] = 0;
                        print_textcolor[4] = 150;
                        print_textcolor[5] = 0;
                        break;
                    case 'R':
                        print_textcolor[0] = 255;
                        print_textcolor[1] = 50;
                        print_textcolor[2] = 50;
                        print_textcolor[3] = 170;
                        print_textcolor[4] = 0;
                        print_textcolor[5] = 0;
                        break;
                    case 'Y':
                        print_textcolor[0] = 255;
                        print_textcolor[1] = 255;
                        print_textcolor[2] = 100;
                        print_textcolor[3] = 255;
                        print_textcolor[4] = 255;
                        print_textcolor[5] = 0;
                        break;
                    case 'B':
                        print_textcolor[0] = 100;
                        print_textcolor[1] = 100;
                        print_textcolor[2] = 255;
                        print_textcolor[3] = 10;
                        print_textcolor[4] = 10;
                        print_textcolor[5] = 255;
                        break;
                    case '0':
                        print_textcolor[0] = 0;
                        print_textcolor[1] = 0;
                        print_textcolor[2] = 0;
                        print_textcolor[3] = 0;
                        print_textcolor[4] = 0;
                        print_textcolor[5] = 0;
                        break;
                    case 'I':
                        //print_italics = 4;
                        break;
                }
                printHead = &str[++charIndex];
            }
            printHead = &str[++charIndex];
            continue;
        }

        u32 codepoint;
        u8 size = utf8_to_codepoint(printHead,&codepoint);
        fontChar * fc = get_fontchar_from_utf8_codepoint(codepoint);

        if (fc != NULL) {
            if (fc->tex != NULL) {
                render_fontchar(fc,x+printX,y+printY);
            }
            printX += fc->size+sPrintFont->spacing;
        }
        
        charIndex += size;
        printHead = &str[charIndex];
    }
}

char sAutoNewlineBuffer[512];
char * utf8_autonewline(char * str, int maxX) {
    int charIndex = 0;
    int printX = 0;
    int printY = 0;
    char * printHead = &str[charIndex];

    int lastSpaceIndex = 0;

    sprintf(sAutoNewlineBuffer,"%s",str); // copy string into autonewline buffer
    str = sAutoNewlineBuffer;

    while((*printHead) != '\0') {
        if ((*printHead) == '\n') {
            printX = 0;
            printY -= PRINT_Y_OFFSET;

            lastSpaceIndex = charIndex;

            charIndex++;
            printHead = &str[charIndex];
            continue;
        }

        if ((*printHead) == '@') {
            print_italics = 0;

            printHead = &str[++charIndex];
            while ((*printHead) != '@') {
                printHead = &str[++charIndex];
            }
            printHead = &str[++charIndex];
            continue;
        }

        u32 codepoint;
        u8 size = utf8_to_codepoint(printHead,&codepoint);
        fontChar * fc = get_fontchar_from_utf8_codepoint(codepoint);

        if (fc != NULL) {
            printX += fc->size+sPrintFont->spacing;
        }
        
        charIndex += size;
        printHead = &str[charIndex];

        if ((*printHead) == ' ' || (*printHead) == '\0' || (*printHead) == '\n') {
            if (printX >= maxX) {
                str[lastSpaceIndex] = '\n';
                charIndex=lastSpaceIndex;
                printHead = &str[charIndex];
            }
            lastSpaceIndex = charIndex;
        }
    }
    return str;
}

void utf8_size(char * str, int * x, int * y) {
    int charIndex = 0;
    int printX = 0;
    int printXmax = 0;
    int printY = 0;
    char * printHead = &str[charIndex];

    while((*printHead) != '\0') {

        if ((*printHead) == '\n') {
            printX = 0;
            printY -= PRINT_Y_OFFSET;

            charIndex++;
            printHead = &str[charIndex];
            continue;
        }

        if ((*printHead) == '@') {
            printHead = &str[++charIndex];
            while ((*printHead) != '@') {
                printHead = &str[++charIndex];
            }
            printHead = &str[++charIndex];
            continue;
        }

        u32 codepoint;
        u8 size = utf8_to_codepoint(printHead,&codepoint);
        fontChar * fc = get_fontchar_from_utf8_codepoint(codepoint);

        if (fc != NULL) {
            printX += fc->size+sPrintFont->spacing;
        }
        if (str[charIndex+1] == '\0' && sPrintFont->spacing < 0) { printX -= sPrintFont->spacing-1;}
        printXmax = MAX(printX,printXmax);
        
        charIndex += size;
        printHead = &str[charIndex];
    }
    *x = printXmax;
    *y = printY;
}

void utf8_init_print(void) {
    sCurrPrintTexture = NULL;
}

void utf8_set_font(int fontID) {
    sPrintFont = &sFontInfoArray[fontID];
}

nineSliceParams * sCur9sliceParams = NULL;

void render_4slice(int x1, int y1, int x2, int y2) {
    Vtx * v = alloc_display_list(16 * sizeof(Vtx));
    u8 cSizL = sCur9sliceParams->xDivide1;
    u8 cSizR = 32-sCur9sliceParams->xDivide1;

    u8 cSizT = cSizL;
    u8 cSizB = cSizR;

    s16 rUv = 32*32;

    make_vertex(v, 0,  x1,       y1,       0,       0,            0,                   255, 255, 255, 255);
    make_vertex(v, 1,  x1+cSizL, y1,       0,       32*cSizL,     0,                   255, 255, 255, 255);
    make_vertex(v, 2,  x2-cSizR, y1,       0,       32*cSizL,     0,                   255, 255, 255, 255);
    make_vertex(v, 3,  x2,       y1,       0,       rUv,          0,                   255, 255, 255, 255);
       
    make_vertex(v, 4,  x1,       y1-cSizT, 0,       0,            cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 5,  x1+cSizL, y1-cSizT, 0,       32*cSizL,     cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 6,  x2-cSizR, y1-cSizT, 0,       32*cSizL,     cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 7,  x2,       y1-cSizT, 0,       rUv,          cSizL*32,            255, 255, 255, 255);
 
    make_vertex(v, 8,  x1,       y2+cSizB, 0,       0,            cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 9,  x1+cSizL, y2+cSizB, 0,       32*cSizL,     cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 10, x2-cSizR, y2+cSizB, 0,       32*cSizL,     cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 11, x2,       y2+cSizB, 0,       rUv,          cSizL*32,            255, 255, 255, 255);

    make_vertex(v, 12, x1,       y2,       0,       0,            rUv,                 255, 255, 255, 255);
    make_vertex(v, 13, x1+cSizL, y2,       0,       32*cSizL,     rUv,                 255, 255, 255, 255);
    make_vertex(v, 14, x2-cSizR, y2,       0,       32*cSizL,     rUv,                 255, 255, 255, 255);
    make_vertex(v, 15, x2,       y2,       0,       rUv,          rUv,                 255, 255, 255, 255);

    gSPVertex(gDisplayListHead++,v,16,0);
    
    for (int i = 0; i < 3; i++) {
        int o = i*4;
        gSP2Triangles(gDisplayListHead++, 5+o, 1+o, 0+o, 0, 5+o, 0+o, 4+o, 0);
        gSP2Triangles(gDisplayListHead++, 5+o, 6+o, 2+o, 0, 2+o, 1+o, 5+o, 0);
        gSP2Triangles(gDisplayListHead++, 7+o, 3+o, 2+o, 0, 2+o, 6+o, 7+o, 0);
    }
}

void render_9slice(int x1, int y1, int x2, int y2) {
    u8 cSizL = 8;
    u8 cSizR = 8;

    u8 cSizT = cSizL;
    u8 cSizB = cSizR;

    s16 rUv = 32*32;

    u16 uvX = ((x2-x1)*32)-(16*32);
    u16 uvY = ((y1-y2)*32)-(16*32);

    // CORNERS
    Vtx * v = alloc_display_list(16 * sizeof(Vtx));

    make_vertex(v, 0,  x1,       y1,       0,       0,            0,                   255, 255, 255, 255);
    make_vertex(v, 1,  x1+cSizL, y1,       0,       32*cSizL,     0,                   255, 255, 255, 255);
    make_vertex(v, 2,  x2-cSizR, y1,       0,       rUv-32*cSizR, 0,                   255, 255, 255, 255);
    make_vertex(v, 3,  x2,       y1,       0,       rUv,          0,                   255, 255, 255, 255);
       
    make_vertex(v, 4,  x1,       y1-cSizT, 0,       0,            cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 5,  x1+cSizL, y1-cSizT, 0,       32*cSizL,     cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 6,  x2-cSizR, y1-cSizT, 0,       rUv-32*cSizR, cSizL*32,            255, 255, 255, 255);
    make_vertex(v, 7,  x2,       y1-cSizT, 0,       rUv,          cSizL*32,            255, 255, 255, 255);
 
    make_vertex(v, 8,  x1,       y2+cSizB, 0,       0,            rUv-cSizL*32,        255, 255, 255, 255);
    make_vertex(v, 9,  x1+cSizL, y2+cSizB, 0,       32*cSizL,     rUv-cSizL*32,        255, 255, 255, 255);
    make_vertex(v, 10, x2-cSizR, y2+cSizB, 0,       rUv-32*cSizR, rUv-cSizL*32,        255, 255, 255, 255);
    make_vertex(v, 11, x2,       y2+cSizB, 0,       rUv,          rUv-cSizL*32,        255, 255, 255, 255);

    make_vertex(v, 12, x1,       y2,       0,       0,            rUv,                 255, 255, 255, 255);
    make_vertex(v, 13, x1+cSizL, y2,       0,       32*cSizL,     rUv,                 255, 255, 255, 255);
    make_vertex(v, 14, x2-cSizR, y2,       0,       rUv-32*cSizR, rUv,                 255, 255, 255, 255);
    make_vertex(v, 15, x2,       y2,       0,       rUv,          rUv,                 255, 255, 255, 255);

    gSPVertex(gDisplayListHead++,v,16,0);
    
    gSP2Triangles(gDisplayListHead++, 5, 1, 0, 0, 5, 0, 4, 0);
    gSP2Triangles(gDisplayListHead++, 7, 3, 2, 0, 2, 6, 7, 0);
    gSP2Triangles(gDisplayListHead++, 5+8, 1+8, 0+8, 0, 5+8, 0+8, 4+8, 0);
    gSP2Triangles(gDisplayListHead++, 7+8, 3+8, 2+8, 0, 2+8, 6+8, 7+8, 0);

    // TOP
    v = alloc_display_list(4 * sizeof(Vtx));

    make_vertex(v, 0,  x1+cSizL, y1,       0,       0,             0,                   255, 255, 255, 255);
    make_vertex(v, 1,  x2-cSizR, y1,       0,       uvX,           0,                   255, 255, 255, 255);
    make_vertex(v, 2,  x1+cSizL, y1-cSizT, 0,       0,             cSizT*32,            255, 255, 255, 255);
    make_vertex(v, 3,  x2-cSizR, y1-cSizT, 0,       uvX,           cSizT*32,            255, 255, 255, 255);

    gSPVertex(gDisplayListHead++,v,4,0);
	gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 2, G_TX_RENDERTILE, 0, G_TX_CLAMP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 3, 0);
    gDPPipeSync(gDisplayListHead++);
    gSP2Triangles(gDisplayListHead++, 0, 2, 1, 0, 2, 3, 1, 0);

    // BOTTOM
    v = alloc_display_list(4 * sizeof(Vtx));

    make_vertex(v, 0,  x1+cSizL, y2+cSizB, 0,       0,             0,                   255, 255, 255, 255);
    make_vertex(v, 1,  x2-cSizR, y2+cSizB, 0,       uvX,           0,                   255, 255, 255, 255);
    make_vertex(v, 2,  x1+cSizL, y2,       0,       0,             cSizT*32,            255, 255, 255, 255);
    make_vertex(v, 3,  x2-cSizR, y2,       0,       uvX,           cSizT*32,            255, 255, 255, 255);

    gSPVertex(gDisplayListHead++,v,4,0);
    gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 194, G_TX_RENDERTILE, 0, G_TX_CLAMP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 3, 0);
    gDPPipeSync(gDisplayListHead++);
    gSP2Triangles(gDisplayListHead++, 0, 2, 1, 0, 2, 3, 1, 0);

    // LEFT
    v = alloc_display_list(4 * sizeof(Vtx));

    make_vertex(v, 0,  x1,       y1-cSizT, 0,       0,            0,            255, 255, 255, 255);
    make_vertex(v, 1,  x1+cSizL, y1-cSizT, 0,       32*cSizL,     0,            255, 255, 255, 255);
    make_vertex(v, 2,  x1,       y2+cSizB, 0,       0,            uvY,          255, 255, 255, 255);
    make_vertex(v, 3,  x1+cSizL, y2+cSizB, 0,       32*cSizL,     uvY,          255, 255, 255, 255);

    gSPVertex(gDisplayListHead++,v,4,0);
    gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 64, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 3, 0);
    gDPPipeSync(gDisplayListHead++);
    gSP2Triangles(gDisplayListHead++, 0, 2, 1, 0, 2, 3, 1, 0);

    // RIGHT
    v = alloc_display_list(4 * sizeof(Vtx));

    make_vertex(v, 0,  x2-cSizR, y1-cSizT, 0,       0,            0,            255, 255, 255, 255);
    make_vertex(v, 1,  x2,       y1-cSizT, 0,       32*cSizL,     0,            255, 255, 255, 255);
    make_vertex(v, 2,  x2-cSizR, y2+cSizB, 0,       0,            uvY,          255, 255, 255, 255);
    make_vertex(v, 3,  x2,       y2+cSizB, 0,       32*cSizL,     uvY,          255, 255, 255, 255);

    gSPVertex(gDisplayListHead++,v,4,0);
    gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 70, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 3, 0);
    gDPPipeSync(gDisplayListHead++);
    gSP2Triangles(gDisplayListHead++, 0, 2, 1, 0, 2, 3, 1, 0);

    // CENTER
    v = alloc_display_list(4 * sizeof(Vtx));

    make_vertex(v, 0,  x1+cSizL, y1-cSizT, 0,       0,            0,            255, 255, 255, 255);
    make_vertex(v, 1,  x2-cSizL, y1-cSizT, 0,       uvX,          0,            255, 255, 255, 255);
    make_vertex(v, 2,  x1+cSizL, y2+cSizB, 0,       0,            uvY,          255, 255, 255, 255);
    make_vertex(v, 3,  x2-cSizR, y2+cSizB, 0,       uvX,          uvY,          255, 255, 255, 255);

    gSPVertex(gDisplayListHead++,v,4,0);
    gDPSetTile(gDisplayListHead++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 66, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0);
    gDPPipeSync(gDisplayListHead++);
    gSP2Triangles(gDisplayListHead++, 0, 2, 1, 0, 2, 3, 1, 0);
}

void init_slice_render(nineSliceParams * params) {
    sCur9sliceParams = params;

    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_POINT);
    gDPSetCombineMode(gDisplayListHead++, G_CC_FADEA, G_CC_FADEA);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

	gSPTexture(gDisplayListHead++,65535, 65535, 0, 0, 1);
    gDPPipeSync(gDisplayListHead++);
    gDPLoadTextureBlock(gDisplayListHead++, params->texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0,
        G_TX_WRAP, G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);

    gDPPipeSync(gDisplayListHead++);

    sCurrPrintTexture = params->texture;
}

void render_slice(int x1, int y1, int x2, int y2) {
    if (sCur9sliceParams->sliceType == SLICE_TYPE_4) {
        render_4slice(x1,y1,x2,y2);
    } else {
        render_9slice(x1,y1,x2,y2);
    }
}

void render_rgba16_texture(int x, int y, Texture * tex) {
    Vtx * v = alloc_display_list(4 * sizeof(Vtx));

    s16 eUv = 32*32;

    make_vertex(v, 0,   x,   y,    0,       0,   eUv,        255, 255, 255, 255);
    make_vertex(v, 1,   x+32,y,    0,       eUv, eUv,        255, 255, 255, 255);
    make_vertex(v, 2,   x,   y+32, 0,       0,   0,      255, 255, 255, 255);
    make_vertex(v, 3,   x+32,y+32, 0,       eUv, 0,      255, 255, 255, 255);

    // Load RGBA16
    set_print_texture(PRINT_RGBA16_32x32,tex);

    gSPVertex(gDisplayListHead++,v,16,0);
    gSP2Triangles(gDisplayListHead++, 0, 1, 2, 0, 1, 3, 2, 0);
}