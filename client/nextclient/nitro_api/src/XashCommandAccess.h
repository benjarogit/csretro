#pragma once

#include <string>

namespace nitroapi
{
    // Borrowed command ABI queries. The caller authenticates the native engine
    // profile and retains the module for this object's entire bound lifetime.
    class XashCommandAccess
    {
    public:
        struct View
        {
            const void* identity{};
            void (*function)(){};
        };

    private:
        void* engine_{};
        void* (*find_)(unsigned, const char*){};
        void (*remove_)(const char*){};

    public:
        bool Bind(void* engine, std::string& error);
        void Shutdown();
        bool Inspect(const char* name, View& out) const;
        void Remove(const char* name) const;
        bool IsBound() const;
        bool IsFileSystemOwner(void* filesystem) const;
    };
} // namespace nitroapi
