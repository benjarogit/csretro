#include "font_resolver.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
std::vector<std::string> g_searchDirs;
bool g_seeded = false;

bool DirExists(const char *path)
{
	if (!path || !*path)
		return false;
	struct stat st {};
	return ::stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool FileExists(const char *path)
{
	if (!path || !*path)
		return false;
	struct stat st {};
	return ::stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void AddDirUnique(const char *dir)
{
	if (!dir || !*dir || !DirExists(dir))
		return;
	for (const auto &d : g_searchDirs)
	{
		if (d == dir)
			return;
	}
	g_searchDirs.emplace_back(dir);
}

void SeedSearchDirs()
{
	if (g_seeded)
		return;
	g_seeded = true;

	if (const char *uiFonts = std::getenv("CSRETRO_UI_FONTS"))
		AddDirUnique(uiFonts);

	if (const char *rodir = std::getenv("XASH3D_RODIR"))
	{
		std::string p = std::string(rodir) + "/platform/resource/linux_fonts";
		AddDirUnique(p.c_str());
	}

	AddDirUnique("gamedata/platform/resource/linux_fonts");
	AddDirUnique("platform/resource/linux_fonts");
	AddDirUnique("./platform/resource/linux_fonts");

	// Known system fallbacks (not a single hard-coded file).
	AddDirUnique("/usr/share/fonts/TTF");
	AddDirUnique("/usr/share/fonts/truetype/dejavu");
	AddDirUnique("/usr/share/fonts/truetype/liberation");
	AddDirUnique("/usr/share/fonts/liberation");
	AddDirUnique("/usr/local/share/fonts");
}

std::string JoinPath(const std::string &dir, const char *file)
{
	if (dir.empty())
		return file ? file : "";
	if (dir.back() == '/')
		return dir + file;
	return dir + "/" + file;
}

bool EndsWithIgnoreCase(const char *s, const char *suffix)
{
	if (!s || !suffix)
		return false;
	const size_t n = std::strlen(s);
	const size_t m = std::strlen(suffix);
	if (m > n)
		return false;
	return strcasecmp(s + (n - m), suffix) == 0;
}

std::string FindExactInDirs(const char *fileName)
{
	if (!fileName || !*fileName)
		return {};
	for (const auto &dir : g_searchDirs)
	{
		const std::string path = JoinPath(dir, fileName);
		if (FileExists(path.c_str()))
			return path;
	}
	// Scan dirs for case-insensitive basename match.
	for (const auto &dir : g_searchDirs)
	{
		DIR *d = opendir(dir.c_str());
		if (!d)
			continue;
		while (dirent *ent = readdir(d))
		{
			if (ent->d_name[0] == '.')
				continue;
			if (!EndsWithIgnoreCase(ent->d_name, ".ttf") && !EndsWithIgnoreCase(ent->d_name, ".otf"))
				continue;
			if (strcasecmp(ent->d_name, fileName) == 0)
			{
				std::string path = JoinPath(dir, ent->d_name);
				closedir(d);
				return path;
			}
		}
		closedir(d);
	}
	return {};
}

void MapFamilyCandidates(const char *family, int weight, std::vector<const char *> &out)
{
	out.clear();
	const bool bold = weight >= 600;
	const bool mono =
	    family && (strcasecmp(family, "Courier New") == 0 || strcasecmp(family, "Courier") == 0 ||
	               strcasecmp(family, "Consolas") == 0 || strcasecmp(family, "Lucida Console") == 0);

	if (mono)
	{
		if (bold)
		{
			out.push_back("DejaVuSansMono-Bold.ttf");
			out.push_back("LiberationMono-Bold.ttf");
		}
		out.push_back("DejaVuSansMono.ttf");
		out.push_back("LiberationMono-Regular.ttf");
		return;
	}

	// Tahoma / Verdana / Arial / Trebuchet MS / generic sans → DejaVu / Liberation
	if (bold)
	{
		out.push_back("DejaVuSans-Bold.ttf");
		out.push_back("LiberationSans-Bold.ttf");
	}
	out.push_back("DejaVuSans.ttf");
	out.push_back("LiberationSans-Regular.ttf");
}

bool LooksLikeFontFile(const char *s)
{
	return EndsWithIgnoreCase(s, ".ttf") || EndsWithIgnoreCase(s, ".otf") || EndsWithIgnoreCase(s, ".ttc");
}
} // namespace

void Csretro_AddFontSearchDir(const char *dir)
{
	SeedSearchDirs();
	AddDirUnique(dir);
}

std::string Csretro_ResolveFontFile(const char *familyOrFile, int weight)
{
	SeedSearchDirs();

	if (familyOrFile && *familyOrFile)
	{
		// Absolute/relative path that already exists.
		if (FileExists(familyOrFile))
			return familyOrFile;

		// Bare filename → exact match in search dirs.
		if (LooksLikeFontFile(familyOrFile))
		{
			const char *base = familyOrFile;
			if (const char *slash = strrchr(familyOrFile, '/'))
				base = slash + 1;
			std::string found = FindExactInDirs(base);
			if (!found.empty())
				return found;
		}
		else
		{
			// Try "<Family With Spaces → Family-With-Spaces>.ttf"
			std::string tryName = std::string(familyOrFile) + ".ttf";
			for (char &c : tryName)
			{
				if (c == ' ')
					c = '-';
			}
			std::string found = FindExactInDirs(tryName.c_str());
			if (!found.empty())
				return found;
		}
	}

	std::vector<const char *> candidates;
	MapFamilyCandidates(familyOrFile, weight, candidates);
	for (const char *cand : candidates)
	{
		std::string found = FindExactInDirs(cand);
		if (!found.empty())
			return found;
	}

	// Last-resort: any DejaVuSans / LiberationSans in search dirs.
	static const char *kLast[] = {"DejaVuSans.ttf", "LiberationSans-Regular.ttf", "FiraSans-Regular.ttf"};
	for (const char *cand : kLast)
	{
		std::string found = FindExactInDirs(cand);
		if (!found.empty())
			return found;
	}
	return {};
}
