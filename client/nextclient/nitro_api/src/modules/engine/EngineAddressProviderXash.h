#pragma once

#include "../../AddressProviderBase.h"
#include "../../SearchConfig.h"

namespace nitroapi
{
    // The product engine is Xash3D. This provider has no retail hw.so symbol
    // table. A missing key is not a hook and is not invented.
    class EngineAddressProviderXash : public AddressProviderBase
    {
        static const std::unordered_map<StaticHookId, SearchConfig> addresses_hook_id_;
        static const std::unordered_map<std::string, SearchConfig> addresses_str_;

    protected:
        const std::unordered_map<StaticHookId, SearchConfig>& get_addresses_id() override { return addresses_hook_id_; }
        const std::unordered_map<std::string, SearchConfig>& get_addresses_str() override { return addresses_str_; }
    };
}
