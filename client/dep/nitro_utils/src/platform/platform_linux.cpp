#include <nitro_utils/platform.h>
#include <algorithm>
#include <cstring>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pmparser.h>

namespace nitro_utils
{
    SysModule GetSysModule(const char *name)
    {
        void* handle;

        handle = dlopen(name, RTLD_NOW | RTLD_NOLOAD);

        if (handle == nullptr)
            return nullptr;

        return handle;
    }

    bool GetSysModuleInfo(SysModule module, uintptr_t* start_address, uintptr_t* size)
    {
        link_map* linkmap = nullptr;
        if (dlinfo((void*) module, RTLD_DI_LINKMAP, &linkmap))
            return false;

        procmaps_iterator* maps = pmparser_parse(-1);
        if (maps == nullptr)
            return false;

        // A shared object is mapped in several segments (r--, r-x, r--, rw-).
        // The span must cover all of them; the first entry alone is only the
        // ELF header page, so a pattern search over it never matches code.
        uintptr_t span_start = 0;
        uintptr_t span_end = 0;
        bool found = false;

        procmaps_struct* maps_tmp = nullptr;
        struct stat module_stat {};
        if (stat(linkmap->l_name, &module_stat) != 0)
        {
            pmparser_free(maps);
            return false;
        }

        while ((maps_tmp = pmparser_next(maps)) != nullptr)
        {
            struct stat mapping_stat {};
            if (stat(maps_tmp->pathname, &mapping_stat) != 0)
                continue;
            if (mapping_stat.st_dev != module_stat.st_dev || mapping_stat.st_ino != module_stat.st_ino)
                continue;

            const uintptr_t mapping_start = reinterpret_cast<uintptr_t>(maps_tmp->addr_start);
            const uintptr_t mapping_end = reinterpret_cast<uintptr_t>(maps_tmp->addr_end);

            if (!found)
            {
                span_start = mapping_start;
                span_end = mapping_end;
                found = true;

                continue;
            }

            span_start = std::min(span_start, mapping_start);
            span_end = std::max(span_end, mapping_end);
        }

        pmparser_free(maps);

        if (!found)
            return false;

        *start_address = span_start;
        *size = span_end - span_start;

        return true;
    }

    SysModule LoadSysModule(const char* name)
    {
        // Valve's hw.so/client.so are built for hl_linux, whose global scope has no
        // libtier0. Ours does (the launcher links it), and without DEEPBIND its
        // symbols win over the module's own - e.g. hw.so's g_Thread ended up with
        // tier0's CThread vtable and died on a pure virtual call.
        constexpr int kFlags = RTLD_NOW | RTLD_DEEPBIND;

        void* module  = nullptr;
        char szAbsoluteModuleName[1024];
        if (name[0] != '/')
        {
            char szCwd[1024];
            getcwd(szCwd, sizeof(szCwd));
            if (szCwd[strlen(szCwd) - 1] == '/')
                szCwd[strlen(szCwd) - 1] = '\0';

            snprintf(szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/%s", szCwd, name);
            module = dlopen(szAbsoluteModuleName, kFlags);
        }
        else
        {
            snprintf(szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s", name);
            module = dlopen(name, kFlags);
        }

        if (!module)
        {
            char str[512];

            printf("Error: %s\n", dlerror());
            snprintf(str, sizeof(str), "%s.so", szAbsoluteModuleName);
            module = dlopen(str, kFlags);
        }

        return module;
    }

    void UnloadSysModule(SysModule module)
    {
        if (dlopen(nullptr, RTLD_LAZY) == module)
            return;

        dlclose(module);
    }

    void* GetProcAddress(SysModule module, const char* func_name)
    {
        return dlsym(module, func_name);
    }

    bool SetProtect(const void *addr, size_t size, ProtectMode protectMode)
    {
        int mode;
        switch (protectMode)
        {
            case ProtectMode::PROTECT_RW:
                mode = PROT_READ | PROT_WRITE;
                break;

            case ProtectMode::PROTECT_RWE:
                mode = PROT_READ | PROT_WRITE | PROT_EXEC;
                break;

            case ProtectMode::PROTECT_RE:
                mode = PROT_READ | PROT_EXEC;
                break;

            default:
                return false;
        }

        size_t pageSize = sysconf(_SC_PAGESIZE);
        size_t iaddr = reinterpret_cast<size_t>(addr);
        size_t roundAddr = iaddr & ~(pageSize - static_cast<size_t>(1));

#ifndef NDEBUG
        if (pageSize != 4096) fprintf(stderr, "large page(%zd) is used. not tested enough.\n", pageSize);
#endif

        return mprotect(reinterpret_cast<void*>(roundAddr), size + (iaddr - roundAddr), mode) == 0;
    }
}
