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
    E_WAIT(20),
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
                sStarSwitchCameraPos[0] += 400.0f;
                sStarSwitchCameraPos[1] += 150.0f;
                event_start(eventStarSwitch);
            }
            break;
        case 1:
            if (o->oPosY > o->oHomeY - 20.0f) {
                o->oPosY-=2.0f;
            }
            if (o->oTimer == 20) {
                myStarGlass->oAction = 1;
            }
            if (gEventHead == NULL) {
                o->oAction++;
            }
            break;
        case 2:
            if (o->oTimer >= 60) {
                event_start(eventStarSwitchEnd);
                o->oAction++;
            }
            break;
        case 3:
            if (o->oTimer >= 30) {
                o->oPosY = o->oHomeY;
                myStarGlass->oAction=3;
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
            o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_STAR_GLASS];
            o->oTimer = 0;
            o->oAction++;
            [[fallthrough]];
        case 2: // Running
            if (o->oTimer <= 10) {
                obj_scale(o->prevObj,(f32)o->oTimer/10.f);
            }
            break;
        case 3: // Delete & Reset
            if (o->oTimer <= 10) {
                obj_scale(o->prevObj,(f32)(10-o->oTimer)/10.f);
            }
            if (o->oTimer == 10) {
                obj_mark_for_deletion(o->prevObj);
                o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_NONE];
                o->oAction=0;
            }
             break;
        case 4: //Shatter, victory
            o->parentObj->oAction = 5;
            obj_mark_for_deletion(o);
            break;
    }
}