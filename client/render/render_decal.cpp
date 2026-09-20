// One offscreen surface-decal path for world and brush.
// Provenance: Xash engine/ref/gl/gl_decals.c DrawSingleDecal / DrawSurfaceDecals
// (precomputed polys, TF_PREMULTIPLIED blend). No second decal manager.
// No DrawSingleDecal() as a hidden renderer. No live R_DecalSetupVerts.
#include "render_decal.h"
#include "render_backend.h"
#include "render_xash_brush.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "render_api.h"

#include <string.h>
#include <stdint.h>

#define GL_POLYGON 0x0009
#define GL_TEXTURE_2D 0x0DE1
#define GL_BLEND 0x0BE2
#define GL_ALPHA_TEST 0x0BC0
#define GL_CULL_FACE 0x0B44
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS 0x2A00
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_BLEND_SRC 0x0BE1
#define GL_BLEND_DST 0x0BE0
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_CURRENT_COLOR 0x0B00
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_FRONT 0x0404
#define XR_PARM_TEX_FLAGS 10
#define XR_TF_PREMULTIPLIED (1 << 23)
#define XR_VERTEXSIZE 7

typedef struct DecalGLSnap_s
{
	unsigned char blend;
	unsigned char alpha;
	unsigned char cull;
	unsigned char poly_offset;
	unsigned char depth_mask;
	unsigned char color_mask[4];
	unsigned char tex2d0;
	unsigned char tex2d1;
	int blend_src;
	int blend_dst;
	int texenv;
	int tex0;
	int cull_mode;
	float color[4];
	float poly_factor;
	float poly_units;
} DecalGLSnap;

static CSRETRO_DecalStats s_stats;
static int s_spawn_logged = 0;
static int s_map_serial = 0;

static unsigned int MixU32( unsigned int h, unsigned int v )
{
	h ^= v;
	h *= 16777619u;
	return h;
}

static unsigned int MixFloat( unsigned int h, float f )
{
	unsigned int bits;
	memcpy( &bits, &f, sizeof( bits ) );
	return MixU32( h, bits );
}

static long TexFlags( int texnum )
{
	if( gRenderAPI.RenderGetParm )
		return (long)gRenderAPI.RenderGetParm( XR_PARM_TEX_FLAGS, texnum );
	return 0;
}

static void SurfaceRange( const xr_model_t *mod, int *first, int *count )
{
	*first = mod->firstmodelsurface;
	*count = mod->nummodelsurfaces;
	if( *count <= 0 || *first < 0 || *first + *count > mod->numsurfaces )
	{
		*first = 0;
		*count = mod->numsurfaces;
	}
}

static unsigned int HashOneDecal( unsigned int h, const xr_decal_t *d )
{
	const xr_glpoly_t *poly;
	int i, n;

	h = MixFloat( h, d->dx );
	h = MixFloat( h, d->dy );
	h = MixFloat( h, d->scale );
	h = MixU32( h, (unsigned int)(unsigned short)d->texture );
	h = MixU32( h, (unsigned int)(unsigned short)d->flags );
	h = MixU32( h, (unsigned int)(unsigned short)d->entityIndex );
	h = MixFloat( h, d->position[0] );
	h = MixFloat( h, d->position[1] );
	h = MixFloat( h, d->position[2] );
	poly = d->polys;
	n = poly ? poly->numverts : 0;
	h = MixU32( h, (unsigned int)n );
	if( poly && poly->numverts > 0 )
	{
		for( i = 0; i < poly->numverts; i++ )
		{
			h = MixFloat( h, poly->verts[i][0] );
			h = MixFloat( h, poly->verts[i][1] );
			h = MixFloat( h, poly->verts[i][2] );
			h = MixFloat( h, poly->verts[i][3] );
			h = MixFloat( h, poly->verts[i][4] );
			h = MixFloat( h, poly->verts[i][5] );
			h = MixFloat( h, poly->verts[i][6] );
		}
	}
	return h;
}

unsigned int CSRETRO_Decal_HashSurfaces( void *modp )
{
	const xr_model_t *mod = (const xr_model_t *)modp;
	unsigned int h = 2166136261u;
	int first, count, i, order = 0;

	if( !mod || !mod->surfaces )
		return h;
	SurfaceRange( mod, &first, &count );
	h = MixU32( h, (unsigned int)count );
	for( i = 0; i < count; i++ )
	{
		const xr_msurface_t *surf = &mod->surfaces[first + i];
		const xr_decal_t *d;
		for( d = (const xr_decal_t *)surf->pdecals; d; d = d->pnext )
		{
			if( d->psurface != surf )
				continue;
			h = MixU32( h, (unsigned int)++order );
			h = HashOneDecal( h, d );
		}
	}
	return h;
}

static void SnapGL( DecalGLSnap *s )
{
	memset( s, 0, sizeof( *s ) );
	s->depth_mask = 1;
	s->color_mask[0] = s->color_mask[1] = s->color_mask[2] = s->color_mask[3] = 1;
	s->blend_src = (int)GL_SRC_ALPHA;
	s->blend_dst = (int)GL_ONE_MINUS_SRC_ALPHA;
	s->texenv = (int)GL_MODULATE;
	s->cull_mode = (int)GL_FRONT;
	s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1.0f;
	if( gXRGL.IsEnabled )
	{
		s->blend = gXRGL.IsEnabled( GL_BLEND );
		s->alpha = gXRGL.IsEnabled( GL_ALPHA_TEST );
		s->cull = gXRGL.IsEnabled( GL_CULL_FACE );
		s->poly_offset = gXRGL.IsEnabled( GL_POLYGON_OFFSET_FILL );
		s->tex2d0 = gXRGL.IsEnabled( GL_TEXTURE_2D );
	}
	if( gXRGL.GetBooleanv )
	{
		gXRGL.GetBooleanv( GL_DEPTH_WRITEMASK, &s->depth_mask );
		gXRGL.GetBooleanv( GL_COLOR_WRITEMASK, s->color_mask );
	}
	if( gXRGL.GetIntegerv )
	{
		gXRGL.GetIntegerv( GL_BLEND_SRC, &s->blend_src );
		gXRGL.GetIntegerv( GL_BLEND_DST, &s->blend_dst );
		gXRGL.GetIntegerv( GL_TEXTURE_ENV_MODE, &s->texenv );
		gXRGL.GetIntegerv( GL_TEXTURE_BINDING_2D, &s->tex0 );
		gXRGL.GetIntegerv( 0x0B45, &s->cull_mode ); // GL_CULL_FACE_MODE
	}
	if( gXRGL.GetFloatv )
	{
		gXRGL.GetFloatv( GL_CURRENT_COLOR, s->color );
		gXRGL.GetFloatv( GL_POLYGON_OFFSET_FACTOR, &s->poly_factor );
		gXRGL.GetFloatv( GL_POLYGON_OFFSET_UNITS, &s->poly_units );
	}
	if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE1 );
		if( gXRGL.IsEnabled )
			s->tex2d1 = gXRGL.IsEnabled( GL_TEXTURE_2D );
		gXRGL.ActiveTexture( GL_TEXTURE0 );
	}
}

static int SnapEqual( const DecalGLSnap *a, const DecalGLSnap *b )
{
	if( a->blend != b->blend || a->alpha != b->alpha || a->cull != b->cull )
		return 0;
	if( a->poly_offset != b->poly_offset || a->depth_mask != b->depth_mask )
		return 0;
	if( a->tex2d0 != b->tex2d0 )
		return 0;
	if( a->blend_src != b->blend_src || a->blend_dst != b->blend_dst )
		return 0;
	if( a->color_mask[0] != b->color_mask[0] || a->color_mask[1] != b->color_mask[1]
		|| a->color_mask[2] != b->color_mask[2] || a->color_mask[3] != b->color_mask[3] )
		return 0;
	return 1;
}

static void RestoreGL( const DecalGLSnap *s )
{
	if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE1 );
		if( gXRGL.Disable )
			gXRGL.Disable( GL_TEXTURE_2D );
		gXRGL.ActiveTexture( GL_TEXTURE0 );
	}
	if( s->tex2d0 )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_TEXTURE_2D );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_TEXTURE_2D );
	if( gXRGL.BindTexture )
		gXRGL.BindTexture( GL_TEXTURE_2D, (unsigned int)s->tex0 );
	if( gXRGL.TexEnvi )
		gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, s->texenv ? s->texenv : (int)GL_MODULATE );
	if( s->blend )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_BLEND );
	if( s->alpha )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_ALPHA_TEST );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_ALPHA_TEST );
	if( s->cull )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_CULL_FACE );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_CULL_FACE );
	if( gXRGL.CullFace && s->cull_mode )
		gXRGL.CullFace( (unsigned int)s->cull_mode );
	if( s->poly_offset )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_POLYGON_OFFSET_FILL );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_POLYGON_OFFSET_FILL );
	if( gXRGL.PolygonOffset )
		gXRGL.PolygonOffset( s->poly_factor, s->poly_units );
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( s->depth_mask );
	if( gXRGL.ColorMask )
		gXRGL.ColorMask( s->color_mask[0], s->color_mask[1], s->color_mask[2], s->color_mask[3] );
	if( gXRGL.BlendFunc )
		gXRGL.BlendFunc( (unsigned int)s->blend_src, (unsigned int)s->blend_dst );
	if( gXRGL.Color4f )
		gXRGL.Color4f( s->color[0], s->color[1], s->color[2], s->color[3] );
}

static void DrawPolyVerts( const float *v, int n )
{
	int i;
	if( n < 3 || !gXRGL.Begin )
		return;
	gXRGL.Begin( GL_POLYGON );
	for( i = 0; i < n; i++, v += XR_VERTEXSIZE )
	{
		if( gXRGL.TexCoord2f )
			gXRGL.TexCoord2f( v[3], v[4] );
		if( gXRGL.Vertex3f )
			gXRGL.Vertex3f( v[0], v[1], v[2] );
	}
	gXRGL.End();
}

static int DrawOneDecal( const xr_decal_t *live, const xr_msurface_t *surf )
{
	const xr_glpoly_t *poly;
	int numVerts = 0;
	long flags;

	if( !live || !live->texture )
		return 0;

	poly = live->polys;
	if( poly && poly->numverts >= 3 )
	{
		numVerts = poly->numverts;
		CSRETRO_Backend_BindTexture( 0, (unsigned int)(unsigned short)live->texture );
		flags = TexFlags( live->texture );
		if( flags & XR_TF_PREMULTIPLIED )
		{
			if( gXRGL.BlendFunc )
				gXRGL.BlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			s_stats.premultiplied_drawn++;
		}
		else
		{
			if( gXRGL.BlendFunc )
				gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			s_stats.standard_blend_drawn++;
		}
		DrawPolyVerts( &poly->verts[0][0], numVerts );
		return 1;
	}

	// polys==NULL: clip can write dx/dy. Never pass the live decal.
	if( gRenderAPI.R_DecalSetupVerts )
	{
		xr_decal_t local = *live;
		float *v;
		v = gRenderAPI.R_DecalSetupVerts( (struct decal_s *)&local, (struct msurface_s *)surf, local.texture, &numVerts );
		if( !v || numVerts < 3 )
			return -1;
		CSRETRO_Backend_BindTexture( 0, (unsigned int)(unsigned short)live->texture );
		flags = TexFlags( live->texture );
		if( flags & XR_TF_PREMULTIPLIED )
		{
			if( gXRGL.BlendFunc )
				gXRGL.BlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			s_stats.premultiplied_drawn++;
		}
		else
		{
			if( gXRGL.BlendFunc )
				gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			s_stats.standard_blend_drawn++;
		}
		DrawPolyVerts( v, numVerts );
		return 2; // fallback used and drawn
	}
	return 0;
}

void CSRETRO_Decal_BeginFrame( void )
{
	int serial = s_stats.map_serial;
	int spawn = s_stats.spawn_world_decals;
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_stats.map_serial = serial;
	s_stats.spawn_world_decals = spawn;
	s_stats.stored_decal_ptrs = 0;
	s_stats.gl_restore_ok = 1;
}

void CSRETRO_Decal_OnNewMap( void )
{
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_map_serial++;
	s_stats.map_serial = s_map_serial;
	s_stats.stored_decal_ptrs = 0;
	s_spawn_logged = 0;
}

void CSRETRO_Decal_DrawSurfaces( void *modp, int rendermode, int is_brush, int entity_index )
{
	const xr_model_t *mod = (const xr_model_t *)modp;
	int first, count, i;
	int surfaces = 0, decals = 0, drawn = 0, polys = 0, fallback = 0;
	unsigned int hash_before, hash_after;
	DecalGLSnap before, after;
	float po_val;
	int trans_mode;

	if( !mod || !mod->surfaces || !gXRGL.Begin )
		return;

	SurfaceRange( mod, &first, &count );
	hash_before = CSRETRO_Decal_HashSurfaces( modp );

	SnapGL( &before );

	trans_mode = ( rendermode == kRenderTransTexture || rendermode == kRenderTransAdd );
	if( rendermode == kRenderNormal || rendermode == kRenderTransAlpha )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		if( rendermode == kRenderTransAlpha && gXRGL.Disable )
			gXRGL.Disable( GL_ALPHA_TEST );
	}
	if( trans_mode && gXRGL.Disable )
		gXRGL.Disable( GL_CULL_FACE );

	po_val = CVAR_GET_FLOAT( "gl_polyoffset" );
	if( po_val != 0.0f && gXRGL.Enable && gXRGL.PolygonOffset )
	{
		gXRGL.Enable( GL_POLYGON_OFFSET_FILL );
		gXRGL.PolygonOffset( -1.0f, -po_val );
	}

	if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE1 );
		if( gXRGL.Disable )
			gXRGL.Disable( GL_TEXTURE_2D );
		gXRGL.ActiveTexture( GL_TEXTURE0 );
	}
	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );
	if( gXRGL.TexEnvi )
		gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	if( gXRGL.BlendFunc )
		gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	for( i = 0; i < count; i++ )
	{
		const xr_msurface_t *surf = &mod->surfaces[first + i];
		const xr_decal_t *d;
		int surf_has = 0;

		if( !surf->pdecals )
			continue;

		if( surf->flags & XR_SURF_TRANSPARENT )
		{
			s_stats.transparent_surfaces++;
			for( d = (const xr_decal_t *)surf->pdecals; d; d = d->pnext )
			{
				if( d->texture )
					s_stats.transparent_decals++;
			}
			s_stats.transparent_skipped++;
			continue;
		}

		for( d = (const xr_decal_t *)surf->pdecals; d; d = d->pnext )
		{
			int r;
			if( d->psurface != surf )
			{
				s_stats.stale_skipped++;
				continue;
			}
			if( !d->texture )
				continue;
			decals++;
			if( d->polys )
				polys++;
			r = DrawOneDecal( d, surf );
			if( r == 1 )
			{
				drawn++;
				surf_has = 1;
			}
			else if( r == 2 )
			{
				drawn++;
				fallback++;
				surf_has = 1;
			}
			else if( r == -1 )
				fallback++;
		}
		if( surf_has )
			surfaces++;
	}

	RestoreGL( &before );
	SnapGL( &after );
	if( !SnapEqual( &before, &after ) )
		s_stats.gl_restore_ok = 0;

	hash_after = CSRETRO_Decal_HashSurfaces( modp );
	s_stats.hash_before = hash_before;
	s_stats.hash_after = hash_after;
	if( hash_after != hash_before )
		s_stats.live_mutate = 1;

	if( is_brush )
	{
		s_stats.brush_decal_surfaces += surfaces;
		s_stats.brush_decals += decals;
		s_stats.brush_decals_drawn += drawn;
		s_stats.brush_decals_polys += polys;
		s_stats.brush_fallback += fallback;
		if( drawn > 0 )
		{
			s_stats.brush_entity_index = entity_index;
			if( mod->name[0] )
				strncpy( s_stats.brush_model, mod->name, sizeof( s_stats.brush_model ) - 1 );
		}
	}
	else
	{
		s_stats.world_decal_surfaces += surfaces;
		s_stats.world_decals += decals;
		s_stats.world_decals_drawn += drawn;
		s_stats.world_decals_polys += polys;
		s_stats.world_fallback += fallback;
		if( !s_spawn_logged )
		{
			s_stats.spawn_world_decals = decals;
			s_spawn_logged = 1;
		}
	}
}

void CSRETRO_Decal_GetStats( CSRETRO_DecalStats *out )
{
	if( out )
		*out = s_stats;
}
