#pragma once

struct render_api_s;

enum
{
	CSRETRO_OFFSCREEN_SIZE = 512
};

typedef struct CSRETRO_OffscreenProof_s
{
	int target_ok;
	int draw_executed;
	int empty;
	unsigned int crc;
	int width;
	int height;
	int nonempty_pixels;
} CSRETRO_OffscreenProof;

int CSRETRO_Backend_Init( struct render_api_s *api );
void CSRETRO_Backend_Shutdown( void );
void CSRETRO_Backend_AllowDump( void );
int CSRETRO_Backend_Ready( void );

// Bind offscreen target, isolate GL state. 0 on failure (caller still returns 0 to Xash).
int CSRETRO_Backend_BeginOffscreen( void );
void CSRETRO_Backend_EndOffscreen( CSRETRO_OffscreenProof *proof, int do_readback );

// Pixelproof ReadPixels — play path keeps this off; probes set dump=1.
void CSRETRO_Backend_SetPixelProof( int enabled );
int CSRETRO_Backend_PixelProofEnabled( void );

// Read FBO while still bound. Does not restore GL state.
// No-op (target_ok=1, crc=0) when pixelproof is disabled.
void CSRETRO_Backend_SampleProof( CSRETRO_OffscreenProof *proof );
int CSRETRO_Backend_DumpPPM( const char *name );

void CSRETRO_Backend_ApplyView( const float *vieworg, const float *viewangles, float fov_x, float fov_y );
// Immediate-mode draw on the bound FBO. Does not restore Xash state.
void CSRETRO_Backend_PrepareImmediateDraw( void );
void CSRETRO_Backend_BindTexture( int tmu, unsigned int texnum );
void CSRETRO_Backend_SelectTexture( int tmu );
void CSRETRO_Backend_CleanupTextures( void );
/* After multitexture world/brush: resync Xash TMU + studio-safe GL state. */
void CSRETRO_Backend_SyncTextureUnits( void );
unsigned int CSRETRO_Backend_WhiteTexture( void );

// Fog is TriAPI render-state. Push/Pop around offscreen triangle draws so Xash-visible fog stays put.
void CSRETRO_Backend_PushFog( void );
void CSRETRO_Backend_PopFog( void );

// Mode-2 viewport-sized takeover target. Separate from 512 diagnostic FBO.
int CSRETRO_Backend_TakeoverPresentCapable( void );
int CSRETRO_Backend_EnsureTakeoverTarget( int w, int h );
int CSRETRO_Backend_BeginTakeover( int w, int h );
int CSRETRO_Backend_PresentTakeover( int dst_x, int dst_y, int dst_w, int dst_h );
void CSRETRO_Backend_EndTakeover( void );
void CSRETRO_Backend_TakeoverSize( int *w, int *h );
/* Drain/report GL errors. Returns first error (0 = clean). Logs once per stage. */
unsigned int CSRETRO_Backend_CheckGL( const char *stage );

typedef struct CSRETRO_GL_s
{
	void ( *ClearColor )( float r, float g, float b, float a );
	void ( *Clear )( unsigned int mask );
	void ( *Enable )( unsigned int cap );
	void ( *Disable )( unsigned int cap );
	void ( *Viewport )( int x, int y, int w, int h );
	void ( *Scissor )( int x, int y, int w, int h );
	void ( *DepthMask )( unsigned char flag );
	void ( *DepthFunc )( unsigned int func );
	void ( *BlendFunc )( unsigned int s, unsigned int d );
	void ( *CullFace )( unsigned int mode );
	void ( *FrontFace )( unsigned int mode );
	void ( *Color4f )( float r, float g, float b, float a );
	void ( *Begin )( unsigned int mode );
	void ( *End )( void );
	void ( *Vertex3f )( float x, float y, float z );
	void ( *TexCoord2f )( float s, float t );
	void ( *MultiTexCoord2f )( unsigned int target, float s, float t );
	void ( *ActiveTexture )( unsigned int texture );
	void ( *MatrixMode )( unsigned int mode );
	void ( *LoadIdentity )( void );
	void ( *PushMatrix )( void );
	void ( *PopMatrix )( void );
	void ( *Frustum )( double l, double r, double b, double t, double n, double f );
	void ( *Rotatef )( float a, float x, float y, float z );
	void ( *Translatef )( float x, float y, float z );
	void ( *ReadPixels )( int x, int y, int w, int h, unsigned int format, unsigned int type, void *data );
	void ( *PixelStorei )( unsigned int pname, int param );
	void ( *GetIntegerv )( unsigned int pname, int *params );
	void ( *GetBooleanv )( unsigned int pname, unsigned char *params );
	void ( *GetFloatv )( unsigned int pname, float *params );
	unsigned char ( *IsEnabled )( unsigned int cap );
	void ( *BindTexture )( unsigned int target, unsigned int texture );
	void ( *BindFramebuffer )( unsigned int target, unsigned int framebuffer );
	void ( *UseProgram )( unsigned int program );
	void ( *BindBuffer )( unsigned int target, unsigned int buffer );
	void ( *BindVertexArray )( unsigned int array );
	void ( *ColorMask )( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	void ( *TexEnvi )( unsigned int target, unsigned int pname, int param );
	void ( *GetTexEnviv )( unsigned int target, unsigned int pname, int *params );
	unsigned int ( *GetError )( void );
	void ( *AlphaFunc )( unsigned int func, float ref );
	void ( *BlendEquation )( unsigned int mode );
	void ( *Vertex3fv )( const float *v );
	void ( *DepthRange )( double n, double f );
	void ( *PolygonMode )( unsigned int face, unsigned int mode );
	void ( *ShadeModel )( unsigned int mode );
	void ( *PolygonOffset )( float factor, float units );
	void ( *Fogi )( unsigned int pname, int param );
	void ( *Fogf )( unsigned int pname, float param );
	void ( *Fogfv )( unsigned int pname, const float *params );
} CSRETRO_GL;

extern CSRETRO_GL gXRGL;
