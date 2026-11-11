// Symbols that the Source SDK imports from Valve's i386 libtier0.so, libvstdlib.so
// and libsteam_api.so. This process does not load those libraries. The functions
// below exist so the x86_64 unit tests can link. They do not start Steam.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include <steam/steam_api.h>
#include <tier0/dbg.h>
#include <tier0/icommandline.h>
#include <tier0/platform.h>
#include <vstdlib/random.h>

namespace
{
    class HostCommandLine final : public ICommandLine
    {
    public:
        void CreateCmdLine(const char*) override {}
        void CreateCmdLine(int, char**) override {}
        const char* GetCmdLine() const override { return ""; }
        const char* CheckParm(const char*, const char** value) const override
        {
            if (value)
                *value = nullptr;
            return nullptr;
        }
        void RemoveParm(const char*) override {}
        void AppendParm(const char*, const char*) override {}
        const char* ParmValue(const char*, const char* default_value) const override { return default_value; }
        int ParmValue(const char*, int default_value) const override { return default_value; }
        float ParmValue(const char*, float default_value) const override { return default_value; }
        int ParmCount() const override { return 0; }
        int FindParm(const char*) const override { return 0; }
        const char* GetParm(int) const override { return ""; }
    };

    void Spew(const char* prefix, const char* format, va_list args)
    {
        std::fputs(prefix, stderr);
        std::vfprintf(stderr, format, args);
    }
}

extern "C" int64 g_ClockSpeed = 2000000000LL;

extern "C" ICommandLine* CommandLine()
{
    static HostCommandLine line;
    return &line;
}

const CPUInformation& GetCPUInformation()
{
    static CPUInformation info{};
    info.m_Size = sizeof(info);
    info.m_bSSE2 = true;
    info.m_nLogicalProcessors = 1;
    info.m_nPhysicalProcessors = 1;
    info.m_Speed = g_ClockSpeed;
    info.m_szProcessorID = const_cast<tchar*>("x86_64");
    return info;
}

extern "C" double Plat_FloatTime()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) / 1000000000.0;
}

extern "C" bool ThreadInMainThread()
{
    return true;
}

extern "C" float RandomFloat(float minimum, float maximum)
{
    return (minimum + maximum) * 0.5f;
}

extern "C" void Msg(tchar const* format, ...)
{
    va_list args;
    va_start(args, format);
    Spew("", format, args);
    va_end(args);
}

extern "C" void Warning(tchar const* format, ...)
{
    va_list args;
    va_start(args, format);
    Spew("warning: ", format, args);
    va_end(args);
}

extern "C" void Error(tchar const* format, ...)
{
    va_list args;
    va_start(args, format);
    Spew("error: ", format, args);
    va_end(args);
}

extern "C" void _SpewInfo(SpewType_t, tchar const*, int) {}

extern "C" SpewRetval_t _SpewMessage(tchar const* format, ...)
{
    va_list args;
    va_start(args, format);
    Spew("", format, args);
    va_end(args);
    return SPEW_CONTINUE;
}

extern "C" void _ExitOnFatalAssert(tchar const* file, int line)
{
    std::fprintf(stderr, "fatal assert at %s:%d\n", file, line);
}

extern "C" bool ShouldUseNewAssertDialog()
{
    return false;
}

extern "C" bool DoNewAssertDialog(tchar const*, int, tchar const*)
{
    return false;
}

extern "C" void _AssertValidReadPtr(void*, int) {}
extern "C" void _AssertValidWritePtr(void*, int) {}
extern "C" void AssertValidStringPtr(tchar const*, int) {}

extern "C" ISteamApps* SteamApps()
{
    return nullptr;
}

extern "C" ISteamMatchmaking* SteamMatchmaking()
{
    return nullptr;
}

extern "C" ISteamMatchmakingServers* SteamMatchmakingServers()
{
    return nullptr;
}

extern "C" ISteamUser* SteamUser()
{
    return nullptr;
}

extern "C" ISteamUtils* SteamUtils()
{
    return nullptr;
}
