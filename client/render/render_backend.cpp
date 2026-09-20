#include "render_backend.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "hud.h"
#include "cl_util.h"
#include "render_api.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_TEXTURE_2D 0x0DE1
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_SCISSOR_TEST 0x0C11
#define GL_LEQUAL 0x0203
#define GL_BACK 0x0405
#define GL_CCW 0x0901
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_VIEWPORT 0x0BA2
#define GL_SCISSOR_BOX 0x0C10
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_FUNC 0x0B74
#define GL_BLEND_SRC 0x0BE1
#define GL_BLEND_DST 0x0BE0
#define GL_CULL_FACE_MODE 0x0B45
#define GL_FRONT_FACE 0x0B46
#define GL_MATRIX_MODE 0x0BA0
#define GL_MODELVIEW_MATRIX 0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_ONE 1
#define GL_ZERO 0
#define GL_ALPHA_TEST 0x0BC0
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_BLEND_EQUATION 0x8009
#define GL_CURRENT_COLOR 0x0B00
#define GL_FUNC_ADD 0x8006
#define GL_DEPTH_RANGE 0x0B70
#define GL_POLYGON_MODE 0x0B40
#define GL_SHADE_MODEL 0x0B54
#define GL_FRONT_AND_BACK 0x0408
#define GL_FILL 0x1B02
#define GL_FLAT 0x1D00
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS 0x2A00
#define GL_FOG 0x0B60
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_MODE 0x0B65
#define GL_FOG_COLOR 0x0B66

CSRETRO_GL gXRGL;

static render_api_t *s_api = NULL;
static int s_ready = 0;
static unsigned int s_fbo = 0;
static unsigned int s_depth_rb = 0;
static int s_color_texnum = 0;
static unsigned int s_color_glname = 0;
static int s_raw_color = 0;
static int s_dumped_ppm = 0;

static void *( *s_get_proc )( const char *name ) = NULL;

static void *LoadProc( const char *a, const char *b )
{
	void *p = NULL;
	if( s_get_proc )
		p = s_get_proc( a );
	if( !p && s_get_proc && b )
		p = s_get_proc( b );
	return p;
}

#define LOAD1( field, name ) gXRGL.field = (decltype( gXRGL.field ))LoadProc( name, NULL )
#define LOAD2( field, name, ext ) gXRGL.field = (decltype( gXRGL.field ))LoadProc( name, ext )

typedef unsigned int ( *PFN_GEN )( int n, unsigned int *ids );
typedef void ( *PFN_DEL )( int n, const unsigned int *ids );
typedef void ( *PFN_BIND )( unsigned int target, unsigned int id );
typedef void ( *PFN_RBSTORAGE )( unsigned int target, unsigned int internalformat, int w, int h );
typedef void ( *PFN_FBTEX )( unsigned int target, unsigned int attachment, unsigned int textarget, unsigned int tex, int level );
typedef void ( *PFN_FBRB )( unsigned int target, unsigned int attachment, unsigned int rbtarget, unsigned int rb );
typedef unsigned int ( *PFN_CHECKFB )( unsigned int target );
typedef void ( *PFN_TEXIMAGE )( unsigned int target, int level, int internal, int w, int h, int border, unsigned int format, unsigned int type, const void *data );
typedef void ( *PFN_TEXPARAMI )( unsigned int target, unsigned int pname, int param );
typedef void ( *PFN_GENTTEX )( int n, unsigned int *ids );

static PFN_GEN pglGenFramebuffers = NULL;
static PFN_DEL pglDeleteFramebuffers = NULL;
static PFN_GEN pglGenRenderbuffers = NULL;
static PFN_DEL pglDeleteRenderbuffers = NULL;
static PFN_BIND pglBindRenderbuffer = NULL;
static PFN_RBSTORAGE pglRenderbufferStorage = NULL;
static PFN_FBTEX pglFramebufferTexture2D = NULL;
static PFN_FBRB pglFramebufferRenderbuffer = NULL;
static PFN_CHECKFB pglCheckFramebufferStatus = NULL;
static PFN_GENTTEX pglGenTextures = NULL;
static PFN_DEL pglDeleteTextures = NULL;
static PFN_TEXIMAGE pglTexImage2D = NULL;
static PFN_TEXPARAMI pglTexParameteri = NULL;

typedef struct GLState_s
{
	int fbo;
	int viewport[4];
	int scissor[4];
	unsigned char scissor_test;
	unsigned char depth_test;
	unsigned char blend;
	unsigned char cull;
	unsigned char tex2d0;
	unsigned char tex2d1;
	unsigned char depth_mask;
	int depth_func;
	int blend_src, blend_dst;
	int cull_mode, front_face;
	int active_tex;
	int tex0, tex1;
	int program;
	int array_buffer, element_buffer;
	int vao;
	int matrix_mode;
	float modelview[16];
	float projection[16];
	unsigned char color_mask[4];
	unsigned char alpha_test;
	int alpha_func;
	float alpha_ref;
	int blend_eq;
	float color[4];
	float depth_range[2];
	int polygon_mode[2];
	int shade_model;
	int texenv0;
	int texenv1;
	unsigned char poly_offset;
	float poly_factor;
	float poly_units;
	unsigned char fog;
	int fog_mode;
	float fog_density;
	float fog_start;
	float fog_end;
	float fog_color[4];
	int saved;
} GLState;

static GLState s_saved;

static int EnsureFBO( void );

int CSRETRO_Backend_Init( struct render_api_s *api )
{
	if( !api || !api->GL_GetProcAddress )
		return 0;

	s_api = api;
	s_get_proc = api->GL_GetProcAddress;
	memset( &gXRGL, 0, sizeof( gXRGL ) );

	LOAD1( ClearColor, "glClearColor" );
	LOAD1( Clear, "glClear" );
	LOAD1( Enable, "glEnable" );
	LOAD1( Disable, "glDisable" );
	LOAD1( Viewport, "glViewport" );
	LOAD1( Scissor, "glScissor" );
	LOAD1( DepthMask, "glDepthMask" );
	LOAD1( DepthFunc, "glDepthFunc" );
	LOAD1( BlendFunc, "glBlendFunc" );
	LOAD1( CullFace, "glCullFace" );
	LOAD1( FrontFace, "glFrontFace" );
	LOAD1( Color4f, "glColor4f" );
	LOAD1( Begin, "glBegin" );
	LOAD1( End, "glEnd" );
	LOAD1( Vertex3f, "glVertex3f" );
	LOAD1( TexCoord2f, "glTexCoord2f" );
	LOAD2( MultiTexCoord2f, "glMultiTexCoord2f", "glMultiTexCoord2fARB" );
	LOAD2( ActiveTexture, "glActiveTexture", "glActiveTextureARB" );
	LOAD1( MatrixMode, "glMatrixMode" );
	LOAD1( LoadIdentity, "glLoadIdentity" );
	LOAD1( PushMatrix, "glPushMatrix" );
	LOAD1( PopMatrix, "glPopMatrix" );
	LOAD1( Frustum, "glFrustum" );
	LOAD1( Rotatef, "glRotatef" );
	LOAD1( Translatef, "glTranslatef" );
	LOAD1( ReadPixels, "glReadPixels" );
	LOAD1( PixelStorei, "glPixelStorei" );
	LOAD1( GetIntegerv, "glGetIntegerv" );
	LOAD1( GetBooleanv, "glGetBooleanv" );
	LOAD1( GetFloatv, "glGetFloatv" );
	LOAD1( IsEnabled, "glIsEnabled" );
	LOAD1( BindTexture, "glBindTexture" );
	LOAD2( BindFramebuffer, "glBindFramebuffer", "glBindFramebufferEXT" );
	LOAD2( UseProgram, "glUseProgram", "glUseProgramObjectARB" );
	LOAD2( BindBuffer, "glBindBuffer", "glBindBufferARB" );
	LOAD2( BindVertexArray, "glBindVertexArray", NULL );
	LOAD1( ColorMask, "glColorMask" );
	LOAD1( TexEnvi, "glTexEnvi" );
	LOAD1( AlphaFunc, "glAlphaFunc" );
	LOAD2( BlendEquation, "glBlendEquation", "glBlendEquationEXT" );
	LOAD1( Vertex3fv, "glVertex3fv" );
	LOAD1( DepthRange, "glDepthRange" );
	LOAD1( PolygonMode, "glPolygonMode" );
	LOAD1( ShadeModel, "glShadeModel" );
	LOAD1( PolygonOffset, "glPolygonOffset" );
	LOAD1( Fogi, "glFogi" );
	LOAD1( Fogf, "glFogf" );
	LOAD1( Fogfv, "glFogfv" );

	pglGenFramebuffers = (PFN_GEN)LoadProc( "glGenFramebuffers", "glGenFramebuffersEXT" );
	pglDeleteFramebuffers = (PFN_DEL)LoadProc( "glDeleteFramebuffers", "glDeleteFramebuffersEXT" );
	pglGenRenderbuffers = (PFN_GEN)LoadProc( "glGenRenderbuffers", "glGenRenderbuffersEXT" );
	pglDeleteRenderbuffers = (PFN_DEL)LoadProc( "glDeleteRenderbuffers", "glDeleteRenderbuffersEXT" );
	pglBindRenderbuffer = (PFN_BIND)LoadProc( "glBindRenderbuffer", "glBindRenderbufferEXT" );
	pglRenderbufferStorage = (PFN_RBSTORAGE)LoadProc( "glRenderbufferStorage", "glRenderbufferStorageEXT" );
	pglFramebufferTexture2D = (PFN_FBTEX)LoadProc( "glFramebufferTexture2D", "glFramebufferTexture2DEXT" );
	pglFramebufferRenderbuffer = (PFN_FBRB)LoadProc( "glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT" );
	pglCheckFramebufferStatus = (PFN_CHECKFB)LoadProc( "glCheckFramebufferStatus", "glCheckFramebufferStatusEXT" );
	pglGenTextures = (PFN_GENTTEX)LoadProc( "glGenTextures", NULL );
	pglDeleteTextures = (PFN_DEL)LoadProc( "glDeleteTextures", NULL );
	pglTexImage2D = (PFN_TEXIMAGE)LoadProc( "glTexImage2D", NULL );
	pglTexParameteri = (PFN_TEXPARAMI)LoadProc( "glTexParameteri", NULL );

	if( !gXRGL.Clear || !gXRGL.Begin || !gXRGL.GetIntegerv || !gXRGL.BindFramebuffer || !pglGenFramebuffers )
		return 0;

	s_ready = 1;
	return 1;
}

static void DestroyFBO( void )
{
	if( s_fbo && pglDeleteFramebuffers )
		pglDeleteFramebuffers( 1, &s_fbo );
	s_fbo = 0;
	if( s_depth_rb && pglDeleteRenderbuffers )
		pglDeleteRenderbuffers( 1, &s_depth_rb );
	s_depth_rb = 0;
	if( s_color_texnum && s_api && s_api->GL_FreeTexture && !s_raw_color )
		s_api->GL_FreeTexture( (unsigned int)s_color_texnum );
	else if( s_raw_color && s_color_glname && pglDeleteTextures )
		pglDeleteTextures( 1, &s_color_glname );
	s_color_texnum = 0;
	s_color_glname = 0;
	s_raw_color = 0;
}

void CSRETRO_Backend_Shutdown( void )
{
	DestroyFBO();
	s_dumped_ppm = 0;
	memset( &gXRGL, 0, sizeof( gXRGL ) );
	s_api = NULL;
	s_get_proc = NULL;
	s_ready = 0;
}

void CSRETRO_Backend_AllowDump( void )
{
	s_dumped_ppm = 0;
}

int CSRETRO_Backend_Ready( void )
{
	return s_ready;
}

static int EnsureFBO( void )
{
	unsigned int status;

	if( s_fbo )
		return 1;
	if( !s_ready || !pglGenFramebuffers )
		return 0;

	if( s_api && s_api->GL_CreateTexture && s_api->RenderGetParm )
	{
		unsigned char *empty = (unsigned char *)calloc( (size_t)CSRETRO_OFFSCREEN_SIZE * CSRETRO_OFFSCREEN_SIZE, 4 );
		s_color_texnum = s_api->GL_CreateTexture( "*csretro_offscreen", CSRETRO_OFFSCREEN_SIZE,
			CSRETRO_OFFSCREEN_SIZE, empty, (texFlags_t)( TF_NOMIPMAP | TF_CLAMP | TF_HAS_ALPHA | TF_NEAREST ) );
		free( empty );
		if( s_color_texnum > 0 )
			s_color_glname = (unsigned int)s_api->RenderGetParm( PARM_TEX_TEXNUM, s_color_texnum );
	}

	if( !s_color_glname && pglGenTextures && pglTexImage2D )
	{
		pglGenTextures( 1, &s_color_glname );
		gXRGL.BindTexture( GL_TEXTURE_2D, s_color_glname );
		pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, CSRETRO_OFFSCREEN_SIZE, CSRETRO_OFFSCREEN_SIZE, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		if( pglTexParameteri )
		{
			pglTexParameteri( GL_TEXTURE_2D, 0x2801, 0x2600 ); // GL_TEXTURE_MIN_FILTER, GL_NEAREST
			pglTexParameteri( GL_TEXTURE_2D, 0x2800, 0x2600 ); // GL_TEXTURE_MAG_FILTER
		}
		s_raw_color = 1;
	}

	if( !s_color_glname )
		return 0;

	pglGenRenderbuffers( 1, &s_depth_rb );
	pglBindRenderbuffer( GL_RENDERBUFFER, s_depth_rb );
	pglRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CSRETRO_OFFSCREEN_SIZE, CSRETRO_OFFSCREEN_SIZE );

	pglGenFramebuffers( 1, &s_fbo );
	gXRGL.BindFramebuffer( GL_FRAMEBUFFER, s_fbo );
	pglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_color_glname, 0 );
	pglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_depth_rb );
	status = pglCheckFramebufferStatus( GL_FRAMEBUFFER );
	gXRGL.BindFramebuffer( GL_FRAMEBUFFER, 0 );
	pglBindRenderbuffer( GL_RENDERBUFFER, 0 );

	if( status != GL_FRAMEBUFFER_COMPLETE )
	{
		DestroyFBO();
		return 0;
	}
	return 1;
}

static void SaveState( void )
{
	memset( &s_saved, 0, sizeof( s_saved ) );
	if( !gXRGL.GetIntegerv )
		return;

	gXRGL.GetIntegerv( GL_FRAMEBUFFER_BINDING, &s_saved.fbo );
	gXRGL.GetIntegerv( GL_VIEWPORT, s_saved.viewport );
	gXRGL.GetIntegerv( GL_SCISSOR_BOX, s_saved.scissor );
	if( gXRGL.IsEnabled )
	{
		s_saved.scissor_test = gXRGL.IsEnabled( GL_SCISSOR_TEST );
		s_saved.depth_test = gXRGL.IsEnabled( GL_DEPTH_TEST );
		s_saved.blend = gXRGL.IsEnabled( GL_BLEND );
		s_saved.cull = gXRGL.IsEnabled( GL_CULL_FACE );
		s_saved.alpha_test = gXRGL.IsEnabled( GL_ALPHA_TEST );
		s_saved.poly_offset = gXRGL.IsEnabled( GL_POLYGON_OFFSET_FILL );
		s_saved.fog = gXRGL.IsEnabled( GL_FOG );
	}
	if( gXRGL.GetBooleanv )
	{
		gXRGL.GetBooleanv( GL_DEPTH_WRITEMASK, &s_saved.depth_mask );
		gXRGL.GetBooleanv( GL_COLOR_WRITEMASK, s_saved.color_mask );
	}
	gXRGL.GetIntegerv( GL_DEPTH_FUNC, &s_saved.depth_func );
	gXRGL.GetIntegerv( GL_BLEND_SRC, &s_saved.blend_src );
	gXRGL.GetIntegerv( GL_BLEND_DST, &s_saved.blend_dst );
	gXRGL.GetIntegerv( GL_BLEND_EQUATION, &s_saved.blend_eq );
	gXRGL.GetIntegerv( GL_ALPHA_TEST_FUNC, &s_saved.alpha_func );
	if( gXRGL.GetFloatv )
	{
		gXRGL.GetFloatv( GL_ALPHA_TEST_REF, &s_saved.alpha_ref );
		gXRGL.GetFloatv( GL_CURRENT_COLOR, s_saved.color );
		gXRGL.GetFloatv( GL_DEPTH_RANGE, s_saved.depth_range );
		gXRGL.GetFloatv( GL_POLYGON_OFFSET_FACTOR, &s_saved.poly_factor );
		gXRGL.GetFloatv( GL_POLYGON_OFFSET_UNITS, &s_saved.poly_units );
		gXRGL.GetFloatv( GL_FOG_DENSITY, &s_saved.fog_density );
		gXRGL.GetFloatv( GL_FOG_START, &s_saved.fog_start );
		gXRGL.GetFloatv( GL_FOG_END, &s_saved.fog_end );
		gXRGL.GetFloatv( GL_FOG_COLOR, s_saved.fog_color );
	}
	gXRGL.GetIntegerv( GL_FOG_MODE, &s_saved.fog_mode );
	if( !s_saved.depth_range[1] )
		s_saved.depth_range[1] = 1.0f;
	gXRGL.GetIntegerv( GL_POLYGON_MODE, s_saved.polygon_mode );
	if( !s_saved.polygon_mode[0] )
		s_saved.polygon_mode[0] = (int)GL_FILL;
	if( !s_saved.polygon_mode[1] )
		s_saved.polygon_mode[1] = (int)GL_FILL;
	gXRGL.GetIntegerv( GL_SHADE_MODEL, &s_saved.shade_model );
	if( !s_saved.shade_model )
		s_saved.shade_model = (int)GL_FLAT;
	if( !s_saved.blend_eq )
		s_saved.blend_eq = (int)GL_FUNC_ADD;
	gXRGL.GetIntegerv( GL_CULL_FACE_MODE, &s_saved.cull_mode );
	gXRGL.GetIntegerv( GL_FRONT_FACE, &s_saved.front_face );
	gXRGL.GetIntegerv( GL_ACTIVE_TEXTURE, &s_saved.active_tex );
	gXRGL.GetIntegerv( GL_MATRIX_MODE, &s_saved.matrix_mode );
	if( gXRGL.GetFloatv )
	{
		gXRGL.GetFloatv( GL_MODELVIEW_MATRIX, s_saved.modelview );
		gXRGL.GetFloatv( GL_PROJECTION_MATRIX, s_saved.projection );
	}
	if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE1 );
		if( gXRGL.IsEnabled )
			s_saved.tex2d1 = gXRGL.IsEnabled( GL_TEXTURE_2D );
		gXRGL.GetIntegerv( GL_TEXTURE_BINDING_2D, &s_saved.tex1 );
		gXRGL.GetIntegerv( GL_TEXTURE_ENV_MODE, &s_saved.texenv1 );
		gXRGL.ActiveTexture( GL_TEXTURE0 );
		if( gXRGL.IsEnabled )
			s_saved.tex2d0 = gXRGL.IsEnabled( GL_TEXTURE_2D );
		gXRGL.GetIntegerv( GL_TEXTURE_BINDING_2D, &s_saved.tex0 );
		gXRGL.GetIntegerv( GL_TEXTURE_ENV_MODE, &s_saved.texenv0 );
	}
	else
		gXRGL.GetIntegerv( GL_TEXTURE_BINDING_2D, &s_saved.tex0 );

	gXRGL.GetIntegerv( GL_CURRENT_PROGRAM, &s_saved.program );
	gXRGL.GetIntegerv( GL_ARRAY_BUFFER_BINDING, &s_saved.array_buffer );
	gXRGL.GetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING, &s_saved.element_buffer );
	gXRGL.GetIntegerv( GL_VERTEX_ARRAY_BINDING, &s_saved.vao );
	s_saved.saved = 1;
}

static void RestoreState( void )
{
	if( !s_saved.saved )
		return;

	if( gXRGL.BindFramebuffer )
		gXRGL.BindFramebuffer( GL_FRAMEBUFFER, (unsigned int)s_saved.fbo );
	if( gXRGL.UseProgram )
		gXRGL.UseProgram( (unsigned int)s_saved.program );
	if( gXRGL.BindVertexArray )
		gXRGL.BindVertexArray( (unsigned int)s_saved.vao );
	if( gXRGL.BindBuffer )
	{
		gXRGL.BindBuffer( GL_ARRAY_BUFFER, (unsigned int)s_saved.array_buffer );
		gXRGL.BindBuffer( GL_ELEMENT_ARRAY_BUFFER, (unsigned int)s_saved.element_buffer );
	}
	if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE1 );
		if( gXRGL.BindTexture )
			gXRGL.BindTexture( GL_TEXTURE_2D, (unsigned int)s_saved.tex1 );
		if( s_saved.tex2d1 )
			gXRGL.Enable( GL_TEXTURE_2D );
		else
			gXRGL.Disable( GL_TEXTURE_2D );
		if( gXRGL.TexEnvi && s_saved.texenv1 )
			gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, s_saved.texenv1 );
		gXRGL.ActiveTexture( GL_TEXTURE0 );
		if( gXRGL.BindTexture )
			gXRGL.BindTexture( GL_TEXTURE_2D, (unsigned int)s_saved.tex0 );
		if( s_saved.tex2d0 )
			gXRGL.Enable( GL_TEXTURE_2D );
		else
			gXRGL.Disable( GL_TEXTURE_2D );
		if( gXRGL.TexEnvi && s_saved.texenv0 )
			gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, s_saved.texenv0 );
		gXRGL.ActiveTexture( (unsigned int)s_saved.active_tex );
	}
	else if( gXRGL.BindTexture )
		gXRGL.BindTexture( GL_TEXTURE_2D, (unsigned int)s_saved.tex0 );

	if( gXRGL.Viewport )
		gXRGL.Viewport( s_saved.viewport[0], s_saved.viewport[1], s_saved.viewport[2], s_saved.viewport[3] );
	if( gXRGL.Scissor )
		gXRGL.Scissor( s_saved.scissor[0], s_saved.scissor[1], s_saved.scissor[2], s_saved.scissor[3] );
	if( s_saved.scissor_test )
		gXRGL.Enable( GL_SCISSOR_TEST );
	else
		gXRGL.Disable( GL_SCISSOR_TEST );
	if( s_saved.depth_test )
		gXRGL.Enable( GL_DEPTH_TEST );
	else
		gXRGL.Disable( GL_DEPTH_TEST );
	if( s_saved.blend )
		gXRGL.Enable( GL_BLEND );
	else
		gXRGL.Disable( GL_BLEND );
	if( s_saved.alpha_test )
		gXRGL.Enable( GL_ALPHA_TEST );
	else
		gXRGL.Disable( GL_ALPHA_TEST );
	if( s_saved.poly_offset )
		gXRGL.Enable( GL_POLYGON_OFFSET_FILL );
	else
		gXRGL.Disable( GL_POLYGON_OFFSET_FILL );
	if( gXRGL.PolygonOffset )
		gXRGL.PolygonOffset( s_saved.poly_factor, s_saved.poly_units );
	if( s_saved.cull )
		gXRGL.Enable( GL_CULL_FACE );
	else
		gXRGL.Disable( GL_CULL_FACE );
	if( gXRGL.AlphaFunc )
		gXRGL.AlphaFunc( (unsigned int)s_saved.alpha_func, s_saved.alpha_ref );
	if( gXRGL.BlendEquation && s_saved.blend_eq )
		gXRGL.BlendEquation( (unsigned int)s_saved.blend_eq );
	if( gXRGL.Color4f )
		gXRGL.Color4f( s_saved.color[0], s_saved.color[1], s_saved.color[2], s_saved.color[3] );
	if( gXRGL.DepthRange )
		gXRGL.DepthRange( (double)s_saved.depth_range[0], (double)s_saved.depth_range[1] );
	if( gXRGL.PolygonMode )
	{
		gXRGL.PolygonMode( GL_FRONT_AND_BACK, (unsigned int)s_saved.polygon_mode[0] );
		if( s_saved.polygon_mode[1] && s_saved.polygon_mode[1] != s_saved.polygon_mode[0] )
			gXRGL.PolygonMode( GL_BACK, (unsigned int)s_saved.polygon_mode[1] );
	}
	if( gXRGL.ShadeModel && s_saved.shade_model )
		gXRGL.ShadeModel( (unsigned int)s_saved.shade_model );
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( s_saved.depth_mask );
	if( gXRGL.DepthFunc && s_saved.depth_func )
		gXRGL.DepthFunc( (unsigned int)s_saved.depth_func );
	if( gXRGL.BlendFunc )
		gXRGL.BlendFunc( (unsigned int)s_saved.blend_src, (unsigned int)s_saved.blend_dst );
	if( gXRGL.CullFace && s_saved.cull_mode )
		gXRGL.CullFace( (unsigned int)s_saved.cull_mode );
	if( gXRGL.FrontFace && s_saved.front_face )
		gXRGL.FrontFace( (unsigned int)s_saved.front_face );
	if( gXRGL.ColorMask )
		gXRGL.ColorMask( s_saved.color_mask[0], s_saved.color_mask[1], s_saved.color_mask[2], s_saved.color_mask[3] );
	if( gXRGL.MatrixMode && gXRGL.LoadIdentity )
	{
		typedef void ( *PFN_LOADMAT )( const float *m );
		static PFN_LOADMAT pglLoadMatrixf = NULL;
		if( !pglLoadMatrixf && s_get_proc )
			pglLoadMatrixf = (PFN_LOADMAT)s_get_proc( "glLoadMatrixf" );
		gXRGL.MatrixMode( GL_PROJECTION );
		if( pglLoadMatrixf )
			pglLoadMatrixf( s_saved.projection );
		else
			gXRGL.LoadIdentity();
		gXRGL.MatrixMode( GL_MODELVIEW );
		if( pglLoadMatrixf )
			pglLoadMatrixf( s_saved.modelview );
		else
			gXRGL.LoadIdentity();
		gXRGL.MatrixMode( (unsigned int)s_saved.matrix_mode );
	}
	if ( s_saved.fog )
		gXRGL.Enable( GL_FOG );
	else
		gXRGL.Disable( GL_FOG );
	if ( gXRGL.Fogi && s_saved.fog_mode )
		gXRGL.Fogi( GL_FOG_MODE, s_saved.fog_mode );
	if ( gXRGL.Fogf )
	{
		gXRGL.Fogf( GL_FOG_DENSITY, s_saved.fog_density );
		gXRGL.Fogf( GL_FOG_START, s_saved.fog_start );
		gXRGL.Fogf( GL_FOG_END, s_saved.fog_end );
	}
	if ( gXRGL.Fogfv )
		gXRGL.Fogfv( GL_FOG_COLOR, s_saved.fog_color );
	s_saved.saved = 0;
}

static struct
{
	unsigned char fog;
	int fog_mode;
	float fog_density;
	float fog_start;
	float fog_end;
	float fog_color[4];
	int saved;
} s_fog_push;

void CSRETRO_Backend_PushFog( void )
{
	memset( &s_fog_push, 0, sizeof( s_fog_push ) );
	if( !gXRGL.IsEnabled )
		return;
	s_fog_push.fog = gXRGL.IsEnabled( GL_FOG );
	if( gXRGL.GetIntegerv )
		gXRGL.GetIntegerv( GL_FOG_MODE, &s_fog_push.fog_mode );
	if( gXRGL.GetFloatv )
	{
		gXRGL.GetFloatv( GL_FOG_DENSITY, &s_fog_push.fog_density );
		gXRGL.GetFloatv( GL_FOG_START, &s_fog_push.fog_start );
		gXRGL.GetFloatv( GL_FOG_END, &s_fog_push.fog_end );
		gXRGL.GetFloatv( GL_FOG_COLOR, s_fog_push.fog_color );
	}
	s_fog_push.saved = 1;
}

void CSRETRO_Backend_PopFog( void )
{
	if( !s_fog_push.saved || !gXRGL.Enable )
		return;
	if( s_fog_push.fog )
		gXRGL.Enable( GL_FOG );
	else
		gXRGL.Disable( GL_FOG );
	if( gXRGL.Fogi && s_fog_push.fog_mode )
		gXRGL.Fogi( GL_FOG_MODE, s_fog_push.fog_mode );
	if( gXRGL.Fogf )
	{
		gXRGL.Fogf( GL_FOG_DENSITY, s_fog_push.fog_density );
		gXRGL.Fogf( GL_FOG_START, s_fog_push.fog_start );
		gXRGL.Fogf( GL_FOG_END, s_fog_push.fog_end );
	}
	if( gXRGL.Fogfv )
		gXRGL.Fogfv( GL_FOG_COLOR, s_fog_push.fog_color );
	s_fog_push.saved = 0;
}

int CSRETRO_Backend_BeginOffscreen( void )
{
	if( !EnsureFBO() )
		return 0;

	SaveState();
	gXRGL.BindFramebuffer( GL_FRAMEBUFFER, s_fbo );
	if( gXRGL.UseProgram )
		gXRGL.UseProgram( 0 );
	if( gXRGL.BindVertexArray )
		gXRGL.BindVertexArray( 0 );
	if( gXRGL.BindBuffer )
	{
		gXRGL.BindBuffer( GL_ARRAY_BUFFER, 0 );
		gXRGL.BindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	}
	gXRGL.Viewport( 0, 0, CSRETRO_OFFSCREEN_SIZE, CSRETRO_OFFSCREEN_SIZE );
	gXRGL.Disable( GL_SCISSOR_TEST );
	gXRGL.Enable( GL_DEPTH_TEST );
	gXRGL.DepthMask( GL_TRUE );
	gXRGL.DepthFunc( GL_LEQUAL );
	gXRGL.Disable( GL_BLEND );
	gXRGL.Disable( GL_POLYGON_OFFSET_FILL );
	gXRGL.Disable( GL_FOG );
	gXRGL.Enable( GL_CULL_FACE );
	gXRGL.CullFace( GL_BACK );
	gXRGL.FrontFace( GL_CCW );
	if( gXRGL.ColorMask )
		gXRGL.ColorMask( 1, 1, 1, 1 );
	gXRGL.ClearColor( 0.04f, 0.04f, 0.12f, 1.0f );
	gXRGL.Clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	return 1;
}

void CSRETRO_Backend_ApplyView( const float *vieworg, const float *viewangles, float fov_x, float fov_y )
{
	double xmin, xmax, ymin, ymax;
	const double znear = 4.0;
	const double zfar = 16384.0;

	if( !gXRGL.MatrixMode )
		return;
	if( fov_y <= 1.0f )
		fov_y = 75.0f;
	if( fov_x <= 1.0f )
		fov_x = fov_y;

	ymax = znear * tan( (double)fov_y * M_PI / 360.0 );
	ymin = -ymax;
	xmax = znear * tan( (double)fov_x * M_PI / 360.0 );
	xmin = -xmax;

	gXRGL.MatrixMode( GL_PROJECTION );
	gXRGL.LoadIdentity();
	gXRGL.Frustum( xmin, xmax, ymin, ymax, znear, zfar );

	gXRGL.MatrixMode( GL_MODELVIEW );
	gXRGL.LoadIdentity();
	// GoldSrc / Quake view: X forward after these rotations.
	gXRGL.Rotatef( -90.0f, 1.0f, 0.0f, 0.0f );
	gXRGL.Rotatef( 90.0f, 0.0f, 0.0f, 1.0f );
	gXRGL.Rotatef( -viewangles[2], 1.0f, 0.0f, 0.0f );
	gXRGL.Rotatef( -viewangles[0], 0.0f, 1.0f, 0.0f );
	gXRGL.Rotatef( -viewangles[1], 0.0f, 0.0f, 1.0f );
	gXRGL.Translatef( -vieworg[0], -vieworg[1], -vieworg[2] );
}

void CSRETRO_Backend_PrepareImmediateDraw( void )
{
	if( gXRGL.BindFramebuffer && s_fbo )
		gXRGL.BindFramebuffer( GL_FRAMEBUFFER, s_fbo );
	if( gXRGL.Viewport )
		gXRGL.Viewport( 0, 0, CSRETRO_OFFSCREEN_SIZE, CSRETRO_OFFSCREEN_SIZE );
	if( gXRGL.UseProgram )
		gXRGL.UseProgram( 0 );
	if( gXRGL.BindVertexArray )
		gXRGL.BindVertexArray( 0 );
	if( gXRGL.BindBuffer )
	{
		gXRGL.BindBuffer( GL_ARRAY_BUFFER, 0 );
		gXRGL.BindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	}
	if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE1 );
		gXRGL.Disable( GL_TEXTURE_2D );
		gXRGL.ActiveTexture( GL_TEXTURE0 );
	}
	if( gXRGL.Enable )
	{
		gXRGL.Enable( GL_TEXTURE_2D );
		gXRGL.Enable( GL_DEPTH_TEST );
		gXRGL.Enable( GL_CULL_FACE );
	}
	if( gXRGL.DepthMask )
		gXRGL.DepthMask( GL_TRUE );
	if( gXRGL.DepthFunc )
		gXRGL.DepthFunc( GL_LEQUAL );
	if( gXRGL.Disable )
	{
		gXRGL.Disable( GL_BLEND );
		gXRGL.Disable( GL_POLYGON_OFFSET_FILL );
	}
	if( gXRGL.Color4f )
		gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	if( gXRGL.DepthRange )
		gXRGL.DepthRange( 0.0, 1.0 );
	if( gXRGL.PolygonMode )
		gXRGL.PolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	if( gXRGL.ShadeModel )
		gXRGL.ShadeModel( GL_FLAT );
	if( gXRGL.TexEnvi )
		gXRGL.TexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}

void CSRETRO_Backend_BindTexture( int tmu, unsigned int texnum )
{
	if( s_api && s_api->GL_Bind )
		s_api->GL_Bind( tmu, texnum );
	else if( gXRGL.ActiveTexture && gXRGL.BindTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE0 + (unsigned int)tmu );
		gXRGL.BindTexture( GL_TEXTURE_2D, texnum );
	}
}

void CSRETRO_Backend_CleanupTextures( void )
{
	if( s_api && s_api->GL_CleanUpTextureUnits )
		s_api->GL_CleanUpTextureUnits( 0 );
	else if( gXRGL.ActiveTexture )
	{
		gXRGL.ActiveTexture( GL_TEXTURE1 );
		gXRGL.Disable( GL_TEXTURE_2D );
		gXRGL.ActiveTexture( GL_TEXTURE0 );
	}
}

static unsigned int CRC32_Buf( const unsigned char *data, int len )
{
	unsigned int crc = 0xFFFFFFFFu;
	int i, j;
	for( i = 0; i < len; i++ )
	{
		crc ^= data[i];
		for( j = 0; j < 8; j++ )
			crc = ( crc >> 1 ) ^ ( 0xEDB88320u & (unsigned int)-(int)( crc & 1u ) );
	}
	return crc ^ 0xFFFFFFFFu;
}

static void FillProof( CSRETRO_OffscreenProof *proof, int write_ppm )
{
	unsigned char *pixels = NULL;
	int i, nonempty = 0;
	const int count = CSRETRO_OFFSCREEN_SIZE * CSRETRO_OFFSCREEN_SIZE;

	if( proof )
		memset( proof, 0, sizeof( *proof ) );
	if( !gXRGL.ReadPixels )
	{
		if( proof )
			proof->target_ok = 1;
		return;
	}

	pixels = (unsigned char *)malloc( (size_t)count * 4 );
	if( !pixels )
	{
		if( proof )
			proof->target_ok = 1;
		return;
	}
	if( gXRGL.PixelStorei )
		gXRGL.PixelStorei( GL_PACK_ALIGNMENT, 1 );
	gXRGL.ReadPixels( 0, 0, CSRETRO_OFFSCREEN_SIZE, CSRETRO_OFFSCREEN_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
	for( i = 0; i < count; i++ )
	{
		const unsigned char *p = pixels + i * 4;
		if( p[0] > 20 || p[1] > 20 || p[2] > 40 )
			nonempty++;
	}
	if( proof )
	{
		proof->target_ok = 1;
		proof->width = CSRETRO_OFFSCREEN_SIZE;
		proof->height = CSRETRO_OFFSCREEN_SIZE;
		proof->nonempty_pixels = nonempty;
		proof->empty = nonempty == 0;
		if( s_api && s_api->pfnFileBufferCRC32 )
			proof->crc = s_api->pfnFileBufferCRC32( pixels, count * 4 );
		else
			proof->crc = CRC32_Buf( pixels, count * 4 );
	}
	if( write_ppm && !s_dumped_ppm && s_api && s_api->pfnSaveFile && nonempty > 0 )
	{
		const int rgb_len = count * 3;
		unsigned char *ppm = (unsigned char *)malloc( (size_t)rgb_len + 64 );
		if( ppm )
		{
			int hdr = snprintf( (char *)ppm, 64, "P6\n%i %i\n255\n",
				CSRETRO_OFFSCREEN_SIZE, CSRETRO_OFFSCREEN_SIZE );
			int p;
			unsigned char *dst = ppm + hdr;
			for( p = 0; p < count; p++ )
			{
				dst[0] = pixels[p * 4 + 0];
				dst[1] = pixels[p * 4 + 1];
				dst[2] = pixels[p * 4 + 2];
				dst += 3;
			}
			if( s_api->pfnSaveFile( "csretro_offscreen.ppm", ppm, hdr + rgb_len ) )
				s_dumped_ppm = 1;
			free( ppm );
		}
	}
	free( pixels );
}

void CSRETRO_Backend_SampleProof( CSRETRO_OffscreenProof *proof )
{
	FillProof( proof, 0 );
}

void CSRETRO_Backend_EndOffscreen( CSRETRO_OffscreenProof *proof, int do_readback )
{
	if( do_readback )
		FillProof( proof, 1 );
	else if( proof )
	{
		memset( proof, 0, sizeof( *proof ) );
		proof->target_ok = 1;
	}

	RestoreState();
	if( s_api && s_api->GL_CleanUpTextureUnits )
		s_api->GL_CleanUpTextureUnits( 0 );
}
