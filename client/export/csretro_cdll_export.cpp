#include "hud.h"
#include "exportdef.h"

extern "C" void HUD_ChatInputPosition( int *x, int *y );

/*
 * ABI: Feldreihenfolge und Zeigerbreite = engine/engine/cdll_exp.h.
 * Ref-A-APIProxy-cldll_func_t ist inkompatibel (HUD_GetPlayerTeam auf pfnGetRenderInterface).
 * Eigenes Struct: Xash-cdll_exp.h nicht includen (typedef-Konflikt mit APIProxy).
 */
struct xash_cldll_func_t
{
	int	(*pfnInitialize)( cl_enginefunc_t *pEnginefuncs, int iVersion );
	void	(*pfnInit)( void );
	int	(*pfnVidInit)( void );
	int	(*pfnRedraw)( float flTime, int intermission );
	int	(*pfnUpdateClientData)( client_data_t *cdata, float flTime );
	void	(*pfnReset)( void );
	void	(*pfnPlayerMove)( struct playermove_s *ppmove, int server );
	void	(*pfnPlayerMoveInit)( struct playermove_s *ppmove );
	char	(*pfnPlayerMoveTexture)( char *name );
	void	(*IN_ActivateMouse)( void );
	void	(*IN_DeactivateMouse)( void );
	void	(*IN_MouseEvent)( int mstate );
	void	(*IN_ClearStates)( void );
	void	(*IN_Accumulate)( void );
	void	(*CL_CreateMove)( float frametime, struct usercmd_s *cmd, int active );
	int	(*CL_IsThirdPerson)( void );
	void	(*CL_CameraOffset)( float *ofs );
	void	*(*KB_Find)( const char *name );
	void	(*CAM_Think)( void );
	void	(*pfnCalcRefdef)( struct ref_params_s *pparams );
	int	(*pfnAddEntity)( int type, struct cl_entity_s *ent, const char *modelname );
	void	(*pfnCreateEntities)( void );
	void	(*pfnDrawNormalTriangles)( void );
	void	(*pfnDrawTransparentTriangles)( void );
	void	(*pfnStudioEvent)( const struct mstudioevent_s *event, const struct cl_entity_s *entity );
	void	(*pfnPostRunCmd)( struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed );
	void	(*pfnShutdown)( void );
	void	(*pfnTxferLocalOverrides)( struct entity_state_s *state, const struct clientdata_s *client );
	void	(*pfnProcessPlayerState)( struct entity_state_s *dst, const struct entity_state_s *src );
	void	(*pfnTxferPredictionData)( struct entity_state_s *ps, const struct entity_state_s *pps, struct clientdata_s *pcd, const struct clientdata_s *ppcd, struct weapon_data_s *wd, const struct weapon_data_s *pwd );
	void	(*pfnDemo_ReadBuffer)( int size, unsigned char *buffer );
	int	(*pfnConnectionlessPacket)( const struct netadr_s *net_from, const char *args, char *buffer, int *size );
	int	(*pfnGetHullBounds)( int hullnumber, float *mins, float *maxs );
	void	(*pfnFrame)( double time );
	int	(*pfnKey_Event)( int eventcode, int keynum, const char *pszCurrentBinding );
	void	(*pfnTempEntUpdate)( double frametime, double client_time, double cl_gravity, struct tempent_s **ppTempEntFree, struct tempent_s **ppTempEntActive, int ( *Callback_AddVisibleEntity )( struct cl_entity_s *pEntity ), void ( *Callback_TempEntPlaySound )( struct tempent_s *pTemp, float damp ));
	struct cl_entity_s *(*pfnGetUserEntity)( int index );
	void	(*pfnVoiceStatus)( int entindex, int bTalking );
	void	(*pfnDirectorMessage)( int iSize, void *pbuf );
	int	(*pfnGetStudioModelInterface)( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio );
	void	(*pfnChatInputPosition)( int *x, int *y );
	int	(*pfnGetRenderInterface)( int version, render_api_t *renderfuncs, render_interface_t *callback );
	void	(*pfnMoveEvent)( float forwardmove, float sidemove );
	void	(*pfnLookEvent)( float relyaw, float relpitch );
};

extern "C" DLLEXPORT int GetClientAPI( struct xash_cldll_func_t *p )
{
	if( !p )
		return 0;

	*p = xash_cldll_func_t{};

	p->pfnInitialize = Initialize;
	p->pfnInit = HUD_Init;
	p->pfnVidInit = HUD_VidInit;
	p->pfnRedraw = HUD_Redraw;
	p->pfnUpdateClientData = HUD_UpdateClientData;
	p->pfnReset = HUD_Reset;
	p->pfnPlayerMove = HUD_PlayerMove;
	p->pfnPlayerMoveInit = HUD_PlayerMoveInit;
	p->pfnPlayerMoveTexture = HUD_PlayerMoveTexture;
	p->IN_ActivateMouse = IN_ActivateMouse;
	p->IN_DeactivateMouse = IN_DeactivateMouse;
	p->IN_MouseEvent = IN_MouseEvent;
	p->IN_ClearStates = IN_ClearStates;
	p->IN_Accumulate = IN_Accumulate;
	p->CL_CreateMove = CL_CreateMove;
	p->CL_IsThirdPerson = CL_IsThirdPerson;
	p->CL_CameraOffset = CL_CameraOffset;
	p->KB_Find = reinterpret_cast<void *(*)( const char * )>( KB_Find );
	p->CAM_Think = CAM_Think;
	p->pfnCalcRefdef = V_CalcRefdef;
	p->pfnAddEntity = HUD_AddEntity;
	p->pfnCreateEntities = HUD_CreateEntities;
	p->pfnDrawNormalTriangles = HUD_DrawNormalTriangles;
	p->pfnDrawTransparentTriangles = HUD_DrawTransparentTriangles;
	p->pfnStudioEvent = reinterpret_cast<void (*)( const struct mstudioevent_s *, const struct cl_entity_s * )>( HUD_StudioEvent );
	p->pfnPostRunCmd = HUD_PostRunCmd;
	p->pfnShutdown = HUD_Shutdown;
	p->pfnTxferLocalOverrides = HUD_TxferLocalOverrides;
	p->pfnProcessPlayerState = HUD_ProcessPlayerState;
	p->pfnTxferPredictionData = HUD_TxferPredictionData;
	p->pfnDemo_ReadBuffer = Demo_ReadBuffer;
	p->pfnConnectionlessPacket = HUD_ConnectionlessPacket;
	p->pfnGetHullBounds = HUD_GetHullBounds;
	p->pfnFrame = HUD_Frame;
	p->pfnKey_Event = HUD_Key_Event;
	p->pfnTempEntUpdate = HUD_TempEntUpdate;
	p->pfnGetUserEntity = HUD_GetUserEntity;
	p->pfnVoiceStatus = reinterpret_cast<void (*)( int, int )>( HUD_VoiceStatus );
	p->pfnDirectorMessage = HUD_DirectorMessage;
	p->pfnGetStudioModelInterface = HUD_GetStudioModelInterface;
	p->pfnChatInputPosition = HUD_ChatInputPosition;
	p->pfnGetRenderInterface = HUD_GetRenderInterface;
	p->pfnMoveEvent = IN_ClientMoveEvent;
	p->pfnLookEvent = IN_ClientLookEvent;

	return 1;
}
