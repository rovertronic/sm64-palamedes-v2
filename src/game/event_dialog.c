#include "event_dialog.h"
#include "text_load.h"
#include "utf8_print.h"
#include "level_update.h"
#include "mario.h"
#include "camera.h"
#include "engine/math_util.h"
#include "ui.h"
#include "src/data/event.inc.c"
#include "game_init.h"
#include "object_helpers.h"


Vec3f gEventCameraPos;
Vec3f gEventCameraFoc;
Vec3f sEventCameraTargetPos;
Vec3f sEventCameraTargetFoc;
f32 * sEventCameraTargetPosPointer;
f32 * sEventCameraTargetFocPointer;
f32 sEventCameraTransition;
Bool8 sEventCameraTransitionDone = FALSE;
s16 sEventTimer = 0;

struct CutsceneSplinePoint * sEventCameraSpline[2];
s16 sEventCameraSplineSegment[2];
f32 sEventCameraSplineProgress[2];

Vec3f sEventCameraConversationPos;
Vec3f sEventCameraConversationFoc;

f32 sEventCameraTransition = 0.0f;

EventDialogOption sEventDialogOptionArray[5];
int sEventDialogOptionCount = 0;
int sEventDialogOptionIndex = 0;

EventData * gEventHead = NULL;
EventData * sEventStackArray[5];
EventData * sEventStartStackArray[5];
int sEventStackIndex = 0;
Bool8 sEventHalt = FALSE;

uiid sUiidDialogTransform;
uiid sUiidDialogSlice;
uiid sUiidDialogText;
uiid sUiidDialogOptionTransform[5];

uiid sUiidCinemaTrans[2] = {UI_NONE};

// Branch
void event_branch(EventData * branchLocation) {
    // Check if it's already in the stack to avoid infinite recursion
    for (int i = 0; i < sEventStackIndex+1; i++) {
        if (sEventStartStackArray[i] == branchLocation) {
            gEventHead = sEventStartStackArray[i];
            sEventStackIndex = i;
            return;
        }
    }

    sEventStackArray[sEventStackIndex] = gEventHead;
    sEventStackIndex++;
    sEventStartStackArray[sEventStackIndex] = branchLocation;
    gEventHead = branchLocation;
}

// Event argument getters
int event_arg_int(int num) {
    num++;
    return (*(gEventHead+num)).i;
}

f32 * event_arg_vec3f(int num) {
    num++;
    return (f32 *)((*(gEventHead+num)).v);
}

EventData * event_arg_event(int num) {
    num++;
    return (EventData *)(*(gEventHead+num)).v;
}

struct CutsceneSplinePoint * event_arg_spline(int num) {
    num++;
    return (struct CutsceneSplinePoint *)((*(gEventHead+num)).v);
}

// DLCC Only, not formally part of engine
u8 stickyNoteColors[4][3] = {
    {0xFF,0xF9,0x5D},
    {0xA9,0xCC,0x28},
    {0xFF,0xC6,0xB8},
    {0xFF,0xD8,0x00},
};

// Commands
void event_wait(int callContext) {
    switch(callContext) {
        case EVENT_CALL_CONTEXT_EXECUTE:
            sEventTimer = 0;
            sEventHalt = TRUE;
            break;
        case EVENT_CALL_CONTEXT_HALTED:
            sEventTimer++;
            if (sEventTimer >= event_arg_int(0)) {
                sEventHalt = FALSE;
                gEventHead+=2;
            }
            break;
    }
}

void event_set_dialog(int callContext) {
    switch(callContext) {
        case EVENT_CALL_CONTEXT_EXECUTE:
            sEventHalt = TRUE;

            u8 first = TRUE;
            if (sUiidDialogTransform != UI_NONE) {
                ui_object_ptr(sUiidDialogSlice)->ptr = &gNotepadRipSliceParams;
                ui_trans_begin_remove(sUiidDialogTransform);
                ui_trans_ptr(sUiidDialogTransform)->layer = 1;
                first = FALSE;
            }

            sUiidDialogTransform = ui_create_transform(gUiidScreen);
            ui_set_trans_xy(sUiidDialogTransform, 40, 62);
            sUiidDialogSlice = ui_create_slice(sUiidDialogTransform,&gNotepadSliceParams,-10,27,247,-39);
            sUiidDialogText = ui_create_text(sUiidDialogTransform,event_arg_int(0));
            ui_object_ptr(sUiidDialogText)->x2 = 240;
            if (!first) {
                ui_set_transition_instant(sUiidDialogTransform);
            }
            ui_trans_ptr(sUiidDialogTransform)->transitionFunction[1] = ui_trans_transition_page_rip_out;
            
            for (int i = 0; i < sEventDialogOptionCount; i++) {
                sUiidDialogOptionTransform[i] = ui_create_transform(gUiidScreen);
                ui_set_trans_xy(sUiidDialogOptionTransform[i], 300, 200-(i*50));
                uiid btn = ui_create_btn(sUiidDialogOptionTransform[i], sEventDialogOptionArray[i].textId);
                ui_object_ptr(btn)->printOrigin = PRINT_ORIGIN_RIGHT;
                ui_set_trans_color(sUiidDialogOptionTransform[i],stickyNoteColors[i][0],stickyNoteColors[i][1],stickyNoteColors[i][2]);
            }
            break;
        case EVENT_CALL_CONTEXT_HALTED:
            if (gMarioState->controller->buttonPressed & A_BUTTON) {
                gEventHead+=2;
                sEventHalt = FALSE;
                if (sEventDialogOptionCount > 0) {
                    // Since the question dialog is skewed, fade out instead
                    ui_trans_ptr(sUiidDialogTransform)->transitionFunction[1] = ui_trans_transition_fade_out;

                    // Clear options
                    for (int i = 0; i < sEventDialogOptionCount; i++) {
                        ui_trans_begin_remove(sUiidDialogOptionTransform[i]);
                        sUiidDialogOptionTransform[i] = UI_NONE;
                    }

                    event_branch(sEventDialogOptionArray[sEventDialogOptionIndex].jump);
                    sEventDialogOptionCount = 0;
                    sEventDialogOptionIndex = 0;
                }
            }
            break;
    }
}

void event_close_dialog(UNUSED int callContext) {
    if (sUiidDialogTransform != UI_NONE) {
        ui_trans_begin_remove(sUiidDialogTransform);
    }

    gEventHead++;
}

void event_follow_spline(UNUSED int callContext) {
    sEventCameraSplineSegment[0] = 0;
    sEventCameraSplineProgress[0] = 0.0f;
    sEventCameraSplineSegment[1] = 0;
    sEventCameraSplineProgress[1] = 0.0f;

    sEventCameraTransition = 0.0f;

    sEventCameraSpline[0] = event_arg_spline(0);
    sEventCameraSpline[1] = event_arg_spline(1);

    gEventHead+=3;
}

void event_v3ptr_camera_move(UNUSED int callContext) {
    event_camera_set(event_arg_vec3f(0),event_arg_vec3f(1));

    gEventHead+=3;
}

void event_return_to_conversation(UNUSED int callContext) {
    event_camera_set_target_pointer(sEventCameraConversationPos,sEventCameraConversationFoc);
    gEventHead++;
}

void event_dialog_choice(UNUSED int callContext) {
    sEventDialogOptionArray[sEventDialogOptionCount].textId = event_arg_int(0);
    sEventDialogOptionArray[sEventDialogOptionCount].jump = event_arg_event(1);
    sEventDialogOptionCount++;
    gEventHead+=3;
}

void event_branch_cmd(UNUSED int callContext) {
    gEventHead+=2;
    event_branch(event_arg_event(0));
}

void event_end_branch(UNUSED int callContext) {
    sEventStackIndex--;
    gEventHead = sEventStackArray[sEventStackIndex];
}

void event_absolute_cinema(UNUSED int callContext) {
    if (sUiidCinemaTrans[0] == UI_NONE) {
        sUiidCinemaTrans[0] = ui_create_transform(gUiidScreen);
        sUiidCinemaTrans[1] = ui_create_transform(gUiidScreen);
        ui_set_trans_color(sUiidCinemaTrans[0],0,0,0);
        ui_set_trans_color(sUiidCinemaTrans[1],0,0,0);

        ui_create_rectangle(sUiidCinemaTrans[0],-142,56,320,0);
        ui_create_rectangle(sUiidCinemaTrans[1],-142,240,320,240-56);

        Vec3f start = {0,-56,0};
        Vec3f end = {0,0,0};
        ui_set_transition_slide(sUiidCinemaTrans[0],start,end);
        Vec3f start2 = {0,56,0};
        ui_set_transition_slide(sUiidCinemaTrans[1],start2,end);
    } else {
        ui_trans_begin_remove(sUiidCinemaTrans[0]);
        ui_trans_begin_remove(sUiidCinemaTrans[1]);
        sUiidCinemaTrans[0] = UI_NONE;
    }

    gEventHead++;
}

void event_end(UNUSED int callContext) {
    switch(callContext) {
        case EVENT_CALL_CONTEXT_EXECUTE:
            sEventHalt = TRUE;
            event_camera_set_target_pointer(gCamera->pos,gCamera->focus);

            if (sUiidDialogTransform != UI_NONE) {
                ui_trans_begin_remove(sUiidDialogTransform);
            }
            break;
        case EVENT_CALL_CONTEXT_HALTED:
            if (sEventCameraTransitionDone) {
                if (gMarioState->action == ACT_READING_NPC_DIALOG) {
                    set_mario_action(gMarioState, ACT_IDLE, 0);
                }
                disable_time_stop_including_mario();
                gEventHead = NULL;
            }
            break;
    }
}

// Event camera functions

void event_camera_set_target_pointer(Vec3f pos, Vec3f foc) {
    sEventCameraSpline[0] = 0;

    sEventCameraTargetPosPointer = pos;
    sEventCameraTargetFocPointer = foc;
    sEventCameraTransitionDone = FALSE;
    sEventCameraTransition = 0.0f;
}

void event_camera_set_target(Vec3f pos, Vec3f foc) {
    vec3f_copy(sEventCameraTargetPos,pos);
    vec3f_copy(sEventCameraTargetFoc,foc);
    event_camera_set_target_pointer(sEventCameraTargetPos,sEventCameraTargetFoc);
}

void event_camera_set(Vec3f pos, Vec3f foc) {
    vec3f_copy(sEventCameraTargetPos,pos);
    vec3f_copy(sEventCameraTargetFoc,foc);
    vec3f_copy(gEventCameraPos,pos);
    vec3f_copy(gEventCameraFoc,foc);
    event_camera_set_target_pointer(sEventCameraTargetPos,sEventCameraTargetFoc);
}

// Event control functions
void event_start(EventData * event) {
    if (gEventHead == NULL) {
        sEventHalt = FALSE;

        sUiidDialogTransform = UI_NONE;

        sEventCameraSpline[0] = 0;

        vec3f_copy(gEventCameraPos,gLakituState.pos);
        vec3f_copy(gEventCameraFoc,gLakituState.focus);
        event_camera_set_target_pointer(gEventCameraPos,gEventCameraFoc);
        sEventCameraTransition = 0.0f;

        gEventHead = event;

        sEventStartStackArray[0] = gEventHead;
        sEventStackIndex = 0;

        sEventDialogOptionCount = 0;

        enable_time_stop_including_mario();
    }
}

void event_start_npc(EventData * event, struct Object * npcObj) {
    if (gEventHead == NULL) {
        gMarioState->usedObj = npcObj;
        event_start(event);

        s16 talkAngle = obj_angle_to_object(gMarioState->marioObj,npcObj);

        vec3f_set(sEventCameraConversationPos,
            gMarioState->pos[0] + sins(talkAngle)*-250.0f - coss(talkAngle)*-200.0f,
            gMarioState->pos[1] + 70.0f,
            gMarioState->pos[2] + coss(talkAngle)*-250.0f + sins(talkAngle)*-200.0f
        );
        vec3f_set(sEventCameraConversationFoc,
            npcObj->oPosX - (sins(talkAngle)*-250.0f - coss(talkAngle)*-200.0f),
            npcObj->oPosY + 70.0f,
            npcObj->oPosZ - (coss(talkAngle)*-250.0f + sins(talkAngle)*-200.0f)
        );
        event_camera_set_target_pointer(sEventCameraConversationPos,sEventCameraConversationFoc);
    }
}

// Event hooks
void event_system_logic_loop(void) {
    if (gEventHead != NULL) {
        if (sEventHalt) {
            gEventHead->func(EVENT_CALL_CONTEXT_HALTED);
        } else {
            while(gEventHead && !sEventHalt) {
               gEventHead->func(EVENT_CALL_CONTEXT_EXECUTE);
            }
        }
    }

    // Choice Logic
    if (sEventDialogOptionCount > 0) {
        if (gMarioState->controller->buttonPressed & D_JPAD) {
            sEventDialogOptionIndex++;
        }
        if (gMarioState->controller->buttonPressed & U_JPAD) {
            sEventDialogOptionIndex--;
        }
        sEventDialogOptionIndex = (sEventDialogOptionCount +sEventDialogOptionIndex) % sEventDialogOptionCount;
    }

    // Camera Logic

    if (sEventCameraSpline[0]) {
        sEventCameraTargetPosPointer = sEventCameraTargetPos;
        sEventCameraTargetFocPointer = sEventCameraTargetFoc;

        if (
            move_point_along_spline(sEventCameraTargetPos, segmented_to_virtual(sEventCameraSpline[0]), &sEventCameraSplineSegment[0], &sEventCameraSplineProgress[0]) ||
            move_point_along_spline(sEventCameraTargetFoc, segmented_to_virtual(sEventCameraSpline[1]), &sEventCameraSplineSegment[1], &sEventCameraSplineProgress[1])
        ) {
            sEventCameraSpline[0] = 0;
        }
    }

    if (sEventCameraTargetPosPointer) {
        sEventCameraTransition+=.04f;
        if (sEventCameraTransition >= 1.0f) {
            sEventCameraTransition = 1.0f;
            sEventCameraTransitionDone = TRUE;
        }
        for (int i = 0; i < 3; i++) {
            gEventCameraPos[i] = approach_f32_asymptotic(gEventCameraPos[i],sEventCameraTargetPosPointer[i],smoothstep2(sEventCameraTransition));
            gEventCameraFoc[i] = approach_f32_asymptotic(gEventCameraFoc[i],sEventCameraTargetFocPointer[i],smoothstep2(sEventCameraTransition));
        }
    }

    // Event ui
    if (sEventDialogOptionCount > 0) {
        ui_trans_ptr(sUiidDialogTransform)->rot[1] =
        approach_f32_asymptotic(ui_trans_ptr(sUiidDialogTransform)->rot[1],0x4000,.2f);
    }
}

//render_rgba16_texture(130-hx,180-(i*40)+yOffset,pin_rgba16);