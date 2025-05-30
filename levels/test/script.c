#include <ultra64.h>
#include "sm64.h"
#include "behavior_data.h"
#include "model_ids.h"
#include "seq_ids.h"
#include "dialog_ids.h"
#include "segment_symbols.h"
#include "level_commands.h"

#include "game/level_update.h"

#include "levels/scripts.h"

#include "make_const_nonconst.h"
#include "levels/test/header.h"

/* Fast64 begin persistent block [scripts] */
/* Fast64 end persistent block [scripts] */

const LevelScript level_test_entry[] = {
	INIT_LEVEL(),
	LOAD_YAY0(0x7, _test_segment_7SegmentRomStart, _test_segment_7SegmentRomEnd), 
	LOAD_YAY0(0xa, _water_skybox_yay0SegmentRomStart, _water_skybox_yay0SegmentRomEnd), 
	ALLOC_LEVEL_POOL(),
	MARIO(MODEL_MARIO, 0x00000001, bhvMario), 
	/* Fast64 begin persistent block [level commands] */
	/* Fast64 end persistent block [level commands] */

	AREA(1, test_area_1),
		BRIDGE_START(7, testbridge_collision, 886.2921752929688, 466.65478515625, 384.58197021484375),
		BRIDGE_END(5886.29248046875, 1271.6546630859375, 384.58209228515625),
		BRIDGE_START(9, testbridge2_collision, 4386.29248046875, 246.65663146972656, -7815.4189453125),
		BRIDGE_END(7986.2900390625, 246.65663146972656, -7815.4189453125),
		MARIO_POS(0x01, 0, -314, 471, 280),
		CUBIC_VOLUME(6393.318359375, 49.11333465576172, -3944.277099609375,
			7.695290565490723, 0.8631196022033691, 0.7068579792976379,
			0.7820308804512024, 0.5485619306564331, -0.07849472761154175, -0.2852123975753784),
		OBJECT(MODEL_STAR, -1689, -1884, -1439, 0, 0, 0, 0x00000000, bhvStar),
		TERRAIN(test_area_1_collision),
		MACRO_OBJECTS(test_area_1_macro_objs),
		STOP_MUSIC(0),
		TERRAIN_TYPE(TERRAIN_GRASS),
		/* Fast64 begin persistent block [area commands] */
		/* Fast64 end persistent block [area commands] */
	END_AREA(),
	FREE_LEVEL_POOL(),
	MARIO_POS(0x01, 0, -314, 471, 280),
	CALL(0, lvl_init_or_update),
	CALL_LOOP(1, lvl_init_or_update),
	CLEAR_LEVEL(),
	SLEEP_BEFORE_EXIT(1),
	EXIT(),
};