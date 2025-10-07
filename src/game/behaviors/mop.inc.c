/* Star Switch from SM64 DS*/
void bhv_star_switch(void) {

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