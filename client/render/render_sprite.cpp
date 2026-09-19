// Offscreen sprite draw. Provenance: SNMetamorph/PrimeXT 46fb05b
// client/render/gl_sprite.cpp (GetSpriteFrame, DrawSpriteQuad, orientation,
// rendermode/color/amt). Adapted to CS Retro copies + Xash msprite view.
// Does not steal entities. Does not take over the visible frame.
#include "render_sprite.h"
#include "render_scene.h"
#include "render_backend.h"
#include "render_xash_brush.h"
#include "render_xash_sprite.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GL_TRIANGLES 0x0004
#define GL_TEXTURE_2D 0x0DE1
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_ALPHA_TEST 0x0BC0
#define GL_CULL_FACE 0x0B44
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_GREATER 0x0204
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_FALSE 0
#define GL_TRUE 1

static void XR_AngleVectors( const float *angles, float *forward, float *right, float *up )
{
	float pitch = angles[0] * (float)M_PI / 180.0f;
	float yaw = angles[1] * (float)M_PI / 180.0f;
	float roll = angles[2] * (float)M_PI / 180.0f;
	float sp = sinf( pitch ), cp = cosf( pitch );
	float sy = sinf( yaw ), cy = cosf( yaw );
	float sr = sinf( roll ), cr = cosf( roll );

	if( forward )
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if( right )
	{
		right[0] = -sr * sp * cy + cr * sy;
		right[1] = -sr * sp * sy - cr * cy;
		right[2] = -sr * cp;
	}
	if( up )
	{
		up[0] = cr * sp * cy + sr * sy;
		up[1] = cr * sp * sy - sr * cy;
		up[2] = cr * cp;
	}
}

static void Normalize3( float *v )
{
	float len = sqrtf( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if( len <= 0.0001f )
		return;
	v[0] /= len;
	v[1] /= len;
	v[2] /= len;
}

static const xr_mspriteframe_t *GetFrame( const xr_msprite_t *hdr, int frame, float yaw, float cl_time )
{
	const xr_mspriteframedesc_t *desc;
	int i;

	(void)yaw;
	if( !hdr || hdr->numframes <= 0 )
		return NULL;
	if( frame < 0 )
		frame = 0;
	if( frame >= hdr->numframes )
		frame = hdr->numframes - 1;

	desc = &hdr->frames[frame];
	if( desc->type == XR_SPR_SINGLE )
		return (const xr_mspriteframe_t *)desc->frameptr;
	if( desc->type == XR_SPR_GROUP )
	{
		const xr_mspritegroup_t *group = (const xr_mspritegroup_t *)desc->frameptr;
		float full, target;
		if( !group || group->numframes <= 0 || !group->intervals )
			return NULL;
		full = group->intervals[group->numframes - 1];
		if( full <= 0.0f )
			return group->frames[0];
		target = cl_time - (float)( (int)( cl_time / full ) ) * full;
		for( i = 0; i < group->numframes - 1; i++ )
		{
			if( group->intervals[i] > target )
				break;
		}
		return group->frames[i];
	}
	// SPR_ANGLED: not used by CS smoke/HE tents. Documented missing.
	return NULL;
}

static void ApplyMode( int rendermode )
{
	if( !gXRGL.Enable )
		return;
	switch( rendermode )
	{
	case kRenderTransAlpha:
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		gXRGL.Enable( GL_BLEND );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		gXRGL.Enable( GL_BLEND );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		break;
	case kRenderGlow:
	case kRenderWorldGlow:
		gXRGL.Disable( GL_DEPTH_TEST );
		// fallthrough
	case kRenderTransAdd:
		gXRGL.Enable( GL_BLEND );
		if( gXRGL.BlendFunc )
			gXRGL.BlendFunc( GL_SRC_ALPHA, GL_ONE );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_FALSE );
		break;
	case kRenderNormal:
	default:
		gXRGL.Disable( GL_BLEND );
		if( gXRGL.DepthMask )
			gXRGL.DepthMask( GL_TRUE );
		break;
	}
	gXRGL.Enable( GL_ALPHA_TEST );
	if( gXRGL.AlphaFunc )
		gXRGL.AlphaFunc( GL_GREATER, 0.0f );
	if( gXRGL.TexEnvi )
		gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}

static void EmitVert( const float *org, const float *right, const float *up,
	float u, float v, float s, float t, float scale )
{
	float point[3];
	int i;
	for( i = 0; i < 3; i++ )
		point[i] = org[i] + up[i] * ( v * scale ) + right[i] * ( u * scale );
	if( gXRGL.TexCoord2f )
		gXRGL.TexCoord2f( s, t );
	gXRGL.Vertex3f( point[0], point[1], point[2] );
}

static void DrawQuad( const xr_mspriteframe_t *frame, const float *org, const float *right, const float *up, float scale )
{
	if( !gXRGL.Begin || !frame )
		return;
	// Two triangles — same immediate-mode path as the world pass (no GL_QUADS).
	gXRGL.Begin( GL_TRIANGLES );
	EmitVert( org, right, up, frame->left, frame->down, 0.0f, 1.0f, scale );
	EmitVert( org, right, up, frame->left, frame->up, 0.0f, 0.0f, scale );
	EmitVert( org, right, up, frame->right, frame->up, 1.0f, 0.0f, scale );
	EmitVert( org, right, up, frame->left, frame->down, 0.0f, 1.0f, scale );
	EmitVert( org, right, up, frame->right, frame->up, 1.0f, 0.0f, scale );
	EmitVert( org, right, up, frame->right, frame->down, 1.0f, 1.0f, scale );
	gXRGL.End();
}

static int DrawOne( const CSRETRO_EntCopy *e, const float *vieworg, const float *vright, const float *vup, const float *vforward, float cl_time )
{
	const xr_model_t *mod;
	const xr_msprite_t *hdr;
	const xr_mspriteframe_t *frame;
	float origin[3], right[3], up[3], color[3];
	float scale, alpha;
	int type, i;

	if( !e || !e->model )
		return 0;
	mod = (const xr_model_t *)e->model;
	if( mod->type != XR_MOD_SPRITE || !mod->cache )
		return 0;
	hdr = (const xr_msprite_t *)mod->cache;
	frame = GetFrame( hdr, (int)e->frame, e->angles[1], cl_time );
	if( !frame || frame->gl_texturenum <= 0 )
		return 0;

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];
	scale = e->scale > 0.0f ? e->scale : 1.0f;
	alpha = e->renderamt > 0 ? (float)e->renderamt * ( 1.0f / 255.0f ) : 1.0f;

	if( e->rendercolor[0] || e->rendercolor[1] || e->rendercolor[2] )
	{
		color[0] = e->rendercolor[0] * ( 1.0f / 255.0f );
		color[1] = e->rendercolor[1] * ( 1.0f / 255.0f );
		color[2] = e->rendercolor[2] * ( 1.0f / 255.0f );
	}
	else
	{
		color[0] = color[1] = color[2] = 1.0f;
	}

	type = hdr->type;
	if( e->angles[2] != 0.0f && type == XR_SPR_FWD_PARALLEL )
		type = XR_SPR_FWD_PARALLEL_ORIENTED;

	switch( type )
	{
	case XR_SPR_ORIENTED:
	{
		float fwd[3];
		XR_AngleVectors( e->angles, fwd, right, up );
		for( i = 0; i < 3; i++ )
			origin[i] -= fwd[i] * 0.01f;
		break;
	}
	case XR_SPR_FACING_UPRIGHT:
		right[0] = origin[1] - vieworg[1];
		right[1] = -( origin[0] - vieworg[0] );
		right[2] = 0.0f;
		up[0] = up[1] = 0.0f;
		up[2] = 1.0f;
		Normalize3( right );
		break;
	case XR_SPR_FWD_PARALLEL_UPRIGHT:
		if( vforward[2] > 0.999848f || vforward[2] < -0.999848f )
			return 0;
		right[0] = vforward[1];
		right[1] = -vforward[0];
		right[2] = 0.0f;
		up[0] = up[1] = 0.0f;
		up[2] = 1.0f;
		Normalize3( right );
		break;
	case XR_SPR_FWD_PARALLEL_ORIENTED:
	{
		float angle = e->angles[2] * ( (float)M_PI * 2.0f / 360.0f );
		float sr = sinf( angle ), cr = cosf( angle );
		for( i = 0; i < 3; i++ )
		{
			right[i] = vright[i] * cr + vup[i] * sr;
			up[i] = vright[i] * -sr + vup[i] * cr;
		}
		break;
	}
	case XR_SPR_FWD_PARALLEL:
	default:
		right[0] = vright[0];
		right[1] = vright[1];
		right[2] = vright[2];
		up[0] = vup[0];
		up[1] = vup[1];
		up[2] = vup[2];
		break;
	}

	if( hdr->facecull == XR_SPR_CULL_NONE && gXRGL.Disable )
		gXRGL.Disable( GL_CULL_FACE );
	else if( gXRGL.Enable )
		gXRGL.Enable( GL_CULL_FACE );

	ApplyMode( e->rendermode );
	if( gXRGL.Color4f )
		gXRGL.Color4f( color[0], color[1], color[2], alpha );
	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );
	CSRETRO_Backend_BindTexture( 0, (unsigned int)frame->gl_texturenum );
	{
		static int s_once = 0;
		if( !s_once && e->kind == CSRETRO_KIND_TENT_SPRITE )
		{
			s_once = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: sprite frame tex=%i %ix%i L%.1f R%.1f U%.1f D%.1f scale=%.2f origin=%.0f %.0f %.0f mode=%i\n",
				frame->gl_texturenum, frame->width, frame->height,
				frame->left, frame->right, frame->up, frame->down,
				scale, origin[0], origin[1], origin[2], e->rendermode );
		}
	}
	DrawQuad( frame, origin, right, up, scale );

	if( gXRGL.Enable )
	{
		gXRGL.Enable( GL_DEPTH_TEST );
		gXRGL.Enable( GL_CULL_FACE );
	}
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( GL_TRUE );
	return 1;
}

int CSRETRO_Sprite_DrawList( const float *vieworg, const float *viewangles, CSRETRO_SceneStats *stats )
{
	float vforward[3], vright[3], vup[3];
	float cl_time;
	int i, n, drawn = 0;
	const CSRETRO_EntCopy *e;

	if( !vieworg || !viewangles )
		return 0;
	if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( 0x84C1 ); // GL_TEXTURE1
		gXRGL.Disable( GL_TEXTURE_2D );
		gXRGL.ActiveTexture( 0x84C0 ); // GL_TEXTURE0
	}
	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );
	CSRETRO_Backend_CleanupTextures();
	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );
	XR_AngleVectors( viewangles, vforward, vright, vup );
	cl_time = (float)gEngfuncs.GetClientTime();
	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		e = CSRETRO_Scene_Get( i );
		if( !e )
			continue;
		if( e->kind != CSRETRO_KIND_TENT_SPRITE && e->kind != CSRETRO_KIND_NORMAL_SPRITE )
			continue;
		if( DrawOne( e, vieworg, vright, vup, vforward, cl_time ) )
		{
			CSRETRO_Scene_NoteDrawn( e->kind );
			drawn++;
		}
	}
	if( stats )
		CSRETRO_Scene_GetStats( stats );
	if( gXRGL.Disable )
	{
		gXRGL.Disable( GL_BLEND );
		gXRGL.Disable( GL_ALPHA_TEST );
	}
	return drawn;
}
