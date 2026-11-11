#include "AddressProviderBase.h"
#include <easylogging++.h>

namespace nitroapi
{
    uintptr_t AddressProviderBase::FindAddress(nitro_utils::SysModule hModule, const std::string& key)
    {
        InitScanner(hModule);

        auto it = get_addresses_str().find(key);
        if (it == get_addresses_str().cend())
        {
            LOG(INFO) << "Provider: Not found address for " << key;
            return 0;
        }

        uintptr_t addr = it->second.FindAddress(memory_scanner_);
        if (addr == 0)
            LOG(INFO) << "MemoryScanner: Not found address for " << key;

        return addr;
    }

    uintptr_t AddressProviderBase::FindAddress(nitro_utils::SysModule hModule, StaticHookId key)
    {
        InitScanner(hModule);

        auto it = get_addresses_id().find(key);
        if (it == get_addresses_id().cend())
        {
            LOG(INFO) << "Provider: Not found address for " << magic_enum::enum_name(key);
            return 0;
        }

        uintptr_t addr = it->second.FindAddress(memory_scanner_);
        if (addr == 0)
            LOG(INFO) << "MemoryScanner: Not found address for " << magic_enum::enum_name(key);

        return addr;
    }

    void AddressProviderBase::InitScanner(nitro_utils::SysModule hModule)
    {
        if (memory_module_.Module() == hModule)
            return;

        memory_module_.Init(hModule);
        memory_scanner_.SetContext(memory_module_);
    }
}