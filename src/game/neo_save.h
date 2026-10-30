#include "sm64.h"

#ifndef NEO_SAVE_H
#define NEO_SAVE_H

#define SAVE_MAGIC 0x020EDD10
#define SAVE_FILE_COUNT 1

struct StarMission {
    u16 nameText;
    u16 hintText;
    u8 groupId;
    u8 starType;
};

enum starMissionIds {
    STAR_MISSION_TEST,
    STAR_MISSION_COUNT
};

enum starGroupIds {
    STAR_GROUP_TEST,
    STAR_GROUP_COUNT
};

enum starTypeIds {
    STAR_TYPE_NORMAL,
};

enum saveFlags {
    SAVE_FLAG_EXISTS
};

struct NeoSaveFile { 
    u32 stars[((STAR_MISSION_COUNT-1)/32)+1];
    u32 flags;
    u32 funValue;
};

struct NeoSaveData {
    struct NeoSaveFile file[SAVE_FILE_COUNT];
    u32 saveMagic;
};

void neo_save_collectible_obtain(u32 * flaglist, int id);
u32 neo_save_collectible_collected(u32 * flaglist, int id);
u32 neo_save_collectible_count(u32 * flaglist, int total);

extern struct StarMission gStarMissionList[];
extern struct NeoSaveFile * gCurrNeoSaveFile;

#endif