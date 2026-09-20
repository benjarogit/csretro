// Isolated 64-bit view of Xash sprite headers (engine/common/com_model.h).
// GoldSrc client com_model.h does not match the runtime layout.
// Provenance: SNMetamorph/PrimeXT pin 46fb05b client/render/gl_sprite.cpp
// plus Xash msprite_t / mspriteframe_t. Targeted adapt, not a blanket copy.
#pragma once

#include <stddef.h>

enum
{
	XR_SPR_SINGLE = 0,
	XR_SPR_GROUP = 1,
	XR_SPR_ANGLED = 2
};

enum
{
	XR_SPR_FWD_PARALLEL_UPRIGHT = 0,
	XR_SPR_FACING_UPRIGHT = 1,
	XR_SPR_FWD_PARALLEL = 2,
	XR_SPR_ORIENTED = 3,
	XR_SPR_FWD_PARALLEL_ORIENTED = 4
};

enum
{
	XR_SPR_CULL_FRONT = 0,
	XR_SPR_CULL_NONE = 1
};

enum
{
	XR_SPR_NORMAL = 0,
	XR_SPR_ADDITIVE = 1,
	XR_SPR_INDEXALPHA = 2,
	XR_SPR_ALPHTEST = 3
};

enum
{
	XR_MOD_BRUSH = 0,
	XR_MOD_SPRITE = 1,
	XR_MOD_ALIAS = 2,
	XR_MOD_STUDIO = 3
};

typedef struct xr_mspriteframe_s
{
	int width;
	int height;
	float up, down, left, right;
	int gl_texturenum;
} xr_mspriteframe_t;

typedef struct xr_mspritegroup_s
{
	int numframes;
	int _pad;
	float *intervals;
	xr_mspriteframe_t *frames[1];
} xr_mspritegroup_t;

typedef struct xr_mspriteframedesc_s
{
	int type;
	int _pad;
	void *frameptr;
} xr_mspriteframedesc_t;

typedef struct xr_msprite_s
{
	short type;
	short texFormat;
	int maxwidth;
	int maxheight;
	int numframes;
	int radius;
	int facecull;
	int synctype;
	xr_mspriteframedesc_t frames[1];
} xr_msprite_t;

static_assert( sizeof( xr_mspriteframe_t ) == 28, "mspriteframe_t" );
static_assert( offsetof( xr_mspriteframedesc_t, frameptr ) == 8, "framedesc.frameptr" );
static_assert( offsetof( xr_msprite_t, frames ) == 32, "msprite_t.frames" );
