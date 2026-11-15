// Synthetic native command ABI and a real callback table for ownership tests.
#include <goldsrc/client_api.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
    using Callback = void (*)();
    struct Command
    {
        Command* next{};
        const char* name{};
        Callback function{};
        int flags{};
        std::string storage;
        unsigned token{};
        Command(std::string text, Callback callback, unsigned id) :
            storage(std::move(text)),
            token(id)
        {
            name = storage.c_str();
            function = callback;
        }
    };
    std::map<std::string, std::unique_ptr<Command>> commands;
    unsigned next_token{1};
    int add_count{}, remove_count{}, add_mode{}, remove_mode{};
    bool bad_lookup{};
    std::string argument;
    void (*add_hook)();
    std::string Key(const char* name)
    {
        std::string key(name ? name : "");
        for (char& c : key)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c += 'a' - 'A';
            }
        }
        return key;
    }
    int Add(const char* name, Callback callback)
    {
        ++add_count;
        if (add_mode == 1 || commands.count(Key(name)))
        {
            return 0;
        }
        commands.emplace(Key(name), std::make_unique<Command>(name, callback, next_token++));
        if (add_hook)
        {
            add_hook();
        }
        if (add_mode == 2)
        {
            throw std::runtime_error("synthetic registration exception after install");
        }
        return add_mode == 3 ? 0 : 1;
    }
    unsigned First()
    {
        return commands.empty() ? 0 : commands.begin()->second->token;
    }
    unsigned Next(unsigned token)
    {
        for (auto it = commands.begin(); it != commands.end(); ++it)
        {
            if (it->second->token == token)
            {
                ++it;
                return it == commands.end() ? 0 : it->second->token;
            }
        }
        return 0;
    }
    const char* CommandName(unsigned token)
    {
        for (const auto& item : commands)
        {
            if (item.second->token == token)
            {
                return item.second->name;
            }
        }
        return nullptr;
    }
    char* Argv(int index)
    {
        return index == 0 ? argument.data() : nullptr;
    }
    goldsrc::cl_enginefunc_t engine;
} // namespace
extern "C" __attribute__((visibility("hidden"))) void* BaseCmd_Find(unsigned type, const char* name)
{
    if (type != 2)
    {
        return nullptr;
    }
    const auto found = commands.find(Key(name));
    if (found == commands.end())
    {
        return nullptr;
    }
    found->second->name = bad_lookup ? nullptr : found->second->storage.c_str();
    return found->second.get();
}
extern "C" __attribute__((visibility("hidden"))) void Cmd_RemoveCommand(const char* name)
{
    ++remove_count;
    if (remove_mode == 1)
    {
        return;
    }
    if (remove_mode == 2)
    {
        throw std::runtime_error("synthetic removal exception before erase");
    }
    commands.erase(Key(name));
    if (remove_mode == 3)
    {
        throw std::runtime_error("synthetic removal exception after erase");
    }
}
extern "C" void FixtureNativeCommandsReset()
{
    commands.clear();
    next_token = 1;
    add_count = remove_count = add_mode = remove_mode = 0;
    bad_lookup = false;
    argument.clear();
    add_hook = nullptr;
    engine = {};
    engine.pfnAddCommand = Add;
    engine.GetFirstCmdFunctionHandle = First;
    engine.GetNextCmdFunctionHandle = Next;
    engine.GetCmdFunctionName = CommandName;
    engine.Cmd_Argv = Argv;
}
extern "C" goldsrc::cl_enginefunc_t* FixtureNativeCommandsEngine()
{
    return &engine;
}
extern "C" void FixtureNativeCommandsMode(int add, int remove, int bad)
{
    add_mode = add;
    remove_mode = remove;
    bad_lookup = bad;
}
extern "C" void FixtureNativeCommandsAddHook(void (*hook)())
{
    add_hook = hook;
}
extern "C" int FixtureNativeCommandsCount(int kind)
{
    return kind == 0 ? add_count : remove_count;
}
extern "C" Callback FixtureNativeCommandsHandler(const char* name)
{
    const auto found = commands.find(Key(name));
    return found == commands.end() ? nullptr : found->second->function;
}
extern "C" void FixtureNativeCommandsForeign(const char* name, Callback callback, int reuse)
{
    const auto key = Key(name);
    const auto found = commands.find(key);
    if (reuse && found != commands.end())
    {
        found->second->function = callback;
        return;
    }
    auto command = std::make_unique<Command>(name, callback, next_token++);
    commands[key] = std::move(command);
}
extern "C" void FixtureNativeCommandsInvoke(const char* name)
{
    argument = name;
    const auto found = commands.find(Key(name));
    if (found != commands.end() && found->second->function)
    {
        const auto callback = found->second->function;
        callback();
    }
}
extern "C" void FixtureNativeCommandsErase(const char* name)
{
    commands.erase(Key(name));
}
extern "C" void FixtureForeignNativeCommand() {}

extern "C" void* CreateInterface(const char* name, int*)
{
    static int filesystem;
    return name && std::string(name) == "VFileSystem009" ? &filesystem : nullptr;
}
extern "C" {
__attribute__((visibility("hidden"))) void* (*fs_pfnCreateInterface)(const char*, int*) = nullptr;
__attribute__((visibility("hidden"))) char fs_rootdir[1024]{};
}
extern "C" void FixtureNativeFileSystemFactory(void* (*factory)(const char*, int*))
{
    fs_pfnCreateInterface = factory;
}

extern "C" void FixtureNativeFileSystemRoot(const char* directory)
{
    std::memset(fs_rootdir, 0, sizeof(fs_rootdir));
    if (directory)
    {
        std::strncpy(fs_rootdir, directory, sizeof(fs_rootdir) - 1);
    }
}
