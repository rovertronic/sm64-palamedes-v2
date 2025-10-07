#include <ultra64.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "geo_commands.h"

#include "make_const_nonconst.h"

// Note: This bin does not use bin IDs, unlike the other segmented bins.
#include "mario/model.inc.c"

#include "bubble/model.inc.c"

#include "walk_smoke/model.inc.c"

#include "burn_smoke/model.inc.c"

#include "small_water_splash/model.inc.c"

#include "water_wave/model.inc.c"

#include "sparkle/model.inc.c"

#include "water_splash/model.inc.c"

#include "white_particle_small/model.inc.c"

#include "sparkle_animation/model.inc.c"

#ifdef S2DEX_TEXT_ENGINE
#include "src/s2d_engine/s2d_config.h"
#include FONT_C_FILE
#endif

#include "testcube/model.inc.c"
#include "shadow_head/model.inc.c"
#include "shadow_foot/model.inc.c"
#include "shadow_hand/model.inc.c"
#include "shadow_foot_2/model.inc.c"
#include "shadow_limb/model.inc.c"
#include "shadow_torso/model.inc.c"
#include "placeholderNPC/model.inc.c"
#include "coin3D/model.inc.c"
#include "coin3DB/model.inc.c"
#include "coin3Dhi/model.inc.c"
#include "noteblock/collision.inc.c"
#include "noteblock/model.inc.c"