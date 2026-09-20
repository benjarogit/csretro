// Offscreen sprite draw. Provenance: SNMetamorph/PrimeXT 46fb05b
// client/render/gl_sprite.cpp (GetSpriteFrame, DrawSpriteQuad, orientation,
// rendermode/color/amt). Adapted to CS Retro copies + Xash msprite view.
// Does not steal entities. Does not take over the visible frame.
#include "render_sprite.h"
#include "render_scene.h"
#include "render_backend.h"
#include "render_xash_brush.h"
#include "render_xash_sprite.h"
#include "render_trans.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cl_entity.h"
#include "triangleapi.h"

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
#define GL_EQUAL 0x0202
#define GL_SRC_COLOR 0x0300
#define GL_ZERO 0
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_DEPTH_FUNC 0x0B74
#define GL_BLEND_SRC 0x0BE1
#define GL_BLEND_DST 0x0BE0
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_CURRENT_COLOR 0x0B00
#define GL_CULL_FACE_MODE 0x0B45

#define XR_Q_RINT(x) ( (x) < 0.0f ? ((int)((x) - 0.5f)) : ((int)((x) + 0.5f)) )

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

typedef struct SpriteFrameResult_s
{
	const xr_mspriteframe_t *old_frame;
	const xr_mspriteframe_t *current_frame;
	float lerp;
} SpriteFrameResult;

static int s_dump_left = 8;
static int s_nodepth = 0;
static int s_tent_dumped = 0;
static int s_lerp_candidates;
static int s_lerp_drawn;
static int s_lerp_old_ne;
static int s_light_candidates;
static int s_light_at_point;
static int s_light_pass;
static int s_angled_path;
static int s_sprite_logged;
static int s_live_logged;
static int s_lerp_proof_logged;
static int s_light_proof_logged;
static int s_angled_proof_logged;

static int ClampFrameIndex( const xr_msprite_t *hdr, int frame )
{
	if( frame < 0 )
		return 0;
	if( hdr && frame >= hdr->numframes )
		return hdr->numframes - 1;
	return frame;
}

static int AngleFrame( float view_yaw, float entity_yaw )
{
	float x = ( view_yaw - entity_yaw + 45.0f ) / 360.0f * 8.0f;
	return ( XR_Q_RINT( x ) - 4 ) & 7;
}

static const xr_mspriteframe_t *GroupFrameAt( const xr_mspritegroup_t *group, int index )
{
	if( !group || group->numframes <= 0 )
		return NULL;
	if( index < 0 )
		index = 0;
	if( index >= group->numframes )
		index = group->numframes - 1;
	return group->frames[index];
}

static const xr_mspriteframe_t *StaticFrame( const xr_msprite_t *hdr, int frame, float view_yaw, float entity_yaw, float cl_time )
{
	const xr_mspriteframedesc_t *desc;
	int i;

	if( !hdr || hdr->numframes <= 0 )
		return NULL;
	frame = ClampFrameIndex( hdr, frame );
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
	if( desc->type == XR_SPR_ANGLED )
	{
		const xr_mspritegroup_t *group = (const xr_mspritegroup_t *)desc->frameptr;
		s_angled_path++;
		return GroupFrameAt( group, AngleFrame( view_yaw, entity_yaw ) );
	}
	return NULL;
}

static int SpriteAllowLerping( const cl_entity_t *ent, const xr_msprite_t *hdr )
{
	if( !ent || !hdr )
		return 0;
	if( gEngfuncs.pfnGetCvarFloat( "r_sprite_lerping" ) == 0.0f )
		return 0;
	if( hdr->numframes <= 1 )
		return 0;
	if( hdr->texFormat != XR_SPR_ADDITIVE )
		return 0;
	if( ent->curstate.rendermode == kRenderNormal || ent->curstate.rendermode == kRenderTransAlpha )
		return 0;
	if( ent->curstate.effects & EF_NOINTERP )
		return 0;
	return 1;
}

static int SpriteHasLighting( const cl_entity_t *ent, const xr_msprite_t *hdr )
{
	if( !ent || !hdr )
		return 0;
	if( gEngfuncs.pfnGetCvarFloat( "r_sprite_lighting" ) == 0.0f )
		return 0;
	if( hdr->texFormat != XR_SPR_ALPHTEST )
		return 0;
	if( ent->curstate.effects & EF_FULLBRIGHT )
		return 0;
	if( ent->curstate.renderamt <= 127 )
		return 0;
	switch( ent->curstate.rendermode )
	{
	case kRenderNormal:
	case kRenderTransAlpha:
	case kRenderTransTexture:
		break;
	default:
		return 0;
	}
	return 1;
}

static void GetSpriteFrames( const xr_msprite_t *hdr, cl_entity_t *work, float view_yaw, float cl_time, SpriteFrameResult *out )
{
	int frame;
	int do_interp;
	float lerp = 1.0f;
	const xr_mspriteframe_t *old_f = NULL;
	const xr_mspriteframe_t *cur_f = NULL;

	out->old_frame = NULL;
	out->current_frame = NULL;
	out->lerp = 1.0f;
	if( !hdr || !work || hdr->numframes <= 0 )
		return;

	frame = ClampFrameIndex( hdr, (int)work->curstate.frame );
	do_interp = SpriteAllowLerping( work, hdr );

	if( !do_interp )
	{
		cur_f = StaticFrame( hdr, frame, view_yaw, work->angles[1], cl_time );
		out->old_frame = cur_f;
		out->current_frame = cur_f;
		out->lerp = 1.0f;
		return;
	}

	s_lerp_candidates++;
	if( hdr->frames[frame].type == XR_SPR_SINGLE )
	{
		if( work->latched.prevblending[0] >= hdr->numframes
			|| hdr->frames[work->latched.prevblending[0]].type != XR_SPR_SINGLE )
		{
			work->latched.prevblending[0] = work->latched.prevblending[1] = (byte)frame;
			work->latched.sequencetime = cl_time;
			lerp = 1.0f;
		}
		if( work->latched.sequencetime < cl_time )
		{
			if( frame != work->latched.prevblending[1] )
			{
				work->latched.prevblending[0] = work->latched.prevblending[1];
				work->latched.prevblending[1] = (byte)frame;
				work->latched.sequencetime = cl_time;
				lerp = 0.0f;
			}
			else
				lerp = ( cl_time - work->latched.sequencetime ) * 11.0f;
		}
		else
		{
			work->latched.prevblending[0] = work->latched.prevblending[1] = (byte)frame;
			work->latched.sequencetime = cl_time;
			lerp = 0.0f;
		}
		if( work->latched.prevblending[0] >= hdr->numframes )
		{
			work->latched.prevblending[0] = work->latched.prevblending[1] = (byte)frame;
			work->latched.sequencetime = cl_time;
			lerp = 0.0f;
		}
		old_f = (const xr_mspriteframe_t *)hdr->frames[work->latched.prevblending[0]].frameptr;
		cur_f = (const xr_mspriteframe_t *)hdr->frames[frame].frameptr;
	}
	else if( hdr->frames[frame].type == XR_SPR_GROUP )
	{
		const xr_mspritegroup_t *group = (const xr_mspritegroup_t *)hdr->frames[frame].frameptr;
		float *pintervals;
		int numframes, i, j;
		float fullinterval, jinterval, jtime, targettime;

		if( !group || group->numframes <= 0 || !group->intervals )
			return;
		pintervals = group->intervals;
		numframes = group->numframes;
		fullinterval = pintervals[numframes - 1];
		if( fullinterval <= 0.0f )
		{
			cur_f = group->frames[0];
			out->old_frame = cur_f;
			out->current_frame = cur_f;
			out->lerp = 1.0f;
			return;
		}
		jinterval = ( numframes > 1 ) ? ( pintervals[1] - pintervals[0] ) : fullinterval;
		jtime = 0.0f;
		targettime = cl_time - (float)( (int)( cl_time / fullinterval ) ) * fullinterval;
		for( i = 0, j = numframes - 1; i < ( numframes - 1 ); i++ )
		{
			if( pintervals[i] > targettime )
				break;
			j = i;
			jinterval = pintervals[i] - jtime;
			jtime = pintervals[i];
		}
		if( jinterval > 0.0f )
			lerp = ( targettime - jtime ) / jinterval;
		else
			lerp = 1.0f;
		old_f = group->frames[j];
		cur_f = group->frames[i];
	}
	else if( hdr->frames[frame].type == XR_SPR_ANGLED )
	{
		int angleframe = AngleFrame( view_yaw, work->angles[1] );
		const xr_mspritegroup_t *old_group;
		const xr_mspritegroup_t *cur_group;

		s_angled_path++;
		if( work->latched.prevblending[0] >= hdr->numframes
			|| hdr->frames[work->latched.prevblending[0]].type != XR_SPR_ANGLED )
		{
			work->latched.prevblending[0] = work->latched.prevblending[1] = (byte)frame;
			work->latched.sequencetime = cl_time;
			lerp = 1.0f;
		}
		if( work->latched.sequencetime < cl_time )
		{
			if( frame != work->latched.prevblending[1] )
			{
				work->latched.prevblending[0] = work->latched.prevblending[1];
				work->latched.prevblending[1] = (byte)frame;
				work->latched.sequencetime = cl_time;
				lerp = 0.0f;
			}
			else
				lerp = ( cl_time - work->latched.sequencetime ) * work->curstate.framerate;
		}
		else
		{
			work->latched.prevblending[0] = work->latched.prevblending[1] = (byte)frame;
			work->latched.sequencetime = cl_time;
			lerp = 0.0f;
		}
		old_group = (const xr_mspritegroup_t *)hdr->frames[work->latched.prevblending[0]].frameptr;
		cur_group = (const xr_mspritegroup_t *)hdr->frames[frame].frameptr;
		old_f = GroupFrameAt( old_group, angleframe );
		cur_f = GroupFrameAt( cur_group, angleframe );
	}

	out->old_frame = old_f;
	out->current_frame = cur_f;
	out->lerp = lerp;
}

static unsigned int MixU32( unsigned int h, unsigned int v )
{
	h ^= v;
	h *= 16777619u;
	return h;
}

static unsigned int MixF32( unsigned int h, float f )
{
	union { float f; unsigned int u; } x;
	x.f = f;
	return MixU32( h, x.u );
}

static unsigned int HashLiveSprite( const cl_entity_t *e )
{
	unsigned int h = 2166136261u;
	if( !e )
		return h;
	h = MixF32( h, e->curstate.frame );
	h = MixU32( h, e->latched.prevblending[0] );
	h = MixU32( h, e->latched.prevblending[1] );
	h = MixF32( h, e->latched.sequencetime );
	h = MixF32( h, e->origin[0] );
	h = MixF32( h, e->origin[1] );
	h = MixF32( h, e->origin[2] );
	h = MixF32( h, e->angles[0] );
	h = MixF32( h, e->angles[1] );
	h = MixF32( h, e->angles[2] );
	h = MixU32( h, (unsigned int)e->curstate.renderamt );
	h = MixU32( h, e->curstate.rendercolor.r );
	h = MixU32( h, e->curstate.rendercolor.g );
	h = MixU32( h, e->curstate.rendercolor.b );
	h = MixU32( h, (unsigned int)e->curstate.effects );
	return h;
}

static unsigned int HashMirroredLives( void )
{
	unsigned int h = 2166136261u;
	int i, n;
	const CSRETRO_EntCopy *e;

	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		e = CSRETRO_Scene_Get( i );
		if( !e || ( e->kind != CSRETRO_KIND_TENT_SPRITE && e->kind != CSRETRO_KIND_NORMAL_SPRITE ) )
			continue;
		h = MixU32( h, HashLiveSprite( e->live ) );
	}
	return h;
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

void CSRETRO_Sprite_ResetDump( void )
{
	s_dump_left = 8;
	s_tent_dumped = 0;
}

void CSRETRO_Sprite_SetNoDepth( int enabled )
{
	s_nodepth = enabled ? 1 : 0;
}

static void RestoreSpriteGL( int depth_func, int blend_src, int blend_dst, int alpha_func,
	float alpha_ref, unsigned char depth_mask, unsigned int tex, float color[4],
	int cull_mode, unsigned char blend_on, unsigned char alpha_on, unsigned char cull_on )
{
	if( gXRGL.DepthFunc && depth_func )
		gXRGL.DepthFunc( (unsigned int)depth_func );
	if( gXRGL.BlendFunc )
		gXRGL.BlendFunc( (unsigned int)blend_src, (unsigned int)blend_dst );
	if( gXRGL.AlphaFunc )
		gXRGL.AlphaFunc( (unsigned int)alpha_func, alpha_ref );
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( depth_mask );
	if( gXRGL.BindTexture )
		gXRGL.BindTexture( GL_TEXTURE_2D, tex );
	if( gXRGL.Color4f )
		gXRGL.Color4f( color[0], color[1], color[2], color[3] );
	if( gXRGL.CullFace && cull_mode )
		gXRGL.CullFace( (unsigned int)cull_mode );
	if( gXRGL.Enable && gXRGL.Disable )
	{
		if( blend_on )
			gXRGL.Enable( GL_BLEND );
		else
			gXRGL.Disable( GL_BLEND );
		if( alpha_on )
			gXRGL.Enable( GL_ALPHA_TEST );
		else
			gXRGL.Disable( GL_ALPHA_TEST );
		if( cull_on )
			gXRGL.Enable( GL_CULL_FACE );
		else
			gXRGL.Disable( GL_CULL_FACE );
	}
}

static int DrawOne( const CSRETRO_EntCopy *e, const float *vieworg, const float *vright, const float *vup, const float *vforward, float view_yaw, float cl_time )
{
	const xr_model_t *mod;
	const xr_msprite_t *hdr;
	SpriteFrameResult frames;
	const xr_mspriteframe_t *frame;
	const xr_mspriteframe_t *oldframe;
	const cl_entity_t *snap;
	cl_entity_t work;
	float origin[3], right[3], up[3], color[3], light[3];
	float scale, alpha, lerp;
	int type, i, lighting;
	unsigned int white_tex;

	if( !e || !e->model )
		return 0;
	mod = (const xr_model_t *)e->model;
	if( mod->type != XR_MOD_SPRITE || !mod->cache )
		return 0;
	hdr = (const xr_msprite_t *)mod->cache;

	memset( (void *)&work, 0, sizeof( work ) );
	snap = ( e->snap_index >= 0 ) ? CSRETRO_Scene_StudioSnap( e->snap_index ) : NULL;
	if( snap )
		work = *snap;
	else
	{
		work.curstate.frame = e->frame;
		work.curstate.effects = e->effects;
		work.curstate.rendermode = e->rendermode;
		work.curstate.renderamt = e->renderamt;
		work.curstate.rendercolor.r = e->rendercolor[0];
		work.curstate.rendercolor.g = e->rendercolor[1];
		work.curstate.rendercolor.b = e->rendercolor[2];
		work.curstate.scale = e->scale;
		work.angles[0] = e->angles[0];
		work.angles[1] = e->angles[1];
		work.angles[2] = e->angles[2];
		work.origin[0] = e->origin[0];
		work.origin[1] = e->origin[1];
		work.origin[2] = e->origin[2];
	}
	work.model = (struct model_s *)e->model;
	work.curstate.rendermode = e->rendermode;
	work.curstate.renderamt = e->renderamt;
	work.curstate.effects = e->effects;

	memset( &frames, 0, sizeof( frames ) );
	GetSpriteFrames( hdr, &work, view_yaw, cl_time, &frames );
	frame = frames.current_frame;
	oldframe = frames.old_frame;
	lerp = frames.lerp;
	if( !frame || frame->gl_texturenum <= 0 )
		return 0;
	if( !oldframe )
		oldframe = frame;

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

	if( gXRGL.Disable )
		gXRGL.Disable( GL_CULL_FACE );

	if( e->rendermode == kRenderGlow || e->rendermode == kRenderWorldGlow )
	{
		float dx = origin[0] - vieworg[0];
		float dy = origin[1] - vieworg[1];
		float dz = origin[2] - vieworg[2];
		float dist = sqrtf( dx * dx + dy * dy + dz * dz );
		if( dist > 1.0f )
			scale *= dist * ( 1.0f / 200.0f );
	}

	ApplyMode( e->rendermode );
	if( s_nodepth && gXRGL.Disable )
		gXRGL.Disable( GL_DEPTH_TEST );
	if( ( e->rendermode == kRenderGlow || e->rendermode == kRenderTransAdd
		|| e->rendermode == kRenderWorldGlow ) && gXRGL.Disable )
		gXRGL.Disable( GL_ALPHA_TEST );

	lighting = SpriteHasLighting( &work, hdr );
	light[0] = light[1] = light[2] = 1.0f;
	if( lighting )
	{
		s_light_candidates++;
		if( gEngfuncs.pTriAPI && gEngfuncs.pTriAPI->LightAtPoint )
		{
			float sampled[3] = { 0.0f, 0.0f, 0.0f };
			gEngfuncs.pTriAPI->LightAtPoint( origin, sampled );
			s_light_at_point++;
			light[0] = sampled[0] * ( 1.0f / 255.0f );
			light[1] = sampled[1] * ( 1.0f / 255.0f );
			light[2] = sampled[2] * ( 1.0f / 255.0f );
		}
		if( gXRGL.AlphaFunc )
			gXRGL.AlphaFunc( GL_GREATER, 1.0f / 3.0f );
	}

	if( gXRGL.Enable )
		gXRGL.Enable( GL_TEXTURE_2D );

	if( oldframe == frame || oldframe->gl_texturenum == frame->gl_texturenum )
	{
		if( gXRGL.Color4f )
			gXRGL.Color4f( color[0], color[1], color[2], alpha );
		CSRETRO_Backend_BindTexture( 0, (unsigned int)frame->gl_texturenum );
		DrawQuad( frame, origin, right, up, scale );
	}
	else
	{
		if( lerp < 0.0f )
			lerp = 0.0f;
		if( lerp > 1.0f )
			lerp = 1.0f;
		s_lerp_drawn++;
		s_lerp_old_ne++;
		if( ( 1.0f - lerp ) != 0.0f )
		{
			if( gXRGL.Color4f )
				gXRGL.Color4f( color[0], color[1], color[2], alpha * ( 1.0f - lerp ) );
			CSRETRO_Backend_BindTexture( 0, (unsigned int)oldframe->gl_texturenum );
			DrawQuad( oldframe, origin, right, up, scale );
		}
		if( lerp != 0.0f )
		{
			if( gXRGL.Color4f )
				gXRGL.Color4f( color[0], color[1], color[2], alpha * lerp );
			CSRETRO_Backend_BindTexture( 0, (unsigned int)frame->gl_texturenum );
			DrawQuad( frame, origin, right, up, scale );
		}
	}

	if( lighting )
	{
		int depth_func = 0, blend_src = 0, blend_dst = 0, alpha_func = 0, cull_mode = 0, tex = 0;
		float alpha_ref = 0.0f, cur_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		unsigned char depth_mask = 1, blend_on = 0, alpha_on = 0, cull_on = 0;

		if( gXRGL.GetIntegerv )
		{
			gXRGL.GetIntegerv( GL_DEPTH_FUNC, &depth_func );
			gXRGL.GetIntegerv( GL_BLEND_SRC, &blend_src );
			gXRGL.GetIntegerv( GL_BLEND_DST, &blend_dst );
			gXRGL.GetIntegerv( GL_ALPHA_TEST_FUNC, &alpha_func );
			gXRGL.GetIntegerv( GL_TEXTURE_BINDING_2D, &tex );
			gXRGL.GetIntegerv( GL_CULL_FACE_MODE, &cull_mode );
		}
		if( gXRGL.GetFloatv )
		{
			gXRGL.GetFloatv( GL_ALPHA_TEST_REF, &alpha_ref );
			gXRGL.GetFloatv( GL_CURRENT_COLOR, cur_color );
		}
		if( gXRGL.GetBooleanv )
			gXRGL.GetBooleanv( GL_DEPTH_WRITEMASK, &depth_mask );
		if( gXRGL.IsEnabled )
		{
			blend_on = gXRGL.IsEnabled( GL_BLEND );
			alpha_on = gXRGL.IsEnabled( GL_ALPHA_TEST );
			cull_on = gXRGL.IsEnabled( GL_CULL_FACE );
		}

		white_tex = CSRETRO_Backend_WhiteTexture();
		if( white_tex && gXRGL.Enable && gXRGL.Disable && gXRGL.BlendFunc && gXRGL.DepthFunc )
		{
			gXRGL.Enable( GL_BLEND );
			gXRGL.DepthFunc( GL_EQUAL );
			gXRGL.Disable( GL_ALPHA_TEST );
			gXRGL.BlendFunc( GL_ZERO, GL_SRC_COLOR );
			if( gXRGL.TexEnvi )
				gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			if( gXRGL.Color4f )
				gXRGL.Color4f( light[0], light[1], light[2], alpha );
			if( gXRGL.BindTexture )
				gXRGL.BindTexture( GL_TEXTURE_2D, white_tex );
			DrawQuad( frame, origin, right, up, scale );
			s_light_pass++;
		}
		RestoreSpriteGL( depth_func, blend_src, blend_dst, alpha_func, alpha_ref, depth_mask,
			(unsigned int)tex, cur_color, cull_mode, blend_on, alpha_on, cull_on );
	}

	if( s_dump_left > 0 || ( e->kind == CSRETRO_KIND_TENT_SPRITE && !s_tent_dumped ) )
	{
		float dx = origin[0] - vieworg[0];
		float dy = origin[1] - vieworg[1];
		float dz = origin[2] - vieworg[2];
		float dist = sqrtf( dx * dx + dy * dy + dz * dz );
		float facing = 0.0f;
		if( dist > 0.001f )
			facing = ( dx * vforward[0] + dy * vforward[1] + dz * vforward[2] ) / dist;
		gEngfuncs.Con_Printf(
			"CS Retro: sprite dump kind=%s tex=%i %ix%i L%.1f R%.1f U%.1f D%.1f origin=%.0f %.0f %.0f dist=%.0f facing=%.2f mode=%i scale=%.2f amt=%i fmt=%i lerp=%.2f light=%i angled=%i\n",
			e->kind == CSRETRO_KIND_TENT_SPRITE ? "tent" : "normal",
			frame->gl_texturenum, frame->width, frame->height,
			frame->left, frame->right, frame->up, frame->down,
			origin[0], origin[1], origin[2], dist, facing,
			e->rendermode, scale, e->renderamt, hdr->texFormat, frames.lerp, lighting, s_angled_path );
		if( e->kind == CSRETRO_KIND_TENT_SPRITE )
			s_tent_dumped = 1;
		else if( s_dump_left > 0 )
			s_dump_left--;
	}

	if( gXRGL.Enable )
	{
		gXRGL.Enable( GL_DEPTH_TEST );
		gXRGL.Enable( GL_CULL_FACE );
	}
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( GL_TRUE );
	return 1;
}

static int DrawSpriteRange( int only_index, int opaque_only, const float *vieworg, const float *viewangles, CSRETRO_SceneStats *stats )
{
	float vforward[3], vright[3], vup[3];
	float cl_time;
	int i, n, drawn = 0;
	unsigned int live_before = 0, live_after = 0;
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
	if( !s_live_logged )
		live_before = HashMirroredLives();
	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		e = CSRETRO_Scene_Get( i );
		if( only_index >= 0 && i != only_index )
			continue;
		if( !e )
			continue;
		if( e->kind != CSRETRO_KIND_TENT_SPRITE && e->kind != CSRETRO_KIND_NORMAL_SPRITE )
			continue;
		if( only_index < 0 )
		{
			int cls = CSRETRO_Trans_DrawClass( i );
			if( opaque_only && cls != CSRETRO_DRAW_OPAQUE )
				continue;
			if( !opaque_only && cls != CSRETRO_DRAW_TRANS )
				continue;
		}
		if( DrawOne( e, vieworg, vright, vup, vforward, viewangles[1], cl_time ) )
		{
			CSRETRO_Scene_NoteDrawn( e->kind );
			CSRETRO_Trans_NoteDrawn( i );
			drawn++;
		}
	}
	if( !s_live_logged )
	{
		live_after = HashMirroredLives();
		s_live_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: sprite live state before=%08x after=%08x mutate=%i\n",
			live_before, live_after, live_before != live_after ? 1 : 0 );
	}
	if( !s_sprite_logged && drawn > 0 )
	{
		s_sprite_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: sprite lerp candidates=%i drawn=%i old_ne_current=%i\n",
			s_lerp_candidates, s_lerp_drawn, s_lerp_old_ne );
		gEngfuncs.Con_Printf(
			"CS Retro: sprite lighting candidates=%i lightatpoint=%i pass=%i\n",
			s_light_candidates, s_light_at_point, s_light_pass );
		gEngfuncs.Con_Printf(
			"CS Retro: sprite angled path=%i\n", s_angled_path );
	}
	if( !s_lerp_proof_logged && s_lerp_old_ne > 0 )
	{
		s_lerp_proof_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: sprite lerp candidates=%i drawn=%i old_ne_current=%i\n",
			s_lerp_candidates, s_lerp_drawn, s_lerp_old_ne );
	}
	if( !s_light_proof_logged && s_light_pass > 0 )
	{
		s_light_proof_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: sprite lighting candidates=%i lightatpoint=%i pass=%i\n",
			s_light_candidates, s_light_at_point, s_light_pass );
	}
	if( !s_angled_proof_logged && s_angled_path > 0 )
	{
		s_angled_proof_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: sprite angled path=%i\n", s_angled_path );
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

int CSRETRO_Sprite_DrawList( const float *vieworg, const float *viewangles, CSRETRO_SceneStats *stats )
{
	return DrawSpriteRange( -1, 0, vieworg, viewangles, stats );
}

int CSRETRO_Sprite_DrawSolid( const float *vieworg, const float *viewangles, CSRETRO_SceneStats *stats )
{
	return DrawSpriteRange( -1, 1, vieworg, viewangles, stats );
}

int CSRETRO_Sprite_DrawOne( int scene_index, const float *vieworg, const float *viewangles, CSRETRO_SceneStats *stats )
{
	return DrawSpriteRange( scene_index, 0, vieworg, viewangles, stats );
}
