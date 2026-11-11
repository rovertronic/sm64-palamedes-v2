#include "sm64.h"
#include "src/data/text_enums.h"

#ifndef TEXT_LOAD_H
#define TEXT_LOAD_H

enum language {
    LANGUAGE_ENGLISH,
    LANGUAGE_GERMAN,
};

char * get_text(int textEnum);

extern int gLanguage;

#endif