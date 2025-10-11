#include "src/game/envfx_snow.h"

const GeoLayout onoffhollow_geo[] = {
	GEO_CULLING_RADIUS(500),
	GEO_OPEN_NODE(),
		GEO_DISPLAY_LIST(LAYER_OPAQUE, onoffhollow_onoffhollow_mesh_layer_1),
	GEO_CLOSE_NODE(),
	GEO_END(),
};
