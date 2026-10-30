
/**
 * This file contains the initialization and behavior for red coins.
 * Behavior controls audio and the orange number spawned, as well as interacting with
 * the course's red coin star.
 */

/**
 * Red coin's hitbox details.
 */
static struct ObjectHitbox sRedCoinHitbox = {
    /* interactType:      */ INTERACT_COIN,
    /* downOffset:        */ 0,
    /* damageOrCoinValue: */ 2,
    /* health:            */ 0,
    /* numLootCoins:      */ 0,
    /* radius:            */ 100,
    /* height:            */ 64,
    /* hurtboxRadius:     */ 0,
    /* hurtboxHeight:     */ 0,
};

/**
 * Red coin initialization function. Sets the coin's hitbox and parent object.
 */
void bhv_red_coin_init(void) {
    coin_obj_create(1);
    obj_set_hitbox(o, &sRedCoinHitbox);
}

/**
 * Main behavior for red coins. Primarily controls coin collection noise and spawning
 * the orange number counter.
 */
void bhv_red_coin_loop(void) {
    s8 redCoinSlot = o->primaryRegionId;
    if (o->primaryRegionId == -1) {
        redCoinSlot = 32;
    }
    // If Mario interacted with the object...
    if (o->oInteractStatus & INT_STATUS_INTERACTED) {
        // ...increment the star's counter.
        gRegionRedCoins[redCoinSlot]++;

        // Spawn the orange number counter, as long as it isn't the last coin.
        if (gRegionRedCoins[redCoinSlot] != 8) {
            // Cap visible count to 99
            if (gRegionRedCoins[redCoinSlot] > 99) {
                spawn_orange_number(9, 28, 0, 0);
                spawn_orange_number(9, -28, 0, 0);
            }
            else if (gRegionRedCoins[redCoinSlot] >= 10) {
                spawn_orange_number(gRegionRedCoins[redCoinSlot] % 10, 28, 0, 0);
                spawn_orange_number(gRegionRedCoins[redCoinSlot] / 10, -28, 0, 0);
            }
            else {
                spawn_orange_number(gRegionRedCoins[redCoinSlot], 0, 0, 0);
            }
        }

#ifdef JP_RED_COIN_SOUND
        // For JP version, play an identical sound for all coins.
        create_sound_spawner(SOUND_GENERAL_RED_COIN);
#else
        if (8 - gRegionRedCoins[redCoinSlot] > 7) {
            // Play the first red coin sound until it gets to the final 8
            play_sound(SOUND_MENU_COLLECT_RED_COIN, gGlobalSoundSource);
        }
        else {
            // On all versions but the JP version, each coin collected plays a higher noise.
            play_sound(SOUND_MENU_COLLECT_RED_COIN
                    + (((u8) 7 - (8 - gRegionRedCoins[redCoinSlot])) << 16),
                    gGlobalSoundSource);
        }
#endif

        coin_collected();
        // Despawn the coin.
        o->oInteractStatus = INT_STATUS_NONE;
    }
}