// Offscreen brush entities from the HUD_AddEntity mirror list.
// Transform: GoldSrc/Xash R_RotateForEntity / R_TranslateForEntity
// (origin, then yaw / -pitch / roll). Rendermode from Xash R_SetRenderMode.
// Mesh: shared CSRETRO_BspMesh_Build. Cache by model_t*. No worldmodel draw.
// Provenance (targeted): Xash engine/ref/gl/gl_rsurf.c R_DrawBrushModel /
// R_SetRenderMode; PrimeXT 46fb05b gl_rsurf.cpp texture-anim only (DEFERRED).
#include "render_brush.h"
#include "render_bsp_mesh.h"
#include "render_backend.h"
#include "render_scene.h"
#include "render_world.h"
#include "render_xash_brush.h"
#include "render_decal.h"
#include "render_dlight.h"
#include "render_xash_sprite.h"
#include "render_trans.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "const.h"
#include "render_api.h"

#include <string.h>
#include <math.h>

#define BRUSH_CACHE_MAX 256
#define BRUSH_MOVE_TRACK 128

#define GL_TEXTURE_2D 0x0DE1
#define GL_BLEND 0x0BE2
#define GL_ALPHA_TEST 0x0BC0
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_GREATER 0x0204
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_POLYGON_OFFSET_FILL 0x8037

static CSRETRO_BspMesh s_cache[BRUSH_CACHE_MAX];
static int s_cache_count = 0;
static CSRETRO_BrushStats s_bstats;
static CSRETRO_BrushMove s_last_move;
static int s_move_logged = 0;
static int s_logged_first = 0;
static int s_logged_nonzero = 0;
static int s_logged_water = 0;

typedef struct BrushPrev_s
{
	int index;
	int used;
	int seen;
	float origin[3];
	float angles[3];
} BrushPrev;

static BrushPrev s_prev[BRUSH_MOVE_TRACK];

static long GetParm( int parm, int arg )
{
	if( gRenderAPI.RenderGetParm )
		return (long)gRenderAPI.RenderGetParm( parm, arg );
	return 0;
}

static int IsWorldBrushModel( const xr_model_t *m, int index )
{
	if( index == 0 )
		return 1;
	if( !m )
		return 1;
	if( m->flags & XR_MODEL_WORLD )
		return 1;
	if( m->name[0] == 'm' && strstr( m->name, ".bsp" ) )
		return 1;
	if( CSRETRO_World_Model() && m == (const xr_model_t *)CSRETRO_World_Model() )
		return 1;
	return 0;
}

static int IsOpaqueMode( int rendermode )
{
	return rendermode == kRenderNormal;
}

static CSRETRO_BspMesh *CacheLookup( void *mod )
{
	int i;
	for( i = 0; i < s_cache_count; i++ )
	{
		if( s_cache[i].model == mod )
			return &s_cache[i];
	}
	return NULL;
}

static CSRETRO_BspMesh *CacheBuild( void *mod )
{
	CSRETRO_BspMesh *mesh;
	xr_model_t *m = (xr_model_t *)mod;
	int i;

	if( !mod || !m->surfaces )
		return NULL;
	mesh = CacheLookup( mod );
	if( mesh )
	{
		if( mesh->firstmodelsurface == m->firstmodelsurface
			&& mesh->nummodelsurfaces == m->nummodelsurfaces
			&& mesh->captured )
			return mesh;
		CSRETRO_BspMesh_Clear( mesh );
	}
	else
	{
		if( s_cache_count >= BRUSH_CACHE_MAX )
			return NULL;
		mesh = &s_cache[s_cache_count++];
	}
	if( !CSRETRO_BspMesh_Build( mesh, mod ) )
		return NULL;
	s_bstats.cache_models = s_cache_count;
	s_bstats.cache_verts = 0;
	for( i = 0; i < s_cache_count; i++ )
		s_bstats.cache_verts += s_cache[i].vert_count;
	return mesh;
}

static BrushPrev *PrevSlot( int index )
{
	int i, free_i = -1;
	for( i = 0; i < BRUSH_MOVE_TRACK; i++ )
	{
		if( s_prev[i].used && s_prev[i].index == index )
			return &s_prev[i];
		if( !s_prev[i].used && free_i < 0 )
			free_i = i;
	}
	if( free_i < 0 )
		return NULL;
	s_prev[free_i].used = 1;
	s_prev[free_i].seen = 0;
	s_prev[free_i].index = index;
	s_prev[free_i].origin[0] = s_prev[free_i].origin[1] = s_prev[free_i].origin[2] = 0.0f;
	s_prev[free_i].angles[0] = s_prev[free_i].angles[1] = s_prev[free_i].angles[2] = 0.0f;
	return &s_prev[free_i];
}

static int AnglesActive( const float *angles )
{
	return fabsf( angles[0] ) > 0.01f || fabsf( angles[1] ) > 0.01f || fabsf( angles[2] ) > 0.01f;
}

static void ApplyEntityTransform( const float *origin, const float *angles )
{
	if( !gXRGL.Translatef )
		return;
	if( gXRGL.PushMatrix )
		gXRGL.PushMatrix();
	gXRGL.Translatef( origin[0], origin[1], origin[2] );
	if( AnglesActive( angles ) && gXRGL.Rotatef )
	{
		// GoldSrc R_RotateForEntity: yaw, -pitch, roll.
		gXRGL.Rotatef( angles[1], 0.0f, 0.0f, 1.0f );
		gXRGL.Rotatef( -angles[0], 0.0f, 1.0f, 0.0f );
		gXRGL.Rotatef( angles[2], 1.0f, 0.0f, 0.0f );
	}
}

static void PopEntityTransform( void )
{
	if( gXRGL.PopMatrix )
		gXRGL.PopMatrix();
}

static int ApplyRenderMode( const CSRETRO_EntCopy *e )
{
	float amt;
	int bind_textures = 1;

	amt = (float)e->renderamt / 255.0f;
	if( amt < 0.0f )
		amt = 0.0f;
	if( amt > 1.0f )
		amt = 1.0f;

	if( gXRGL.Disable )
	{
		gXRGL.Disable( GL_BLEND );
		gXRGL.Disable( GL_ALPHA_TEST );
	}
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( GL_TRUE );
	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );

	switch( e->rendermode )
	{
	case kRenderNormal:
	default:
		if( gXRGL.Color4f )
			gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		break;
	case kRenderTransAlpha:
		if( gXRGL.Enable )
			gXRGL.Enable( GL_ALPHA_TEST );
		if( gXRGL.AlphaFunc )
			gXRGL.AlphaFunc( GL_GREATER, 0.25f );
		if( gXRGL.Color4f )
			gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		break;
	case kRenderTransTexture:
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		if( gXRGL.Color4f )
			gXRGL.Color4f( 1.0f, 1.0f, 1.0f, amt );
		break;
	case kRenderTransColor:
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
		if( gXRGL.Disable )
			gXRGL.Disable( GL_TEXTURE_2D );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		if( gXRGL.Color4f )
			gXRGL.Color4f(
				(float)e->rendercolor[0] / 255.0f,
				(float)e->rendercolor[1] / 255.0f,
				(float)e->rendercolor[2] / 255.0f,
				amt );
		bind_textures = 0;
		break;
	case kRenderTransAdd:
	case kRenderGlow:
	case kRenderWorldGlow:
		if( gXRGL.Enable )
			gXRGL.Enable( GL_BLEND );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_ONE, GL_ONE );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		if( gXRGL.Color4f )
			gXRGL.Color4f( amt, amt, amt, 1.0f );
		break;
	}
	return bind_textures;
}

static void RestoreDrawState( void )
{
	if( gXRGL.Disable )
	{
		gXRGL.Disable( GL_BLEND );
		gXRGL.Disable( GL_ALPHA_TEST );
	}
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( GL_TRUE );
	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );
	if( gXRGL.Color4f )
		gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
}

static void NoteMove( const CSRETRO_EntCopy *e, const xr_model_t *mod )
{
	BrushPrev *prev;
	int moved;

	if( e->index <= 0 )
		return;
	prev = PrevSlot( e->index );
	if( !prev )
		return;
	if( !prev->seen )
	{
		prev->seen = 1;
		prev->origin[0] = e->origin[0];
		prev->origin[1] = e->origin[1];
		prev->origin[2] = e->origin[2];
		prev->angles[0] = e->angles[0];
		prev->angles[1] = e->angles[1];
		prev->angles[2] = e->angles[2];
		return;
	}
	moved = fabsf( prev->origin[0] - e->origin[0] ) > 0.5f
		|| fabsf( prev->origin[1] - e->origin[1] ) > 0.5f
		|| fabsf( prev->origin[2] - e->origin[2] ) > 0.5f
		|| fabsf( prev->angles[0] - e->angles[0] ) > 0.5f
		|| fabsf( prev->angles[1] - e->angles[1] ) > 0.5f
		|| fabsf( prev->angles[2] - e->angles[2] ) > 0.5f;
	if( moved && !s_move_logged )
	{
		memset( &s_last_move, 0, sizeof( s_last_move ) );
		s_last_move.index = e->index;
		if( mod )
			strncpy( s_last_move.model, mod->name, sizeof( s_last_move.model ) - 1 );
		s_last_move.origin_before[0] = prev->origin[0];
		s_last_move.origin_before[1] = prev->origin[1];
		s_last_move.origin_before[2] = prev->origin[2];
		s_last_move.origin_after[0] = e->origin[0];
		s_last_move.origin_after[1] = e->origin[1];
		s_last_move.origin_after[2] = e->origin[2];
		s_last_move.angles_before[0] = prev->angles[0];
		s_last_move.angles_before[1] = prev->angles[1];
		s_last_move.angles_before[2] = prev->angles[2];
		s_last_move.angles_after[0] = e->angles[0];
		s_last_move.angles_after[1] = e->angles[1];
		s_last_move.angles_after[2] = e->angles[2];
		s_last_move.happened = 1;
		s_move_logged = 1;
	}
	prev->origin[0] = e->origin[0];
	prev->origin[1] = e->origin[1];
	prev->origin[2] = e->origin[2];
	prev->angles[0] = e->angles[0];
	prev->angles[1] = e->angles[1];
	prev->angles[2] = e->angles[2];
}

void CSRETRO_Brush_Release( void )
{
	int i;
	for( i = 0; i < s_cache_count; i++ )
		CSRETRO_BspMesh_Clear( &s_cache[i] );
	s_cache_count = 0;
	memset( &s_bstats, 0, sizeof( s_bstats ) );
	memset( &s_last_move, 0, sizeof( s_last_move ) );
	memset( s_prev, 0, sizeof( s_prev ) );
	s_move_logged = 0;
	s_logged_first = 0;
	s_logged_nonzero = 0;
	s_logged_water = 0;
}

void CSRETRO_Brush_OnModel( void *mod, int create )
{
	int i;
	xr_model_t *m = (xr_model_t *)mod;
	if( !m || m->type != XR_MOD_BRUSH )
		return;
	if( IsWorldBrushModel( m, -1 ) )
		return;
	if( !create )
	{
		for( i = 0; i < s_cache_count; i++ )
		{
			if( s_cache[i].model == mod )
				CSRETRO_BspMesh_Clear( &s_cache[i] );
		}
		return;
	}
}

void CSRETRO_Brush_OnNewMap( void )
{
	CSRETRO_Brush_Release();
}

void CSRETRO_Brush_OnLightmaps( void )
{
	int i;
	void *models[BRUSH_CACHE_MAX];
	int n = s_cache_count;
	for( i = 0; i < n; i++ )
		models[i] = s_cache[i].model;
	for( i = 0; i < n; i++ )
	{
		if( models[i] )
			CacheBuild( models[i] );
	}
}

void CSRETRO_Brush_GetStats( CSRETRO_BrushStats *out )
{
	if( out )
		*out = s_bstats;
}

const CSRETRO_BrushMove *CSRETRO_Brush_LastMove( void )
{
	return &s_last_move;
}

static int DrawBrushRange( int only_index, int opaque_only, CSRETRO_SceneStats *stats )
{
	int i, n, drawn = 0;

	n = CSRETRO_Scene_Count();
	if( gXRGL.Enable )
		gXRGL.Enable( GL_POLYGON_OFFSET_FILL );
	if( gXRGL.PolygonOffset )
		gXRGL.PolygonOffset( -0.5f, -1.0f );

	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		if( only_index >= 0 && i != only_index )
			continue;
		const xr_model_t *mod;
		CSRETRO_BspMesh *mesh;
		int bind_tex;

		if( !e || e->kind != CSRETRO_KIND_BRUSH || !e->model )
			continue;
		mod = (const xr_model_t *)e->model;
		if( IsWorldBrushModel( mod, e->index ) )
		{
			s_bstats.skipped_world++;
			continue;
		}
		if( only_index < 0 )
		{
			if( opaque_only && CSRETRO_Trans_DrawClass( i ) != CSRETRO_DRAW_OPAQUE )
				continue;
			if( !opaque_only && CSRETRO_Trans_DrawClass( i ) == CSRETRO_DRAW_OPAQUE )
				continue;
		}

		mesh = CacheBuild( e->model );
		if( !mesh || !mesh->captured )
		{
			s_bstats.skipped_empty++;
			continue;
		}

		NoteMove( e, mod );
		bind_tex = ApplyRenderMode( e );
		ApplyEntityTransform( e->origin, e->angles );
		{
			CSRETRO_MeshDrawContext ctx;
			float mins[3];
			int a;

			memset( &ctx, 0, sizeof( ctx ) );
			ctx.time = (float)gEngfuncs.GetClientTime();
			ctx.entity_frame = e->frame;
			ctx.rendercolor[0] = e->rendercolor[0];
			ctx.rendercolor[1] = e->rendercolor[1];
			ctx.rendercolor[2] = e->rendercolor[2];
			ctx.rendermode = e->rendermode;
			ctx.get_parm = GetParm;
			ctx.bind_textures = bind_tex;
			ctx.wave_scale = e->scale;
			ctx.effects = e->effects;
			ctx.is_brush = 1;
			ctx.water_pass = CSRETRO_WATER_BRUSH;
			ctx.water_alpha = 1.0f;
			if( AnglesActive( e->angles ) )
			{
				for( a = 0; a < 3; a++ )
					mins[a] = e->origin[a] - mod->radius;
			}
			else
			{
				mins[0] = e->origin[0] + mod->mins[0];
				mins[1] = e->origin[1] + mod->mins[1];
				mins[2] = e->origin[2] + mod->mins[2];
			}
			ctx.entity_mins[0] = mins[0];
			ctx.entity_mins[1] = mins[1];
			ctx.entity_mins[2] = mins[2];
			ctx.skip_fullbright = 1;
			{
				cl_entity_t local_ent;
				memset( (void *)&local_ent, 0, sizeof( local_ent ) );
				local_ent.origin[0] = e->origin[0];
				local_ent.origin[1] = e->origin[1];
				local_ent.origin[2] = e->origin[2];
				local_ent.angles[0] = e->angles[0];
				local_ent.angles[1] = e->angles[1];
				local_ent.angles[2] = e->angles[2];
				CSRETRO_DLight_PrepareMesh( mesh, &local_ent, 0 );
			}
			CSRETRO_BspMesh_Draw( mesh, &ctx );
			CSRETRO_Decal_DrawSurfaces( e->model, e->rendermode, 1, e->index );
			ctx.skip_base = 1;
			ctx.skip_fullbright = 0;
			CSRETRO_BspMesh_Draw( mesh, &ctx );
			ctx.skip_base = 0;
			ctx.skip_fullbright = 0;
			if( mesh->water_vert_count >= 3 )
			{
				s_bstats.turb_candidates += mesh->turb_surfaces;
				s_bstats.waterside_candidates += mesh->waterside_candidates;
				if( mesh->liquid_model )
					s_bstats.liquid_models++;
				CSRETRO_BspMesh_DrawWater( mesh, &ctx );
				s_bstats.turb_drawn += mesh->turb_surfaces;
				if( s_logged_water < 8 )
				{
					s_logged_water++;
					gEngfuncs.Con_Printf(
						"CS Retro: brush water model=%s index=%i origin=%.0f %.0f %.0f angles=%.1f %.1f %.1f scale=%.3f effects=%i rendermode=%i renderamt=%i turb=%i sides=%i liquid=%i\n",
						mod->name[0] ? mod->name : "?", e->index,
						e->origin[0], e->origin[1], e->origin[2],
						e->angles[0], e->angles[1], e->angles[2],
						e->scale, e->effects, e->rendermode, e->renderamt,
						mesh->turb_surfaces, mesh->waterside_candidates, mesh->liquid_model );
				}
			}
		}
		PopEntityTransform();
		RestoreDrawState();

		CSRETRO_Scene_NoteDrawn( CSRETRO_KIND_BRUSH );
		CSRETRO_Trans_NoteDrawn( i );
		if( opaque_only )
		{
			s_bstats.opaque_drawn++;
			CSRETRO_Scene_NoteBrush( 1 );
		}
		else
		{
			s_bstats.trans_drawn++;
			CSRETRO_Scene_NoteBrush( 0 );
		}
		drawn++;
		if( !s_logged_first && mod->name[0] )
		{
			s_logged_first = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: offscreen brush draw model=%s index=%i origin=%.0f %.0f %.0f angles=%.1f %.1f %.1f mode=%i amt=%i opaque=%i\n",
				mod->name, e->index,
				e->origin[0], e->origin[1], e->origin[2],
				e->angles[0], e->angles[1], e->angles[2],
				e->rendermode, e->renderamt, opaque_only );
		}
		if( s_logged_nonzero < 8 && ( AnglesActive( e->origin ) || AnglesActive( e->angles ) ) )
		{
			s_logged_nonzero++;
			gEngfuncs.Con_Printf(
				"CS Retro: brush placed index=%i model=%s origin=%.1f %.1f %.1f angles=%.1f %.1f %.1f mode=%i\n",
				e->index, mod->name[0] ? mod->name : "?",
				e->origin[0], e->origin[1], e->origin[2],
				e->angles[0], e->angles[1], e->angles[2],
				e->rendermode );
		}
	}

	if( gXRGL.Disable )
		gXRGL.Disable( GL_POLYGON_OFFSET_FILL );

	if( stats )
		CSRETRO_Scene_GetStats( stats );
	return drawn;
}

int CSRETRO_Brush_DrawPass( int opaque_only, CSRETRO_SceneStats *stats )
{
	return DrawBrushRange( -1, opaque_only, stats );
}

int CSRETRO_Brush_DrawOne( int scene_index, CSRETRO_SceneStats *stats )
{
	return DrawBrushRange( scene_index, 0, stats );
}
