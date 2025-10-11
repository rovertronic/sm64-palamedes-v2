/* Noteblock */
void bhv_noteblock(void) {
    o->oPosY = o->oHomeY + (sins(o->oTimer*5000) * o->oVelY);
    o->oVelY *= 0.95f;

    if (cur_obj_is_mario_on_platform()) {
        mario_stop_riding_and_holding(gMarioState);
        o->oTimer = 0;
        o->oVelY = 50.0f;
        cur_obj_play_sound_2(SOUND_GENERAL_CRAZY_BOX_BOING_SLOW);
        set_mario_action(gMarioState, ACT_DOUBLE_JUMP, 0);
        gMarioStates[0].vel[1] = 95.0f + (o->oBehParams2ndByte*10.0f);
    }
}
/* End Noteblock */

/* Star Switch from SM64 DS*/
Vec3f sStarSwitchCameraPos;
Vec3f sStarSwitchCameraFoc;

EventData eventStarSwitch[] = {
    E_WAIT(20),
    E_CAMERA_TO_V3PTR(sStarSwitchCameraPos,sStarSwitchCameraFoc),
    E_WAIT(30),
    E_END(),
};

EventData eventStarSwitchEnd[] = {
    E_CAMERA_TO_V3PTR(sStarSwitchCameraPos,sStarSwitchCameraFoc),
    E_WAIT(30),
    E_END(),
};

void bhv_star_switch(void) {
    struct Object * myStarGlass = cur_obj_nearest_object_with_behavior(bhvStarSwitchStar);
    if (myStarGlass == NULL) {return;}

    switch(o->oAction) {
        case 0:
            if (gMarioObject->platform == o) {
                o->oAction++;
                vec3f_copy(sStarSwitchCameraFoc,&myStarGlass->oPosVec);
                vec3f_copy(sStarSwitchCameraPos,&myStarGlass->oPosVec);
                s16 angle = obj_angle_to_object(myStarGlass,gMarioObject);
                sStarSwitchCameraPos[0] += sins(angle) * 900.0f;
                sStarSwitchCameraPos[2] += coss(angle) * 900.0f;
                sStarSwitchCameraPos[1] += 250.0f;

                cur_obj_play_sound_2(SOUND_GENERAL2_PURPLE_SWITCH);

                event_start(eventStarSwitch);
                o->activeFlags |= ACTIVE_FLAG_INITIATED_TIME_STOP;
                myStarGlass->activeFlags |= ACTIVE_FLAG_INITIATED_TIME_STOP;
            }
            break;
        case 1:
            if (o->oPosY > o->oHomeY - 20.0f) {
                o->oPosY-=2.0f;
                gMarioObject->header.gfx.pos[1] -= 2.0f;
            }
            if (o->oTimer == 20) {
                myStarGlass->oAction = 1;
            }
            if (gEventHead == NULL) {
                o->oAction++;
            }
            break;
        case 2:
            if (o->oTimer <= (o->oBehParams2ndByte*30)-((o->oBehParams2ndByte*30)*.4f)) {
                play_sound(SOUND_GENERAL2_SWITCH_TICK_FAST, gGlobalSoundSource);
            } else {
                play_sound(SOUND_GENERAL2_SWITCH_TICK_SLOW, gGlobalSoundSource);
            }

            if (o->oTimer >= o->oBehParams2ndByte*30) {
                o->oAction++;
                myStarGlass->oAction=3;
            }
            break;
        case 3:
            if (o->oTimer == 0) {
                event_start(eventStarSwitchEnd);

                o->oPosY = o->oHomeY;
                myStarGlass->oAction=4;
                o->oAction++;
            }
            if (o->oTimer >= 10) {
                o->oPosY = o->oHomeY;
                myStarGlass->oAction=4;
                o->oAction++;
            }
            break;
        case 4:
            if (gEventHead == NULL && gMarioObject->platform != o) {
                o->oAction = 0;
            }
            break;
    }
}
void bhv_star_switch_star(void) {
    switch(o->oAction) {
        case 1:
            o->prevObj = spawn_object(o,MODEL_STAR,bhvStar);
            o->prevObj->activeFlags |= ACTIVE_FLAG_INITIATED_TIME_STOP;
            o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_STAR_GLASS];
            o->oTimer = 0;
            o->oAction++;
            // fallthrough

        case 2: // Running
            if (o->oTimer <= 10) {
                obj_scale(o->prevObj,(f32)o->oTimer/10.f);
            }
            break;
        case 3: // Fail cutscene start
            o->prevObj->activeFlags |= ACTIVE_FLAG_INITIATED_TIME_STOP;
            break;
        case 4: // Delete & Reset
            if (o->oTimer <= 10) {
                obj_scale(o->prevObj,(f32)(10-o->oTimer)/10.f);
            }
            if (o->oTimer == 10) {
                obj_mark_for_deletion(o->prevObj);
            }
            if (o->oTimer >= 20) {
                o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_NONE];
                o->oAction=0;
            }
             break;
        case 5: //Shatter, victory
            for (int i = 0; i < 15; i++) {
                spawn_object(o,MODEL_STAR_GLASS_SHARD,bhvStarSwitchShard);
            }
            o->parentObj->oAction = 6;
            obj_mark_for_deletion(o);
            break;
    }
}

void bhv_star_switch_star_shard(void) {
    if (o->oTimer == 0) {
        o->oVelX = random_float()*40.0f-20.0f;
        o->oVelY = random_float()*40.0f;
        o->oVelZ = random_float()*40.0f-20.0f;

        o->oFaceAngleYaw = random_u16();
        o->oFaceAnglePitch = random_u16();
    }

    o->oPosX += o->oVelX;
    o->oPosY += o->oVelY;
    o->oPosZ += o->oVelZ;

    o->oVelY -= 2.0f;

    o->oFaceAngleYaw += 0x200;
    o->oFaceAnglePitch += 0x20;
    o->oFaceAngleRoll += 0x300;

    if (o->oTimer > 200) {
        obj_mark_for_deletion(o);
    }
}
/* End Star Switch from SM64 DS*/

/* On/Off Block & Switch */
u8 sOnOffState = FALSE;

void bhv_onoffswitch(void) {
    switch(o->oAction) {
        case 0: // init
            o->oAnimState = o->oBehParams2ndByte;
            o->oAction = 1;
            break;
        case 1: // switch up
            o->header.gfx.scale[1] = approach_f32_symmetric(o->header.gfx.scale[1], 1.0f ,0.1f);
            if (gMarioObject->platform == o) {
                cur_obj_play_sound_2(SOUND_GENERAL_DOOR_TURN_KEY);
                sOnOffState = o->oBehParams2ndByte;
            }
            if (o->oBehParams2ndByte == 0) {
                if (!sOnOffState) {
                    o->oAction = 2;
                }
            } else { // Blue
                if (sOnOffState) {
                    o->oAction = 2;
                }
            }
            break;
        case 2: // switch down
            o->header.gfx.scale[1] = approach_f32_symmetric(o->header.gfx.scale[1], 0.1f ,0.1f);
            if (o->oBehParams2ndByte == 0) {
                if (sOnOffState) {
                    o->oAction = 1;
                }
            } else { // Blue
                if (!sOnOffState) {
                    o->oAction = 1;
                }
            break;
            }
    }
}

void bhv_onoffblock(void) {
    o->header.gfx.node.flags |= GRAPH_RENDER_ACTIVE;

    switch(o->oAction){
        case 0: //init
            if (o->oBehParams2ndByte == 0) {
                o->oAnimState = 0;
            } else {
                o->oAnimState = 1;
            }
            o->oAction = 1;
            o->prevObj = spawn_object(o,MODEL_ONOFF_EXCLAMATION,bhvStaticObject);
            o->prevObj->oAnimState = !o->oBehParams2ndByte;
            break;
        case 1: //loop
            if (o->oBehParams2ndByte == 0) {
                if (sOnOffState) {
                    o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_ONOFF_HOLLOW];
                } else {
                    load_object_collision_model();
                    o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_ONOFF_SOLID];
                }
            } else {
                if (sOnOffState) {
                    load_object_collision_model();
                    o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_ONOFF_SOLID];
                } else {
                    o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_ONOFF_HOLLOW];
                }
            }
            o->prevObj->oFaceAngleYaw += 0x100;
            o->prevObj->oPosY = o->oPosY + sins(o->oTimer*0x200) * 4.0f;
            break;
    }
}
/* End On/Off Block & Switch */