#include "window_geometry.h"

#include "../src/menu_priv.h"

#include <cerrno>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace CsretroWindowGeometry
{
namespace
{
// CS-Retro-owned, always under XASH3D_BASEDIR (never Steam/GameData).
bool ResolvePath(char *out, size_t outSize)
{
	const char *basedir = getenv("XASH3D_BASEDIR");
	if (!basedir || !*basedir)
		basedir = getenv("CSRETRO_RUN_DIR");
	if (!basedir || !*basedir)
		return false;
	snprintf(out, outSize, "%s/cfg/csretro_ui_geometry.txt", basedir);
	return true;
}

bool EnsureParentDir(const char *filePath)
{
	char dir[1024];
	snprintf(dir, sizeof(dir), "%s", filePath);
	char *slash = strrchr(dir, '/');
	if (!slash)
		return false;
	*slash = '\0';
#if defined(_WIN32)
	return _mkdir(dir) == 0 || errno == EEXIST;
#else
	struct stat st{};
	if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode))
		return true;
	return mkdir(dir, 0755) == 0 || errno == EEXIST;
#endif
}
} // namespace

void ClampToWorkspace(int &x, int &y, int w, int h, int workX, int workY, int workW, int workH)
{
	ClampBounds(x, y, w, h, 1, 1, workX, workY, workW, workH);
}

void ClampBounds(int &x, int &y, int &w, int &h, int minW, int minH,
	int workX, int workY, int workW, int workH)
{
	if (minW < 1)
		minW = 1;
	if (minH < 1)
		minH = 1;
	if (workW < 1)
		workW = minW;
	if (workH < 1)
		workH = minH;
	w = std::max(minW, std::min(w, workW));
	h = std::max(minH, std::min(h, workH));
	// Keep the complete window inside whenever it fits. Only fall back to a
	// reachable title/edge strip when the declared minimum is larger than the
	// workspace and complete containment is mathematically impossible.
	const int title = 28;
	const int minVisibleX = 32;
	const bool fitsX = w <= workW;
	const bool fitsY = h <= workH;
	const int minX = fitsX ? workX : workX + minVisibleX - w;
	const int maxX = fitsX ? workX + workW - w : workX + workW - minVisibleX;
	const int minY = workY;
	const int maxY = fitsY ? workY + workH - h : workY + workH - title;
	x = std::max(minX, std::min(x, maxX));
	y = std::max(minY, std::min(y, maxY));
}

Bounds Load(const char *windowId)
{
	Bounds out;
	if (!windowId || !*windowId)
		return out;
	char path[1024];
	if (!ResolvePath(path, sizeof(path)))
	{
		Menu_Con("CSRETRO_UI_GEOM_LOAD_FAIL no_basedir id=%s", windowId);
		return out;
	}
	FILE *fp = fopen(path, "r");
	if (!fp)
		return out;
	char line[256];
	char id[64];
	int x = 0, y = 0, w = 0, h = 0;
	while (fgets(line, sizeof(line), fp))
	{
		if (sscanf(line, "%63s %d %d %d %d", id, &x, &y, &w, &h) == 5 && !strcmp(id, windowId))
		{
			out.x = x;
			out.y = y;
			out.w = w;
			out.h = h;
			out.valid = w > 0 && h > 0;
			break;
		}
	}
	fclose(fp);
	if (out.valid)
		Menu_Con("CSRETRO_UI_GEOM_LOAD %s %d,%d %dx%d path=%s", windowId, out.x, out.y, out.w, out.h, path);
	return out;
}

void Save(const char *windowId, int x, int y, int w, int h)
{
	if (!windowId || !*windowId || w <= 0 || h <= 0)
		return;
	char path[1024];
	if (!ResolvePath(path, sizeof(path)))
	{
		Menu_Con("CSRETRO_UI_GEOM_SAVE_FAIL no_basedir id=%s", windowId);
		return;
	}
	if (!EnsureParentDir(path))
	{
		Menu_Con("CSRETRO_UI_GEOM_SAVE_FAIL mkdir path=%s", path);
		return;
	}

	// Merge: read existing, rewrite all ids, replace/add this one.
	struct Entry
	{
		char id[64];
		int x, y, w, h;
	};
	Entry entries[32];
	int n = 0;
	FILE *in = fopen(path, "r");
	if (in)
	{
		char line[256];
		while (n < 32 && fgets(line, sizeof(line), in))
		{
			Entry &e = entries[n];
			if (sscanf(line, "%63s %d %d %d %d", e.id, &e.x, &e.y, &e.w, &e.h) == 5)
			{
				if (strcmp(e.id, windowId) != 0)
					++n;
			}
		}
		fclose(in);
	}
	if (n < 32)
	{
		Entry &e = entries[n++];
		snprintf(e.id, sizeof(e.id), "%s", windowId);
		e.x = x;
		e.y = y;
		e.w = w;
		e.h = h;
	}

	FILE *out = fopen(path, "w");
	if (!out)
	{
		Menu_Con("CSRETRO_UI_GEOM_SAVE_FAIL open path=%s", path);
		return;
	}
	for (int i = 0; i < n; ++i)
		fprintf(out, "%s %d %d %d %d\n", entries[i].id, entries[i].x, entries[i].y, entries[i].w, entries[i].h);
	fclose(out);
	Menu_Con("CSRETRO_UI_GEOM_SAVE %s %d,%d %dx%d path=%s", windowId, x, y, w, h, path);
}
} // namespace CsretroWindowGeometry
