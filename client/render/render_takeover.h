// PX6A Mode-2 eligibility / commit gate. Mode 0/1 unchanged.
#pragma once

struct ref_viewpass_s;
struct csretro_custom_frame_info_s;

typedef struct CSRETRO_TakeoverProof_s
{
	int mode;
	int eligible;
	int preflight_ok;
	int committed;
	int present_ok;
	int return_code;
	int viewport_w;
	int viewport_h;
	int fbo_w;
	int fbo_h;
	int framecount_before;
	int framecount_after;
	int dlight_pushes;
	int player_light;
	int vis_consumed;
	int efx_solid;
	int efx_trans;
	int tri_normal_owned;
	int tri_trans_owned;
	int extra_updates;
	int fog_pre;
	int fog_post;
	int fault_latched;
	int reject_reason; // CSRETRO_TAKEOVER_REJECT_*
} CSRETRO_TakeoverProof;

enum
{
	CSRETRO_TAKEOVER_OK = 0,
	CSRETRO_TAKEOVER_REJECT_FLAGS,
	CSRETRO_TAKEOVER_REJECT_OVERVIEW,
	CSRETRO_TAKEOVER_REJECT_CUBEMAP,
	CSRETRO_TAKEOVER_REJECT_CLIENTDRAW,
	CSRETRO_TAKEOVER_REJECT_RIPPLE,
	CSRETRO_TAKEOVER_REJECT_VIEWPORT,
	CSRETRO_TAKEOVER_REJECT_ALIAS,
	CSRETRO_TAKEOVER_REJECT_BACKEND,
	CSRETRO_TAKEOVER_REJECT_PRESENT,
	CSRETRO_TAKEOVER_REJECT_FBO,
	CSRETRO_TAKEOVER_REJECT_FAULT_LATCH,
	CSRETRO_TAKEOVER_REJECT_PREPARE
};

int CSRETRO_Renderer_Mode( void );
int CSRETRO_Takeover_Eligible( const struct ref_viewpass_s *rvp, int *reason_out );
int CSRETRO_Takeover_FaultLatched( void );
void CSRETRO_Takeover_LatchFault( void );
void CSRETRO_Takeover_ClearFault( void );
void CSRETRO_Takeover_ResetProof( void );
void CSRETRO_Takeover_GetProof( CSRETRO_TakeoverProof *out );
void CSRETRO_Takeover_NoteProof( const CSRETRO_TakeoverProof *src );
int CSRETRO_Scene_HasAlias( void );
