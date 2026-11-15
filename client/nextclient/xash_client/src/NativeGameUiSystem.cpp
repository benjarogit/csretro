#include "NativeGameUiSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

class IBaseInterface
{
public:
    virtual ~IBaseInterface() = default;
};
class IBaseSystem;
class ISystemModule : public IBaseInterface
{
public:
    virtual ~ISystemModule() = default;
    virtual bool Init(IBaseSystem*, int, char*) = 0;
    virtual void RunFrame(double) = 0;
    virtual void ReceiveSignal(ISystemModule*, unsigned, void*) = 0;
    virtual void ExecuteCommand(int, char*) = 0;
    virtual void RegisterListener(ISystemModule*) = 0;
    virtual void RemoveListener(ISystemModule*) = 0;
    virtual IBaseSystem* GetSystem() = 0;
    virtual int GetSerial() = 0;
    virtual char* GetStatusLine() = 0;
    virtual char* GetType() = 0;
    virtual char* GetName() = 0;
    virtual int GetState() = 0;
    virtual int GetVersion() = 0;
    virtual void ShutDown() = 0;
};
class IFileSystem;
class IVGuiModule;
class ObjectList;
class IBaseSystem : virtual public ISystemModule
{
public:
    virtual ~IBaseSystem() = default;
    virtual double GetTime() = 0;
    virtual unsigned GetTick() = 0;
    virtual void SetFPS(float) = 0;
    virtual void Printf(char*, ...) = 0;
    virtual void DPrintf(char*, ...) = 0;
    virtual void RedirectOutput(char*, int) = 0;
    virtual IFileSystem* GetFileSystem() = 0;
    virtual unsigned char* LoadFile(const char*, int*) = 0;
    virtual void FreeFile(unsigned char*) = 0;
    virtual void SetTitle(char*) = 0;
    virtual void SetStatusLine(char*) = 0;
    virtual void ShowConsole(bool) = 0;
    virtual void LogConsole(char*) = 0;
    virtual bool InitVGUI(IVGuiModule*) = 0;
    virtual bool RegisterCommand(char*, ISystemModule*, int) = 0;
    virtual void GetCommandMatches(char*, ObjectList*) = 0;
    virtual void ExecuteString(char*) = 0;
    virtual void ExecuteFile(char*) = 0;
    virtual void Errorf(char*, ...) = 0;
    virtual char* CheckParam(char*) = 0;
    virtual bool AddModule(ISystemModule*, char*) = 0;
    virtual ISystemModule* GetModule(char*, char*, char*) = 0;
    virtual bool RemoveModule(ISystemModule*) = 0;
    virtual void Stop() = 0;
    virtual char* GetBaseDir() = 0;
};

namespace
{
    constexpr int kInitializing = 1;
    constexpr int kRunning = 3;
    constexpr int kDisconnected = 4;
    constexpr size_t kCommandLimit = 8192;

    bool Same(const char* left, const char* right)
    {
        return left && right && std::strcmp(left, right) == 0;
    }

    bool SameCommand(const char* left, const char* right)
    {
        if (!left || !right)
        {
            return false;
        }
        for (;; ++left, ++right)
        {
            unsigned char a = static_cast<unsigned char>(*left);
            unsigned char b = static_cast<unsigned char>(*right);
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
    }

    bool CommandName(const char* name)
    {
        if (!name || !*name || std::strlen(name) > 127)
        {
            return false;
        }
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p)
        {
            if (*p <= 32 || *p == ';' || *p == '"' || *p == '\\' || *p >= 127)
            {
                return false;
            }
        }
        return true;
    }
} // namespace

namespace csr::client
{
    struct NativeGameUiSystem::Impl final : IBaseSystem
    {
        struct Module
        {
            ISystemModule* object{};
            std::string name;
            int serial{};
            bool initialized{};
            bool removing{};
            bool shutdown_called{};
        };
        struct Command
        {
            std::string name;
            std::shared_ptr<Module> module;
            int id{};
            bool active{};
        };
        struct Dispatch
        {
            Impl& owner;
            explicit Dispatch(Impl& value) :
                owner(value)
            {
                ++owner.depth;
            }
            ~Dispatch()
            {
                if (--owner.depth == 0 && owner.closing && !owner.sealed)
                {
                    try
                    {
                        owner.Cleanup();
                    }
                    catch (...)
                    {
                        owner.cleaning = false;
                        owner.state = kDisconnected;
                    }
                }
            }
        };

        Host host;
        std::vector<std::shared_ptr<Module>> modules;
        std::vector<std::shared_ptr<Command>> commands;
        std::vector<ISystemModule*> listeners;
        std::string name{"CSRETRO"};
        std::string status;
        char* redirect{};
        int redirect_capacity{};
        int serial{};
        int next_serial{1};
        int state{kDisconnected};
        unsigned tick{};
        unsigned depth{};
        double time{};
        bool closing{};
        bool cleaning{};
        bool ready{};
        bool sealed{};
        static Impl* active;

        bool Operational() const
        {
            return ready && !closing && !sealed;
        }

        std::shared_ptr<Module> Find(ISystemModule* object) const
        {
            auto it = std::find_if(modules.begin(), modules.end(), [object](const auto& entry) { return entry->object == object; });
            return it == modules.end() ? nullptr : *it;
        }

        bool EngineCommand(const char* value) const
        {
            if (!Operational())
            {
                return true;
            }
            unsigned handle = host.engine->GetFirstCmdFunctionHandle();
            if (!Operational())
            {
                return true;
            }
            for (unsigned count = 0; handle && count < 65536; ++count)
            {
                const char* candidate = host.engine->GetCmdFunctionName(handle);
                if (!Operational() || SameCommand(candidate, value))
                {
                    return true;
                }
                unsigned next = host.engine->GetNextCmdFunctionHandle(handle);
                if (!Operational() || next == handle)
                {
                    return true;
                }
                handle = next;
            }
            return handle != 0;
        }

        bool RemoveCommands(const std::shared_ptr<Module>& module)
        {
            if (sealed)
            {
                return commands.empty();
            }
            bool complete = true;
            auto snapshot = commands;
            for (auto& command : snapshot)
            {
                if (sealed)
                {
                    complete = false;
                    break;
                }
                if (module && command->module != module)
                {
                    continue;
                }
                command->active = false;
                bool removed = false;
                try
                {
                    removed = host.remove_command(command->name.c_str(), host.context);
                }
                catch (...)
                {
                    removed = false;
                }
                if (removed)
                {
                    commands.erase(std::remove(commands.begin(), commands.end(), command), commands.end());
                }
                else
                {
                    complete = false;
                }
            }
            return complete;
        }

        void Cleanup()
        {
            if (sealed || cleaning || depth)
            {
                return;
            }
            cleaning = true;
            if (!RemoveCommands(nullptr))
            {
                cleaning = false;
                return;
            }
            while (!modules.empty())
            {
                auto entry = modules.front();
                if (!entry->shutdown_called && entry->initialized)
                {
                    entry->removing = true;
                    entry->shutdown_called = true;
                    ++depth;
                    try
                    {
                        entry->object->ShutDown();
                    }
                    catch (...)
                    {
                        state = kDisconnected;
                    }
                    --depth;
                }
                modules.erase(std::remove(modules.begin(), modules.end(), entry), modules.end());
            }
            listeners.clear();
            redirect = nullptr;
            redirect_capacity = 0;
            ready = false;
            closing = false;
            cleaning = false;
            if (active == this)
            {
                active = nullptr;
            }
        }

        void Print(bool developer, const char* format, va_list args)
        {
            if (sealed || !format || !host.engine)
            {
                return;
            }
            char text[8192]{};
            std::vsnprintf(text, sizeof(text), format, args);
            if (!developer && redirect && redirect_capacity > 0)
            {
                size_t size = strnlen(redirect, static_cast<size_t>(redirect_capacity));
                if (size < static_cast<size_t>(redirect_capacity))
                {
                    std::snprintf(redirect + size, static_cast<size_t>(redirect_capacity) - size, "%s", text);
                }
                return;
            }
            if (developer)
            {
                host.engine->Con_DPrintf("%s", text);
            }
            else
            {
                host.engine->Con_Printf("%s", text);
            }
        }

        static void Forward()
        {
            if (!active || !active->Operational())
            {
                return;
            }
            Impl& owner = *active;
            Dispatch scope(owner);
            int argc = owner.host.engine->Cmd_Argc();
            if (argc < 1 || argc > 1024)
            {
                return;
            }
            const char* command_name = owner.host.engine->Cmd_Argv(0);
            auto snapshot = owner.commands;
            for (auto& command : snapshot)
            {
                if (!command->active || !SameCommand(command->name.c_str(), command_name) || command->module->removing)
                {
                    continue;
                }
                std::string line(command_name);
                for (int i = 1; i < argc; ++i)
                {
                    const char* argument = owner.host.engine->Cmd_Argv(i);
                    if (!argument)
                    {
                        return;
                    }
                    line += " \"";
                    for (const char* p = argument; *p; ++p)
                    {
                        // GoldSrc command quoting cannot represent an embedded quote losslessly.
                        if (*p == '"' || *p == '\n' || *p == '\r' || line.size() >= kCommandLimit)
                        {
                            return;
                        }
                        line += *p;
                    }
                    line += '"';
                }
                try
                {
                    command->module->object->ExecuteCommand(command->id, line.data());
                }
                catch (...)
                {
                    owner.ShutDown();
                }
                return;
            }
        }

        bool Init(IBaseSystem* system, int number, char* instance) override
        {
            if (system != this || !Operational() || number < 0)
            {
                return false;
            }
            serial = number;
            name = instance ? instance : "CSRETRO";
            return true;
        }
        void RunFrame(double delta) override
        {
            if (!Operational() || !std::isfinite(delta) || delta < 0 || !std::isfinite(time + delta))
            {
                return;
            }
            Dispatch scope(*this);
            time += delta;
            ++tick;
            if (state != kRunning)
            {
                return;
            }
            auto snapshot = modules;
            for (auto& module : snapshot)
            {
                if (closing || state != kRunning)
                {
                    break;
                }
                if (module->initialized && !module->removing && Find(module->object) == module)
                {
                    try
                    {
                        module->object->RunFrame(time);
                    }
                    catch (...)
                    {
                        ShutDown();
                    }
                }
            }
        }
        void ReceiveSignal(ISystemModule* sender, unsigned signal, void* data) override
        {
            if (!Operational())
            {
                return;
            }
            Dispatch scope(*this);
            auto snapshot = listeners;
            for (auto* listener : snapshot)
            {
                if (closing)
                {
                    break;
                }
                if (std::find(listeners.begin(), listeners.end(), listener) != listeners.end())
                {
                    try
                    {
                        listener->ReceiveSignal(sender, signal, data);
                    }
                    catch (...)
                    {
                        ShutDown();
                    }
                }
            }
        }
        void ExecuteCommand(int id, char*) override
        {
            if (sealed || !host.engine)
            {
                return;
            }
            host.engine->Con_DPrintf("CSRETRO system: unsupported system command %d\n", id);
        }
        void RegisterListener(ISystemModule* module) override
        {
            if (Operational() && module && std::find(listeners.begin(), listeners.end(), module) == listeners.end())
            {
                listeners.push_back(module);
            }
        }
        void RemoveListener(ISystemModule* module) override
        {
            listeners.erase(std::remove(listeners.begin(), listeners.end(), module), listeners.end());
        }
        IBaseSystem* GetSystem() override
        {
            return this;
        }
        int GetSerial() override
        {
            return serial;
        }
        char* GetStatusLine() override
        {
            return status.data();
        }
        char* GetType() override
        {
            static char type[] = "basesystem002";
            return type;
        }
        char* GetName() override
        {
            return name.data();
        }
        int GetState() override
        {
            return state;
        }
        int GetVersion() override
        {
            return host.product_version;
        }
        void ShutDown() override
        {
            if (sealed)
            {
                return;
            }
            closing = true;
            state = kDisconnected;
            if (!depth)
            {
                Cleanup();
            }
        }
        double GetTime() override
        {
            return time;
        }
        unsigned GetTick() override
        {
            return tick;
        }
        void SetFPS(float) override {}
        void Printf(char* format, ...) override
        {
            va_list args;
            va_start(args, format);
            Print(false, format, args);
            va_end(args);
        }
        void DPrintf(char* format, ...) override
        {
            va_list args;
            va_start(args, format);
            Print(true, format, args);
            va_end(args);
        }
        void RedirectOutput(char* buffer, int capacity) override
        {
            if (sealed)
            {
                return;
            }
            redirect = buffer && capacity > 0 ? buffer : nullptr;
            redirect_capacity = redirect ? capacity : 0;
            if (redirect)
            {
                redirect[0] = '\0';
            }
        }
        IFileSystem* GetFileSystem() override
        {
            return sealed ? nullptr : static_cast<IFileSystem*>(host.filesystem);
        }
        unsigned char* LoadFile(const char* file, int* length) override
        {
            if (length)
            {
                *length = 0;
            }
            return Operational() && file && *file ? host.engine->COM_LoadFile(file, 5, length) : nullptr;
        }
        void FreeFile(unsigned char* file) override
        {
            if (!sealed && file)
            {
                host.engine->COM_FreeFile(file);
            }
        }
        void SetTitle(char* title) override
        {
            if (Operational() && title && host.set_title)
            {
                host.set_title(title, host.context);
            }
        }
        void SetStatusLine(char* value) override
        {
            if (!sealed)
            {
                status = value ? value : "";
            }
        }
        void ShowConsole(bool visible) override
        {
            if (Operational() && host.show_console)
            {
                host.show_console(visible, host.context);
            }
        }
        void LogConsole(char* file) override
        {
            if (Operational())
            {
                host.engine->pfnClientCmd(file ? "log on\n" : "log off\n");
            }
        }
        bool InitVGUI(IVGuiModule*) override
        {
            return false;
        }
        bool RegisterCommand(char* value, ISystemModule* object, int id) override
        {
            auto module = Find(object);
            if (!Operational() || !module || module->removing || !CommandName(value) || EngineCommand(value))
            {
                return false;
            }
            if (std::any_of(commands.begin(), commands.end(), [value](const auto& command) {
                    return SameCommand(command->name.c_str(), value);
                }))
            {
                return false;
            }
            Dispatch scope(*this);
            auto command = std::make_shared<Command>();
            command->name = value;
            command->module = module;
            command->id = id;
            commands.push_back(command);
            if (!host.engine->pfnAddCommand(command->name.c_str(), Forward))
            {
                commands.erase(std::remove(commands.begin(), commands.end(), command), commands.end());
                return false;
            }
            command->active = !closing && !module->removing;
            return command->active;
        }
        void GetCommandMatches(char*, ObjectList* list) override
        {
            if (!sealed && list)
            {
                host.clear_matches(list, true);
            }
        }
        void ExecuteString(char* line) override
        {
            if (Operational() && line)
            {
                host.engine->pfnClientCmd(line);
            }
        }
        void ExecuteFile(char* file) override
        {
            int length{};
            unsigned char* content = LoadFile(file, &length);
            if (!content)
            {
                return;
            }
            std::string text(reinterpret_cast<char*>(content), static_cast<size_t>(std::max(length, 0)));
            FreeFile(content);
            if (text.find('\0') == std::string::npos)
            {
                ExecuteString(text.data());
            }
        }
        void Errorf(char* format, ...) override
        {
            if (sealed)
            {
                return;
            }
            char text[8192]{};
            va_list args;
            va_start(args, format);
            if (format)
            {
                std::vsnprintf(text, sizeof(text), format, args);
            }
            va_end(args);
            Stop();
            host.engine->Con_Printf("CSRETRO system error: %s\n", text);
            if (!sealed && host.error)
            {
                host.error(text, host.context);
            }
        }
        char* CheckParam(char* parameter) override
        {
            if (!Operational() || !parameter)
            {
                return nullptr;
            }
            char* value{};
            if (!host.engine->CheckParm(parameter, &value))
            {
                return nullptr;
            }
            static char empty[] = "";
            return value ? value : empty;
        }
        bool AddModule(ISystemModule* object, char* instance) override
        {
            if (!Operational() || !object || object == static_cast<ISystemModule*>(this) || Find(object))
            {
                return false;
            }
            Dispatch scope(*this);
            auto entry = std::make_shared<Module>();
            entry->object = object;
            entry->name = instance ? instance : "";
            entry->serial = next_serial;
            while (std::any_of(modules.begin(), modules.end(), [&entry](const auto& item) { return item->serial == entry->serial; }))
            {
                if (entry->serial == std::numeric_limits<int>::max())
                {
                    return false;
                }
                ++entry->serial;
            }
            modules.insert(modules.begin(), entry);
            bool initialized = false;
            try
            {
                initialized = object->Init(this, entry->serial, entry->name.data());
            }
            catch (...)
            {
                initialized = false;
            }
            entry->initialized = initialized;
            if (!initialized || closing || entry->removing || Find(object) != entry)
            {
                if (!sealed && initialized && !entry->shutdown_called)
                {
                    entry->shutdown_called = true;
                    entry->removing = true;
                    try
                    {
                        object->ShutDown();
                    }
                    catch (...)
                    {
                        state = kDisconnected;
                    }
                }
                RemoveCommands(entry);
                RemoveListener(object);
                modules.erase(std::remove(modules.begin(), modules.end(), entry), modules.end());
                return false;
            }
            if (entry->serial < std::numeric_limits<int>::max())
            {
                next_serial = std::max(next_serial, entry->serial + 1);
            }
            return true;
        }
        ISystemModule* GetModule(char* type, char* library, char* instance) override
        {
            if (!Operational() || !type || !*type)
            {
                return nullptr;
            }
            Dispatch scope(*this);
            auto snapshot = modules;
            auto registered = [this](const std::shared_ptr<Module>& entry) {
                return Operational() && entry->initialized && !entry->removing && Find(entry->object) == entry;
            };
            for (auto& entry : snapshot)
            {
                if (!registered(entry))
                {
                    continue;
                }
                try
                {
                    const char* actual_type = entry->object->GetType();
                    if (!registered(entry) || !Same(actual_type, type))
                    {
                        continue;
                    }
                    if (instance)
                    {
                        const char* actual_name = entry->object->GetName();
                        if (!registered(entry) || !Same(actual_name, instance))
                        {
                            continue;
                        }
                    }
                    if (registered(entry))
                    {
                        return entry->object;
                    }
                }
                catch (...)
                {
                    if (!sealed)
                    {
                        host.engine->Con_DPrintf("CSRETRO system: module lookup getter failed\n");
                    }
                    return nullptr;
                }
            }
            if (sealed)
            {
                return nullptr;
            }
            host.engine->Con_DPrintf(
                "CSRETRO system: module '%s' from '%s' is not registered; external HLTV loading is unsupported\n",
                type,
                library ? library : ""
            );
            return nullptr;
        }
        bool RemoveModule(ISystemModule* object) override
        {
            auto entry = Find(object);
            if (!entry)
            {
                return object == nullptr;
            }
            if (entry->removing && entry->shutdown_called)
            {
                return true;
            }
            Dispatch scope(*this);
            entry->removing = true;
            if (!RemoveCommands(entry))
            {
                return false;
            }
            RemoveListener(object);
            if (!sealed && entry->initialized && !entry->shutdown_called)
            {
                entry->shutdown_called = true;
                try
                {
                    object->ShutDown();
                }
                catch (...)
                {
                    state = kDisconnected;
                }
            }
            modules.erase(std::remove(modules.begin(), modules.end(), entry), modules.end());
            return true;
        }
        void Stop() override
        {
            state = kDisconnected;
        }
        char* GetBaseDir() override
        {
            return host.base_directory.data();
        }
    };

    NativeGameUiSystem::Impl* NativeGameUiSystem::Impl::active{};

    NativeGameUiSystem::NativeGameUiSystem() :
        impl_(std::make_unique<Impl>())
    {}
    NativeGameUiSystem::~NativeGameUiSystem()
    {
        Shutdown();
        if (Impl::active == impl_.get())
        {
            Impl::active = nullptr;
        }
    }
    bool NativeGameUiSystem::Bind(const Host& host, std::string& error)
    {
        auto* engine = host.engine;
        if (impl_->sealed || impl_->ready || Impl::active || !engine || !host.filesystem || !host.remove_command || !host.clear_matches ||
            host.base_directory.empty() || host.product_version <= 0 || !engine->Con_Printf || !engine->Con_DPrintf ||
            !engine->COM_LoadFile || !engine->COM_FreeFile || !engine->CheckParm || !engine->pfnClientCmd || !engine->pfnAddCommand ||
            !engine->Cmd_Argc || !engine->Cmd_Argv || !engine->GetFirstCmdFunctionHandle || !engine->GetNextCmdFunctionHandle ||
            !engine->GetCmdFunctionName)
        {
            error = "Native GameUI system requires exclusive complete live engine services and a product identity";
            return false;
        }
        impl_->host = host;
        impl_->state = kInitializing;
        impl_->ready = true;
        impl_->closing = false;
        impl_->time = 0;
        impl_->tick = 0;
        impl_->next_serial = 1;
        Impl::active = impl_.get();
        impl_->state = kRunning;
        error.clear();
        return true;
    }
    void* NativeGameUiSystem::Interface()
    {
        return IsReady() ? static_cast<IBaseSystem*>(impl_.get()) : nullptr;
    }
    void NativeGameUiSystem::Frame(double delta)
    {
        impl_->RunFrame(delta);
    }
    void NativeGameUiSystem::Shutdown()
    {
        impl_->ShutDown();
    }
    void NativeGameUiSystem::EndNativeLifetime() noexcept
    {
        impl_->sealed = true;
        impl_->ready = false;
        impl_->closing = true;
        impl_->state = kDisconnected;
        impl_->redirect = nullptr;
        impl_->redirect_capacity = 0;
        impl_->listeners.clear();
        impl_->modules.clear();
        if (Impl::active == impl_.get() && impl_->commands.empty())
        {
            Impl::active = nullptr;
        }
    }
    bool NativeGameUiSystem::IsReady() const
    {
        return impl_->Operational();
    }
    bool NativeGameUiSystem::HasPendingCommands() const
    {
        return !impl_->commands.empty();
    }
} // namespace csr::client
