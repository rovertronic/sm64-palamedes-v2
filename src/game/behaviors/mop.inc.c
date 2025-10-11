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
            }
            break;
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