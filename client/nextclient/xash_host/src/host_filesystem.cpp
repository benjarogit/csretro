#include "host_filesystem.h"

#include "xash_host/xash_host.h"

#include <FileSystem.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace
{

    bool IsReadMode(const char* options)
    {
        return options != nullptr &&
            (std::strcmp(options, "r") == 0 || std::strcmp(options, "rb") == 0 || std::strcmp(options, "rt") == 0);
    }

    // Keeps the path lexical. ".." is a miss, not a path the proxy invents.
    bool RelativeUnderRoot(const char* name, std::string* relative)
    {
        if (name == nullptr || name[0] == '\0' || name[0] == '/' || name[0] == '\\')
        {
            return false;
        }

        std::string out;
        const char* cursor = name;
        while (*cursor != '\0')
        {
            const char* end = cursor;
            while (*end != '\0' && *end != '/' && *end != '\\')
            {
                ++end;
            }
            const auto length = static_cast<std::size_t>(end - cursor);
            if (length == 1 && cursor[0] == '.')
            {
            }
            else if (length == 2 && cursor[0] == '.' && cursor[1] == '.')
            {
                return false;
            }
            else if (length > 0)
            {
                if (!out.empty())
                {
                    out.push_back('/');
                }
                out.append(cursor, length);
            }
            if (*end == '\0')
            {
                break;
            }
            cursor = end + 1;
        }
        if (out.empty())
        {
            return false;
        }
        *relative = out;
        return true;
    }

    class HostFileSystem : public IFileSystem
    {
    public:
        int SetRoot(const char* root)
        {
            if (root == nullptr || root[0] == '\0')
            {
                return 0;
            }
            struct stat info
            {
            };
            if (stat(root, &info) != 0 || !S_ISDIR(info.st_mode))
            {
                return 0;
            }
            char* canonical = realpath(root, nullptr);
            if (canonical == nullptr)
            {
                return 0;
            }
            std::string next(canonical);
            std::free(canonical);
            if (next.empty())
            {
                return 0;
            }
            CloseAll();
            root_ = next;
            mounted_ = true;
            return 1;
        }

        void Clear()
        {
            CloseAll();
            root_.clear();
            mounted_ = false;
        }

        bool mounted() const
        {
            return mounted_;
        }

        void Mount() override
        {
        }

        void Unmount() override
        {
        }

        void RemoveAllSearchPaths() override
        {
        }

        void AddSearchPath(const char*, const char*) override
        {
        }

        bool RemoveSearchPath(const char*) override
        {
            return false;
        }

        void RemoveFile(const char*, const char*) override
        {
        }

        void CreateDirHierarchy(const char*, const char*) override
        {
        }

        bool FileExists(const char* pFileName) override
        {
            std::string resolved;
            return ResolveExisting(pFileName, &resolved);
        }

        bool IsDirectory(const char* pFileName) override
        {
            std::string resolved;
            if (!ResolveExisting(pFileName, &resolved))
            {
                return false;
            }
            struct stat info
            {
            };
            return stat(resolved.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
        }

        FileHandle_t Open(const char* pFileName, const char* pOptions, const char*) override
        {
            if (!IsReadMode(pOptions))
            {
                return nullptr;
            }
            std::string joined;
            if (!Join(pFileName, &joined))
            {
                return nullptr;
            }
            FILE* file = std::fopen(joined.c_str(), pOptions);
            if (file == nullptr)
            {
                return nullptr;
            }
            if (!OpenedInsideRoot(file))
            {
                std::fclose(file);
                return nullptr;
            }
            open_.push_back(file);
            return file;
        }

        void Close(FileHandle_t file) override
        {
            FILE* stream = Take(file);
            if (stream != nullptr)
            {
                std::fclose(stream);
            }
        }

        void Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType) override
        {
            FILE* stream = Find(file);
            if (stream == nullptr)
            {
                return;
            }
            int whence = SEEK_SET;
            if (seekType == FILESYSTEM_SEEK_CURRENT)
            {
                whence = SEEK_CUR;
            }
            else if (seekType == FILESYSTEM_SEEK_TAIL)
            {
                whence = SEEK_END;
            }
            else if (seekType != FILESYSTEM_SEEK_HEAD)
            {
                return;
            }
            std::fseek(stream, pos, whence);
        }

        unsigned int Tell(FileHandle_t file) override
        {
            FILE* stream = Find(file);
            if (stream == nullptr)
            {
                return 0;
            }
            const long position = std::ftell(stream);
            return position < 0 ? 0u : static_cast<unsigned int>(position);
        }

        unsigned int Size(FileHandle_t file) override
        {
            FILE* stream = Find(file);
            if (stream == nullptr)
            {
                return 0;
            }
            const long position = std::ftell(stream);
            if (position < 0 || std::fseek(stream, 0, SEEK_END) != 0)
            {
                return 0;
            }
            const long size = std::ftell(stream);
            std::fseek(stream, position, SEEK_SET);
            return size < 0 ? 0u : static_cast<unsigned int>(size);
        }

        unsigned int Size(const char* pFileName) override
        {
            std::string resolved;
            if (!ResolveExisting(pFileName, &resolved))
            {
                return static_cast<unsigned int>(-1);
            }
            struct stat info
            {
            };
            if (stat(resolved.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
            {
                return static_cast<unsigned int>(-1);
            }
            return static_cast<unsigned int>(info.st_size);
        }

        long GetFileTime(const char* pFileName) override
        {
            std::string resolved;
            if (!ResolveExisting(pFileName, &resolved))
            {
                return 0;
            }
            struct stat info
            {
            };
            if (stat(resolved.c_str(), &info) != 0)
            {
                return 0;
            }
            return static_cast<long>(info.st_mtime);
        }

        void FileTimeToString(char* pStrip, int maxCharsIncludingTerminator, long fileTime) override
        {
            if (pStrip == nullptr || maxCharsIncludingTerminator <= 0)
            {
                return;
            }
            pStrip[0] = '\0';
            char formatted[64];
            const time_t stamp = static_cast<time_t>(fileTime);
            if (ctime_r(&stamp, formatted) == nullptr)
            {
                return;
            }
            std::snprintf(pStrip, static_cast<std::size_t>(maxCharsIncludingTerminator), "%s", formatted);
        }

        bool IsOk(FileHandle_t file) override
        {
            FILE* stream = Find(file);
            return stream != nullptr && std::ferror(stream) == 0;
        }

        void Flush(FileHandle_t file) override
        {
            FILE* stream = Find(file);
            if (stream != nullptr)
            {
                std::fflush(stream);
            }
        }

        bool EndOfFile(FileHandle_t file) override
        {
            FILE* stream = Find(file);
            return stream != nullptr && std::feof(stream) != 0;
        }

        int Read(void* pOutput, int size, FileHandle_t file) override
        {
            FILE* stream = Find(file);
            if (stream == nullptr || pOutput == nullptr || size <= 0)
            {
                return 0;
            }
            return static_cast<int>(std::fread(pOutput, 1, static_cast<std::size_t>(size), stream));
        }

        int Write(const void*, int, FileHandle_t) override
        {
            return 0;
        }

        char* ReadLine(char* pOutput, int maxChars, FileHandle_t file) override
        {
            static char empty[] = "";
            FILE* stream = Find(file);
            if (stream == nullptr)
            {
                return empty;
            }
            if (pOutput == nullptr || maxChars <= 0)
            {
                return nullptr;
            }
            if (std::fgets(pOutput, maxChars, stream) == nullptr)
            {
                return nullptr;
            }
            return pOutput;
        }

        int FPrintf(FileHandle_t, const char*, ...) override
        {
            return -1;
        }

        void* GetReadBuffer(FileHandle_t, int* outBufferSize, bool) override
        {
            if (outBufferSize != nullptr)
            {
                *outBufferSize = 0;
            }
            return nullptr;
        }

        void ReleaseReadBuffer(FileHandle_t, void*) override
        {
        }

        const char* FindFirst(const char*, FileFindHandle_t*, const char*) override
        {
            return nullptr;
        }

        const char* FindNext(FileFindHandle_t) override
        {
            return nullptr;
        }

        bool FindIsDirectory(FileFindHandle_t) override
        {
            return false;
        }

        void FindClose(FileFindHandle_t) override
        {
        }

        void GetLocalCopy(const char*) override
        {
        }

        const char* GetLocalPath(const char* pFileName, char* pLocalPath, int localPathBufferSize) override
        {
            if (pLocalPath == nullptr || localPathBufferSize <= 0)
            {
                return nullptr;
            }
            pLocalPath[0] = '\0';
            std::string resolved;
            if (!ResolveExisting(pFileName, &resolved))
            {
                return nullptr;
            }
            if (static_cast<int>(resolved.size()) >= localPathBufferSize)
            {
                return nullptr;
            }
            std::memcpy(pLocalPath, resolved.c_str(), resolved.size() + 1);
            return pLocalPath;
        }

        char* ParseFile(char* pFileBytes, char* pToken, bool* pWasQuoted) override
        {
            // Callers such as the background layout pass a 512-byte token.
            constexpr int kTokenCap = 511;
            if (pWasQuoted != nullptr)
            {
                *pWasQuoted = false;
            }
            if (pToken != nullptr)
            {
                pToken[0] = '\0';
            }
            if (pFileBytes == nullptr)
            {
                return nullptr;
            }

            unsigned char* cursor = reinterpret_cast<unsigned char*>(pFileBytes);
            for (;;)
            {
                while (*cursor != '\0' && *cursor <= ' ')
                {
                    ++cursor;
                }
                if (*cursor == '/' && cursor[1] == '/')
                {
                    while (*cursor != '\0' && *cursor != '\n')
                    {
                        ++cursor;
                    }
                    continue;
                }
                break;
            }
            if (*cursor == '\0')
            {
                return nullptr;
            }

            const bool quoted = *cursor == '"';
            if (pWasQuoted != nullptr)
            {
                *pWasQuoted = quoted;
            }
            if (quoted)
            {
                ++cursor;
            }

            int length = 0;
            while (*cursor != '\0')
            {
                if (quoted)
                {
                    if (*cursor == '"')
                    {
                        ++cursor;
                        break;
                    }
                }
                else if (*cursor <= ' ')
                {
                    break;
                }
                if (pToken != nullptr && length < kTokenCap)
                {
                    pToken[length] = static_cast<char>(*cursor);
                    ++length;
                }
                ++cursor;
            }
            if (pToken != nullptr)
            {
                pToken[length] = '\0';
            }
            return reinterpret_cast<char*>(cursor);
        }

        bool FullPathToRelativePath(const char* pFullpath, char* pRelative) override
        {
            if (!mounted_ || pFullpath == nullptr || pRelative == nullptr)
            {
                return false;
            }
            char* canonical = realpath(pFullpath, nullptr);
            if (canonical == nullptr)
            {
                return false;
            }
            const std::string resolved(canonical);
            std::free(canonical);
            if (!WithinRoot(resolved) || resolved.size() <= root_.size())
            {
                return false;
            }
            const std::string relative = resolved.substr(root_.size() + 1);
            if (relative.size() >= 260)
            {
                return false;
            }
            std::memcpy(pRelative, relative.c_str(), relative.size() + 1);
            return true;
        }

        bool GetCurrentDirectory(char* pDirectory, int maxlen) override
        {
            if (!mounted_ || pDirectory == nullptr || maxlen <= 0)
            {
                return false;
            }
            if (static_cast<int>(root_.size()) >= maxlen)
            {
                return false;
            }
            std::memcpy(pDirectory, root_.c_str(), root_.size() + 1);
            return true;
        }

        void PrintOpenedFiles() override
        {
        }

        void SetWarningFunc(void (*)(const char*, ...)) override
        {
        }

        void SetWarningLevel(FileWarningLevel_t) override
        {
        }

        void LogLevelLoadStarted(const char*) override
        {
        }

        void LogLevelLoadFinished(const char*) override
        {
        }

        int HintResourceNeed(const char*, int) override
        {
            return 0;
        }

        int PauseResourcePreloading() override
        {
            return 0;
        }

        int ResumeResourcePreloading() override
        {
            return 0;
        }

        int SetVBuf(FileHandle_t, char*, int, long) override
        {
            return -1;
        }

        void GetInterfaceVersion(char* p, int maxlen) override
        {
            if (p == nullptr || maxlen <= 0)
            {
                return;
            }
            std::snprintf(p, static_cast<std::size_t>(maxlen), "%s", FILESYSTEM_INTERFACE_VERSION);
        }

        bool IsFileImmediatelyAvailable(const char* pFileName) override
        {
            return FileExists(pFileName);
        }

        WaitForResourcesHandle_t WaitForResources(const char*) override
        {
            return 0;
        }

        bool GetWaitForResourcesProgress(WaitForResourcesHandle_t, float* progress, bool* complete) override
        {
            if (progress != nullptr)
            {
                *progress = 0.0f;
            }
            if (complete != nullptr)
            {
                *complete = true;
            }
            return false;
        }

        void CancelWaitForResources(WaitForResourcesHandle_t) override
        {
        }

        bool IsAppReadyForOfflinePlay(int) override
        {
            return false;
        }

        bool AddPackFile(const char*, const char*) override
        {
            return false;
        }

        FileHandle_t OpenFromCacheForRead(const char* pFileName, const char* pOptions, const char* pathID) override
        {
            return Open(pFileName, pOptions, pathID);
        }

        void AddSearchPathNoWrite(const char*, const char*) override
        {
        }

        int ReadAll(const char* name, unsigned char* dest, int cap)
        {
            std::string joined;
            if (!Join(name, &joined))
            {
                return -1;
            }
            FILE* file = std::fopen(joined.c_str(), "rb");
            if (file == nullptr)
            {
                return -1;
            }
            if (std::fseek(file, 0, SEEK_END) != 0)
            {
                std::fclose(file);
                return -1;
            }
            const long size = std::ftell(file);
            if (size < 0 || size > 8 * 1024 * 1024)
            {
                std::fclose(file);
                return -1;
            }
            if (dest == nullptr || cap == 0)
            {
                std::fclose(file);
                return static_cast<int>(size);
            }
            if (cap < size || std::fseek(file, 0, SEEK_SET) != 0)
            {
                std::fclose(file);
                return -1;
            }
            const auto read = std::fread(dest, 1, static_cast<size_t>(size), file);
            std::fclose(file);
            return read == static_cast<size_t>(size) ? static_cast<int>(size) : -1;
        }

    private:
        bool Join(const char* name, std::string* joined) const
        {
            std::string relative;
            if (!mounted_ || !RelativeUnderRoot(name, &relative))
            {
                return false;
            }
            *joined = root_ + "/" + relative;
            return true;
        }

        bool WithinRoot(const std::string& canonical) const
        {
            if (canonical == root_)
            {
                return true;
            }
            if (canonical.size() <= root_.size() || canonical.compare(0, root_.size(), root_) != 0)
            {
                return false;
            }
            return canonical[root_.size()] == '/';
        }

        bool ResolveExisting(const char* name, std::string* resolved) const
        {
            std::string joined;
            if (!Join(name, &joined))
            {
                return false;
            }
            char* canonical = realpath(joined.c_str(), nullptr);
            if (canonical == nullptr)
            {
                return false;
            }
            const bool inside = WithinRoot(canonical);
            if (inside)
            {
                *resolved = canonical;
            }
            std::free(canonical);
            return inside;
        }

        bool OpenedInsideRoot(FILE* file) const
        {
            const int descriptor = fileno(file);
            if (descriptor < 0)
            {
                return false;
            }
            char link_path[64];
            std::snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", descriptor);
            char* canonical = realpath(link_path, nullptr);
            if (canonical == nullptr)
            {
                return false;
            }
            const bool inside = WithinRoot(canonical);
            std::free(canonical);
            return inside;
        }

        FILE* Find(FileHandle_t file) const
        {
            for (FILE* stream : open_)
            {
                if (stream == file)
                {
                    return stream;
                }
            }
            return nullptr;
        }

        FILE* Take(FileHandle_t file)
        {
            for (auto it = open_.begin(); it != open_.end(); ++it)
            {
                if (*it == file)
                {
                    FILE* stream = *it;
                    open_.erase(it);
                    return stream;
                }
            }
            return nullptr;
        }

        void CloseAll()
        {
            for (FILE* stream : open_)
            {
                if (stream != nullptr)
                {
                    std::fclose(stream);
                }
            }
            open_.clear();
        }

        std::string root_;
        std::vector<FILE*> open_;
        bool mounted_ = false;
    };

    HostFileSystem& Instance()
    {
        static HostFileSystem filesystem;
        return filesystem;
    }

} // namespace

int csr_host_filesystem_set_root(const char* root)
{
    return Instance().SetRoot(root);
}

void csr_host_filesystem_clear()
{
    Instance().Clear();
}

int csr_host_filesystem_mounted(void)
{
    return Instance().mounted() ? 1 : 0;
}

int csr_host_filesystem_read(const char* path, unsigned char* dest, int dest_cap)
{
    return Instance().ReadAll(path, dest, dest_cap);
}

void* csr_host_filesystem_factory(const char* name, int* code)
{
    if (name != nullptr && std::strcmp(name, FILESYSTEM_INTERFACE_VERSION) == 0 && Instance().mounted())
    {
        if (code != nullptr)
        {
            *code = IFACE_OK;
        }
        return &Instance();
    }
    if (code != nullptr)
    {
        *code = IFACE_FAILED;
    }
    return nullptr;
}
