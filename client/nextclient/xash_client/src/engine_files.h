#pragma once

#include <goldsrc/client_api.h>

namespace csr::client
{
    // Borrowed Xash engine file callbacks; all buffers stay with that allocator.
    class EngineFiles
    {
    public:
        bool Bind(goldsrc::cl_enginefunc_t* engine);
        int Read(const char* path, unsigned char* dest, int capacity) const;
        int Font(const char* name, char* path, int capacity) const;
    private:
        goldsrc::cl_enginefunc_t* engine_{};
    };
}
