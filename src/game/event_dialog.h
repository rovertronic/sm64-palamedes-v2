#include "sm64.h"

#ifndef EVENT_DIALOG_H
#define EVENT_DIALOG_H

typedef union {
    void (* func)(int callContext);
    int i;
    f32 f;
    void * v;
} EventData;

typedef struct {
    int textId;
    EventData * jump;
} EventDialogOption;

enum {
    EVENT_CALL_CONTEXT_EXECUTE,
    EVENT_CALL_CONTEXT_HALTED,
};

void event_start(EventData * event);
void event_start_npc(EventData * event, struct Object * npcObj);

void event_system_logic_loop(void);

void event_camera_set_target_pointer(Vec3f pos, Vec3f foc);
void event_camera_set_target(Vec3f pos, Vec3f foc);
void event_camera_set(Vec3f pos, Vec3f foc);

void event_wait(int callContext);
void event_set_dialog(int callContext);
void event_close_dialog(int callContext);
void event_follow_spline(int callContext);
void event_v3ptr_camera_move(int callContext);
void event_return_to_conversation(int callContext);
void event_dialog_choice(int callContext);
void event_branch_cmd(int callContext);
void event_end_branch(int callContext);
void event_absolute_cinema(int callContext);
void event_end(int callContext);

#define E_DIALOG(TEXT)                {.func = event_set_dialog}, { .i = TEXT }
#define E_WAIT(FRAMES)                {.func = event_wait}, { .i = FRAMES }
#define E_CLOSE_DIALOG()              {.func = event_close_dialog}
#define E_SPLINE(POS,FOC)             {.func = event_follow_spline}, { .v = POS }, { .v = FOC }
#define E_CAMERA_TO_V3PTR(POS,FOC)    {.func = event_v3ptr_camera_move}, { .v = POS }, { .v = FOC }
#define E_RETURN_TO_CONVERSATION()    {.func = event_return_to_conversation}
#define E_DIALOG_CHOICE(TEXT, TO)     {.func = event_dialog_choice}, { .i = TEXT }, { .v = TO }
#define E_BRANCH(TO)                  {.func = event_branch_cmd}, { .v = TO }
#define E_END_BRANCH()                {.func = event_end_branch}
#define E_CINEMA_TOGGLE()             {.func = event_absolute_cinema}
#define E_END()                       {.func = event_end}

extern Vec3f gEventCameraPos;
extern Vec3f gEventCameraFoc;

extern EventData * gEventHead;

#endif