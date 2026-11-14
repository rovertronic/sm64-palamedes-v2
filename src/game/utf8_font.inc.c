// Will initialize size based on difference between last element's xUv and this, if size = 0. Otherwise, can be manually set
fontChar sCharListSM64DS[UTF8_COUNT] = {
    [UTF8_SPACE] = {
        .utf8code = ' ',
        .size = 6,
        .tex = NULL,
    },

    [UTF8_EXCLAMATION] = {
        .utf8code = '!',
        .xUv = 378,
        .tex = sm64DS_latin_i4,
        .size = 4,
    },
    [UTF8_APOSTROPHE] = {
        .utf8code = 0x27,
        .xUv = 382,
        .tex = sm64DS_latin_i4,
        .size = 2,
    },
    [UTF8_OPEN_BRACKET] = {
        .utf8code = '(',
        .xUv = 388,
        .tex = sm64DS_latin_i4,
        .size = 3,
    },
    [UTF8_CLOSE_BRACKET] = {
        .utf8code = ')',
        .xUv = 391,
        .tex = sm64DS_latin_i4,
        .size = 3,
    },
    [UTF8_PERCENTAGE] = {
        .utf8code = '*',
        .xUv = 371,
        .tex = sm64DS_latin_i4,
        .size = 6,
    },
    [UTF8_PLUS] = {
        .utf8code = '+',
        .xUv = 397,
        .tex = sm64DS_latin_i4,
        .size = 8,
    },
    [UTF8_COMMA] = {
        .utf8code = 0x2C,
        .xUv = 362,
        .tex = sm64DS_latin_i4,
        .size = 2,
    },
    [UTF8_DASH] = {
        .utf8code = 0x2D,
        .xUv = 395,
        .tex = sm64DS_latin_i4,
        .size = 4,
    },
    [UTF8_PERIOD] = {
        .utf8code = '.',
        .xUv = 356,
        .tex = sm64DS_latin_i4,
        .size = 2,
    },
    [UTF8_SLASH] = {
        .utf8code = '/',
        .xUv = 373,
        .tex = sm64DS_latin_i4,
        .size = 5,
    },

    [UTF8_0] = {
        .utf8code = '0',
        .xUv = 289,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_1] = {
        .utf8code = '1',
        .xUv = 296,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_2] = {
        .utf8code = '2',
        .xUv = 300,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_3] = {
        .utf8code = '3',
        .xUv = 307,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_4] = {
        .utf8code = '4',
        .xUv = 314,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_5] = {
        .utf8code = '5',
        .xUv = 321,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_6] = {
        .utf8code = '6',
        .xUv = 328,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_7] = {
        .utf8code = '7',
        .xUv = 335,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_8] = {
        .utf8code = '8',
        .xUv = 342,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_9] = {
        .utf8code = '9',
        .xUv = 349,
        .tex = sm64DS_latin_i4,
        .size = 6
    },

    [UTF8_COLON] = {
        .utf8code = ':',
        .xUv = 358,
        .tex = sm64DS_latin_i4,
        .size = 2
    },
    [UTF8_QUESTION] = {
        .utf8code = '?',
        .xUv = 364,
        .tex = sm64DS_latin_i4,
        .size = 6
    },

    [UTF8_UPPERCASE_A] = {
        .utf8code = 'A',
        .xUv = 1,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_B] = {
        .utf8code = 'B',
        .xUv = 7,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_C] = {
        .utf8code = 'C',
        .xUv = 13,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_D] = {
        .utf8code = 'D',
        .xUv = 19,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_E] = {
        .utf8code = 'E',
        .xUv = 25,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_F] = {
        .utf8code = 'F',
        .xUv = 31,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_G] = {
        .utf8code = 'G',
        .xUv = 37,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_H] = {
        .utf8code = 'H',
        .xUv = 43,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_I] = {
        .utf8code = 'I',
        .xUv = 50,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_J] = {
        .utf8code = 'J',
        .xUv = 54,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_K] = {
        .utf8code = 'K',
        .xUv = 60,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_L] = {
        .utf8code = 'L',
        .xUv = 66,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_M] = {
        .utf8code = 'M',
        .xUv = 71,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_N] = {
        .utf8code = 'N',
        .xUv = 77,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_O] = {
        .utf8code = 'O',
        .xUv = 83,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_P] = {
        .utf8code = 'P',
        .xUv = 89,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_Q] = {
        .utf8code = 'Q',
        .xUv = 95,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_R] = {
        .utf8code = 'R',
        .xUv = 102,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_S] = {
        .utf8code = 'S',
        .xUv = 108,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_T] = {
        .utf8code = 'T',
        .xUv = 114,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_U] = {
        .utf8code = 'U',
        .xUv = 120,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_V] = {
        .utf8code = 'V',
        .xUv = 126,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_W] = {
        .utf8code = 'W',
        .xUv = 132,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_X] = {
        .utf8code = 'X',
        .xUv = 138,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_Y] = {
        .utf8code = 'Y',
        .xUv = 144,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_UPPERCASE_Z] = {
        .utf8code = 'Z',
        .xUv = 150,
        .tex = sm64DS_latin_i4,
    },

    [UTF8_LOWERCASE_A] = {
        .utf8code = 'a',
        .xUv = 156,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_B] = {
        .utf8code = 'b',
        .xUv = 161,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_C] = {
        .utf8code = 'c',
        .xUv = 166,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_D] = {
        .utf8code = 'd',
        .xUv = 171,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_E] = {
        .utf8code = 'e',
        .xUv = 176,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_F] = {
        .utf8code = 'f',
        .xUv = 181,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_G] = {
        .utf8code = 'g',
        .xUv = 187,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_H] = {
        .utf8code = 'h',
        .xUv = 193,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_I] = {
        .utf8code = 'i',
        .xUv = 198,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_J] = {
        .utf8code = 'j',
        .xUv = 200,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_K] = {
        .utf8code = 'k',
        .xUv = 205,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_L] = {
        .utf8code = 'l',
        .xUv = 210,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_M] = {
        .utf8code = 'm',
        .xUv = 214,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_N] = {
        .utf8code = 'n',
        .xUv = 220,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_O] = {
        .utf8code = 'o',
        .xUv = 225,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_P] = {
        .utf8code = 'p',
        .xUv = 230,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_Q] = {
        .utf8code = 'q',
        .xUv = 235,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_R] = {
        .utf8code = 'r',
        .xUv = 240,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_S] = {
        .utf8code = 's',
        .xUv = 245,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_T] = {
        .utf8code = 't',
        .xUv = 250,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_U] = {
        .utf8code = 'u',
        .xUv = 255,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_V] = {
        .utf8code = 'v',
        .xUv = 260,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_W] = {
        .utf8code = 'w',
        .xUv = 266,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_X] = {
        .utf8code = 'x',
        .xUv = 272,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_Y] = {
        .utf8code = 'y',
        .xUv = 278,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_LOWERCASE_Z] = {
        .utf8code = 'z',
        .xUv = 284,
        .tex = sm64DS_latin_i4,
        .size = 4,
    },

    [UTF8_ESZETT] = {
        .utf8code = 0x00DF,
        .xUv = 257,
        .tex = sm64DS_latin_i4,
        .size = 4,
    },
    [UTF8_LOWERCASE_UMLAUT_A] = {
        .utf8code = 0x00E4,
        .xUv = 129,
        .tex = sm64DS_latin_i4,
        .size = 4,
        .xUvSecondary = LOWERCASE_UMLAUT_X,
        .sizeSecondary = LOWERCASE_UMLAUT_SIZE,
    },
    [UTF8_LOWERCASE_UMLAUT_O] = {
        .utf8code = 0x00F6,
        .xUv = 184,
        .tex = sm64DS_latin_i4,
        .size = 4,
        .xUvSecondary = LOWERCASE_UMLAUT_X,
        .sizeSecondary = LOWERCASE_UMLAUT_SIZE,
    },
    [UTF8_LOWERCASE_UMLAUT_U] = {
        .utf8code = 0x00FC,
        .xUv = 208,
        .tex = sm64DS_latin_i4,
        .size = 4,
        .xUvSecondary = LOWERCASE_UMLAUT_X,
        .sizeSecondary = LOWERCASE_UMLAUT_SIZE,
    },

    [UTF8_LEFT] = {
        .utf8code = 0x2190, /*←*/
        .xUv = 419,
        .size = 4,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_RIGHT] = {
        .utf8code = 0x2192, /*→*/
        .xUv = 423,
        .size = 4,
        .tex = sm64DS_latin_i4,
    },

    [UTF8_L_BUTTON] = {
        .utf8code = 0x1D40B, /*𝐋*/
        .xUv = 413,
        .size = 5,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_R_BUTTON] = {
        .utf8code = 0x1D411, /*𝐑*/
        .xUv = 406,
        .size = 6,
        .tex = sm64DS_latin_i4,
    },

    [UTF8_SMILE] = {
        .utf8code = 0x1F60A,
        .xUv = 496,
        .size = 16,
        .tex = sm64DS_latin_i4,
    },
    [UTF8_ANGER] = {
        .utf8code = 0x1F621,
        .xUv = 480,
        .size = 16,
        .tex = sm64DS_latin_i4,
    },
};

fontChar sCharListPinball[] = {
    {
        .utf8code = ' ',
        .size = 16,
        .tex = NULL,
    },
    {
        .utf8code = '$',
        .xUv = 114,
        .size = 14,
        .tex = pinball_2_rgba16,
    },
    /*
    [UTF8_PERCENTAGE] = {
        .utf8code = '*',
        .xUv = 312,
        .tex = pinball_2_rgba16,
        .size = 6,
    },
    [UTF8_DASH] = {
        .utf8code = 0x2D,
        .xUv = 332,
        .tex = pinball_2_rgba16,
        .size = 4,
    },
    [UTF8_PERIOD] = {
        .utf8code = '.',
        .xUv = 300,
        .tex = pinball_2_rgba16,
        .size = 2,
    },
    [UTF8_SLASH] = {
        .utf8code = '/',
        .xUv = 373,
        .tex = pinball_2_rgba16,
        .size = 5,
    },
    */

    {
        .utf8code = '0',
        .xUv = 0,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '1',
        .xUv = 15,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '2',
        .xUv = 25,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '3',
        .xUv = 39,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '4',
        .xUv = 53,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '5',
        .xUv = 68,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '6',
        .xUv = 82,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '7',
        .xUv = 96,
        .tex = pinball_1_rgba16,
    },
    {
        .utf8code = '8',
        .xUv = 110,
        .tex = pinball_1_rgba16,
        .size = 14,
    },
    {
        .utf8code = '9',
        .xUv = 0,
        .tex = pinball_2_rgba16,
        .size = 14,
    },
    {
        .utf8code = 'X',
        .xUv = 14,
        .tex = pinball_2_rgba16,
        .size = 12
    },
    {
        .utf8code = '|',
        .xUv = 17,
        .size = 16,
        .tex = pinball_3_rgba16,
    },
    {
        .utf8code = 0x2605, /*★*/
        .xUv = 0,
        .size = 16,
        .tex = pinball_3_rgba16,
    },
};