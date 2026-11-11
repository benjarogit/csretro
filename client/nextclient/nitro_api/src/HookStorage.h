#pragma once

#include <cstdint>
#include "hook/GenericHookJit.h"
#include "hook/GenericHookStatic.h"
#include "hook/GenericHookVaformatStatic.h"
#include <easylogging++.h>
#include <unordered_map>
#include <utility>

namespace nitroapi
{
    class HookStorage
    {
        // Keyed by address. A function that wasn't found gets no hook: every one of them has
        // address 0, and those of the same signature would share a single hook object, which the
        // first of them to unload deletes from under the others.
        std::unordered_map<uintptr_t, HookInterface*> hooks_;

    public:
        ~HookStorage()
        {
            int i = 0;
            for (auto& hook : hooks_)
            {
                i++;
                delete hook.second;
            }

            LOG_IF(i > 0, WARNING) << "HookStorage::~HookStorage Auto-removed hooks count: " << i;
        }

        template <StaticHookId HookId, CallingConventions TConv, class TResult, class... TArgs>
        GenericHookInterface<TResult, TArgs...>* GetStaticHook(
            uintptr_t address,
            typename GenericHookStatic<HookId, TConv, TResult, TArgs...>::CallbackWrapperFunc callback_wrapper = {}
        )
        {
            if (address == 0)
                return nullptr;

            auto result = hooks_.find(address);
            if (result != hooks_.end())
                return dynamic_cast<GenericHookInterface<TResult, TArgs...>*>(result->second);

            auto hook = new GenericHookStatic<HookId, TConv, TResult, TArgs...>(address, std::move(callback_wrapper));
            auto empl_result = hooks_.try_emplace(address, hook);

            if (!empl_result.second)
                return nullptr;

            return hook;
        }

        template<StaticHookId HookId, class TResult, class... TArgs>
        GenericHookInterface<TResult, TArgs...>* GetStaticVaformatHook(uintptr_t address)
        {
            if (address == 0)
                return nullptr;

            auto result = hooks_.find(address);
            if (result != hooks_.end())
                return dynamic_cast<GenericHookInterface<TResult, TArgs...>*>(result->second);

            auto hook = new GenericHookVaformatStatic<HookId, TResult, TArgs...>(address);
            auto empl_result = hooks_.try_emplace(address, hook);

            if (!empl_result.second)
                return nullptr;

            return hook;
        }

        template<CallingConventions TConv, class TResult, class... TArgs>
        GenericHookInterface<TResult, TArgs...>* GetJitHook(uintptr_t address)
        {
            if (address == 0)
                return nullptr;

            auto result = hooks_.find(address);
            if (result != hooks_.end())
                return dynamic_cast<GenericHookInterface<TResult, TArgs...>*>(result->second);

            auto hook = new GenericHookJit<TConv, TResult, TArgs...>(address);
            auto empl_result = hooks_.try_emplace(address, hook);

            if (!empl_result.second)
                return nullptr;

            return hook;
        }

        void RemoveHook(uintptr_t address)
        {
            auto result = hooks_.find(address);
            if (result == hooks_.end())
                return;

            delete result->second;

            hooks_.erase(result);
        }
    };
}
