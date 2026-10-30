#include "sm64.h"
#include "neo_save.h"
#include "src/data/text_enums.h"

u8 sFileNum = 0;

struct NeoSaveData sSaveData;
struct NeoSaveFile * gCurrNeoSaveFile = &sSaveData.file[0];

void neo_save_collectible_obtain(u32 * flaglist, int id) {
    int block = id/32;
    int index = id%32;
    u32 * flagblock = flaglist+block;
    (*flagblock) |= (1 << index);
}

u32 neo_save_collectible_collected(u32 * flaglist, int id) {
    int block = id/32;
    int index = id%32;
    u32 * flagblock = flaglist+block;
    u32 flagblockcopy = (*flagblock);
    return (flagblockcopy & (1 << index));
}

u32 neo_save_collectible_count(u32 * flaglist, int total) {
    int ct = 0;
    for (int i = 0; i < total; i++) {
        if (neo_save_collectible_collected(flaglist,i)) {
            ct++;
        }
    }
    return ct;
}

struct StarMission gStarMissionList[] = {
    [STAR_MISSION_TEST] = {
        .nameText = TEXT_MISSION_TEST,
        .hintText = TEXT_MISSION_TEST,
        .groupId = STAR_GROUP_TEST,
        .starType = STAR_TYPE_NORMAL
    },
};