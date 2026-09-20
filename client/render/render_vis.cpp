#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "entity_types.h"

#include "render_vis.h"
#include "csretro_render.h"
#include "render_scene.h"

#include <string.h>

static unsigned char s_pvs[CSRETRO_VIS_PVS_MAX];
static unsigned char s_surf_mask[CSRETRO_VIS_SURF_MAX_BYTES];
static csretro_efrag_info_t s_efrags[CSRETRO_VIS_EFRAG_MAX];
static csretro_frame_vis_t s_info;
static int s_valid = 0;
static int s_fed = 0;
static int s_logged = 0;
static int s_map_logged = 0;
static int s_efrag_logged = 0;
static int s_overview_logged = 0;
static int s_backface_logged = 0;
static unsigned int s_last_pvs_hash = 0;
static int s_last_viewleaf = -1;

static unsigned int HashBytes( const unsigned char *p, int n )
{
	unsigned int h = 2166136261u;
	int i;

	for( i = 0; i < n; i++ )
		h = ( h ^ p[i] ) * 16777619u;
	return h;
}

void CSRETRO_Vis_OnNewMap( void )
{
	s_valid = 0;
	s_fed = 0;
	s_logged = 0;
	s_map_logged = 0;
	s_efrag_logged = 0;
	s_overview_logged = 0;
	s_backface_logged = 0;
	s_last_pvs_hash = 0;
	s_last_viewleaf = -1;
	memset( s_pvs, 0, sizeof( s_pvs ) );
	memset( s_surf_mask, 0, sizeof( s_surf_mask ) );
	memset( s_efrags, 0, sizeof( s_efrags ) );
	memset( &s_info, 0, sizeof( s_info ) );
}

int CSRETRO_Vis_Prepare( const struct ref_viewpass_s *rvp )
{
	csretro_vis_request_t req;
	csretro_frame_vis_t second;
	int rc;
	unsigned int client_hash;

	s_valid = 0;
	s_fed = 0;
	memset( &s_info, 0, sizeof( s_info ) );
	if( !rvp || !gRenderAPI.PrepareCurrentFrameVis )
		return 0;

	memset( &req, 0, sizeof( req ) );
	req.version = CSRETRO_FRAME_VIS_VERSION;
	req.flags = CSRETRO_VIS_PREPARE | CSRETRO_VIS_COLLECT_SURF | CSRETRO_VIS_COLLECT_EFRAG;
	req.rvp = rvp;
	req.pvs_out = s_pvs;
	req.pvs_capacity = (int)sizeof( s_pvs );
	req.surf_mask_out = s_surf_mask;
	req.mask_capacity = (int)sizeof( s_surf_mask );
	req.efrag_out = s_efrags;
	req.efrag_capacity = CSRETRO_VIS_EFRAG_MAX;
	req.info = &s_info;
	rc = gRenderAPI.PrepareCurrentFrameVis( &req );
	if( !rc || !s_info.prepared )
		return 0;

	s_valid = 1;
	client_hash = ( s_info.pvsbytes > 0 ) ? HashBytes( s_pvs, s_info.pvsbytes ) : 0;

	memset( &second, 0, sizeof( second ) );
	req.info = &second;
	req.flags = CSRETRO_VIS_PREPARE;
	req.surf_mask_out = NULL;
	req.efrag_out = NULL;
	gRenderAPI.PrepareCurrentFrameVis( &req );

	if( !s_logged )
	{
		s_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: vis frame origin=%.1f %.1f %.1f angles=%.1f %.1f %.1f vf=%.3f %.3f %.3f vr=%.3f %.3f %.3f vu=%.3f %.3f %.3f viewleaf=%i oldviewleaf=%i pvsbytes=%i visleafs=%i engine_pvs=%08x client_pvs=%08x pvs_match=%i frustum=%08x novis=%i lockpvs=%i cross_leaf=%i overview=%i box_visible=%i box_hidden=%i reused=%i helper=%i farclip=%.1f\n",
			s_info.origin[0], s_info.origin[1], s_info.origin[2],
			s_info.angles[0], s_info.angles[1], s_info.angles[2],
			s_info.vforward[0], s_info.vforward[1], s_info.vforward[2],
			s_info.vright[0], s_info.vright[1], s_info.vright[2],
			s_info.vup[0], s_info.vup[1], s_info.vup[2],
			s_info.viewleaf, s_info.oldviewleaf, s_info.pvsbytes, s_info.visleafs,
			s_info.pvs_hash, client_hash, ( s_info.pvs_hash == client_hash ) ? 1 : 0,
			s_info.frustum_sig, s_info.novis, s_info.lockpvs, s_info.cross_leaf,
			s_info.overview, s_info.box_visible, s_info.box_hidden, second.reused,
			gRenderAPI.PrepareCurrentFrameVis ? 1 : 0, s_info.farclip );
		gEngfuncs.Con_Printf(
			"CS Retro: world vis total=%i pvs_rejected=%i frustum_rejected=%i backface=%i drawn=%i selection_hash=%08x sky_candidates=%i efrag_count=%i farclip=%.1f\n",
			s_info.world_surfaces, s_info.pvs_rejected, s_info.frustum_rejected,
			s_info.backface_rejected, s_info.drawn, s_info.selection_hash,
			s_info.sky_candidates, s_info.efrag_count, s_info.farclip );
		if( !s_backface_logged )
		{
			s_backface_logged = 1;
			if( s_info.backface_rejected > 0 )
				gEngfuncs.Con_Printf(
					"CS Retro: backface classifier CONFIRMED code parity with R_CullSurface world default GL_FRONT, runtime rejected=%i\n",
					s_info.backface_rejected );
			else
				gEngfuncs.Con_Printf(
					"CS Retro: backface classifier CONFIRMED code parity with R_CullSurface world default GL_FRONT, runtime backface reject NOT REPRODUCIBLE\n" );
		}
	}
	else if( s_info.viewleaf != s_last_viewleaf || s_info.pvs_hash != s_last_pvs_hash )
	{
		gEngfuncs.Con_Printf(
			"CS Retro: vis leaf-change viewleaf=%i oldviewleaf=%i pvs=%08x drawn=%i selection_hash=%08x cross_leaf=%i\n",
			s_info.viewleaf, s_info.oldviewleaf, s_info.pvs_hash, s_info.drawn,
			s_info.selection_hash, s_info.cross_leaf );
	}
	s_last_viewleaf = s_info.viewleaf;
	s_last_pvs_hash = s_info.pvs_hash;
	if( !s_overview_logged && s_info.overview )
	{
		s_overview_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: vis overview=1 prepared=%i pvsbytes=%i pvs_hash=%08x drawn=%i selection_hash=%08x mask_bytes=%i farclip=%.1f\n",
			s_info.prepared, s_info.pvsbytes, s_info.pvs_hash, s_info.drawn,
			s_info.selection_hash, CSRETRO_Vis_SurfMaskBytes(), s_info.farclip );
	}
	if( !s_map_logged && s_info.prepared )
	{
		s_map_logged = 1;
		gEngfuncs.Con_Printf( "CS Retro: Mod_GetCurrentVis active bytes=%i hash=%08x\n",
			s_info.pvsbytes, client_hash );
		gEngfuncs.Con_Printf( "CS Retro: r_renderscene ownership fog=visible-Xash-R_DrawFog-R_CheckFog client-triangle-fog=draw-only underwater=visible-Xash ripple=visible-Xash-R_AnimateRipples extraupdate=visible-Xash alias=NOT_REPRODUCIBLE_WITH_CURRENT_GAME_CONTENT\n" );
	}
	return 1;
}

unsigned char *CSRETRO_Vis_CurrentBuffer( void )
{
	return s_pvs;
}

unsigned char *CSRETRO_Mod_GetCurrentVis( void )
{
	return s_pvs;
}

int CSRETRO_Vis_Valid( void )
{
	return s_valid;
}

const csretro_frame_vis_t *CSRETRO_Vis_Info( void )
{
	return &s_info;
}

const unsigned char *CSRETRO_Vis_SurfMask( void )
{
	return s_valid ? s_surf_mask : NULL;
}

int CSRETRO_Vis_SurfMaskBytes( void )
{
	if( !s_valid || s_info.world_surfaces <= 0 )
		return 0;
	return ( s_info.world_surfaces + 7 ) >> 3;
}

int CSRETRO_Vis_EfragCount( void )
{
	return s_valid ? s_info.efrag_count : 0;
}

const csretro_efrag_info_t *CSRETRO_Vis_Efrag( int index )
{
	if( !s_valid || index < 0 || index >= s_info.efrag_count || index >= CSRETRO_VIS_EFRAG_MAX )
		return NULL;
	return &s_efrags[index];
}

void CSRETRO_Vis_FeedEfrags( void )
{
	int i, n;

	if( s_fed || !s_valid )
		return;
	s_fed = 1;
	n = s_info.efrag_count;
	if( n > CSRETRO_VIS_EFRAG_MAX )
		n = CSRETRO_VIS_EFRAG_MAX;
	for( i = 0; i < n; i++ )
	{
		cl_entity_t *live = gEngfuncs.GetEntityByIndex( s_efrags[i].entity_index );

		if( !live || !live->model )
			continue;
		if( CSRETRO_Scene_FindByIndex( s_efrags[i].entity_index ) )
			continue;
		CSRETRO_Renderer_AddEntity( ET_FRAGMENTED, live );
	}
	if( !s_efrag_logged )
	{
		s_efrag_logged = 1;
		if( s_info.efrag_count == 0 )
			gEngfuncs.Con_Printf( "CS Retro: efrag inventory count=0 implemented safety contract runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT\n" );
		else
			gEngfuncs.Con_Printf(
				"CS Retro: efrag feed count=%i first_index=%i model_type=%i leaf=%i origin=%.0f %.0f %.0f\n",
				s_info.efrag_count, s_efrags[0].entity_index, s_efrags[0].model_type,
				s_efrags[0].leaf, s_efrags[0].origin[0], s_efrags[0].origin[1], s_efrags[0].origin[2] );
	}
}

int CSRETRO_Vis_DrawSky( void )
{
	csretro_vis_request_t req;
	csretro_frame_vis_t sky;
	int rc;

	if( !s_valid || !gRenderAPI.PrepareCurrentFrameVis )
		return 0;

	memset( &req, 0, sizeof( req ) );
	memset( &sky, 0, sizeof( sky ) );
	req.version = CSRETRO_FRAME_VIS_VERSION;
	req.flags = CSRETRO_VIS_DRAW_SKY;
	req.surf_mask_out = s_surf_mask;
	req.mask_capacity = CSRETRO_Vis_SurfMaskBytes();
	req.info = &sky;
	rc = gRenderAPI.PrepareCurrentFrameVis( &req );
	if( sky.sky_candidates > 0 )
	{
		s_info.sky_drawn = sky.sky_drawn;
		s_info.sky_sides_nonempty = sky.sky_sides_nonempty;
		if( sky.farclip > 0.0f )
			s_info.farclip = sky.farclip;
	}
	return rc;
}

int CSRETRO_Vis_SurfaceVisible( int surface_index )
{
	int bytes;

	if( !s_valid || surface_index < 0 )
		return 0;
	bytes = CSRETRO_Vis_SurfMaskBytes();
	if( bytes <= 0 )
		return 1;
	if( ( surface_index >> 3 ) >= bytes )
		return 0;
	return ( s_surf_mask[surface_index >> 3] >> ( surface_index & 7 ) ) & 1;
}
