// Isolated 64-bit view of Xash engine brush models.
// GoldSrc client/body/common/com_model.h does NOT match the runtime layout.
// Offsets CONFIRMED against engine/common/com_model.h via offsetof on amd64.
#pragma once

#include <stdint.h>
#include <stddef.h>

#define XR_VERTEXSIZE 7
#define XR_MAXLIGHTMAPS 4
#define XR_MAX_MAP_HULLS 4
#define XR_MODEL_LIQUID (1 << 2)
#define XR_MODEL_WORLD (1 << 29)
#define XR_SURF_DRAWSKY (1 << 2)
#define XR_SURF_DRAWTURB_QUADS (1 << 3)
#define XR_SURF_DRAWTURB (1 << 4)
#define XR_SURF_DRAWTILED (1 << 5)
#define XR_SURF_CONVEYOR (1 << 6)
#define XR_SURF_TRANSPARENT (1 << 8)
#define XR_PLANE_Z 2
#define XR_EF_WATERSIDES (1 << 26)

typedef struct xr_mplane_s
{
	float normal[3];
	float dist;
	unsigned char type;
	unsigned char signbits;
	unsigned char pad[2];
} xr_mplane_t;

typedef struct xr_texture_s
{
	char name[16];
	unsigned int width, height;
	int gl_texturenum;
	void *texturechain;
	int anim_total, anim_min, anim_max;
	struct xr_texture_s *anim_next;
	struct xr_texture_s *alternate_anims;
	unsigned short fb_texturenum;
	unsigned short dt_texturenum;
	unsigned int unused[3];
} xr_texture_t;

typedef struct xr_texinfo_s
{
	float vecs[2][4];
	void *faceinfo;
	xr_texture_t *texture;
	int flags;
	int _pad;
} xr_texinfo_t;

typedef struct xr_glpoly_s
{
	struct xr_glpoly_s *next;
	struct xr_glpoly_s *chain;
	int numverts;
	int flags;
	float verts[1][XR_VERTEXSIZE];
} xr_glpoly_t;

typedef struct xr_msurface_s
{
	int visframe;
	int _pad0;
	void *plane;
	int flags;
	int firstedge;
	int numedges;
	short texturemins[2];
	short extents[2];
	int light_s, light_t;
	int _pad1;
	xr_glpoly_t *polys;
	struct xr_msurface_s *texturechain;
	xr_texinfo_t *texinfo;
	int dlightframe;
	int dlightbits;
	int lightmaptexturenum;
	unsigned char styles[XR_MAXLIGHTMAPS];
	int cached_light[XR_MAXLIGHTMAPS];
	void *info;
	void *samples;
	struct xr_decal_s *pdecals;
} xr_msurface_t;

// Xash engine/common/com_model.h decal_t on amd64. Size 88 CONFIRMED
// (STATIC_CHECK_SIZEOF(decal_t, 60, 88)). Offsets checked against that layout.
typedef struct xr_decal_s
{
	struct xr_decal_s *pnext;
	xr_msurface_t *psurface;
	float dx;
	float dy;
	float scale;
	short texture;
	short flags;
	short entityIndex;
	short _pad_ei;
	float position[3];
	xr_glpoly_t *polys;
	intptr_t reserved[4];
} xr_decal_t;

typedef struct xr_vertex_s
{
	float position[3];
} xr_vertex_t;

typedef struct xr_edge16_s
{
	unsigned short v[2];
	unsigned int cachededgeoffset;
} xr_edge16_t;

typedef struct xr_model_s
{
	char name[64];
	int needload;
	int type;
	int numframes;
	unsigned int mempool;
	int flags;
	float mins[3], maxs[3];
	float radius;
	int firstmodelsurface;
	int nummodelsurfaces;
	int numsubmodels;
	int _pad_sub;
	void *submodels;
	int numplanes;
	int _pad_pl;
	void *planes;
	int numleafs;
	int _pad_lf;
	void *leafs;
	int numvertexes;
	int _pad_vt;
	xr_vertex_t *vertexes;
	int numedges;
	int _pad_ed;
	xr_edge16_t *edges16;
	int numnodes;
	int _pad_nd;
	void *nodes;
	int numtexinfo;
	int _pad_ti;
	void *texinfo;
	int numsurfaces;
	int _pad_sf;
	xr_msurface_t *surfaces;
	int numsurfedges;
	int _pad_se;
	int *surfedges;
	unsigned char _hull_block[224];
	int numtextures;
	int _pad_tx;
	xr_texture_t **textures;
	void *visdata;
	void *lightdata;
	char *entities;
	void *cache;
} xr_model_t;

static_assert( sizeof( xr_msurface_t ) == 128, "xr_msurface_t must match Xash 64-bit msurface_t" );
static_assert( sizeof( xr_texture_t ) == 88, "xr_texture_t must match Xash 64-bit texture_t" );
static_assert( sizeof( xr_texinfo_t ) == 56, "xr_texinfo_t must match Xash 64-bit mtexinfo_t" );
static_assert( offsetof( xr_model_t, numsurfaces ) == 232, "numsurfaces offset" );
static_assert( offsetof( xr_model_t, surfaces ) == 240, "surfaces offset" );
static_assert( offsetof( xr_model_t, numtextures ) == 488, "numtextures offset" );
static_assert( offsetof( xr_msurface_t, polys ) == 48, "polys offset" );
static_assert( offsetof( xr_msurface_t, lightmaptexturenum ) == 80, "lightmaptexturenum offset" );
static_assert( offsetof( xr_msurface_t, pdecals ) == 120, "pdecals offset" );
static_assert( offsetof( xr_texture_t, gl_texturenum ) == 24, "gl_texturenum offset" );
static_assert( sizeof( xr_mplane_t ) == 20, "xr_mplane_t must match Xash 64-bit mplane_t" );
static_assert( sizeof( xr_decal_t ) == 88, "xr_decal_t must match Xash 64-bit decal_t" );
static_assert( offsetof( xr_decal_t, pnext ) == 0, "decal pnext" );
static_assert( offsetof( xr_decal_t, psurface ) == 8, "decal psurface" );
static_assert( offsetof( xr_decal_t, dx ) == 16, "decal dx" );
static_assert( offsetof( xr_decal_t, dy ) == 20, "decal dy" );
static_assert( offsetof( xr_decal_t, scale ) == 24, "decal scale" );
static_assert( offsetof( xr_decal_t, texture ) == 28, "decal texture" );
static_assert( offsetof( xr_decal_t, flags ) == 30, "decal flags" );
static_assert( offsetof( xr_decal_t, entityIndex ) == 32, "decal entityIndex" );
static_assert( offsetof( xr_decal_t, position ) == 36, "decal position" );
static_assert( offsetof( xr_decal_t, polys ) == 48, "decal polys" );
