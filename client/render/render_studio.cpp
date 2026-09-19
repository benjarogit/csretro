// Offscreen non-player studio. GSMR stays the CS renderer.
// STUDIO_EVENTS never. No live entity across frames — local snapshot only.
#include "render_studio.h"
#include "render_scene.h"
#include "render_backend.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "com_model.h"
#include "const.h"
#include "entity_types.h"
#include "r_studioint.h"
#include "GameStudioModelRenderer.h"
#include "render_api.h"

extern engine_studio_api_t IEngineStudio;

static int Eligible( const CSRETRO_EntCopy *e )
{
	if( !e || e->kind != CSRETRO_KIND_STUDIO )
		return 0;
	if( e->type != ET_NORMAL )
		return 0;
	if( e->player || e->is_viewmodel || e->is_follow || e->is_preview )
		return 0;
	if( e->snap_index < 0 )
		return 0;
	return 1;
}

int CSRETRO_Studio_DrawList( CSRETRO_SceneStats *stats )
{
	cl_entity_t *saved_ent = NULL;
	struct model_s *saved_model = NULL;
	int i, n, drawn = 0;
	static int s_logged_model = 0;

	if( !gRenderAPI.R_SetCurrentEntity )
		return 0;

	if( IEngineStudio.GetCurrentEntity )
		saved_ent = IEngineStudio.GetCurrentEntity();
	if( saved_ent )
		saved_model = saved_ent->model;

	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		cl_entity_t *snap;
		int ok;

		if( !Eligible( e ) )
			continue;
		snap = CSRETRO_Scene_StudioSnap( e->snap_index );
		if( !snap || !snap->model )
			continue;
		if( snap->curstate.renderfx == kRenderFxDeadPlayer )
			continue;

		CSRETRO_Scene_NoteAttempted();
		gRenderAPI.R_SetCurrentEntity( snap );
		// STUDIO_RENDER only. Never STUDIO_EVENTS (no sounds, muzzle, attachment writeback).
		ok = g_StudioRenderer.StudioDrawModel( STUDIO_RENDER );
		if( ok )
		{
			CSRETRO_Scene_NoteDrawn( CSRETRO_KIND_STUDIO );
			drawn++;
			if( !s_logged_model && snap->model->name[0] )
			{
				s_logged_model = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen studio draw model=%s index=%i events=0\n",
					snap->model->name, snap->index );
			}
		}
	}

	gRenderAPI.R_SetCurrentEntity( saved_ent );
	if( gRenderAPI.R_SetCurrentModel )
		gRenderAPI.R_SetCurrentModel( saved_model );

	if( stats )
		CSRETRO_Scene_GetStats( stats );
	return drawn;
}
