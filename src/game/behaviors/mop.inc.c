/* Star Switch from SM64 DS*/
EventData eventStarSwitch[] = {
    E_CINEMA_TOGGLE(),
    E_DIALOG(0),
    E_END(),
};

void bhv_star_switch(void) {
    struct Object * myStarGlass = cur_obj_nearest_object_with_behavior(bhvStarSwitchStar);
    if (myStarGlass == NULL) {return;}

    switch(o->oAction) {
        case 0:
            if (gMarioObject->platform == o) {
                o->oAction++;
                event_start(eventStarSwitch);
            }
            break;
        case 1:
            if (o->oPosY > o->oHomeY - 20.0f) {
                o->oPosY-=2.0f;
            }
            break;
    }
}

void bhv_star_switch_star(void) {
    switch(o->oAction) {
        case 0:
            spawn_object(o,MODEL_STAR,bhvStar);
            o->header.gfx.sharedChild = gLoadedGraphNodes[MODEL_STAR_GLASS];
            o->oAction++;
            break;
    }
}