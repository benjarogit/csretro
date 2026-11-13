#include "engine_files.h"

#include <cctype>
#include <cstring>
#include <memory>
#include <string>

namespace csr::client
{
    bool EngineFiles::Bind(goldsrc::cl_enginefunc_t* engine)
    {
        engine_ = nullptr;
        if (!engine || !engine->COM_LoadFile || !engine->COM_FreeFile || !engine->COM_ExpandFilename)
            return false;
        engine_ = engine;
        return true;
    }

    int EngineFiles::Read(const char* path, unsigned char* dest, int capacity) const
    {
        if (!engine_ || !path || (dest && capacity < 0))
            return -1;
        int size{};
        std::unique_ptr<unsigned char, goldsrc::pfnEngSrc_COM_FreeFile_t> data(
                engine_->COM_LoadFile(path, 5, &size), engine_->COM_FreeFile);
        if (!data || size < 0 || (dest && size > capacity))
            return -1;
        if (dest && size > 0)
            std::memcpy(dest, data.get(), static_cast<size_t>(size));
        return size;
    }

    int EngineFiles::Font(const char* name, char* path, int capacity) const
    {
        if (!engine_ || !name || !path || capacity <= 0)
            return 0;
        path[0] = '\0';
        std::string family(name);
        for (char& letter : family)
            letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        if (family != "marlett")
            return 0;
        path[capacity - 1] = '\0';
        const int result = engine_->COM_ExpandFilename("resource/marlett.ttf", path, capacity);
        path[capacity - 1] = '\0';
        return result && path[0] ? 1 : 0;
    }
}
