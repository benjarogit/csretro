// IFileSystem over POSIX — search paths from gamedata/cstrike + BASEDIR.
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>

#include "FileSystem.h"
#include "tier1/interface.h"

#if defined(__GNUC__) || defined(__clang__)
#define CSRETRO_FS_PRINTF_LIKE(fmt_index, first_arg) __attribute__((format(printf, fmt_index, first_arg)))
#else
#define CSRETRO_FS_PRINTF_LIKE(fmt_index, first_arg)
#endif

namespace
{
struct SearchPath
{
	std::string path;
	std::string pathID;
	bool writable = true;
};

struct FindState
{
	DIR *dir = nullptr;
	std::string pattern;
	std::string dirname;
	std::string lastName;
	bool lastIsDir = false;
	std::vector<std::string> roots;
	size_t rootIndex = 0;
};

std::vector<SearchPath> g_paths;
std::vector<FindState> g_finds;
FileWarningLevel_t g_warnLevel = FILESYSTEM_WARNING_QUIET;
void (*g_warnFn)(const char *fmt, ...) = nullptr;

std::string JoinPath(const std::string &a, const char *b)
{
	if (!b || !*b)
		return a;
	if (b[0] == '/')
		return b;
	std::string out = a;
	if (!out.empty() && out.back() != '/')
		out.push_back('/');
	out += b;
	return out;
}

bool ResolveExisting(const char *rel, char *out, int outLen)
{
	if (!rel || !out || outLen < 2)
		return false;
	if (rel[0] == '/')
	{
		strncpy(out, rel, static_cast<size_t>(outLen - 1));
		out[outLen - 1] = '\0';
		return access(out, F_OK) == 0;
	}
	for (const auto &sp : g_paths)
	{
		std::string full = JoinPath(sp.path, rel);
		if (access(full.c_str(), F_OK) == 0)
		{
			strncpy(out, full.c_str(), static_cast<size_t>(outLen - 1));
			out[outLen - 1] = '\0';
			return true;
		}
	}
	return false;
}

const char *WriteRoot(const char *pathID)
{
	for (const auto &sp : g_paths)
	{
		if (!sp.writable)
			continue;
		if (!pathID || !*pathID || sp.pathID == pathID)
			return sp.path.c_str();
	}
	return g_paths.empty() ? "." : g_paths.front().path.c_str();
}
} // namespace

class CFileSystemXash : public IFileSystem
{
public:
	void Mount() override {}
	void Unmount() override {}

	void RemoveAllSearchPaths() override { g_paths.clear(); }

	void AddSearchPath(const char *pPath, const char *pathID) override
	{
		if (!pPath || !*pPath)
			return;
		SearchPath sp;
		sp.path = pPath;
		while (!sp.path.empty() && sp.path.back() == '/')
			sp.path.pop_back();
		sp.pathID = pathID ? pathID : "";
		sp.writable = true;
		g_paths.push_back(std::move(sp));
	}

	bool RemoveSearchPath(const char *pPath) override
	{
		if (!pPath)
			return false;
		for (auto it = g_paths.begin(); it != g_paths.end(); ++it)
		{
			if (it->path == pPath)
			{
				g_paths.erase(it);
				return true;
			}
		}
		return false;
	}

	void RemoveFile(const char *pRelativePath, const char *) override
	{
		char full[1024];
		if (ResolveExisting(pRelativePath, full, sizeof(full)))
			::remove(full);
	}

	void CreateDirHierarchy(const char *path, const char *pathID) override
	{
		if (!path || !*path)
			return;
		std::string full;
		if (path[0] == '/')
			full = path;
		else
			full = JoinPath(WriteRoot(pathID), path);

		std::string cur;
		for (size_t i = 0; i < full.size(); ++i)
		{
			cur.push_back(full[i]);
			if (full[i] == '/' || i + 1 == full.size())
			{
				if (cur.size() > 1 && cur.back() == '/')
					cur.pop_back();
				if (!cur.empty() && cur != "/")
					mkdir(cur.c_str(), 0755);
				if (i + 1 != full.size() && !cur.empty() && cur.back() != '/')
					cur.push_back('/');
			}
		}
	}

	bool FileExists(const char *pFileName) override
	{
		char full[1024];
		return ResolveExisting(pFileName, full, sizeof(full));
	}

	bool IsDirectory(const char *pFileName) override
	{
		char full[1024];
		if (!ResolveExisting(pFileName, full, sizeof(full)))
			return false;
		struct stat st{};
		return stat(full, &st) == 0 && S_ISDIR(st.st_mode);
	}

	FileHandle_t Open(const char *pFileName, const char *pOptions, const char *) override
	{
		if (!pFileName || !pOptions)
			return FILESYSTEM_INVALID_HANDLE;
		char full[1024];
		bool write = strchr(pOptions, 'w') || strchr(pOptions, 'a') || strchr(pOptions, '+');
		if (write)
		{
			if (pFileName[0] == '/')
				strncpy(full, pFileName, sizeof(full) - 1);
			else
			{
				std::string f = JoinPath(WriteRoot(nullptr), pFileName);
				strncpy(full, f.c_str(), sizeof(full) - 1);
			}
			full[sizeof(full) - 1] = '\0';
		}
		else if (!ResolveExisting(pFileName, full, sizeof(full)))
			return FILESYSTEM_INVALID_HANDLE;

		FILE *fp = fopen(full, pOptions);
		return fp ? reinterpret_cast<FileHandle_t>(fp) : FILESYSTEM_INVALID_HANDLE;
	}

	void Close(FileHandle_t file) override
	{
		if (file)
			fclose(reinterpret_cast<FILE *>(file));
	}

	void Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType) override
	{
		if (!file)
			return;
		int whence = SEEK_SET;
		if (seekType == FILESYSTEM_SEEK_CURRENT)
			whence = SEEK_CUR;
		else if (seekType == FILESYSTEM_SEEK_TAIL)
			whence = SEEK_END;
		fseek(reinterpret_cast<FILE *>(file), pos, whence);
	}

	unsigned int Tell(FileHandle_t file) override
	{
		if (!file)
			return 0;
		long t = ftell(reinterpret_cast<FILE *>(file));
		return t < 0 ? 0 : static_cast<unsigned int>(t);
	}

	unsigned int Size(FileHandle_t file) override
	{
		if (!file)
			return 0;
		FILE *fp = reinterpret_cast<FILE *>(file);
		long cur = ftell(fp);
		fseek(fp, 0, SEEK_END);
		long end = ftell(fp);
		fseek(fp, cur, SEEK_SET);
		return end < 0 ? 0 : static_cast<unsigned int>(end);
	}

	unsigned int Size(const char *pFileName) override
	{
		char full[1024];
		if (!ResolveExisting(pFileName, full, sizeof(full)))
			return static_cast<unsigned int>(-1);
		struct stat st{};
		if (stat(full, &st) != 0)
			return static_cast<unsigned int>(-1);
		return static_cast<unsigned int>(st.st_size);
	}

	long GetFileTime(const char *pFileName) override
	{
		char full[1024];
		if (!ResolveExisting(pFileName, full, sizeof(full)))
			return 0;
		struct stat st{};
		if (stat(full, &st) != 0)
			return 0;
		return static_cast<long>(st.st_mtime);
	}

	void FileTimeToString(char *pStrip, int maxCharsIncludingTerminator, long fileTime) override
	{
		if (!pStrip || maxCharsIncludingTerminator < 1)
			return;
		time_t t = static_cast<time_t>(fileTime);
		char *s = ctime(&t);
		if (!s)
		{
			pStrip[0] = '\0';
			return;
		}
		strncpy(pStrip, s, static_cast<size_t>(maxCharsIncludingTerminator - 1));
		pStrip[maxCharsIncludingTerminator - 1] = '\0';
	}

	bool IsOk(FileHandle_t file) override { return file && ferror(reinterpret_cast<FILE *>(file)) == 0; }

	void Flush(FileHandle_t file) override
	{
		if (file)
			fflush(reinterpret_cast<FILE *>(file));
	}

	bool EndOfFile(FileHandle_t file) override { return !file || feof(reinterpret_cast<FILE *>(file)) != 0; }

	int Read(void *pOutput, int size, FileHandle_t file) override
	{
		if (!file || !pOutput || size <= 0)
			return 0;
		return static_cast<int>(fread(pOutput, 1, static_cast<size_t>(size), reinterpret_cast<FILE *>(file)));
	}

	int Write(void const *pInput, int size, FileHandle_t file) override
	{
		if (!file || !pInput || size <= 0)
			return 0;
		return static_cast<int>(fwrite(pInput, 1, static_cast<size_t>(size), reinterpret_cast<FILE *>(file)));
	}

	char *ReadLine(char *pOutput, int maxChars, FileHandle_t file) override
	{
		static char empty[] = "";
		if (!file)
			return empty;
		if (!pOutput || maxChars < 1)
			return nullptr;
		return fgets(pOutput, maxChars, reinterpret_cast<FILE *>(file));
	}

	int FPrintf(FileHandle_t file, const char *pFormat, ...) override CSRETRO_FS_PRINTF_LIKE(3, 4)
	{
		if (!file || !pFormat)
			return -1;
		va_list ap;
		va_start(ap, pFormat);
		int n = vfprintf(reinterpret_cast<FILE *>(file), pFormat, ap);
		va_end(ap);
		return n;
	}

	void *GetReadBuffer(FileHandle_t, int *outBufferSize, bool) override
	{
		if (outBufferSize)
			*outBufferSize = 0;
		return nullptr;
	}
	void ReleaseReadBuffer(FileHandle_t, void *) override {}

	const char *FindFirst(const char *pWildCard, FileFindHandle_t *pHandle, const char *) override
	{
		if (!pWildCard || !pHandle)
			return nullptr;

		FindState st;
		std::string wild = pWildCard;
		auto slash = wild.find_last_of('/');
		if (slash != std::string::npos)
		{
			st.dirname = wild.substr(0, slash);
			st.pattern = wild.substr(slash + 1);
		}
		else
		{
			st.dirname.clear();
			st.pattern = wild;
		}
		for (const auto &sp : g_paths)
			st.roots.push_back(JoinPath(sp.path, st.dirname.c_str()));
		if (st.roots.empty())
			st.roots.emplace_back(st.dirname.empty() ? "." : st.dirname);

		g_finds.push_back(std::move(st));
		*pHandle = static_cast<FileFindHandle_t>(g_finds.size() - 1);
		return FindNext(*pHandle);
	}

	const char *FindNext(FileFindHandle_t handle) override
	{
		if (handle < 0 || static_cast<size_t>(handle) >= g_finds.size())
			return nullptr;
		FindState &st = g_finds[static_cast<size_t>(handle)];
		for (;;)
		{
			if (!st.dir)
			{
				if (st.rootIndex >= st.roots.size())
					return nullptr;
				st.dir = opendir(st.roots[st.rootIndex].c_str());
				++st.rootIndex;
				if (!st.dir)
					continue;
			}
			dirent *ent = readdir(st.dir);
			if (!ent)
			{
				closedir(st.dir);
				st.dir = nullptr;
				continue;
			}
			if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
				continue;
			if (fnmatch(st.pattern.c_str(), ent->d_name, 0) != 0)
				continue;
			st.lastName = ent->d_name;
			std::string full = JoinPath(st.roots[st.rootIndex - 1], ent->d_name);
			struct stat stbuf{};
			st.lastIsDir = (stat(full.c_str(), &stbuf) == 0 && S_ISDIR(stbuf.st_mode));
			return st.lastName.c_str();
		}
	}

	bool FindIsDirectory(FileFindHandle_t handle) override
	{
		if (handle < 0 || static_cast<size_t>(handle) >= g_finds.size())
			return false;
		return g_finds[static_cast<size_t>(handle)].lastIsDir;
	}

	void FindClose(FileFindHandle_t handle) override
	{
		if (handle < 0 || static_cast<size_t>(handle) >= g_finds.size())
			return;
		FindState &st = g_finds[static_cast<size_t>(handle)];
		if (st.dir)
		{
			closedir(st.dir);
			st.dir = nullptr;
		}
		st.roots.clear();
	}

	void GetLocalCopy(const char *) override {}

	const char *GetLocalPath(const char *pFileName, char *pLocalPath, int localPathBufferSize) override
	{
		if (!pLocalPath || localPathBufferSize < 2)
			return nullptr;
		if (!ResolveExisting(pFileName, pLocalPath, localPathBufferSize))
			return nullptr;
		return pLocalPath;
	}

	char *ParseFile(char *pFileBytes, char *pToken, bool *pWasQuoted) override
	{
		if (pWasQuoted)
			*pWasQuoted = false;
		if (!pFileBytes || !pToken)
			return pFileBytes;
		pToken[0] = '\0';
		while (*pFileBytes && (*pFileBytes == ' ' || *pFileBytes == '\t' || *pFileBytes == '\r' || *pFileBytes == '\n'))
			++pFileBytes;
		if (!*pFileBytes)
			return pFileBytes;
		if (*pFileBytes == '"')
		{
			if (pWasQuoted)
				*pWasQuoted = true;
			++pFileBytes;
			char *o = pToken;
			while (*pFileBytes && *pFileBytes != '"')
				*o++ = *pFileBytes++;
			*o = '\0';
			if (*pFileBytes == '"')
				++pFileBytes;
			return pFileBytes;
		}
		char *o = pToken;
		while (*pFileBytes && !strchr(" \t\r\n{}()':", *pFileBytes))
			*o++ = *pFileBytes++;
		*o = '\0';
		return pFileBytes;
	}

	bool FullPathToRelativePath(const char *pFullpath, char *pRelative) override
	{
		if (!pFullpath || !pRelative)
			return false;
		for (const auto &sp : g_paths)
		{
			if (sp.path.empty())
				continue;
			size_t n = sp.path.size();
			if (strncmp(pFullpath, sp.path.c_str(), n) == 0 && (pFullpath[n] == '/' || pFullpath[n] == '\0'))
			{
				const char *rel = pFullpath + n;
				if (*rel == '/')
					++rel;
				strcpy(pRelative, rel);
				return true;
			}
		}
		return false;
	}

	bool GetCurrentDirectory(char *pDirectory, int maxlen) override
	{
		if (!pDirectory || maxlen < 2)
			return false;
		return getcwd(pDirectory, static_cast<size_t>(maxlen)) != nullptr;
	}

	void PrintOpenedFiles() override {}
	void SetWarningFunc(void (*pfnWarning)(const char *fmt, ...)) override { g_warnFn = pfnWarning; }
	void SetWarningLevel(FileWarningLevel_t level) override { g_warnLevel = level; }
	void LogLevelLoadStarted(const char *) override {}
	void LogLevelLoadFinished(const char *) override {}
	int HintResourceNeed(const char *, int) override { return 0; }
	int PauseResourcePreloading() override { return 0; }
	int ResumeResourcePreloading() override { return 0; }

	int SetVBuf(FileHandle_t stream, char *buffer, int mode, long size) override
	{
		if (!stream)
			return -1;
		return setvbuf(reinterpret_cast<FILE *>(stream), buffer, mode, static_cast<size_t>(size));
	}

	void GetInterfaceVersion(char *p, int maxlen) override
	{
		if (!p || maxlen < 1)
			return;
		strncpy(p, "CSRetro_FileSystem_Xash", static_cast<size_t>(maxlen - 1));
		p[maxlen - 1] = '\0';
	}

	bool IsFileImmediatelyAvailable(const char *) override { return true; }
	WaitForResourcesHandle_t WaitForResources(const char *) override { return 0; }
	bool GetWaitForResourcesProgress(WaitForResourcesHandle_t, float *progress, bool *complete) override
	{
		if (progress)
			*progress = 1.f;
		if (complete)
			*complete = true;
		return false;
	}
	void CancelWaitForResources(WaitForResourcesHandle_t) override {}
	bool IsAppReadyForOfflinePlay(int) override { return true; }
	bool AddPackFile(const char *, const char *) override { return false; }
	FileHandle_t OpenFromCacheForRead(const char *pFileName, const char *pOptions, const char *pathID) override
	{
		return Open(pFileName, pOptions, pathID);
	}
	void AddSearchPathNoWrite(const char *pPath, const char *pathID) override
	{
		AddSearchPath(pPath, pathID);
		if (!g_paths.empty())
			g_paths.back().writable = false;
	}
};

static CFileSystemXash g_FileSystemXash;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CFileSystemXash, IFileSystem, FILESYSTEM_INTERFACE_VERSION, g_FileSystemXash);

CFileSystemXash *Csretro_FileSystem()
{
	return &g_FileSystemXash;
}
