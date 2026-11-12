#include "src/data/text_enums.h"

char * sEnglishText[TEXT_COUNT] = {
    [TEXT_LANGUAGE] = "%sLanguage: English%s",
    [TEXT_ERROR] = "Missing Translation",

    // Hud Strings
    [TEXT_PAUSE] = "Paused...",

    [TEXT_PROGRESS] = "Progress",
    [TEXT_SETTINGS] = "Settings",

    [TEXT_WIDESCREEN] = "%sWidescreen:%s",

    // Test Strings
    [TEXT_TEST] = "+-/()𝐋𝐑→← they/them 23/100 Falsches üben von xylophonmusik quält jeden größeren zwerg.\n@R@😡 DAS WAR EIN BEFEHL 😡@@ \nwow... @Y@😊@@\nThe quick brown fox jumps over the lazy dog.\nOh, @R@Ryan,@@ my beautiful @G@shrine bachelor@@, who hates @R@cardio@@ and @R@women@@!@@ Guide me the way with your @B@blue boxes@@.",
    [TEXT_TEST_2] = "@0@Another test string",
    [TEXT_TEST_3] = "@0@Automatic newline test. This string has no manual newlines, yet if it hits the edge of the box, it will wrap.",
    [TEXT_TEST_4] = "@0@This is the evil fucking tower. In order to find what you want, you're gonna have to lock in and climb it.",
    [TEXT_TESTCHOICE_1] = "@0@Tell me about the tower",
    [TEXT_TESTCHOICE_2] = "@0@Go fuck yourself",
    [TEXT_TESTCHOICE_3] = "@0@Say what?",
    [TEXT_TEST_PROMPT] = "@0@Got any questions?",
    [TEXT_TEST_END] = "@0@Well, that's all.",

    // Mission Names & Hints
    [TEXT_MISSION_TEST] = "Test Mission",

    // Star Groups (For organization in Star Display)
    [TEXT_STAR_GROUP_0] = "Course 1",
};

char * sGermanText[TEXT_COUNT] = {
    [TEXT_LANGUAGE] = "%sSprache: Deutsch%s",
    [TEXT_ERROR] = "Fehlende Übersetzung",

    [TEXT_PROGRESS] = "Fortschritt",
    [TEXT_SETTINGS] = "Einstellungen",

    [TEXT_WIDESCREEN] = "%sBreitbild:%s",
    [TEXT_PAUSE] = "Paused...",
};
