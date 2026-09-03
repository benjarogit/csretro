#pragma once

// CS-Retro-owned window geometry persistence (not Steam/GameData).
// Stable IDs → user config under BASEDIR. Clamp to workspace on restore.

namespace CsretroWindowGeometry
{
struct Bounds
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	bool valid = false;
};

// Load saved bounds for id (e.g. "Options"). invalid if missing/corrupt.
Bounds Load(const char *windowId);

// Persist bounds. Writes cfg/csretro_ui_geometry.txt (GAMECONFIG / BASEDIR).
void Save(const char *windowId, int x, int y, int w, int h);

// Clamp position so the title bar stays on-screen (legacy; size unchanged).
void ClampToWorkspace(int &x, int &y, int w, int h, int workX, int workY, int workW, int workH);

// Clamp size to [minW, workspace] then position. Title bar never offscreen.
void ClampBounds(int &x, int &y, int &w, int &h, int minW, int minH,
	int workX, int workY, int workW, int workH);
} // namespace CsretroWindowGeometry
