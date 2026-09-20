// Classic Xash surface-dlight draw for the offscreen FBO.
// Provenance: engine/ref/gl/gl_rsurf.c R_BuildLightMap / R_AddDynamicLights
// and R_MarkLights end-check. Engine helper evaluates; this file owns the
// transient atlas and lighting selection. No R_PushDlights, no live writes,
// no tr.dlightTexture, no second static BSP atlas, no blob/vertex lights.
#include "render_dlight.h"
#include "render_backend.h"
#include "render_bsp_mesh.h"
#include "render_xash_brush.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "render_api.h"
#include "dlight.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define CSRETRO_DLIGHT_PATCH_MAX 256
#define CSRETRO_SURF_MUT_MAX 16
#define CSRETRO_LM_TEMP 132
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define XR_PARM_TEX_WIDTH 1
#define XR_PARM_TEX_LIGHTMAP 7
#define XR_PARM_TEX_TEXNUM 9
#define XR_TF_CLAMP (1 << 11)
#define XR_TF_NOMIPMAP (1 << 12)

typedef void ( *PFN_TEXSUB )( unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void * );
typedef void ( *PFN_TEXPARAMI )( unsigned int, unsigned int, int );

typedef struct CSRETRO_DLightSnap_s
{
	float origin[3];
	float radius;
	unsigned char color[3];
	float die;
	float decay;
	float minlight;
	int key;
	int dark;
} CSRETRO_DLightSnap;

typedef struct SurfMut_s
{
	void *model;
	int index;
	int dlightframe;
	int dlightbits;
} SurfMut;

static CSRETRO_DLightSnap s_before[CSRETRO_MAX_DLIGHTS];
static CSRETRO_DLightSnap s_live[CSRETRO_MAX_DLIGHTS];
static int s_active = 0;
static SurfMut s_surf_before[CSRETRO_SURF_MUT_MAX];
static int s_surf_mut_n = 0;
static CSRETRO_DLightPatch s_patches[CSRETRO_DLIGHT_PATCH_MAX];
static int s_patch_n = 0;
static CSRETRO_DLightStats s_stats;
static int s_inventory_logged = 0;
static int s_world_proof_logged = 0;
static int s_brush_proof_logged = 0;
static unsigned int s_crc_static = 0;
static int s_had_static_crc = 0;
static int s_mesh_serial_before = 0;
static unsigned int s_mesh_hash_before = 0;
static int s_atlas_texnum = 0;
static int s_atlas_size = 128;
static int s_block_size = 128;
static int s_shelf_x = 0;
static int s_shelf_y = 0;
static int s_shelf_row_h = 0;
static unsigned char s_lm_temp[CSRETRO_LM_TEMP * CSRETRO_LM_TEMP * 4];
static PFN_TEXSUB pglTexSubImage2D = NULL;
static int s_procs = 0;
static int s_r_dyn_off_seen = 0;
static int s_r_dyn_on_seen = 0;
static int s_expired_logged = 0;
static int s_saw_patches = 0;
static int s_rdyn0_logged = 0;
static int s_rdyn1_logged = 0;
static int s_scan_zero = 0;
static int s_scan_live = 0;

static void LoadProcs( void )
{
	if( s_procs )
		return;
	if( gRenderAPI.GL_GetProcAddress )
		pglTexSubImage2D = (PFN_TEXSUB)gRenderAPI.GL_GetProcAddress( "glTexSubImage2D" );
	s_procs = 1;
}

static void CopyDlight( CSRETRO_DLightSnap *dst, const dlight_t *src )
{
	memset( dst, 0, sizeof( *dst ) );
	if( !src )
		return;
	dst->origin[0] = src->origin[0];
	dst->origin[1] = src->origin[1];
	dst->origin[2] = src->origin[2];
	dst->radius = src->radius;
	dst->color[0] = src->color.r;
	dst->color[1] = src->color.g;
	dst->color[2] = src->color.b;
	dst->die = src->die;
	dst->decay = src->decay;
	dst->minlight = src->minlight;
	dst->key = src->key;
	dst->dark = src->dark ? 1 : 0;
}

static int SnapEqual( const CSRETRO_DLightSnap *a, const CSRETRO_DLightSnap *b )
{
	return memcmp( a, b, sizeof( *a ) ) == 0;
}

static int DlightActive( const CSRETRO_DLightSnap *s, float now )
{
	return s->die >= now && s->radius != 0.0f;
}

static void SnapshotLights( CSRETRO_DLightSnap *dst, int *active, float now )
{
	int i;

	*active = 0;
	memset( dst, 0, sizeof( CSRETRO_DLightSnap ) * CSRETRO_MAX_DLIGHTS );
	if( !gRenderAPI.GetDynamicLight )
		return;
	for( i = 0; i < CSRETRO_MAX_DLIGHTS; i++ )
	{
		const dlight_t *dl = gRenderAPI.GetDynamicLight( i );
		if( !dl )
			break;
		CopyDlight( &dst[i], dl );
		if( DlightActive( &dst[i], now ) )
			( *active )++;
	}
}

static void CaptureSurfMut( void *model, int max_n )
{
	const xr_model_t *mod = (const xr_model_t *)model;
	int i, n;

	s_surf_mut_n = 0;
	if( !mod || !mod->surfaces || mod->numsurfaces <= 0 )
		return;
	n = mod->numsurfaces;
	for( i = 0; i < n && s_surf_mut_n < max_n; i++ )
	{
		const xr_msurface_t *surf = &mod->surfaces[i];
		if( surf->flags & ( XR_SURF_DRAWTILED | XR_SURF_DRAWSKY ) )
			continue;
		s_surf_before[s_surf_mut_n].model = model;
		s_surf_before[s_surf_mut_n].index = i;
		s_surf_before[s_surf_mut_n].dlightframe = surf->dlightframe;
		s_surf_before[s_surf_mut_n].dlightbits = surf->dlightbits;
		s_surf_mut_n++;
	}
}

static int SurfMutated( void )
{
	int i;
	for( i = 0; i < s_surf_mut_n; i++ )
	{
		const xr_model_t *mod = (const xr_model_t *)s_surf_before[i].model;
		const xr_msurface_t *surf;
		if( !mod || !mod->surfaces )
			continue;
		if( s_surf_before[i].index < 0 || s_surf_before[i].index >= mod->numsurfaces )
			continue;
		surf = &mod->surfaces[s_surf_before[i].index];
		if( surf->dlightframe != s_surf_before[i].dlightframe || surf->dlightbits != s_surf_before[i].dlightbits )
			return 1;
	}
	return 0;
}

static void ResetShelf( void )
{
	s_shelf_x = 0;
	s_shelf_y = 0;
	s_shelf_row_h = 0;
}

static int AllocShelf( int w, int h, int *x, int *y )
{
	if( w <= 0 || h <= 0 || w > s_atlas_size || h > s_atlas_size )
		return 0;
	if( s_shelf_x + w > s_atlas_size )
	{
		s_shelf_y += s_shelf_row_h;
		s_shelf_x = 0;
		s_shelf_row_h = 0;
	}
	if( s_shelf_y + h > s_atlas_size )
		return 0;
	*x = s_shelf_x;
	*y = s_shelf_y;
	s_shelf_x += w;
	if( h > s_shelf_row_h )
		s_shelf_row_h = h;
	return 1;
}

static int EnsureAtlas( void )
{
	int lm0;
	unsigned char *black;
	int bytes;

	LoadProcs();
	if( gRenderAPI.RenderGetParm )
	{
		lm0 = (int)gRenderAPI.RenderGetParm( XR_PARM_TEX_LIGHTMAP, 0 );
		if( lm0 > 0 )
		{
			int w = (int)gRenderAPI.RenderGetParm( XR_PARM_TEX_WIDTH, lm0 );
			if( w >= 64 && w <= 1024 )
				s_block_size = w;
		}
	}
	s_atlas_size = s_block_size;
	if( s_atlas_size < 128 )
		s_atlas_size = 128;
	if( s_atlas_texnum )
		return 1;
	if( !gRenderAPI.GL_CreateTexture )
		return 0;
	bytes = s_atlas_size * s_atlas_size * 4;
	black = (unsigned char *)malloc( (size_t)bytes );
	if( !black )
		return 0;
	memset( black, 0, (size_t)bytes );
	s_atlas_texnum = gRenderAPI.GL_CreateTexture( "*csretro_dlight_atlas", s_atlas_size, s_atlas_size, black,
		(texFlags_t)( XR_TF_NOMIPMAP | XR_TF_CLAMP ) );
	free( black );
	return s_atlas_texnum > 0;
}

static void DestroyAtlas( void )
{
	if( s_atlas_texnum && gRenderAPI.GL_FreeTexture )
		gRenderAPI.GL_FreeTexture( (unsigned int)s_atlas_texnum );
	s_atlas_texnum = 0;
}

static void UploadPatch( int x, int y, int w, int h, const unsigned char *rgba )
{
	int unpack = 4;
	int bind0 = 0;
	int active = GL_TEXTURE0;
	unsigned int tmu0_bind = 0;

	if( !pglTexSubImage2D || !s_atlas_texnum )
		return;
	if( gXRGL.GetIntegerv )
	{
		gXRGL.GetIntegerv( GL_UNPACK_ALIGNMENT, &unpack );
		gXRGL.GetIntegerv( GL_ACTIVE_TEXTURE, &active );
		gXRGL.GetIntegerv( GL_TEXTURE_BINDING_2D, &bind0 );
	}
	if( gRenderAPI.GL_Bind )
		gRenderAPI.GL_Bind( 0, (unsigned int)s_atlas_texnum );
	if( gXRGL.PixelStorei )
		gXRGL.PixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	pglTexSubImage2D( GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
	if( gXRGL.PixelStorei )
		gXRGL.PixelStorei( GL_UNPACK_ALIGNMENT, unpack );
	if( gXRGL.ActiveTexture )
		gXRGL.ActiveTexture( (unsigned int)active );
	if( gXRGL.BindTexture )
		gXRGL.BindTexture( GL_TEXTURE_2D, (unsigned int)bind0 );
	(void)tmu0_bind;
}

void CSRETRO_DLight_Init( void )
{
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_inventory_logged = 0;
	s_world_proof_logged = 0;
	s_brush_proof_logged = 0;
	s_had_static_crc = 0;
	s_procs = 0;
	s_r_dyn_off_seen = 0;
	s_r_dyn_on_seen = 0;
	s_expired_logged = 0;
	s_saw_patches = 0;
}

void CSRETRO_DLight_Shutdown( void )
{
	DestroyAtlas();
	s_patch_n = 0;
	s_procs = 0;
	pglTexSubImage2D = NULL;
}

void CSRETRO_DLight_OnNewMap( void )
{
	s_patch_n = 0;
	ResetShelf();
	s_surf_mut_n = 0;
	s_inventory_logged = 0;
	s_world_proof_logged = 0;
	s_brush_proof_logged = 0;
	s_had_static_crc = 0;
	s_expired_logged = 0;
	s_saw_patches = 0;
	s_scan_live = 0;
	memset( &s_stats, 0, sizeof( s_stats ) );
}

void CSRETRO_DLight_OnVidInit( void )
{
	DestroyAtlas();
	s_procs = 0;
	pglTexSubImage2D = NULL;
	s_patch_n = 0;
	ResetShelf();
}

void CSRETRO_DLight_BeginOffscreen( void )
{
	float now = (float)gEngfuncs.GetClientTime();
	int i;

	EnsureAtlas();
	ResetShelf();
	s_patch_n = 0;
	s_stats.affected_world = 0;
	s_stats.affected_brush = 0;
	s_stats.patches = 0;
	s_stats.transform_used = 0;
	s_stats.helper_ok = gRenderAPI.BuildSurfaceLightmapReadOnly ? 1 : 0;
	s_stats.r_dynamic = ( CVAR_GET_FLOAT( "r_dynamic" ) != 0.0f ) ? 1 : 0;
	s_stats.gl_restore = 1;
	s_mesh_serial_before = CSRETRO_BspMesh_BuildCount();

	SnapshotLights( s_before, &s_active, now );
	s_stats.active = s_active;
	{
		int n_radius = 0;
		int i;

		for( i = 0; i < CSRETRO_MAX_DLIGHTS; i++ )
		{
			if( s_before[i].radius != 0.0f )
				n_radius++;
		}
		if( !s_scan_zero )
		{
			s_scan_zero = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: dlight scan now=%.3f GetDynamicLight=%i helper=%i n_radius=%i n_active=%i r_dynamic=%i\n",
				now, gRenderAPI.GetDynamicLight ? 1 : 0, s_stats.helper_ok,
				n_radius, s_active, s_stats.r_dynamic );
		}
		if( n_radius > 0 && !s_scan_live )
		{
			s_scan_live = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: dlight scan live now=%.3f n_radius=%i n_active=%i helper=%i r_dynamic=%i\n",
				now, n_radius, s_active, s_stats.helper_ok, s_stats.r_dynamic );
			for( i = 0; i < CSRETRO_MAX_DLIGHTS; i++ )
			{
				if( s_before[i].radius == 0.0f )
					continue;
				gEngfuncs.Con_Printf(
					"CS Retro: dlight slot i=%i key=%i radius=%.1f die=%.3f now=%.3f color=%i %i %i dark=%i\n",
					i, s_before[i].key, s_before[i].radius, s_before[i].die, now,
					s_before[i].color[0], s_before[i].color[1], s_before[i].color[2],
					s_before[i].dark );
			}
		}
	}
	if( s_active > 0 )
	{
		for( i = 0; i < CSRETRO_MAX_DLIGHTS; i++ )
		{
			if( DlightActive( &s_before[i], now ) )
			{
				s_stats.key0 = s_before[i].key;
				s_stats.radius0 = s_before[i].radius;
				s_stats.color0[0] = s_before[i].color[0];
				s_stats.color0[1] = s_before[i].color[1];
				s_stats.color0[2] = s_before[i].color[2];
				s_stats.die0 = s_before[i].die;
				break;
			}
		}
	}
}

void CSRETRO_DLight_PrepareMesh( const CSRETRO_BspMesh *mesh, const cl_entity_t *entity_or_null, int is_world )
{
	int i;
	int r_dyn;
	cl_entity_t local;
	const cl_entity_t *ent;

	if( !mesh || !mesh->spans || mesh->span_count <= 0 )
		return;
	if( is_world && mesh->model )
		CaptureSurfMut( mesh->model, CSRETRO_SURF_MUT_MAX );
	s_mesh_hash_before = mesh->geom_hash;

	r_dyn = ( CVAR_GET_FLOAT( "r_dynamic" ) != 0.0f ) ? 1 : 0;
	s_stats.r_dynamic = r_dyn;
	if( !r_dyn || !gRenderAPI.BuildSurfaceLightmapReadOnly || s_active <= 0 )
		return;
	if( !EnsureAtlas() )
		return;

	ent = NULL;
	if( entity_or_null )
	{
		memset( (void *)&local, 0, sizeof( local ) );
		local.origin[0] = entity_or_null->origin[0];
		local.origin[1] = entity_or_null->origin[1];
		local.origin[2] = entity_or_null->origin[2];
		local.angles[0] = entity_or_null->angles[0];
		local.angles[1] = entity_or_null->angles[1];
		local.angles[2] = entity_or_null->angles[2];
		ent = &local;
		if( local.origin[0] != 0.0f || local.origin[1] != 0.0f || local.origin[2] != 0.0f
			|| local.angles[0] != 0.0f || local.angles[1] != 0.0f || local.angles[2] != 0.0f )
			s_stats.transform_used = 1;
	}

	for( i = 0; i < mesh->span_count; i++ )
	{
		const CSRETRO_SurfaceSpan *span = &mesh->spans[i];
		const xr_model_t *mod = (const xr_model_t *)mesh->model;
		const xr_msurface_t *surf;
		int w = 0, h = 0, dyn = 0, ax = 0, ay = 0, stride;
		int ok;

		if( !mod || !mod->surfaces )
			break;
		if( span->surface_index < 0 || span->surface_index >= mod->numsurfaces )
			continue;
		if( span->flags & ( XR_SURF_DRAWTILED | XR_SURF_DRAWTURB | XR_SURF_DRAWSKY ) )
			continue;
		surf = &mod->surfaces[span->surface_index];
		stride = 0;
		ok = gRenderAPI.BuildSurfaceLightmapReadOnly(
			(const struct msurface_s *)surf, ent,
			s_lm_temp, stride, (int)sizeof( s_lm_temp ),
			&w, &h, &dyn );
		if( !ok || !dyn || w <= 0 || h <= 0 )
			continue;
		if( w > CSRETRO_LM_TEMP || h > CSRETRO_LM_TEMP )
			continue;
		if( s_patch_n >= CSRETRO_DLIGHT_PATCH_MAX )
			break;
		if( !AllocShelf( w, h, &ax, &ay ) )
			continue;
		UploadPatch( ax, ay, w, h, s_lm_temp );
		s_patches[s_patch_n].model = mesh->model;
		s_patches[s_patch_n].surface_index = span->surface_index;
		s_patches[s_patch_n].atlas_x = ax;
		s_patches[s_patch_n].atlas_y = ay;
		s_patches[s_patch_n].width = w;
		s_patches[s_patch_n].height = h;
		s_patches[s_patch_n].light_s = span->light_s;
		s_patches[s_patch_n].light_t = span->light_t;
		s_patches[s_patch_n].used = 1;
		s_patch_n++;
		if( is_world )
			s_stats.affected_world++;
		else
			s_stats.affected_brush++;
	}
	s_stats.patches = s_patch_n;
	if( s_patch_n > 0 )
		s_saw_patches = 1;
}

void CSRETRO_DLight_EndOffscreen( void )
{
	float now = (float)gEngfuncs.GetClientTime();
	int i, after_active = 0, mutate = 0;

	SnapshotLights( s_live, &after_active, now );
	for( i = 0; i < CSRETRO_MAX_DLIGHTS; i++ )
	{
		if( !SnapEqual( &s_before[i], &s_live[i] ) )
			mutate = 1;
	}
	s_stats.dlight_mutate = mutate;
	s_stats.surface_mutate = SurfMutated();
	s_stats.mesh_mutate = ( CSRETRO_BspMesh_BuildCount() != s_mesh_serial_before ) ? 1 : 0;
	s_stats.active = after_active > s_active ? after_active : s_active;
	s_stats.patches = s_patch_n;

	if( s_stats.r_dynamic == 0 )
		s_stats.r_dynamic_off_patches = s_patch_n;
	else
		s_stats.r_dynamic_on_patches = s_patch_n;

	if( s_stats.r_dynamic == 0 && s_patch_n == 0 )
		s_r_dyn_off_seen = 1;
	if( s_stats.r_dynamic == 1 && s_patch_n > 0 )
		s_r_dyn_on_seen = 1;
	if( s_saw_patches && s_patch_n == 0 && s_stats.r_dynamic == 1 && s_active == 0 && !s_expired_logged )
	{
		s_stats.expired_static = 1;
		s_expired_logged = 1;
	}

	static int s_gate_logged = 0;
	static int s_pending_logged = 0;

	if( !s_inventory_logged && s_stats.active > 0 )
	{
		s_inventory_logged = 1;
		s_stats.inventory_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: dlight inventory active_dlights=%i key=%i radius=%.1f color=%i %i %i die=%.2f affected_world_surfaces=%i affected_brush_surfaces=%i patches=%i helper=%i r_dynamic=%i transform=%i\n",
			s_stats.active, s_stats.key0, s_stats.radius0,
			s_stats.color0[0], s_stats.color0[1], s_stats.color0[2],
			s_stats.die0, s_stats.affected_world, s_stats.affected_brush,
			s_stats.patches, s_stats.helper_ok, s_stats.r_dynamic, s_stats.transform_used );
	}

	if( !s_world_proof_logged && s_stats.affected_world > 0 && s_stats.patches > 0 && s_stats.pixel_differ )
	{
		s_world_proof_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: world dlight pixelproof before=%08x after=%08x differ=1 active=%i affected=%i patches=%i dlight_mutate=%i surface_mutate=%i mesh_mutate=%i\n",
			s_stats.crc_before, s_stats.crc_after, s_stats.active,
			s_stats.affected_world, s_stats.patches,
			s_stats.dlight_mutate, s_stats.surface_mutate, s_stats.mesh_mutate );
	}
	else if( !s_pending_logged && s_stats.affected_world > 0 && s_stats.patches > 0 )
	{
		s_pending_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: world dlight patches=%i affected=%i active=%i dlight_mutate=%i surface_mutate=%i mesh_mutate=%i helper=%i\n",
			s_stats.patches, s_stats.affected_world, s_stats.active,
			s_stats.dlight_mutate, s_stats.surface_mutate, s_stats.mesh_mutate, s_stats.helper_ok );
	}

	if( !s_brush_proof_logged && s_stats.affected_brush > 0 && s_stats.patches > 0 )
	{
		s_brush_proof_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: brush dlight affected=%i transform_used=%i patches=%i dlight_mutate=%i surface_mutate=%i mesh_mutate=%i\n",
			s_stats.affected_brush, s_stats.transform_used, s_stats.patches,
			s_stats.dlight_mutate, s_stats.surface_mutate, s_stats.mesh_mutate );
	}

	if( s_expired_logged == 1 && s_stats.expired_static )
	{
		gEngfuncs.Con_Printf( "CS Retro: dlight expiration static LM path restored patches=0 mesh_mutate=%i\n", s_stats.mesh_mutate );
		s_expired_logged = 2;
	}

	if( !s_gate_logged && ( s_stats.active > 0 || s_stats.patches > 0 ) )
	{
		s_gate_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: dlight gate active=%i world=%i brush=%i patches=%i dlight_mutate=%i surface_mutate=%i mesh_mutate=%i r_dynamic=%i gl_restore=%i helper=%i\n",
			s_stats.active, s_stats.affected_world, s_stats.affected_brush, s_stats.patches,
			s_stats.dlight_mutate, s_stats.surface_mutate, s_stats.mesh_mutate,
			s_stats.r_dynamic, s_stats.gl_restore, s_stats.helper_ok );
	}
	if( !s_rdyn0_logged && s_stats.r_dynamic == 0 && s_patch_n == 0 )
	{
		s_rdyn0_logged = 1;
		gEngfuncs.Con_Printf( "CS Retro: dlight r_dynamic 0 disables patches=0\n" );
	}
	if( !s_rdyn1_logged && s_stats.r_dynamic == 1 && s_patch_n > 0 )
	{
		s_rdyn1_logged = 1;
		gEngfuncs.Con_Printf( "CS Retro: dlight r_dynamic 1 enables patches=%i\n", s_patch_n );
	}
}

void CSRETRO_DLight_NoteWorldCrc( unsigned int crc, int patches )
{
	if( patches <= 0 )
	{
		s_crc_static = crc;
		s_had_static_crc = 1;
		s_stats.crc_before = crc;
		return;
	}
	s_stats.crc_after = crc;
	if( s_had_static_crc && s_crc_static != crc )
	{
		s_stats.crc_before = s_crc_static;
		s_stats.pixel_differ = 1;
	}
}

int CSRETRO_DLight_Lookup( void *model, int surface_index, CSRETRO_DLightPatch *out )
{
	int i;
	for( i = 0; i < s_patch_n; i++ )
	{
		if( s_patches[i].used && s_patches[i].model == model && s_patches[i].surface_index == surface_index )
		{
			if( out )
				*out = s_patches[i];
			return 1;
		}
	}
	return 0;
}

unsigned int CSRETRO_DLight_AtlasTexnum( void )
{
	return (unsigned int)s_atlas_texnum;
}

int CSRETRO_DLight_BlockSize( void )
{
	return s_block_size > 0 ? s_block_size : 128;
}

int CSRETRO_DLight_AtlasSize( void )
{
	return s_atlas_size > 0 ? s_atlas_size : 128;
}

void CSRETRO_DLight_GetStats( CSRETRO_DLightStats *out )
{
	if( out )
		*out = s_stats;
}

int CSRETRO_DLight_PatchCount( void )
{
	return s_patch_n;
}
