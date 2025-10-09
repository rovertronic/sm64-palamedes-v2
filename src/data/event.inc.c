#include "src/data/text_enums.h"
#include "levels/test/header.inc.h"

EventData subEventTestOption1[] = {
    E_CINEMA_TOGGLE(),
    E_SPLINE(test_area_1_spline_testPos,test_area_1_spline_testFoc),
    E_DIALOG(TEXT_TEST_4),
    E_CINEMA_TOGGLE(),
    E_RETURN_TO_CONVERSATION(),
    E_END_BRANCH(),
};

Vec3f tst1 = {0,0,0};
Vec3f tst2 = {1,0,0};

EventData subEventTestOption2[] = {
    E_CAMERA_TO_V3PTR(tst1,tst2),
    E_DIALOG(TEXT_TESTCHOICE_2),
    E_END_BRANCH(),
};

EventData eventTestNPC[] = {
    E_DIALOG(TEXT_TEST_2),
    E_WAIT(60),
    E_DIALOG(TEXT_TEST_3),
    E_DIALOG_CHOICE(TEXT_TESTCHOICE_1,subEventTestOption1),
    E_DIALOG_CHOICE(TEXT_TESTCHOICE_2,subEventTestOption2),
    E_DIALOG_CHOICE(TEXT_TESTCHOICE_3,eventTestNPC),
    E_DIALOG(TEXT_TEST_PROMPT),
    E_DIALOG(TEXT_TEST_END),
    E_END(),
};