#include "render_trans.h"
#include "render_scene.h"
#include "render_brush.h"
#include "render_sprite.h"
#include "render_studio.h"
#include "render_backend.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "const.h"
#include "com_model.h"
#include "render_api.h"
#include "render_xash_sprite.h"

#include <stdlib.h>
#include <string.h>

#define TRANS_MAX 2048

enum
{
	TRANS_BRUSH = 1,
	TRANS_SPRITE,
	TRANS_STUDIO,
	TRANS_PLAYER
};

typedef struct
{
	int scene_index;
	int kind;
	int effective_mode;
	int opaque;
	int fxblend;
	int rank;
	float center[3];
	float dist;
} TransItem;

static int s_class[CSRETRO_COPY_MAX];
static int s_drawn[CSRETRO_COPY_MAX];
static int s_class_n = 0;
static int s_duplicates = 0;
static int s_logged = 0;
static int s_cmp_logged = 0;
static int s_alias_logged = 0;
static int s_counts[5];

static int KindOf( const CSRETRO_EntCopy *e )
{
	if( !e )
		return 0;
	if( e->kind == CSRETRO_KIND_BRUSH )
		return TRANS_BRUSH;
	if( e->kind == CSRETRO_KIND_STUDIO || e->kind == CSRETRO_KIND_STUDIO_LOCAL )
		return e->player ? TRANS_PLAYER : TRANS_STUDIO;
	if( e->kind == CSRETRO_KIND_TENT_SPRITE || e->kind == CSRETRO_KIND_NORMAL_SPRITE )
		return TRANS_SPRITE;
	return 0;
}

static int ResolveEntity( const CSRETRO_EntCopy *e, csretro_entity_render_info_t *out )
{
	cl_entity_t *src;

	if( !e || !out || !gRenderAPI.GetEntityRenderInfoReadOnly )
		return 0;
	src = CSRETRO_Scene_StudioSnap( e->snap_index );
	if( !src )
		src = e->live;
	if( !src )
		return 0;
	memset( out, 0, sizeof( *out ) );
	out->version = CSRETRO_ENTITY_RENDER_INFO_VERSION;
	return gRenderAPI.GetEntityRenderInfoReadOnly( src, out );
}

static int TransCompare( const void *a, const void *b )
{
	const TransItem *ia = (const TransItem *)a;
	const TransItem *ib = (const TransItem *)b;

	if( ia->dist > ib->dist )
		return -1;
	if( ia->dist < ib->dist )
		return 1;
	if( ia->rank > ib->rank )
		return 1;
	if( ia->rank < ib->rank )
		return -1;
	return 0;
}

void CSRETRO_Trans_ClassifyScene( const float *vieworg )
{
	int i, n;

	(void)vieworg;
	s_class_n = 0;
	s_duplicates = 0;
	memset( s_class, 0, sizeof( s_class ) );
	memset( s_drawn, 0, sizeof( s_drawn ) );

	n = CSRETRO_Scene_Count();
	if( n > CSRETRO_COPY_MAX )
		n = CSRETRO_COPY_MAX;
	s_class_n = n;

	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		csretro_entity_render_info_t info;

		if( !e )
			continue;
		if( e->is_viewmodel || e->is_follow || e->is_preview )
		{
			s_class[i] = CSRETRO_DRAW_SKIP;
			continue;
		}
		if( e->model_type == XR_MOD_ALIAS )
		{
			s_class[i] = CSRETRO_DRAW_SKIP;
			if( !s_alias_logged )
			{
				s_alias_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: alias seen index=%i — KIND_OTHER skip, visible Xash R_DrawAliasModel remains owner\n",
					e->index );
			}
			continue;
		}
		if( !KindOf( e ) )
		{
			s_class[i] = CSRETRO_DRAW_SKIP;
			continue;
		}
		if( !ResolveEntity( e, &info ) )
		{
			s_class[i] = CSRETRO_DRAW_SKIP;
			continue;
		}
		if( info.opaque )
			s_class[i] = CSRETRO_DRAW_OPAQUE;
		else if( info.fxblend <= 0 )
			s_class[i] = CSRETRO_DRAW_SKIP;
		else
			s_class[i] = CSRETRO_DRAW_TRANS;
	}
	if( !s_alias_logged )
	{
		s_alias_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: alias status runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT — KIND_OTHER, not studio/sprite, visible Xash owns R_DrawAliasModel\n" );
	}
}

int CSRETRO_Trans_DrawClass( int scene_index )
{
	if( scene_index < 0 || scene_index >= s_class_n )
		return CSRETRO_DRAW_SKIP;
	return s_class[scene_index];
}

void CSRETRO_Trans_NoteDrawn( int scene_index )
{
	if( scene_index < 0 || scene_index >= s_class_n )
		return;
	if( s_drawn[scene_index] )
		s_duplicates++;
	s_drawn[scene_index] = 1;
}

int CSRETRO_Trans_DuplicateDraws( void )
{
	return s_duplicates;
}

int CSRETRO_Trans_Draw( const float *vieworg, const float *viewangles,
	CSRETRO_SceneStats *stats, const struct ref_viewpass_s *rvp )
{
	TransItem items[TRANS_MAX];
	int n, i, count = 0, drawn = 0;

	if( !vieworg || !viewangles )
		return 0;

	n = CSRETRO_Scene_Count();
	memset( s_counts, 0, sizeof( s_counts ) );
	for( i = 0; i < n && count < TRANS_MAX; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		csretro_entity_render_info_t info;
		TransItem *it;
		int kind;

		if( !e || CSRETRO_Trans_DrawClass( i ) != CSRETRO_DRAW_TRANS )
			continue;
		kind = KindOf( e );
		if( !kind )
			continue;
		if( !ResolveEntity( e, &info ) )
			continue;

		it = &items[count++];
		it->scene_index = i;
		it->kind = kind;
		it->effective_mode = info.effective_rendermode;
		it->opaque = info.opaque;
		it->fxblend = info.fxblend;
		it->rank = info.rank;
		it->center[0] = info.center[0];
		it->center[1] = info.center[1];
		it->center[2] = info.center[2];
		it->dist = info.distance;
		s_counts[kind]++;
	}

	qsort( items, (size_t)count, sizeof( items[0] ), TransCompare );

	for( i = 0; i < count; i++ )
	{
		int ok = 0;

		if( items[i].kind == TRANS_BRUSH )
			ok = CSRETRO_Brush_DrawOne( items[i].scene_index, stats );
		else if( items[i].kind == TRANS_SPRITE )
		{
			/* Brush lightmaps leave TMU1 enabled; sprites must be single-texture. */
			CSRETRO_Backend_SyncTextureUnits();
			ok = CSRETRO_Sprite_DrawOne( items[i].scene_index, vieworg, viewangles, stats );
		}
		else if( items[i].kind == TRANS_STUDIO )
		{
			CSRETRO_Backend_SyncTextureUnits();
			ok = CSRETRO_Studio_DrawOne( items[i].scene_index, stats );
		}
		else if( items[i].kind == TRANS_PLAYER )
		{
			CSRETRO_Backend_SyncTextureUnits();
			ok = CSRETRO_Studio_DrawPlayerOne( items[i].scene_index, stats, rvp );
		}
		if( ok )
			drawn++;
	}

	if( !s_logged )
	{
		s_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: trans order count=%i drawn=%i brush=%i sprite=%i studio=%i player=%i alias=0 duplicate_scene_draws=%i resolver=%i\n",
			count, drawn, s_counts[TRANS_BRUSH], s_counts[TRANS_SPRITE],
			s_counts[TRANS_STUDIO], s_counts[TRANS_PLAYER], s_duplicates,
			gRenderAPI.GetEntityRenderInfoReadOnly ? 1 : 0 );
		if( s_counts[TRANS_BRUSH] == 0 || s_counts[TRANS_STUDIO] == 0 )
			gEngfuncs.Con_Printf( "CS Retro: trans overlapping brush+studio runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT — comparator uses Xash distance/rank\n" );
		if( s_counts[TRANS_PLAYER] == 0 )
			gEngfuncs.Con_Printf( "CS Retro: trans player runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT — DrawPlayerOne path complete\n" );
	}
	if( !s_cmp_logged && count > 0 )
	{
		int shown = count < 8 ? count : 8;
		s_cmp_logged = 1;
		gEngfuncs.Con_Printf( "CS Retro: trans comparator proof entries=%i xash_order=1 tie_break=0\n", count );
		for( i = 0; i < shown; i++ )
		{
			gEngfuncs.Con_Printf(
				"CS Retro: trans cmp i=%i scene=%i kind=%i mode=%i opaque=%i fxblend=%i center=%.1f %.1f %.1f dist=%.1f rank=%i\n",
				i, items[i].scene_index, items[i].kind, items[i].effective_mode,
				items[i].opaque, items[i].fxblend,
				items[i].center[0], items[i].center[1], items[i].center[2],
				items[i].dist, items[i].rank );
		}
	}
	return drawn;
}
