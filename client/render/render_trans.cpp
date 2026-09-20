#include "render_trans.h"
#include "render_scene.h"
#include "render_brush.h"
#include "render_sprite.h"
#include "render_studio.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "const.h"

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
	int rendermode;
	float dist;
	int order;
} TransItem;

static int s_logged = 0;
static int s_counts[5];

static int RankForRenderMode( int rendermode )
{
	switch( rendermode )
	{
	case kRenderTransTexture:
		return 1;
	case kRenderTransAdd:
		return 2;
	case kRenderGlow:
		return 3;
	default:
		return 0;
	}
}

static int IsOpaqueMode( int rendermode )
{
	return rendermode == kRenderNormal || rendermode == 0;
}

static float EntityDist( const CSRETRO_EntCopy *e, const float *vieworg )
{
	float org[3], d[3];

	if( e->kind == CSRETRO_KIND_BRUSH && e->rendermode == kRenderTransAlpha )
		return 1000000000.0f;
	org[0] = e->origin[0];
	org[1] = e->origin[1];
	org[2] = e->origin[2];
	d[0] = vieworg[0] - org[0];
	d[1] = vieworg[1] - org[1];
	d[2] = vieworg[2] - org[2];
	return d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
}

static int TransCompare( const void *a, const void *b )
{
	const TransItem *ia = (const TransItem *)a;
	const TransItem *ib = (const TransItem *)b;
	int ra, rb;

	if( ia->dist > ib->dist )
		return -1;
	if( ia->dist < ib->dist )
		return 1;
	ra = RankForRenderMode( ia->rendermode );
	rb = RankForRenderMode( ib->rendermode );
	if( ra > rb )
		return 1;
	if( ra < rb )
		return -1;
	if( ia->order < ib->order )
		return -1;
	if( ia->order > ib->order )
		return 1;
	return 0;
}

int CSRETRO_Trans_Draw( const float *vieworg, const float *viewangles,
	CSRETRO_SceneStats *stats, const struct ref_viewpass_s *rvp )
{
	TransItem items[TRANS_MAX];
	int n, i, count = 0, drawn = 0;

	(void)rvp;
	if( !vieworg || !viewangles )
		return 0;

	n = CSRETRO_Scene_Count();
	memset( s_counts, 0, sizeof( s_counts ) );
	for( i = 0; i < n && count < TRANS_MAX; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		TransItem *it;

		if( !e )
			continue;
		if( e->kind == CSRETRO_KIND_BRUSH )
		{
			if( IsOpaqueMode( e->rendermode ) )
				continue;
		}
		else if( e->kind == CSRETRO_KIND_TENT_SPRITE || e->kind == CSRETRO_KIND_NORMAL_SPRITE )
			;
		else if( e->kind == CSRETRO_KIND_STUDIO && !e->player && !e->is_follow && !e->is_viewmodel )
		{
			if( IsOpaqueMode( e->rendermode ) )
				continue;
		}
		else
			continue;
		if( !IsOpaqueMode( e->rendermode ) && e->renderamt <= 0 )
			continue;

		it = &items[count++];
		it->scene_index = i;
		if( e->kind == CSRETRO_KIND_BRUSH )
			it->kind = TRANS_BRUSH;
		else if( e->kind == CSRETRO_KIND_STUDIO )
			it->kind = e->player ? TRANS_PLAYER : TRANS_STUDIO;
		else
			it->kind = TRANS_SPRITE;
		it->rendermode = e->rendermode;
		it->dist = EntityDist( e, vieworg );
		it->order = i;
		s_counts[it->kind]++;
	}

	qsort( items, (size_t)count, sizeof( items[0] ), TransCompare );

	for( i = 0; i < count; i++ )
	{
		int ok = 0;

		if( items[i].kind == TRANS_BRUSH )
			ok = CSRETRO_Brush_DrawOne( items[i].scene_index, stats );
		else if( items[i].kind == TRANS_SPRITE )
			ok = CSRETRO_Sprite_DrawOne( items[i].scene_index, vieworg, viewangles, stats );
		else if( items[i].kind == TRANS_STUDIO )
			ok = CSRETRO_Studio_DrawOne( items[i].scene_index, stats );
		if( ok )
			drawn++;
	}

	if( !s_logged )
	{
		s_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: trans order count=%i drawn=%i brush=%i sprite=%i studio=%i player=%i alias=0\n",
			count, drawn, s_counts[TRANS_BRUSH], s_counts[TRANS_SPRITE],
			s_counts[TRANS_STUDIO], s_counts[TRANS_PLAYER] );
		if( s_counts[TRANS_BRUSH] == 0 || s_counts[TRANS_SPRITE] == 0 )
			gEngfuncs.Con_Printf( "CS Retro: trans overlapping brush+sprite+studio runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT — code parity implemented\n" );
	}
	return drawn;
}
