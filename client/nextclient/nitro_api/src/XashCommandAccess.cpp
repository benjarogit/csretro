#include "XashCommandAccess.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "elf_symbols.h"

namespace nitroapi
{
    namespace
    {
        // The registered e9b94dc reference's cmd_s prefix is verified against
        // its DWARF: next/name/function/flags at 0/4/8/12, sizeof 16 on i386.
        // The flexible description is outside the inspected prefix.
        struct CommandPrefix
        {
            void* next;
            const char* name;
            void (*function)();
            int flags;
        };
#if INTPTR_MAX == INT32_MAX
        static_assert(sizeof(CommandPrefix) == 16);
        static_assert(offsetof(CommandPrefix, function) == 8);
#endif
        constexpr unsigned kCommandKind = 2;
        bool SameName(const char* left, const char* right)
        {
            if (!left || !right)
            {
                return false;
            }
            for (size_t i = 0; i < 128; ++i)
            {
                unsigned char a = left[i];
                unsigned char b = right[i];
                if (a >= 'A' && a <= 'Z')
                {
                    a += 'a' - 'A';
                }
                if (b >= 'A' && b <= 'Z')
                {
                    b += 'a' - 'A';
                }
                if (a != b)
                {
                    return false;
                }
                if (!a)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    bool XashCommandAccess::Bind(void* engine, std::string& error)
    {
        error.clear();
        if (!engine || IsBound())
        {
            error = "Native command access requires an unbound, authenticated engine owner";
            return false;
        }
        auto find = reinterpret_cast<decltype(find_)>(FindFunctionAddress(engine, "BaseCmd_Find"));
        auto remove = reinterpret_cast<decltype(remove_)>(FindFunctionAddress(engine, "Cmd_RemoveCommand"));
        if (!find || !remove)
        {
            error = "Loaded engine has no verified command lookup/removal functions";
            return false;
        }
        engine_ = engine;
        find_ = find;
        remove_ = remove;
        return true;
    }

    void XashCommandAccess::Shutdown()
    {
        engine_ = nullptr;
        find_ = nullptr;
        remove_ = nullptr;
    }

    bool XashCommandAccess::Inspect(const char* name, View& out) const
    {
        out = {};
        if (!IsBound() || !name || !*name || strnlen(name, 128) == 128)
        {
            return false;
        }
        const void* command = find_(kCommandKind, name);
        if (!command)
        {
            return true;
        }
        CommandPrefix prefix{};
        std::memcpy(&prefix, command, sizeof(prefix));
        if (!SameName(name, prefix.name))
        {
            return false;
        }
        out = {command, prefix.function};
        return true;
    }

    void XashCommandAccess::Remove(const char* name) const
    {
        if (!IsBound() || !name || !*name || strnlen(name, 128) == 128)
        {
            throw std::runtime_error("Cannot remove a command without its live native owner");
        }
        remove_(name);
    }

    bool XashCommandAccess::IsFileSystemOwner(void* filesystem) const
    {
        if (!IsBound() || !filesystem)
        {
            return false;
        }
        const uintptr_t slot = FindSymbolAddress(engine_, "fs_pfnCreateInterface");
        const uintptr_t candidate = FindFunctionAddress(filesystem, "CreateInterface");
        uintptr_t actual{};
        if (slot)
        {
            std::memcpy(&actual, reinterpret_cast<const void*>(slot), sizeof(actual));
        }
        // Xash populated this named callback when it initialized its filesystem.
        // Equal bytes in a separately loaded library are not the same live state.
        return candidate && actual == candidate;
    }

    bool XashCommandAccess::IsBound() const
    {
        return find_ && remove_;
    }
} // namespace nitroapi
