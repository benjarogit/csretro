#pragma once

struct cl_entity_s;

enum
{
	CSRETRO_COPY_MAX = 2048
};

typedef struct CSRETRO_EntCopy_s
{
	int type;
	int index;
	int player;
	int model_type;
	void *model;
	float origin[3];
	float angles[3];
	int rendermode;
	int renderfx;
	int renderamt;
	unsigned char rendercolor[3];
	float scale;
	float frame;
	int effects;
	int movetype;
	int aiment;
	int body;
	int kind; // CSRETRO_KIND_*
} CSRETRO_EntCopy;

enum
{
	CSRETRO_KIND_OTHER = 0,
	CSRETRO_KIND_TENT_SPRITE,
	CSRETRO_KIND_NORMAL_SPRITE,
	CSRETRO_KIND_BRUSH,
	CSRETRO_KIND_STUDIO,
	CSRETRO_KIND_STUDIO_LOCAL,
	CSRETRO_KIND_BEAM
};

typedef struct CSRETRO_SceneStats_s
{
	int mirrored;
	int tent_sprite;
	int normal_sprite;
	int brush;
	int studio;
	int studio_local;
	int other;
	int tent_drawn;
	int normal_drawn;
	int overflow;
} CSRETRO_SceneStats;

void CSRETRO_Scene_Clear( void );
void CSRETRO_Scene_Add( int type, struct cl_entity_s *ent );
int CSRETRO_Scene_Count( void );
const CSRETRO_EntCopy *CSRETRO_Scene_Get( int index );
void CSRETRO_Scene_GetStats( CSRETRO_SceneStats *out );
void CSRETRO_Scene_NoteDrawn( int kind );
