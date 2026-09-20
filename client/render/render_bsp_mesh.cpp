// Shared BSP triangulator. Provenance: CS Retro world capture (PX3B) plus
// Xash surface walk (firstmodelsurface/nummodelsurfaces). Texture animation,
// conveyor UV, fullbright overlay and SURF_DRAWTURB water follow Xash
// gl_rsurf.c draw semantics (R_TextureAnimation / DrawGLPoly /
// R_RenderFullbrights / EmitWaterPolys). PrimeXT pin 46fb05b is
// semantics-only. No second BSP parser. No decals, no dlights, no second
// ripple sim. Mesh is cached; animation/scroll/warp/wave resolve at draw.
#include "render_bsp_mesh.h"
#include "render_backend.h"
#include "render_xash_brush.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define XR_MOD_FRAMES 20
#define XR_PARM_TEX_FLAGS 10
#define XR_PARM_WATER_ALPHA 37
#define XR_PARM_WATER_ALPHA_VALUE 41
#define XR_PARM_MAP_HAS_LITWATER 42
#define XR_TF_QUAKEPAL (1 << 8)
#define XR_SUBDIVIDE_SIZE 64.0f
#define XR_TURBSCALE ( (float)( 256.0 / ( M_PI * 2.0 ) ) )

// Xash gl_rsurf.c TURBSCALE / r_turbsin. Table from engine/engine/warpsin.h
// (Id Software 1997-2001). Not exported as r_turbsin.
static const float s_turbsin[] = {
#include "../../engine/engine/warpsin.h"
};

#define GL_TRIANGLES 0x0004
#define GL_TEXTURE_2D 0x0DE1
#define GL_BLEND 0x0BE2
#define GL_ALPHA_TEST 0x0BC0
#define GL_FOG 0x0B60
#define GL_ONE 1
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_REPLACE 0x1E01
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_CULL_FACE 0x0B44

static int s_build_count = 0;
static CSRETRO_MeshDrawStats s_draw;
static int s_proof_inited = 0;
#define XR_ANIM_TRACK 32

static unsigned int s_anim_tex_first = 0;
static unsigned int s_anim_tex_last = 0;
static int s_anim_tex_changed = 0;
static void *s_anim_src[XR_ANIM_TRACK];
static unsigned int s_anim_tex0[XR_ANIM_TRACK];
static int s_anim_track_n = 0;

static void AnimTrackReset( void )
{
	s_anim_tex_first = 0;
	s_anim_tex_last = 0;
	s_anim_tex_changed = 0;
	s_anim_track_n = 0;
	memset( s_anim_src, 0, sizeof( s_anim_src ) );
	memset( s_anim_tex0, 0, sizeof( s_anim_tex0 ) );
}

static void NoteAnim( const xr_texture_t *base, const xr_texture_t *resolved )
{
	unsigned int tex;
	int i;
	if( !base || !resolved || base->anim_total <= 0 || base->name[0] == '-' )
		return;
	tex = (unsigned int)resolved->gl_texturenum;
	for( i = 0; i < s_anim_track_n; i++ )
	{
		if( s_anim_src[i] != (void *)base )
			continue;
		s_anim_tex_last = tex;
		if( s_anim_tex0[i] && tex && tex != s_anim_tex0[i] )
			s_anim_tex_changed = 1;
		return;
	}
	if( s_anim_track_n >= XR_ANIM_TRACK )
		return;
	s_anim_src[s_anim_track_n] = (void *)base;
	s_anim_tex0[s_anim_track_n] = tex;
	s_anim_track_n++;
	if( !s_anim_tex_first )
		s_anim_tex_first = tex;
	s_anim_tex_last = tex;
}
static float s_conv_s_first = 0.0f;
static float s_conv_t_first = 0.0f;
static int s_conv_have_first = 0;
static int s_conv_uv_changed = 0;
static int s_build_serial_first = 0;
static int s_builds_prev = -1;
static int s_rebuilds_stable = 0;

static unsigned int MixU32( unsigned int h, unsigned int v )
{
	h ^= v;
	h *= 16777619u;
	return h;
}

static unsigned int MixF32( unsigned int h, float f )
{
	unsigned int bits;
	memcpy( &bits, &f, sizeof( bits ) );
	return MixU32( h, bits );
}

static unsigned int HashBatches( unsigned int h, const CSRETRO_MeshBatch *batches, int count )
{
	int i;
	h = MixU32( h, (unsigned int)count );
	for( i = 0; i < count; i++ )
	{
		const CSRETRO_MeshBatch *b = &batches[i];
		h = MixU32( h, b->tex );
		h = MixU32( h, b->lightmap );
		h = MixU32( h, (unsigned int)b->first_tri );
		h = MixU32( h, (unsigned int)b->tri_count );
		h = MixU32( h, (unsigned int)b->flags );
		h = MixU32( h, (unsigned int)(uintptr_t)b->anim_source );
		h = MixU32( h, (unsigned int)b->surface_index );
		h = MixU32( h, (unsigned int)b->plane_type );
		h = MixF32( h, b->plane_dist );
	}
	return h;
}

static unsigned int HashVertArray( unsigned int h, const CSRETRO_MeshVert *verts, int count )
{
	int i;
	h = MixU32( h, (unsigned int)count );
	if( !verts )
		return h;
	for( i = 0; i < count; i++ )
	{
		const CSRETRO_MeshVert *v = &verts[i];
		h = MixF32( h, v->xyz[0] );
		h = MixF32( h, v->xyz[1] );
		h = MixF32( h, v->xyz[2] );
		h = MixF32( h, v->st[0] );
		h = MixF32( h, v->st[1] );
		h = MixF32( h, v->lm[0] );
		h = MixF32( h, v->lm[1] );
		h = MixF32( h, v->poly_z0 );
		h = MixU32( h, (unsigned int)v->surface_index );
	}
	return h;
}

static unsigned int HashVerts( const CSRETRO_BspMesh *mesh )
{
	unsigned int h = 2166136261u;
	if( !mesh )
		return 0;
	h = HashVertArray( h, mesh->verts, mesh->vert_count );
	h = HashVertArray( h, mesh->water_verts, mesh->water_vert_count );
	h = HashBatches( h, mesh->batches, mesh->batch_count );
	h = HashBatches( h, mesh->water_batches, mesh->water_batch_count );
	return h;
}

float CSRETRO_DecodeWaterAlpha( long bits )
{
	uint32_t u;
	float v;

	u = (uint32_t)bits;
	if( !u )
		return 1.0f;
	memcpy( &v, &u, sizeof( v ) );
	if( v < 0.0f )
		return 0.0f;
	if( v > 1.0f )
		return 1.0f;
	return v;
}

static void AddTri( CSRETRO_MeshVert **cursor, int *used, int cap, const float *a, const float *b, const float *c, int surface_index, float poly_z0 )
{
	CSRETRO_MeshVert *v;
	if( *used + 3 > cap )
		return;
	v = *cursor;
	memcpy( v[0].xyz, a, sizeof( float ) * 3 );
	v[0].st[0] = a[3];
	v[0].st[1] = a[4];
	v[0].lm[0] = a[5];
	v[0].lm[1] = a[6];
	v[0].poly_z0 = poly_z0;
	v[0].surface_index = surface_index;
	memcpy( v[1].xyz, b, sizeof( float ) * 3 );
	v[1].st[0] = b[3];
	v[1].st[1] = b[4];
	v[1].lm[0] = b[5];
	v[1].lm[1] = b[6];
	v[1].poly_z0 = poly_z0;
	v[1].surface_index = surface_index;
	memcpy( v[2].xyz, c, sizeof( float ) * 3 );
	v[2].st[0] = c[3];
	v[2].st[1] = c[4];
	v[2].lm[0] = c[5];
	v[2].lm[1] = c[6];
	v[2].poly_z0 = poly_z0;
	v[2].surface_index = surface_index;
	*cursor += 3;
	*used += 3;
}

static int SameBatch( const CSRETRO_MeshBatch *batch, const xr_texture_t *src, unsigned int lm, int flags )
{
	if( !batch )
		return 0;
	if( batch->anim_source != (void *)src )
		return 0;
	if( batch->lightmap != lm )
		return 0;
	if( batch->flags != flags )
		return 0;
	return 1;
}

static xr_texture_t *TextureAnimation( xr_texture_t *base, const CSRETRO_MeshDrawContext *ctx )
{
	xr_texture_t *orig = base;
	int relative;
	int count = 0;
	int speed;

	if( !base )
		return NULL;

	if( ctx && ctx->entity_frame != 0.0f && base->alternate_anims )
		base = base->alternate_anims;

	if( !base || base->anim_total <= 0 )
		return base ? base : orig;

	// Random tiled ('-') uses Xash rtable filled at ref init via COM_RandomLong.
	// That table is not in the Client-API. Do not invent a second table.
	// Keep the BSP-assigned tile (Xash R_TextureAnim without surface).
	if( base->name[0] == '-' )
		return orig;

	speed = 20;
	if( ctx && ctx->get_parm && base->gl_texturenum )
	{
		long flags = ctx->get_parm( XR_PARM_TEX_FLAGS, base->gl_texturenum );
		if( flags & XR_TF_QUAKEPAL )
			speed = 10;
	}

	relative = (int)( ( ctx ? ctx->time : 0.0f ) * (float)speed ) % base->anim_total;
	if( relative < 0 )
		relative += base->anim_total;

	while( base->anim_min > relative || base->anim_max <= relative )
	{
		base = base->anim_next;
		if( !base || ++count > XR_MOD_FRAMES )
			return orig;
	}
	return base;
}

static void ConveyorOffset( const CSRETRO_MeshBatch *batch, const CSRETRO_MeshDrawContext *ctx, float *sOff, float *tOff )
{
	float speed, rate, angle, sy, cy, sOffset, tOffset;
	unsigned int width;

	*sOff = 0.0f;
	*tOff = 0.0f;
	if( !batch || !ctx || !( batch->flags & XR_SURF_CONVEYOR ) )
		return;

	speed = (float)( ( ctx->rendercolor[1] << 8 ) | ctx->rendercolor[2] ) / 16.0f;
	if( ctx->rendercolor[0] )
		speed = -speed;

	width = batch->tex_width;
	if( !width )
		width = 1;
	rate = fabsf( speed ) / (float)width;
	angle = ( speed >= 0.0f ) ? 180.0f : 0.0f;
	sy = sinf( angle * ( (float)M_PI / 180.0f ) );
	cy = cosf( angle * ( (float)M_PI / 180.0f ) );
	sOffset = ctx->time * cy * rate;
	tOffset = ctx->time * sy * rate;
	if( sOffset < 0.0f )
		sOffset += 1.0f + -(float)(int)sOffset;
	if( tOffset < 0.0f )
		tOffset += 1.0f + -(float)(int)tOffset;
	*sOff = sOffset - (float)(int)sOffset;
	*tOff = tOffset - (float)(int)tOffset;
}

static void NoteConveyor( float sOff, float tOff )
{
	s_draw.conveyor_s = sOff;
	s_draw.conveyor_t = tOff;
	if( !s_conv_have_first )
	{
		s_conv_s_first = sOff;
		s_conv_t_first = tOff;
		s_conv_have_first = 1;
	}
	else if( sOff != s_conv_s_first || tOff != s_conv_t_first )
		s_conv_uv_changed = 1;
}

static void DrawBatchTris( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshBatch *batch, float sOff, float tOff, int has_lm )
{
	int i;
	if( !gXRGL.Begin )
		return;
	gXRGL.Begin( GL_TRIANGLES );
	for( i = 0; i < batch->tri_count * 3; i++ )
	{
		const CSRETRO_MeshVert *v = &mesh->verts[batch->first_tri + i];
		if( gXRGL.TexCoord2f )
			gXRGL.TexCoord2f( v->st[0] + sOff, v->st[1] + tOff );
		if( has_lm && gXRGL.MultiTexCoord2f )
			gXRGL.MultiTexCoord2f( GL_TEXTURE1, v->lm[0], v->lm[1] );
		gXRGL.Vertex3f( v->xyz[0], v->xyz[1], v->xyz[2] );
	}
	gXRGL.End();
}

void CSRETRO_BspMesh_Clear( CSRETRO_BspMesh *mesh )
{
	if( !mesh )
		return;
	free( mesh->verts );
	free( mesh->batches );
	free( mesh->water_verts );
	free( mesh->water_batches );
	memset( mesh, 0, sizeof( *mesh ) );
}

static void InitBatch( CSRETRO_MeshBatch *batch, unsigned int tex, unsigned int lm, int first_tri, int flags, xr_texture_t *src, unsigned int width, unsigned int height, int random_tile, const xr_msurface_t *surf, int surface_index )
{
	const xr_mplane_t *pl;

	batch->tex = tex;
	batch->lightmap = lm;
	batch->first_tri = first_tri;
	batch->tri_count = 0;
	batch->flags = flags;
	batch->anim_source = src;
	batch->tex_width = width;
	batch->tex_height = height;
	batch->random_tile = random_tile;
	batch->plane_type = 0;
	batch->plane_dist = 0.0f;
	batch->surface_index = surface_index;
	batch->surface_z0 = 0.0f;
	pl = (const xr_mplane_t *)surf->plane;
	if( pl )
	{
		batch->plane_type = (int)pl->type;
		batch->plane_dist = pl->dist;
	}
	if( surf->polys && surf->polys->numverts > 0 )
		batch->surface_z0 = surf->polys->verts[0][2];
}

int CSRETRO_BspMesh_Build( CSRETRO_BspMesh *mesh, void *modp )
{
	xr_model_t *mod = (xr_model_t *)modp;
	int first, count, i, guess_verts = 0, guess_water = 0, used = 0, water_used = 0;
	int polys = 0, surfaces = 0, lm_max = -1, flags_union = 0;
	int skipped_turb = 0, skipped_sky = 0, turb_surfaces = 0, turb_polys = 0, waterside_c = 0;
	int anim_c = 0, conv_c = 0, fb_c = 0, alt_c = 0, rnd_c = 0;
	CSRETRO_MeshVert *cursor;
	CSRETRO_MeshVert *water_cursor;
	CSRETRO_MeshBatch *batch = NULL;

	if( !mesh )
		return 0;
	CSRETRO_BspMesh_Clear( mesh );

	if( !mod || !mod->surfaces || mod->numsurfaces <= 0 || mod->numsurfaces > 262144 )
		return 0;

	first = mod->firstmodelsurface;
	count = mod->nummodelsurfaces;
	if( count <= 0 || first < 0 || first + count > mod->numsurfaces )
	{
		first = 0;
		count = mod->numsurfaces;
	}

	for( i = 0; i < count; i++ )
	{
		xr_msurface_t *surf = &mod->surfaces[first + i];
		xr_glpoly_t *p;
		if( surf->flags & XR_SURF_DRAWSKY )
			continue;
		if( surf->flags & XR_SURF_DRAWTURB )
		{
			if( !surf->polys )
				continue;
			for( p = surf->polys; p; p = p->next )
			{
				if( p->numverts >= 3 )
					guess_water += ( p->numverts - 2 ) * 3;
			}
			continue;
		}
		if( !surf->polys )
		{
			if( surf->numedges >= 3 )
				guess_verts += ( surf->numedges - 2 ) * 3;
			continue;
		}
		for( p = surf->polys; p; p = p->next )
		{
			if( p->numverts >= 3 )
				guess_verts += ( p->numverts - 2 ) * 3;
		}
	}

	if( guess_verts <= 0 && guess_water <= 0 )
		return 0;

	if( guess_verts > 0 )
	{
		mesh->verts = (CSRETRO_MeshVert *)malloc( sizeof( CSRETRO_MeshVert ) * (size_t)guess_verts );
		mesh->batches = (CSRETRO_MeshBatch *)malloc( sizeof( CSRETRO_MeshBatch ) * XR_MESH_MAX_BATCH );
		if( !mesh->verts || !mesh->batches )
		{
			CSRETRO_BspMesh_Clear( mesh );
			return 0;
		}
	}
	if( guess_water > 0 )
	{
		mesh->water_verts = (CSRETRO_MeshVert *)malloc( sizeof( CSRETRO_MeshVert ) * (size_t)guess_water );
		mesh->water_batches = (CSRETRO_MeshBatch *)malloc( sizeof( CSRETRO_MeshBatch ) * XR_MESH_MAX_BATCH );
		if( !mesh->water_verts || !mesh->water_batches )
		{
			CSRETRO_BspMesh_Clear( mesh );
			return 0;
		}
	}

	cursor = mesh->verts;
	water_cursor = mesh->water_verts;

	for( i = 0; i < count; i++ )
	{
		xr_msurface_t *surf = &mod->surfaces[first + i];
		xr_glpoly_t *p;
		xr_texture_t *src = NULL;
		unsigned int tex = 0, lm = 0, width = 0, height = 0;
		int random_tile = 0;
		int surface_index = first + i;

		if( surf->flags & XR_SURF_DRAWSKY )
		{
			skipped_sky++;
			continue;
		}

		if( surf->texinfo && surf->texinfo->texture )
			src = surf->texinfo->texture;
		if( src )
		{
			tex = (unsigned int)src->gl_texturenum;
			width = src->width;
			height = src->height;
			if( src->anim_total > 0 && src->name[0] != '-' )
				anim_c++;
			if( src->alternate_anims )
				alt_c++;
			if( src->name[0] == '-' && src->anim_total > 0 )
			{
				random_tile = 1;
				rnd_c++;
			}
			// Water uses texture->fb_texturenum as the Xash ripple handle
			// (R_UploadRipples). Do not classify SURF_DRAWTURB as fullbright.
			if( src->fb_texturenum && !( surf->flags & XR_SURF_DRAWTURB ) )
				fb_c++;
		}
		if( surf->flags & XR_SURF_CONVEYOR )
			conv_c++;
		if( !( surf->flags & XR_SURF_DRAWTILED ) && surf->lightmaptexturenum >= 0 )
			lm = (unsigned int)surf->lightmaptexturenum;

		if( surf->flags & XR_SURF_DRAWTURB )
		{
			CSRETRO_MeshBatch *wbatch;
			const xr_mplane_t *pl = (const xr_mplane_t *)surf->plane;

			flags_union |= surf->flags;
			if( pl && pl->type != XR_PLANE_Z )
				waterside_c++;
			if( !surf->polys )
			{
				skipped_turb++;
				continue;
			}
			if( mesh->water_batch_count >= XR_MESH_MAX_BATCH )
				continue;
			wbatch = &mesh->water_batches[mesh->water_batch_count++];
			InitBatch( wbatch, tex, lm, water_used, surf->flags, src, width, height, random_tile, surf, surface_index );
			for( p = surf->polys; p; p = p->next )
			{
				int v;
				float poly_z0;
				if( p->numverts < 3 )
					continue;
				poly_z0 = p->verts[0][2];
				turb_polys++;
				polys++;
				for( v = 1; v + 1 < p->numverts; v++ )
				{
					AddTri( &water_cursor, &water_used, guess_water, p->verts[0], p->verts[v], p->verts[v + 1], surface_index, poly_z0 );
					wbatch->tri_count++;
				}
			}
			if( wbatch->tri_count > 0 )
			{
				turb_surfaces++;
				surfaces++;
			}
			else
				skipped_turb++;
			if( (int)lm > lm_max )
				lm_max = (int)lm;
			continue;
		}

		flags_union |= surf->flags;

		if( !SameBatch( batch, src, lm, surf->flags ) )
		{
			if( mesh->batch_count >= XR_MESH_MAX_BATCH )
				break;
			if( !mesh->batches )
				break;
			batch = &mesh->batches[mesh->batch_count++];
			InitBatch( batch, tex, lm, used, surf->flags, src, width, height, random_tile, surf, surface_index );
		}

		if( surf->polys )
		{
			for( p = surf->polys; p; p = p->next )
			{
				int v;
				if( p->numverts < 3 )
					continue;
				polys++;
				for( v = 1; v + 1 < p->numverts; v++ )
				{
					AddTri( &cursor, &used, guess_verts, p->verts[0], p->verts[v], p->verts[v + 1], surface_index, 0.0f );
					batch->tri_count++;
				}
			}
			surfaces++;
		}
		else if( surf->numedges >= 3 && mod->surfedges && mod->vertexes && mod->edges16 )
		{
			float fan[64][XR_VERTEXSIZE];
			int e, n = surf->numedges;
			if( n > 64 )
				n = 64;
			for( e = 0; e < n; e++ )
			{
				int l = mod->surfedges[surf->firstedge + e];
				int idx = l < 0 ? -l : l;
				int vert;
				if( idx < 0 || idx >= mod->numedges )
					break;
				vert = (int)mod->edges16[idx].v[l < 0 ? 1 : 0];
				if( vert < 0 || vert >= mod->numvertexes )
					break;
				fan[e][0] = mod->vertexes[vert].position[0];
				fan[e][1] = mod->vertexes[vert].position[1];
				fan[e][2] = mod->vertexes[vert].position[2];
				if( surf->texinfo )
				{
					fan[e][3] = fan[e][0] * surf->texinfo->vecs[0][0] + fan[e][1] * surf->texinfo->vecs[0][1]
						+ fan[e][2] * surf->texinfo->vecs[0][2] + surf->texinfo->vecs[0][3];
					fan[e][4] = fan[e][0] * surf->texinfo->vecs[1][0] + fan[e][1] * surf->texinfo->vecs[1][1]
						+ fan[e][2] * surf->texinfo->vecs[1][2] + surf->texinfo->vecs[1][3];
					if( src && src->width )
						fan[e][3] /= (float)src->width;
					if( src && src->height )
						fan[e][4] /= (float)src->height;
				}
				else
					fan[e][3] = fan[e][4] = 0.0f;
				fan[e][5] = fan[e][6] = 0.0f;
			}
			if( e == n )
			{
				int v;
				polys++;
				surfaces++;
				for( v = 1; v + 1 < n; v++ )
				{
					AddTri( &cursor, &used, guess_verts, fan[0], fan[v], fan[v + 1], surface_index, 0.0f );
					batch->tri_count++;
				}
			}
		}

		if( (int)lm > lm_max )
			lm_max = (int)lm;
	}

	s_build_count++;
	mesh->model = mod;
	mesh->firstmodelsurface = first;
	mesh->nummodelsurfaces = count;
	mesh->vert_count = used;
	mesh->water_vert_count = water_used;
	mesh->lightmap_pages = lm_max >= 0 ? lm_max + 1 : 0;
	mesh->flags_union = flags_union;
	mesh->surfaces = surfaces;
	mesh->polys = polys;
	mesh->skipped_turb = skipped_turb;
	mesh->skipped_sky = skipped_sky;
	mesh->turb_surfaces = turb_surfaces;
	mesh->turb_polys = turb_polys;
	mesh->turb_verts = water_used;
	mesh->waterside_candidates = waterside_c;
	mesh->liquid_model = ( mod->flags & XR_MODEL_LIQUID ) ? 1 : 0;
	mesh->captured = ( used > 0 || water_used > 0 );
	mesh->anim_candidates = anim_c;
	mesh->conveyor_candidates = conv_c;
	mesh->fullbright_candidates = fb_c;
	mesh->alternate_candidates = alt_c;
	mesh->random_tile_candidates = rnd_c;
	mesh->geom_hash = HashVerts( mesh );
	mesh->build_serial = s_build_count;
	return mesh->captured;
}

static int BindLightmap( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshBatch *batch, const CSRETRO_MeshDrawContext *ctx, int bind_textures )
{
	int has_lm;
	has_lm = bind_textures && ( batch->lightmap != 0 || ( mesh->lightmap_pages > 0 && !( batch->flags & XR_SURF_DRAWTILED ) ) );
	if( has_lm && ctx && ctx->get_parm )
	{
		int lmtex = (int)ctx->get_parm( 7, (int)batch->lightmap ); // PARM_TEX_LIGHTMAP
		if( lmtex > 0 && gXRGL.ActiveTexture )
		{
			CSRETRO_Backend_BindTexture( 1, (unsigned int)lmtex );
			gXRGL.ActiveTexture( GL_TEXTURE1 );
			gXRGL.Enable( GL_TEXTURE_2D );
			if( gXRGL.TexEnvi )
				gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			gXRGL.ActiveTexture( GL_TEXTURE0 );
			return 1;
		}
	}
	return 0;
}

static void UnbindLightmap( void )
{
	if( !gXRGL.ActiveTexture )
		return;
	gXRGL.ActiveTexture( GL_TEXTURE1 );
	if( gXRGL.Disable )
		gXRGL.Disable( GL_TEXTURE_2D );
	gXRGL.ActiveTexture( GL_TEXTURE0 );
}

void CSRETRO_BspMesh_Draw( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshDrawContext *ctx )
{
	int b;
	int bind_textures;
	int any_fb = 0;
	unsigned int hash_before;
	CSRETRO_MeshDrawContext local;

	if( !mesh || !mesh->captured || !gXRGL.Begin )
		return;
	if( mesh->vert_count < 3 && mesh->water_vert_count < 3 )
		return;

	if( !ctx )
	{
		memset( &local, 0, sizeof( local ) );
		local.bind_textures = 1;
		ctx = &local;
	}
	bind_textures = ctx->bind_textures;

	hash_before = HashVerts( mesh );
	if( !s_proof_inited )
	{
		s_build_serial_first = s_build_count;
		s_proof_inited = 1;
	}

	if( !ctx->skip_base )
	{
		s_draw.verts += mesh->vert_count;
		s_draw.build_serial = s_build_count;
		s_draw.build_serial_first = s_build_serial_first;
		s_draw.rebuilds_unchanged = s_rebuilds_stable;
		s_draw.geom_hash_before = hash_before;
		s_draw.geom_unchanged = ( !s_draw.geom_hash_after ) ? 1 : s_draw.geom_unchanged;
		if( hash_before != mesh->geom_hash )
			s_draw.geom_unchanged = 0;
		s_draw.skipped_turb += mesh->skipped_turb;
		s_draw.turb_surfaces += mesh->turb_surfaces;
		s_draw.turb_polys += mesh->turb_polys;
		s_draw.turb_verts += mesh->turb_verts;
		s_draw.waterside_candidates += mesh->waterside_candidates;
		if( mesh->liquid_model )
			s_draw.liquid_models++;
		s_draw.anim_candidates += mesh->anim_candidates;
		s_draw.conveyor_candidates += mesh->conveyor_candidates;
		s_draw.fullbright_candidates += mesh->fullbright_candidates;
		s_draw.alternate_candidates += mesh->alternate_candidates;
		s_draw.random_tile_candidates += mesh->random_tile_candidates;
		if( ctx->entity_frame != 0.0f && mesh->alternate_candidates )
			s_draw.alternate_used++;
	}

	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );

	if( !ctx->skip_base )
	{
		for( b = 0; b < mesh->batch_count; b++ )
		{
			const CSRETRO_MeshBatch *batch = &mesh->batches[b];
			xr_texture_t *base = (xr_texture_t *)batch->anim_source;
			xr_texture_t *resolved;
			unsigned int tex;
			int has_lm;
			float sOff = 0.0f, tOff = 0.0f;

			if( batch->tri_count <= 0 )
				continue;

			resolved = TextureAnimation( base, ctx );
			if( !resolved )
				resolved = base;
			NoteAnim( base, resolved );
			if( resolved && resolved->fb_texturenum )
				any_fb = 1;

			tex = resolved ? (unsigned int)resolved->gl_texturenum : batch->tex;
			if( bind_textures )
				CSRETRO_Backend_BindTexture( 0, tex );
			has_lm = BindLightmap( mesh, batch, ctx, bind_textures );

			if( batch->flags & XR_SURF_CONVEYOR )
			{
				ConveyorOffset( batch, ctx, &sOff, &tOff );
				NoteConveyor( sOff, tOff );
			}

			DrawBatchTris( mesh, batch, sOff, tOff, has_lm );
			if( has_lm )
				UnbindLightmap();
		}

		if( bind_textures )
			CSRETRO_Backend_CleanupTextures();
	}
	else
	{
		for( b = 0; b < mesh->batch_count; b++ )
		{
			xr_texture_t *base = (xr_texture_t *)mesh->batches[b].anim_source;
			xr_texture_t *resolved = TextureAnimation( base, ctx );
			if( resolved && resolved->fb_texturenum )
				any_fb = 1;
		}
	}

	if( any_fb && !ctx->skip_fullbright && bind_textures )
	{
		unsigned char blend_was = 0, alpha_was = 0, fog_was = 0, depth_mask = 1;
		int blend_src = GL_ONE, blend_dst = GL_ONE, texenv = GL_MODULATE;

		if( gXRGL.IsEnabled )
		{
			blend_was = gXRGL.IsEnabled( GL_BLEND );
			alpha_was = gXRGL.IsEnabled( GL_ALPHA_TEST );
			fog_was = gXRGL.IsEnabled( GL_FOG );
		}
		if( gXRGL.GetBooleanv )
			gXRGL.GetBooleanv( 0x0B72, &depth_mask ); // GL_DEPTH_WRITEMASK
		if( gXRGL.GetIntegerv )
		{
			gXRGL.GetIntegerv( 0x0BE1, &blend_src ); // GL_BLEND_SRC
			gXRGL.GetIntegerv( 0x0BE0, &blend_dst ); // GL_BLEND_DST
			gXRGL.GetIntegerv( GL_TEXTURE_ENV_MODE, &texenv );
		}

		CSRETRO_Backend_PushFog();
		if( gXRGL.Disable )
		{
			gXRGL.Disable( GL_FOG );
			gXRGL.Disable( GL_ALPHA_TEST );
		}
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_ONE, GL_ONE );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		if( gXRGL.TexEnvi )
			gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		if( gXRGL.Color4f )
			gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		UnbindLightmap();

		for( b = 0; b < mesh->batch_count; b++ )
		{
			const CSRETRO_MeshBatch *batch = &mesh->batches[b];
			xr_texture_t *base = (xr_texture_t *)batch->anim_source;
			xr_texture_t *resolved;
			float sOff = 0.0f, tOff = 0.0f;

			if( batch->tri_count <= 0 )
				continue;
			resolved = TextureAnimation( base, ctx );
			if( !resolved || !resolved->fb_texturenum )
				continue;
			CSRETRO_Backend_BindTexture( 0, resolved->fb_texturenum );
			if( batch->flags & XR_SURF_CONVEYOR )
				ConveyorOffset( batch, ctx, &sOff, &tOff );
			DrawBatchTris( mesh, batch, sOff, tOff, 0 );
			s_draw.fullbright_drawn++;
		}

		if( gXRGL.TexEnvi )
			gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texenv ? texenv : GL_REPLACE );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( depth_mask );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( (unsigned int)blend_src, (unsigned int)blend_dst );
		if( gXRGL.Disable )
			gXRGL.Disable( GL_ALPHA_TEST );
		if( alpha_was && gXRGL.Enable )
			gXRGL.Enable( GL_ALPHA_TEST );
		if( blend_was )
		{
			if( gXRGL.Enable )
				gXRGL.Enable( GL_BLEND );
		}
		else if( gXRGL.Disable )
			gXRGL.Disable( GL_BLEND );
		CSRETRO_Backend_PopFog();
		if( !fog_was && gXRGL.Disable )
			gXRGL.Disable( GL_FOG );
		if( bind_textures )
			CSRETRO_Backend_CleanupTextures();
	}

	{
		unsigned int hash_after = HashVerts( mesh );
		s_draw.geom_hash_after = hash_after;
		if( hash_after != hash_before || hash_after != mesh->geom_hash )
			s_draw.geom_unchanged = 0;
	}
	s_draw.anim_tex_changed = s_anim_tex_changed;
	s_draw.anim_tex_first = s_anim_tex_first;
	s_draw.anim_tex_last = s_anim_tex_last;
	s_draw.conveyor_uv_changed = s_conv_uv_changed;
}

typedef struct WaterGLSnap_s
{
	unsigned char blend;
	unsigned char alpha;
	unsigned char fog;
	unsigned char cull;
	unsigned char tex2d0;
	unsigned char depth_mask;
	int blend_src;
	int blend_dst;
	int texenv;
	int cull_mode;
	float color[4];
} WaterGLSnap;

static void WaterSnap( WaterGLSnap *s )
{
	memset( s, 0, sizeof( *s ) );
	s->depth_mask = 1;
	s->blend_src = GL_ONE;
	s->blend_dst = GL_ONE;
	s->texenv = GL_MODULATE;
	s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1.0f;
	if( gXRGL.IsEnabled )
	{
		s->blend = gXRGL.IsEnabled( GL_BLEND );
		s->alpha = gXRGL.IsEnabled( GL_ALPHA_TEST );
		s->fog = gXRGL.IsEnabled( GL_FOG );
		s->cull = gXRGL.IsEnabled( GL_CULL_FACE );
		s->tex2d0 = gXRGL.IsEnabled( GL_TEXTURE_2D );
	}
	if( gXRGL.GetBooleanv )
		gXRGL.GetBooleanv( 0x0B72, &s->depth_mask );
	if( gXRGL.GetIntegerv )
	{
		gXRGL.GetIntegerv( 0x0BE1, &s->blend_src );
		gXRGL.GetIntegerv( 0x0BE0, &s->blend_dst );
		gXRGL.GetIntegerv( GL_TEXTURE_ENV_MODE, &s->texenv );
		gXRGL.GetIntegerv( 0x0B45, &s->cull_mode ); // GL_CULL_FACE_MODE
	}
	if( gXRGL.GetFloatv )
		gXRGL.GetFloatv( 0x0B00, s->color ); // GL_CURRENT_COLOR
}

static void WaterRestore( const WaterGLSnap *s )
{
	if( gXRGL.TexEnvi )
		gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, s->texenv ? s->texenv : GL_REPLACE );
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( s->depth_mask );
	if( gXRGL.BlendFunc )
		gXRGL.BlendFunc( (unsigned int)s->blend_src, (unsigned int)s->blend_dst );
	if( s->alpha )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_ALPHA_TEST );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_ALPHA_TEST );
	if( s->blend )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_BLEND );
	if( s->fog )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_FOG );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_FOG );
	if( s->cull )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_CULL_FACE );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_CULL_FACE );
	if( s->tex2d0 )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_TEXTURE_2D );
	}
	else if( gXRGL.Disable )
		gXRGL.Disable( GL_TEXTURE_2D );
	if( gXRGL.Color4f )
		gXRGL.Color4f( s->color[0], s->color[1], s->color[2], s->color[3] );
}

static int WaterGLMatches( const WaterGLSnap *a, const WaterGLSnap *b )
{
	if( a->blend != b->blend || a->alpha != b->alpha || a->fog != b->fog || a->cull != b->cull )
		return 0;
	if( a->tex2d0 != b->tex2d0 || a->depth_mask != b->depth_mask )
		return 0;
	if( a->blend_src != b->blend_src || a->blend_dst != b->blend_dst || a->texenv != b->texenv )
		return 0;
	return 1;
}

static void DrawWaterTris( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshBatch *batch, const CSRETRO_MeshDrawContext *ctx, int has_lm )
{
	int i;
	float time;
	float view_z;
	float wave_scale;

	if( !gXRGL.Begin )
		return;
	time = ctx ? ctx->time : 0.0f;
	view_z = ctx ? ctx->vieworg[2] : 0.0f;
	wave_scale = ctx ? ctx->wave_scale : 0.0f;

	gXRGL.Begin( GL_TRIANGLES );
	for( i = 0; i < batch->tri_count * 3; i++ )
	{
		const CSRETRO_MeshVert *v = &mesh->water_verts[batch->first_tri + i];
		float os = v->st[0];
		float ot = v->st[1];
		float s, t, nv, waveHeight;

		// EmitWaterPolys: os/ot are stored turb s/t, not normalized brush UVs.
		s = os + s_turbsin[(int)( ( ot * 0.125f + time ) * XR_TURBSCALE ) & 255];
		t = ot + s_turbsin[(int)( ( os * 0.125f + time ) * XR_TURBSCALE ) & 255];
		s *= ( 1.0f / XR_SUBDIVIDE_SIZE );
		t *= ( 1.0f / XR_SUBDIVIDE_SIZE );

		waveHeight = wave_scale;
		if( batch->surface_z0 >= view_z )
			waveHeight = -waveHeight;
		if( waveHeight != 0.0f )
		{
			nv = s_turbsin[(int)( time * 160.0f + v->xyz[1] + v->xyz[0] ) & 255] + 8.0f;
			nv = ( s_turbsin[(int)( v->xyz[0] * 5.0f + time * 171.0f - v->xyz[1] ) & 255] + 8.0f ) * 0.8f + nv;
			nv = nv * waveHeight + v->xyz[2];
		}
		else
			nv = v->xyz[2];

		if( gXRGL.TexCoord2f )
			gXRGL.TexCoord2f( s, t );
		if( has_lm && gXRGL.MultiTexCoord2f )
			gXRGL.MultiTexCoord2f( GL_TEXTURE1, v->lm[0], v->lm[1] );
		gXRGL.Vertex3f( v->xyz[0], v->xyz[1], nv );
	}
	gXRGL.End();
}

void CSRETRO_BspMesh_DrawWater( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshDrawContext *ctx )
{
	int b;
	int bind_textures;
	int use_litwater = 0;
	unsigned int hash_before;
	WaterGLSnap before, after;
	CSRETRO_MeshDrawContext local;

	if( !mesh || !mesh->captured || mesh->water_vert_count < 3 || !gXRGL.Begin )
		return;
	if( !ctx )
	{
		memset( &local, 0, sizeof( local ) );
		local.bind_textures = 1;
		local.water_alpha = 1.0f;
		local.water_pass = CSRETRO_WATER_OPAQUE;
		ctx = &local;
	}
	if( ctx->water_pass == CSRETRO_WATER_SKIP )
		return;

	bind_textures = ctx->bind_textures;
	hash_before = HashVerts( mesh );
	WaterSnap( &before );

	s_draw.effective_alpha = ctx->water_alpha;
	if( ctx->get_parm )
	{
		s_draw.alpha_capability = (int)ctx->get_parm( XR_PARM_WATER_ALPHA, 0 );
		s_draw.litwater = (int)ctx->get_parm( XR_PARM_MAP_HAS_LITWATER, 0 );
		use_litwater = s_draw.litwater;
	}

	if( ctx->water_pass == CSRETRO_WATER_LATE )
	{
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		if( gXRGL.Disable )
			gXRGL.Disable( GL_ALPHA_TEST );
		if( gXRGL.TexEnvi )
			gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		if( gXRGL.Color4f )
			gXRGL.Color4f( 1.0f, 1.0f, 1.0f, ctx->water_alpha );
	}
	else if( ctx->water_pass == CSRETRO_WATER_OPAQUE )
	{
		if( gXRGL.Disable )
		{
			gXRGL.Disable( GL_BLEND );
			gXRGL.Disable( GL_ALPHA_TEST );
		}
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_TRUE );
		if( gXRGL.Color4f )
			gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );

	for( b = 0; b < mesh->water_batch_count; b++ )
	{
		const CSRETRO_MeshBatch *batch = &mesh->water_batches[b];
		xr_texture_t *base = (xr_texture_t *)batch->anim_source;
		xr_texture_t *resolved;
		unsigned int tex;
		int has_lm = 0;

		if( batch->tri_count <= 0 )
			continue;

		if( ctx->is_brush )
		{
			s_draw.brush_turb_candidates += batch->tri_count > 0 ? 1 : 0;
			if( batch->plane_type != XR_PLANE_Z && !( ctx->effects & XR_EF_WATERSIDES ) )
				continue;
			if( ctx->entity_mins[2] + 1.0f >= batch->plane_dist )
				continue;
			if( batch->plane_type != XR_PLANE_Z )
				s_draw.waterside_drawn++;
		}

		resolved = TextureAnimation( base, ctx );
		if( !resolved )
			resolved = base;
		NoteAnim( base, resolved );
		tex = resolved ? (unsigned int)resolved->gl_texturenum : batch->tex;
		if( bind_textures )
			CSRETRO_Backend_BindTexture( 0, tex );
		if( use_litwater && !( batch->flags & XR_SURF_DRAWTILED ) )
			has_lm = BindLightmap( mesh, batch, ctx, bind_textures );

		DrawWaterTris( mesh, batch, ctx, has_lm );
		if( has_lm )
			UnbindLightmap();

		if( ctx->water_pass == CSRETRO_WATER_OPAQUE )
			s_draw.world_water_opaque++;
		else if( ctx->water_pass == CSRETRO_WATER_LATE )
			s_draw.world_water_late++;
		else if( ctx->water_pass == CSRETRO_WATER_BRUSH )
			s_draw.brush_turb_drawn++;
	}

	if( ctx->water_pass == CSRETRO_WATER_OPAQUE && s_draw.world_water_late == 0 )
		s_draw.world_water_base = s_draw.world_water_opaque;
	if( ctx->water_pass == CSRETRO_WATER_LATE )
		s_draw.world_water_base = 0;

	if( bind_textures )
		CSRETRO_Backend_CleanupTextures();
	WaterRestore( &before );
	WaterSnap( &after );
	s_draw.gl_restore_ok = WaterGLMatches( &before, &after ) ? 1 : 0;

	{
		unsigned int hash_after = HashVerts( mesh );
		if( hash_after != hash_before || hash_after != mesh->geom_hash )
		{
			s_draw.geom_unchanged = 0;
			s_draw.water_cache_mutate = 1;
		}
		s_draw.geom_hash_after = hash_after;
		if( !s_draw.geom_hash_before )
			s_draw.geom_hash_before = hash_before;
	}
}

void CSRETRO_BspMesh_OnNewMap( void )
{
	memset( &s_draw, 0, sizeof( s_draw ) );
	s_proof_inited = 0;
	AnimTrackReset();
	s_conv_s_first = 0.0f;
	s_conv_t_first = 0.0f;
	s_conv_have_first = 0;
	s_conv_uv_changed = 0;
	s_build_serial_first = 0;
	s_builds_prev = -1;
	s_rebuilds_stable = 0;
}

void CSRETRO_BspMesh_BeginFrame( void )
{
	int changed = s_anim_tex_changed;
	unsigned int first = s_anim_tex_first;
	unsigned int last = s_anim_tex_last;
	void *anim_src[XR_ANIM_TRACK];
	unsigned int anim_tex0[XR_ANIM_TRACK];
	int anim_n = s_anim_track_n;
	memcpy( anim_src, s_anim_src, sizeof( anim_src ) );
	memcpy( anim_tex0, s_anim_tex0, sizeof( anim_tex0 ) );
	int conv_changed = s_conv_uv_changed;
	float conv_s = s_draw.conveyor_s;
	float conv_t = s_draw.conveyor_t;
	int have_conv = s_conv_have_first;
	int proof = s_proof_inited;
	int serial_first = s_build_serial_first;
	int rebuilds_stable = s_rebuilds_stable;
	if( s_builds_prev >= 0 && s_build_count == s_builds_prev )
		rebuilds_stable = 1;
	s_builds_prev = s_build_count;
	s_rebuilds_stable = rebuilds_stable;
	memset( &s_draw, 0, sizeof( s_draw ) );
	s_anim_tex_changed = changed;
	s_anim_tex_first = first;
	s_anim_tex_last = last;
	s_anim_track_n = anim_n;
	memcpy( s_anim_src, anim_src, sizeof( s_anim_src ) );
	memcpy( s_anim_tex0, anim_tex0, sizeof( s_anim_tex0 ) );
	s_conv_uv_changed = conv_changed;
	s_draw.conveyor_s = conv_s;
	s_draw.conveyor_t = conv_t;
	s_conv_have_first = have_conv;
	s_proof_inited = proof;
	s_build_serial_first = serial_first;
	s_rebuilds_stable = rebuilds_stable;
}

void CSRETRO_BspMesh_GetDrawStats( CSRETRO_MeshDrawStats *out )
{
	if( !out )
		return;
	*out = s_draw;
	out->anim_tex_changed = s_anim_tex_changed;
	out->anim_tex_first = s_anim_tex_first;
	out->anim_tex_last = s_anim_tex_last;
	out->conveyor_uv_changed = s_conv_uv_changed;
	out->build_serial_first = s_build_serial_first;
	out->rebuilds_unchanged = s_rebuilds_stable;
}

int CSRETRO_BspMesh_BuildCount( void )
{
	return s_build_count;
}
