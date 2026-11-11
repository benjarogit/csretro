#pragma once

// Retail vgui2_surface when that pointer was resolved, otherwise the bound Xash surface.
void* VGUI2_Surface();

void VGUI2_Draw_Init();
void VGUI2_ResetCurrentTexture();
void VGUI2_Draw_SetTextColor(int r, int g, int b);

// Puts the engine's surface object on the engine text service, for the holders of that object we do
// not compile. Runs from Draw_Init, by which point the engine has resolved the surface: it reads a
// scheme font there.
void VGUI2_InstallTextSurface();

// Restores the surface slots on engine shutdown, before this module is unloaded.
void VGUI2_UninstallTextSurface();
