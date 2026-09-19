#include "render_world.h"
#include "render_backend.h"
#include "render_xash_brush.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XR_MAX_BATCH 4096

typedef struct WorldVert_s
{
	float xyz[3];
	float st[2];
	float lm[2];
} WorldVert;

typedef struct WorldBatch_s
{
	unsigned int tex;
	unsigned int lightmap;
	int first_tri;
	int tri_count;
	int flags;
} WorldBatch;

static CSRETRO_WorldEngine s_eng;
static WorldVert *s_verts = NULL;
static int s_vert_count = 0;
static WorldBatch *s_batches = NULL;
static int s_batch_count = 0;
static void *s_source = NULL;
static void *s_pending = NULL;
static CSRETRO_WorldStats s_stats;
static int s_logged_empty = 0;

static int IsWorldModel( const xr_model_t *m )
{
	if( !m || m->type != 0 )
		return 0;
	if( m->flags & XR_MODEL_WORLD )
		return 1;
	if( m->name[0] == 'm' && strstr( m->name, ".bsp" ) )
		return 1;
	return 0;
}

static void Log( const char *msg )
{
	if( s_eng.print )
		s_eng.print( msg );
}

void CSRETRO_World_SetEngine( const CSRETRO_WorldEngine *engine )
{
	if( engine )
		s_eng = *engine;
	else
		memset( &s_eng, 0, sizeof( s_eng ) );
}

void CSRETRO_World_Release( void )
{
	free( s_verts );
	s_verts = NULL;
	s_vert_count = 0;
	free( s_batches );
	s_batches = NULL;
	s_batch_count = 0;
	s_source = NULL;
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_logged_empty = 0;
}

static void AddTri( WorldVert **cursor, int *used, const float *a, const float *b, const float *c )
{
	WorldVert *v;
	if( *used + 3 > s_vert_count )
		return;
	v = *cursor;
	memcpy( v[0].xyz, a, sizeof( float ) * 3 );
	v[0].st[0] = a[3];
	v[0].st[1] = a[4];
	v[0].lm[0] = a[5];
	v[0].lm[1] = a[6];
	memcpy( v[1].xyz, b, sizeof( float ) * 3 );
	v[1].st[0] = b[3];
	v[1].st[1] = b[4];
	v[1].lm[0] = b[5];
	v[1].lm[1] = b[6];
	memcpy( v[2].xyz, c, sizeof( float ) * 3 );
	v[2].st[0] = c[3];
	v[2].st[1] = c[4];
	v[2].lm[0] = c[5];
	v[2].lm[1] = c[6];
	*cursor += 3;
	*used += 3;
}

static int CaptureFromModel( xr_model_t *mod )
{
	int first, count, i, guess_verts = 0, used = 0, polys = 0, surfaces = 0;
	int lm_max = -1;
	WorldVert *cursor;
	WorldBatch *batch = NULL;

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

	if( guess_verts <= 0 )
		return 0;

	CSRETRO_World_Release();
	s_verts = (WorldVert *)malloc( sizeof( WorldVert ) * (size_t)guess_verts );
	s_batches = (WorldBatch *)malloc( sizeof( WorldBatch ) * XR_MAX_BATCH );
	if( !s_verts || !s_batches )
	{
		CSRETRO_World_Release();
		return 0;
	}
	s_vert_count = guess_verts;
	cursor = s_verts;

	for( i = 0; i < count; i++ )
	{
		xr_msurface_t *surf = &mod->surfaces[first + i];
		xr_glpoly_t *p;
		unsigned int tex = 0, lm = 0;

		if( surf->flags & XR_SURF_DRAWSKY )
			continue;

		if( surf->texinfo && surf->texinfo->texture )
			tex = (unsigned int)surf->texinfo->texture->gl_texturenum;
		if( !( surf->flags & XR_SURF_DRAWTILED ) && surf->lightmaptexturenum >= 0 )
			lm = (unsigned int)surf->lightmaptexturenum;

		if( !batch || batch->tex != tex || batch->lightmap != lm || batch->flags != surf->flags )
		{
			if( s_batch_count >= XR_MAX_BATCH )
				break;
			batch = &s_batches[s_batch_count++];
			batch->tex = tex;
			batch->lightmap = lm;
			batch->first_tri = used;
			batch->tri_count = 0;
			batch->flags = surf->flags;
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
					AddTri( &cursor, &used, p->verts[0], p->verts[v], p->verts[v + 1] );
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
					if( surf->texinfo->texture && surf->texinfo->texture->width )
						fan[e][3] /= (float)surf->texinfo->texture->width;
					if( surf->texinfo->texture && surf->texinfo->texture->height )
						fan[e][4] /= (float)surf->texinfo->texture->height;
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
					AddTri( &cursor, &used, fan[0], fan[v], fan[v + 1] );
					batch->tri_count++;
				}
			}
		}

		if( (int)lm > lm_max )
			lm_max = (int)lm;
	}

	s_vert_count = used;
	s_source = mod;
	memset( &s_stats, 0, sizeof( s_stats ) );
	strncpy( s_stats.map, mod->name, sizeof( s_stats.map ) - 1 );
	s_stats.surfaces = surfaces;
	s_stats.polys = polys;
	s_stats.tris = used / 3;
	s_stats.verts = used;
	s_stats.lightmap_pages = lm_max >= 0 ? lm_max + 1 : 0;
	s_stats.captured = used > 0;
	s_stats.empty_mesh = used == 0;
	s_logged_empty = 0;
	return s_stats.captured;
}

void CSRETRO_World_OnModel( void *mod, int create, const unsigned char *buffer )
{
	xr_model_t *m = (xr_model_t *)mod;
	(void)buffer;
	if( !IsWorldModel( m ) )
		return;
	if( !create )
	{
		if( s_source == mod || s_pending == mod )
			CSRETRO_World_Release();
		s_pending = NULL;
		return;
	}
	s_pending = mod;
}

void CSRETRO_World_OnNewMap( void )
{
	void *mod = s_pending;
	if( !mod && s_eng.get_model )
		mod = s_eng.get_model( 1 );
	if( mod )
		CaptureFromModel( (xr_model_t *)mod );
}

void CSRETRO_World_OnLightmaps( void )
{
	if( s_source )
		CaptureFromModel( (xr_model_t *)s_source );
	else if( s_pending )
		CaptureFromModel( (xr_model_t *)s_pending );
}

int CSRETRO_World_Ready( void )
{
	return s_stats.captured && s_vert_count >= 3;
}

void CSRETRO_World_GetStats( CSRETRO_WorldStats *out )
{
	if( out )
		*out = s_stats;
}

void CSRETRO_World_Draw( const float *vieworg, const float *viewangles, float fov_x, float fov_y )
{
	int b;
	unsigned int last_lm = 0xFFFFFFFFu;

	if( !CSRETRO_World_Ready() || !gXRGL.Begin )
	{
		if( !s_logged_empty )
		{
			s_logged_empty = 1;
			Log( "CS Retro: offscreen world not ready (no captured BSP mesh)\n" );
		}
		return;
	}

	CSRETRO_Backend_ApplyView( vieworg, viewangles, fov_x, fov_y );
	if( gXRGL.Color4f )
		gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	if( gXRGL.Enable )
		gXRGL.Enable( 0x0DE1 ); // GL_TEXTURE_2D

	for( b = 0; b < s_batch_count; b++ )
	{
		WorldBatch *batch = &s_batches[b];
		int i, has_lm;
		if( batch->tri_count <= 0 )
			continue;

		CSRETRO_Backend_BindTexture( 0, batch->tex );
		has_lm = batch->lightmap != 0 || ( s_stats.lightmap_pages > 0 && !( batch->flags & XR_SURF_DRAWTILED ) );
		if( has_lm && s_eng.get_parm )
		{
			int lmtex = (int)s_eng.get_parm( 7, (int)batch->lightmap ); // PARM_TEX_LIGHTMAP
			if( lmtex > 0 && gXRGL.ActiveTexture )
			{
				CSRETRO_Backend_BindTexture( 1, (unsigned int)lmtex );
				gXRGL.ActiveTexture( 0x84C1 );
				gXRGL.Enable( 0x0DE1 );
				if( gXRGL.TexEnvi )
					gXRGL.TexEnvi( 0x2300, 0x2200, 0x2100 );
				gXRGL.ActiveTexture( 0x84C0 );
				last_lm = batch->lightmap;
			}
			else
				has_lm = 0;
		}
		else
			has_lm = 0;

		gXRGL.Begin( 0x0004 ); // GL_TRIANGLES
		for( i = 0; i < batch->tri_count * 3; i++ )
		{
			WorldVert *v = &s_verts[batch->first_tri + i];
			if( gXRGL.TexCoord2f )
				gXRGL.TexCoord2f( v->st[0], v->st[1] );
			if( has_lm && gXRGL.MultiTexCoord2f )
				gXRGL.MultiTexCoord2f( 0x84C1, v->lm[0], v->lm[1] );
			gXRGL.Vertex3f( v->xyz[0], v->xyz[1], v->xyz[2] );
		}
		gXRGL.End();
		(void)last_lm;
	}

	CSRETRO_Backend_CleanupTextures();
}
