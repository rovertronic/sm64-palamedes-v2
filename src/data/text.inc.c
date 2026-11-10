#include "src/data/text_enums.h"

char * sEnglishText[] = {
    // Hud Strings
    [TEXT_PAUSE] = "Paused...",

    // Debug Strings
    [TEXT_DEBUG_RAM] = "RAM Remaining %d*",
    [TEXT_DEBUG_DELETED] = "@0@Deleted.",

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
};

char * sGermanText[] = {
    [TEXT_TEST] = "prüfen",
    [TEXT_TEST_2] = "Eine weitere Testzeichenfolge",
};
