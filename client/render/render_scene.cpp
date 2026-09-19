#include "render_scene.h"
#include "render_xash_brush.h"
#include "render_xash_sprite.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "const.h"

#include <string.h>

enum
{
	CSRETRO_STUDIO_SNAP_MAX = 256
};

static CSRETRO_EntCopy s_ents[CSRETRO_COPY_MAX];
static cl_entity_t s_studio_snaps[CSRETRO_STUDIO_SNAP_MAX];
static int s_snap_count = 0;
static int s_count = 0;
static int s_overflow = 0;
static CSRETRO_SceneStats s_stats;

static int LocalIndex( void )
{
	cl_entity_t *lp = gEngfuncs.GetLocalPlayer();
	return lp ? lp->index : 0;
}

static int Classify( int type, const cl_entity_t *ent, int model_type )
{
	if( type == ET_BEAM )
		return CSRETRO_KIND_BEAM;
	if( model_type == XR_MOD_SPRITE )
	{
		if( type == ET_TEMPENTITY )
			return CSRETRO_KIND_TENT_SPRITE;
		return CSRETRO_KIND_NORMAL_SPRITE;
	}
	if( model_type == XR_MOD_BRUSH )
		return CSRETRO_KIND_BRUSH;
	if( model_type == XR_MOD_STUDIO )
	{
		if( ent->player && ent->index > 0 && ent->index == LocalIndex() )
			return CSRETRO_KIND_STUDIO_LOCAL;
		return CSRETRO_KIND_STUDIO;
	}
	return CSRETRO_KIND_OTHER;
}

void CSRETRO_Scene_Clear( void )
{
	s_count = 0;
	s_snap_count = 0;
	s_overflow = 0;
	memset( &s_stats, 0, sizeof( s_stats ) );
}

void CSRETRO_Scene_Add( int type, struct cl_entity_s *ent )
{
	CSRETRO_EntCopy *dst;
	const xr_model_t *mod;
	int model_type;
	int kind;

	if( !ent )
		return;
	if( s_count >= CSRETRO_COPY_MAX )
	{
		s_overflow++;
		s_stats.overflow = s_overflow;
		return;
	}

	mod = (const xr_model_t *)ent->model;
	model_type = mod ? mod->type : -1;
	kind = Classify( type, ent, model_type );

	dst = &s_ents[s_count++];
	memset( dst, 0, sizeof( *dst ) );
	dst->type = type;
	dst->index = ent->index;
	dst->player = ent->player ? 1 : 0;
	dst->model_type = model_type;
	dst->model = ent->model;
	dst->origin[0] = ent->origin[0];
	dst->origin[1] = ent->origin[1];
	dst->origin[2] = ent->origin[2];
	dst->angles[0] = ent->angles[0];
	dst->angles[1] = ent->angles[1];
	dst->angles[2] = ent->angles[2];
	dst->rendermode = ent->curstate.rendermode;
	dst->renderfx = ent->curstate.renderfx;
	dst->renderamt = ent->curstate.renderamt;
	dst->rendercolor[0] = ent->curstate.rendercolor.r;
	dst->rendercolor[1] = ent->curstate.rendercolor.g;
	dst->rendercolor[2] = ent->curstate.rendercolor.b;
	dst->scale = ent->curstate.scale;
	dst->frame = ent->curstate.frame;
	dst->effects = ent->curstate.effects;
	dst->movetype = ent->curstate.movetype;
	dst->aiment = ent->curstate.aiment;
	dst->body = ent->curstate.body;
	dst->kind = kind;
	dst->snap_index = -1;
	dst->is_viewmodel = ( gEngfuncs.GetViewModel() == ent ) ? 1 : 0;
	dst->is_follow = ( ent->curstate.movetype == MOVETYPE_FOLLOW ) ? 1 : 0;
	dst->is_preview = ( ent->curstate.effects & EF_CSRETRO_PREVIEW ) ? 1 : 0;
	if( ( kind == CSRETRO_KIND_STUDIO || kind == CSRETRO_KIND_STUDIO_LOCAL )
		&& s_snap_count < CSRETRO_STUDIO_SNAP_MAX )
	{
		s_studio_snaps[s_snap_count] = *ent;
		dst->snap_index = s_snap_count++;
	}
	if( dst->is_follow )
		s_stats.studio_follow++;
	if( dst->is_viewmodel )
		s_stats.studio_viewmodel++;
	if( dst->is_preview )
		s_stats.studio_preview++;

	s_stats.mirrored = s_count;
	if( kind == CSRETRO_KIND_TENT_SPRITE )
		s_stats.tent_sprite++;
	else if( kind == CSRETRO_KIND_NORMAL_SPRITE )
		s_stats.normal_sprite++;
	else if( kind == CSRETRO_KIND_BRUSH )
		s_stats.brush++;
	else if( kind == CSRETRO_KIND_STUDIO )
		s_stats.studio++;
	else if( kind == CSRETRO_KIND_STUDIO_LOCAL )
		s_stats.studio_local++;
	else
		s_stats.other++;
}

int CSRETRO_Scene_Count( void )
{
	return s_count;
}

const CSRETRO_EntCopy *CSRETRO_Scene_Get( int index )
{
	if( index < 0 || index >= s_count )
		return NULL;
	return &s_ents[index];
}

void CSRETRO_Scene_GetStats( CSRETRO_SceneStats *out )
{
	if( !out )
		return;
	*out = s_stats;
}

void CSRETRO_Scene_NoteDrawn( int kind )
{
	if( kind == CSRETRO_KIND_TENT_SPRITE )
		s_stats.tent_drawn++;
	else if( kind == CSRETRO_KIND_NORMAL_SPRITE )
		s_stats.normal_drawn++;
	else if( kind == CSRETRO_KIND_STUDIO )
		s_stats.studio_drawn++;
}

void CSRETRO_Scene_NoteAttempted( void )
{
	s_stats.studio_attempted++;
}

struct cl_entity_s *CSRETRO_Scene_StudioSnap( int snap_index )
{
	if( snap_index < 0 || snap_index >= s_snap_count )
		return NULL;
	return &s_studio_snaps[snap_index];
}
