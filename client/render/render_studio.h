#pragma once

struct CSRETRO_SceneStats_s;

// Offscreen studio via GSMR. Non-player: StudioDrawModel(STUDIO_RENDER).
// Remote player: StudioDrawPlayerOffscreen on a local player_info_t copy.
// Never STUDIO_EVENTS. Visible Xash path is unchanged.
int CSRETRO_Studio_DrawList( struct CSRETRO_SceneStats_s *stats );
int CSRETRO_Studio_DrawPlayers( struct CSRETRO_SceneStats_s *stats );

// MOVETYPE_FOLLOW children. Non-player parent: StudioDrawModel(0).
// Player parent: isolated StudioDrawPlayerOffscreen(0) after B-path exists.
int CSRETRO_Studio_DrawFollow( struct CSRETRO_SceneStats_s *stats );

typedef struct CSRETRO_StudioPlayerProof_s
{
	int candidates;
	int drawn;
	int info_mutate;
	int entity_mutate;
	int events;
	int shadow_side_draw;
	unsigned int info_before;
	unsigned int info_after_offscreen;
	unsigned int info_after_visible;
	unsigned int info_local_final;
	unsigned int info_visible_final;
	unsigned int gait_local_final;
	unsigned int gait_visible_final;
	int before_eq_after_offscreen;
	int after_visible_ne_before;
	int local_eq_visible;
	int gait_local_eq_visible;
	unsigned int entity_live_before;
	unsigned int entity_live_after_offscreen;
	unsigned int entity_snap_after;
	int live_entity_eq;
	int models_t;
	int models_ct;
	int distinct_models;
	char model_a[64];
	char model_b[64];
	int local_mirrored;
	int local_firstperson;
	int local_spectator;
	int local_chase;
	int local_ineye;
	int local_thirdperson;
	int local_deferred;
	int follow_player_parent;
	int follow_player_drawn;
	int look_ready;
	float look_origin[3];
} CSRETRO_StudioPlayerProof;

void CSRETRO_Studio_GetPlayerProof( CSRETRO_StudioPlayerProof *out );
int CSRETRO_Studio_ProbeLookTarget( float *origin );
void CSRETRO_Studio_ResetPlayerProof( void );
