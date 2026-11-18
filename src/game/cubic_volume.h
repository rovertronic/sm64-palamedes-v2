#ifndef CUBIC_VOLUME_H
#define CUBIC_VOLUME_H

#include "sm64.h"
#include "types.h"

#define CUBIC_VOLUME_MAX 30

typedef struct {
    u8 type;
    s16 param;
    Vec3f pos;
    Vec3f scale;
    Quat rot;
} CubicVolume;

enum {
    VOLUME_TYPE_REGION,
    VOLUME_TYPE_WARP,
    VOLUME_TYPE_SHADOW,
    VOLUME_TYPE_MUSIC,
    VOLUME_TYPE_BRIDGE,
    VOLUME_TYPE_DMA_MODEL,
};

CubicVolume * cubic_volume_add(void);
CubicVolume * cubic_volume_check_one(Vec3f pos, int cubicVolumeType);
void cubic_volume_check_all(Vec3f pos, int cubicVolumeType, void (* iterateVolume)(CubicVolume *));

void cubic_volume_reset(void);

#endif