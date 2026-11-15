// Independently compiled original Linux contract; no producer or SDK declarations.
#include <cstdint>
#include <cstring>
#include <string>

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
    struct SyntheticModule final : virtual ISystemModule
    {
        IBaseSystem* system{};
        int serial{};
        std::string name;
        std::string line;
        int frames{};
        int shutdowns{};
        int signals{};
        int commands{};
        int command_id{};
        int type_calls{};
        int name_calls{};
        double time{};
        bool accept{true};
        bool recurse{};
        void (*hook)(void*, int){};
        void* context{};
        bool Init(IBaseSystem* owner, int number, char* instance) override
        {
            system = owner;
            serial = number;
            name = instance;
            if (hook)
            {
                hook(context, 1);
            }
            return accept;
        }
        void RunFrame(double value) override
        {
            ++frames;
            time = value;
            if (hook)
            {
                hook(context, 2);
            }
        }
        void ReceiveSignal(ISystemModule*, unsigned, void*) override
        {
            ++signals;
            if (hook)
            {
                hook(context, 5);
            }
        }
        void ExecuteCommand(int id, char* value) override
        {
            ++commands;
            command_id = id;
            line = value;
            if (hook)
            {
                hook(context, 4);
            }
        }
        void RegisterListener(ISystemModule*) override {}
        void RemoveListener(ISystemModule*) override {}
        IBaseSystem* GetSystem() override
        {
            return system;
        }
        int GetSerial() override
        {
            return serial;
        }
        char* GetStatusLine() override
        {
            return name.data();
        }
        char* GetType() override
        {
            static char type[] = "fixture-module001";
            ++type_calls;
            if (hook)
            {
                hook(context, 6);
            }
            return type;
        }
        char* GetName() override
        {
            ++name_calls;
            if (hook)
            {
                hook(context, 7);
            }
            return name.data();
        }
        int GetState() override
        {
            return 3;
        }
        int GetVersion() override
        {
            return 711;
        }
        void ShutDown() override
        {
            ++shutdowns;
            if (recurse)
            {
                system->RemoveModule(this);
            }
            if (hook)
            {
                hook(context, 3);
            }
        }
    };
    SyntheticModule* Module(void* value)
    {
        return dynamic_cast<SyntheticModule*>(static_cast<ISystemModule*>(value));
    }
} // namespace
extern "C" void* FixtureSystemModuleCreate()
{
    return static_cast<ISystemModule*>(new SyntheticModule);
}
extern "C" void FixtureSystemModuleDelete(void* value)
{
    delete static_cast<ISystemModule*>(value);
}
extern "C" void FixtureSystemModuleConfigure(void* value, int accept, int recurse, void (*hook)(void*, int), void* context)
{
    auto* module = Module(value);
    module->accept = accept;
    module->recurse = recurse;
    module->hook = hook;
    module->context = context;
}
extern "C" int FixtureSystemModuleCount(void* value, int field)
{
    auto* module = Module(value);
    switch (field)
    {
        case 0:
            return module->serial;
        case 1:
            return module->frames;
        case 2:
            return module->shutdowns;
        case 3:
            return module->signals;
        case 4:
            return module->commands;
        case 5:
            return module->command_id;
        case 6:
            return module->type_calls;
        default:
            return module->name_calls;
    }
}
extern "C" const char* FixtureSystemModuleLine(void* value)
{
    return Module(value)->line.c_str();
}
extern "C" double FixtureSystemTime(void* value)
{
    return static_cast<IBaseSystem*>(value)->GetTime();
}
extern "C" int FixtureSystemVirtualBase(void* value)
{
    auto* system = static_cast<IBaseSystem*>(value);
    ISystemModule* module = system;
    IBaseInterface* base = module;
    return module->GetSystem() == system && dynamic_cast<IBaseSystem*>(module) == system && dynamic_cast<IBaseSystem*>(base) == system &&
           dynamic_cast<ISystemModule*>(base) == module;
}
extern "C" std::uintptr_t FixtureSystemCall(void* value, int slot, int number, const char* text, void* argument, void* extra)
{
    auto* system = static_cast<IBaseSystem*>(value);
    auto* module = static_cast<ISystemModule*>(argument);
    char* writable = const_cast<char*>(text);
    switch (slot)
    {
        case 0:
        case 1:
            return reinterpret_cast<void***>(value)[0][slot] != nullptr;
        case 2:
            return system->Init(system, number, writable);
        case 3:
            system->RunFrame(number / 10.0);
            break;
        case 4:
            system->ReceiveSignal(module, number, extra);
            break;
        case 5:
            system->ExecuteCommand(number, writable);
            break;
        case 6:
            system->RegisterListener(module);
            break;
        case 7:
            system->RemoveListener(module);
            break;
        case 8:
            return reinterpret_cast<std::uintptr_t>(system->GetSystem());
        case 9:
            return system->GetSerial();
        case 10:
            return reinterpret_cast<std::uintptr_t>(system->GetStatusLine());
        case 11:
            return reinterpret_cast<std::uintptr_t>(system->GetType());
        case 12:
            return reinterpret_cast<std::uintptr_t>(system->GetName());
        case 13:
            return system->GetState();
        case 14:
            return system->GetVersion();
        case 15:
            system->ShutDown();
            break;
        case 16:
            return system->GetTime() * 10;
        case 17:
            return system->GetTick();
        case 18:
            system->SetFPS(30);
            break;
        case 19:
            system->Printf(const_cast<char*>("source:%s:%d"), text, number);
            break;
        case 20:
            system->DPrintf(const_cast<char*>("debug:%s:%d"), text, number);
            break;
        case 21:
            system->RedirectOutput(static_cast<char*>(argument), number);
            break;
        case 22:
            return reinterpret_cast<std::uintptr_t>(system->GetFileSystem());
        case 23:
            return reinterpret_cast<std::uintptr_t>(system->LoadFile(text, static_cast<int*>(argument)));
        case 24:
            system->FreeFile(static_cast<unsigned char*>(argument));
            break;
        case 25:
            system->SetTitle(writable);
            break;
        case 26:
            system->SetStatusLine(writable);
            break;
        case 27:
            system->ShowConsole(number);
            break;
        case 28:
            system->LogConsole(writable);
            break;
        case 29:
            return system->InitVGUI(static_cast<IVGuiModule*>(argument));
        case 30:
            return system->RegisterCommand(writable, module, number);
        case 31:
            system->GetCommandMatches(writable, static_cast<ObjectList*>(argument));
            break;
        case 32:
            system->ExecuteString(writable);
            break;
        case 33:
            system->ExecuteFile(writable);
            break;
        case 34:
            system->Errorf(const_cast<char*>("error:%s:%d"), text, number);
            break;
        case 35:
            return reinterpret_cast<std::uintptr_t>(system->CheckParam(writable));
        case 36:
            return system->AddModule(module, writable);
        case 37:
            return reinterpret_cast<std::uintptr_t>(system->GetModule(writable, static_cast<char*>(argument), static_cast<char*>(extra)));
        case 38:
            return system->RemoveModule(module);
        case 39:
            system->Stop();
            break;
        case 40:
            return reinterpret_cast<std::uintptr_t>(system->GetBaseDir());
    }
    return 0;
}
