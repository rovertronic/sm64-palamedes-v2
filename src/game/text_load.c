#include "text_load.h"

#include "src/data/text.inc.c"

int gLanguage = LANGUAGE_ENGLISH;

char ** sLanguageTable[] = {
    [LANGUAGE_ENGLISH] = sEnglishText,
    [LANGUAGE_GERMAN] = sGermanText,
};

char * get_text(int textEnum) {
    if (sLanguageTable[gLanguage][textEnum] == NULL) {
        return sLanguageTable[gLanguage][TEXT_ERROR];
    }
    return sLanguageTable[gLanguage][textEnum];
}