#include "NativeGameUiSystem.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <gtest/gtest.h>

namespace
{
    class NativeSystem : public testing::Test
    {
    protected:
        static NativeSystem* current;
        void* consumer{};
        void* api{};
        goldsrc::cl_enginefunc_t engine{};
        csr::client::NativeGameUiSystem::Host host;
        csr::client::NativeGameUiSystem system;
        using CallFn = std::uintptr_t (*)(void*, int, int, const char*, void*, void*);
        CallFn invoke{};
        void* (*create)(){};
        void (*destroy)(void*){};
        void (*configure)(void*, int, int, void (*)(void*, int), void*){};
        int (*count)(void*, int){};
        const char* (*line)(void*){};
        double (*time)(void*){};
        int (*virtual_base)(void*){};
        std::vector<void*> modules;
        std::map<std::string, void (*)()> commands;
        std::vector<std::string> args;
        std::string printed;
        std::string debug;
        std::string executed;
        std::string title;
        std::string error_text;
        std::string file_data{"echo fixture\n"};
        bool add_ok{true};
        bool remove_ok{true};
        bool remove_throws{};
        bool seal_on_remove{};
        bool console{};
        int loads{};
        int frees{};
        int load_mode{};
        int clear_count{};
        bool clear_free{};
        void* cleared{};
        int hook_event{};
        void* hook_module{};
        std::string command_name{"fixture_command"};

        template <class T>
        T Symbol(const char* name)
        {
            return reinterpret_cast<T>(dlsym(consumer, name));
        }
        void SetUp() override
        {
            current = this;
            consumer = dlopen(CLIENT_LEGACY_SYSTEM_CONSUMER, RTLD_NOW | RTLD_LOCAL);
            ASSERT_NE(consumer, nullptr) << dlerror();
            invoke = Symbol<CallFn>("FixtureSystemCall");
            create = Symbol<decltype(create)>("FixtureSystemModuleCreate");
            destroy = Symbol<decltype(destroy)>("FixtureSystemModuleDelete");
            configure = Symbol<decltype(configure)>("FixtureSystemModuleConfigure");
            count = Symbol<decltype(count)>("FixtureSystemModuleCount");
            line = Symbol<decltype(line)>("FixtureSystemModuleLine");
            time = Symbol<decltype(time)>("FixtureSystemTime");
            virtual_base = Symbol<decltype(virtual_base)>("FixtureSystemVirtualBase");
            ASSERT_TRUE(invoke && create && destroy && configure && count && line && time && virtual_base);
            engine.Con_Printf = Print;
            engine.Con_DPrintf = Debug;
            engine.COM_LoadFile = Load;
            engine.COM_FreeFile = Free;
            engine.CheckParm = Check;
            engine.pfnClientCmd = Execute;
            engine.pfnAddCommand = Add;
            engine.Cmd_Argc = Argc;
            engine.Cmd_Argv = Argv;
            engine.GetFirstCmdFunctionHandle = First;
            engine.GetNextCmdFunctionHandle = Next;
            engine.GetCmdFunctionName = Name;
            host.engine = &engine;
            host.filesystem = this;
            host.remove_command = Remove;
            host.context = this;
            host.clear_matches = Clear;
            host.base_directory = "/synthetic/runtime";
            host.product_version = 711;
            host.set_title = Title;
            host.show_console = Console;
            host.error = Error;
            std::string error;
            ASSERT_TRUE(system.Bind(host, error)) << error;
            api = system.Interface();
        }
        void TearDown() override
        {
            remove_ok = true;
            remove_throws = false;
            system.Shutdown();
            EXPECT_FALSE(system.HasPendingCommands());
            for (void* module : modules)
            {
                destroy(module);
            }
            if (consumer)
            {
                dlclose(consumer);
            }
            current = nullptr;
        }
        std::uintptr_t Call(int slot, int number = 0, const char* text = "fixture", void* argument = nullptr, void* extra = nullptr)
        {
            return invoke(api, slot, number, text, argument, extra);
        }
        void* Module(bool accept = true, bool recurse = false)
        {
            void* module = create();
            modules.push_back(module);
            configure(module, accept, recurse, nullptr, nullptr);
            return module;
        }
        void* AddModule(const char* name = "instance")
        {
            void* module = Module();
            EXPECT_EQ(Call(36, 0, name, module), 1U);
            return module;
        }
        static void Text(std::string& destination, const char* format, va_list ap)
        {
            char buffer[16384]{};
            std::vsnprintf(buffer, sizeof(buffer), format, ap);
            destination += buffer;
        }
        static void Print(const char* format, ...)
        {
            va_list ap;
            va_start(ap, format);
            Text(current->printed, format, ap);
            va_end(ap);
        }
        static void Debug(const char* format, ...)
        {
            va_list ap;
            va_start(ap, format);
            Text(current->debug, format, ap);
            va_end(ap);
        }
        static unsigned char* Load(const char*, int mode, int* length)
        {
            ++current->loads;
            current->load_mode = mode;
            size_t size = current->file_data.size();
            auto* result = static_cast<unsigned char*>(std::malloc(size + 1));
            std::memcpy(result, current->file_data.c_str(), size + 1);
            if (length)
            {
                *length = static_cast<int>(size);
            }
            return result;
        }
        static void Free(void* value)
        {
            ++current->frees;
            std::free(value);
        }
        static int Check(const char* parameter, char** value)
        {
            *value = nullptr;
            if (std::strcmp(parameter, "-value") == 0)
            {
                *value = const_cast<char*>("actual-value");
                return 1;
            }
            return std::strcmp(parameter, "-flag") == 0 ? 2 : 0;
        }
        static int Execute(const char* text)
        {
            current->executed += text;
            return 1;
        }
        static int Add(const char* name, void (*callback)())
        {
            if (!current->add_ok || current->commands.count(name))
            {
                return 0;
            }
            current->commands[name] = callback;
            return 1;
        }
        static bool Remove(const char* name, void* context)
        {
            auto* self = static_cast<NativeSystem*>(context);
            if (self->remove_throws)
            {
                throw std::runtime_error("fixture removal failed");
            }
            if (!self->remove_ok)
            {
                return false;
            }
            self->commands.erase(name);
            if (self->seal_on_remove)
            {
                self->system.EndNativeLifetime();
            }
            return true;
        }
        static int Argc()
        {
            return static_cast<int>(current->args.size());
        }
        static char* Argv(int i)
        {
            return current->args.at(i).data();
        }
        static unsigned First()
        {
            return current->commands.empty() ? 0 : 1;
        }
        static unsigned Next(unsigned handle)
        {
            return handle < current->commands.size() ? handle + 1 : 0;
        }
        static const char* Name(unsigned handle)
        {
            auto it = current->commands.begin();
            std::advance(it, handle - 1);
            return it->first.c_str();
        }
        static void Clear(void* list, bool free)
        {
            ++current->clear_count;
            current->clear_free = free;
            current->cleared = list;
        }
        static void Title(const char* value, void* context)
        {
            static_cast<NativeSystem*>(context)->title = value;
        }
        static void Console(bool value, void* context)
        {
            static_cast<NativeSystem*>(context)->console = value;
        }
        static void Error(const char* value, void* context)
        {
            static_cast<NativeSystem*>(context)->error_text = value;
        }
        static void Hook(void* context, int event)
        {
            auto* self = static_cast<NativeSystem*>(context);
            if (event != self->hook_event)
            {
                return;
            }
            if (event == 1)
            {
                self->Call(30, 17, self->command_name.c_str(), self->hook_module);
            }
            else if (event == 2 || event == 4 || event == 6 || event == 7)
            {
                self->Call(38, 0, "", self->hook_module);
            }
            else if (event == 5)
            {
                self->Call(7, 0, "", self->hook_module);
            }
        }
    };
    NativeSystem* NativeSystem::current{};
} // namespace

TEST_F(NativeSystem, SeparateDsoChecksDestructorSlotsAndVirtualBaseConversions)
{
    EXPECT_EQ(Call(0), 1U);
    EXPECT_EQ(Call(1), 1U);
    EXPECT_EQ(virtual_base(api), 1);
    EXPECT_EQ(Call(8), reinterpret_cast<std::uintptr_t>(api));
    EXPECT_EQ(Call(2, 22, "named-system"), 1U);
    EXPECT_EQ(Call(9), 22U);
    EXPECT_STREQ(reinterpret_cast<char*>(Call(12)), "named-system");
    EXPECT_STREQ(reinterpret_cast<char*>(Call(11)), "basesystem002");
    EXPECT_EQ(Call(13), 3U);
    EXPECT_EQ(Call(14), 711U);
}
TEST_F(NativeSystem, RejectsMissingServicesAndNonProductIdentity)
{
    system.Shutdown();
    std::string error;
    engine.COM_FreeFile = nullptr;
    EXPECT_FALSE(system.Bind(host, error));
    EXPECT_FALSE(error.empty());
    engine.COM_FreeFile = Free;
    host.product_version = 0;
    EXPECT_FALSE(system.Bind(host, error));
    EXPECT_EQ(system.Interface(), nullptr);
}
TEST_F(NativeSystem, RejectsConcurrentSystemOwner)
{
    csr::client::NativeGameUiSystem other;
    std::string error;
    EXPECT_FALSE(other.Bind(host, error));
    EXPECT_TRUE(system.IsReady());
}
TEST_F(NativeSystem, AddsFindsAndRemovesActualBorrowedModules)
{
    void* module = AddModule();
    EXPECT_EQ(Call(37, 0, "fixture-module001", nullptr, const_cast<char*>("instance")), reinterpret_cast<std::uintptr_t>(module));
    EXPECT_EQ(Call(36, 0, "duplicate", module), 0U);
    EXPECT_EQ(Call(38, 0, "", module), 1U);
    EXPECT_EQ(count(module, 2), 1);
    EXPECT_EQ(Call(38, 0, "", module), 0U);
    EXPECT_EQ(Call(37, 0, "fixture-module001"), 0U);
}
TEST_F(NativeSystem, FailedInitializationRollsBackCommandsAndDoesNotConsumeSerial)
{
    void* failed = Module(false);
    hook_module = failed;
    hook_event = 1;
    configure(failed, 0, 0, Hook, this);
    EXPECT_EQ(Call(36, 0, "failed", failed), 0U);
    EXPECT_TRUE(commands.empty());
    EXPECT_FALSE(system.HasPendingCommands());
    void* success = AddModule();
    EXPECT_EQ(count(success, 0), count(failed, 0));
}
TEST_F(NativeSystem, RejectsUnknownOptionalHltvModulesWithoutClaimingSuccess)
{
    EXPECT_EQ(Call(37, 0, "demoplayer001"), 0U);
    EXPECT_EQ(Call(37, 0, "enginewrapper002"), 0U);
    EXPECT_NE(debug.find("unsupported"), std::string::npos);
}
TEST_F(NativeSystem, AccumulatesDeltasAndFrameTicks)
{
    void* module = AddModule();
    system.Frame(0.25);
    Call(3, 5);
    EXPECT_DOUBLE_EQ(time(api), 0.75);
    EXPECT_EQ(Call(17), 2U);
    EXPECT_EQ(count(module, 1), 2);
}
TEST_F(NativeSystem, RejectsInvalidFrameDeltaWithoutAdvancing)
{
    system.Frame(-1);
    system.Frame(std::numeric_limits<double>::infinity());
    system.Frame(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(Call(17), 0U);
    EXPECT_DOUBLE_EQ(time(api), 0);
}
TEST_F(NativeSystem, StopPreventsFurtherModuleDispatch)
{
    void* module = AddModule();
    Call(39);
    system.Frame(0.5);
    EXPECT_EQ(count(module, 1), 0);
    EXPECT_EQ(Call(13), 4U);
    EXPECT_EQ(Call(17), 1U);
}
TEST_F(NativeSystem, ModuleCanRemoveItselfDuringFrame)
{
    void* module = AddModule();
    hook_module = module;
    hook_event = 2;
    configure(module, 1, 0, Hook, this);
    system.Frame(0.1);
    system.Frame(0.1);
    EXPECT_EQ(count(module, 1), 1);
    EXPECT_EQ(count(module, 2), 1);
}
TEST_F(NativeSystem, ReentrantRemoveFromModuleShutdownCallsShutdownOnce)
{
    void* module = AddModule();
    configure(module, 1, 1, nullptr, nullptr);
    EXPECT_EQ(Call(38, 0, "", module), 1U);
    EXPECT_EQ(count(module, 2), 1);
}
TEST_F(NativeSystem, ListenerCanUnsubscribeDuringSignalAndDuplicatesDoNotMultiply)
{
    void* module = AddModule();
    hook_module = module;
    hook_event = 5;
    configure(module, 1, 0, Hook, this);
    Call(6, 0, "", module);
    Call(6, 0, "", module);
    Call(4, 77);
    Call(4, 77);
    EXPECT_EQ(count(module, 3), 1);
}
TEST_F(NativeSystem, FormatsConsoleOutputAsDataAndMaintainsBoundedRedirect)
{
    Call(19, 7, "100%literal");
    EXPECT_EQ(printed, "source:100%literal:7");
    Call(20, 3, "debug");
    EXPECT_EQ(debug, "debug:debug:3");
    char buffer[8]{};
    Call(21, sizeof(buffer), "", buffer);
    Call(19, 9, "long-message");
    EXPECT_STREQ(buffer, "source:");
    EXPECT_EQ(printed, "source:100%literal:7");
    Call(21);
    Call(19, 1, "again");
    EXPECT_NE(printed.find("again"), std::string::npos);
}
TEST_F(NativeSystem, FileBuffersUseEngineLoadModeFiveAndMatchingFree)
{
    int length{};
    auto value = Call(23, 0, "file", &length);
    ASSERT_NE(value, 0U);
    EXPECT_EQ(length, static_cast<int>(file_data.size()));
    EXPECT_EQ(load_mode, 5);
    Call(24, 0, "", reinterpret_cast<void*>(value));
    EXPECT_EQ(frees, 1);
    EXPECT_EQ(Call(22), reinterpret_cast<std::uintptr_t>(this));
}
TEST_F(NativeSystem, ImplementsTitleStatusConsoleAndOldOptionalCapabilityBoundaries)
{
    Call(25, 0, "actual title");
    EXPECT_EQ(title, "actual title");
    Call(26, 0, "loading");
    EXPECT_STREQ(reinterpret_cast<char*>(Call(10)), "loading");
    Call(27, 1);
    EXPECT_TRUE(console);
    Call(18);
    EXPECT_EQ(Call(29), 0U);
    Call(28);
    EXPECT_EQ(executed, "log on\n");
    Call(28, 0, nullptr);
    EXPECT_EQ(executed, "log on\nlog off\n");
    EXPECT_STREQ(reinterpret_cast<char*>(Call(40)), "/synthetic/runtime");
}
TEST_F(NativeSystem, CommandMatchesOnlyClearsBorrowedOriginalListWithFreeTrue)
{
    int list{};
    Call(31, 0, "prefix", &list);
    EXPECT_EQ(clear_count, 1);
    EXPECT_EQ(cleared, &list);
    EXPECT_TRUE(clear_free);
}
TEST_F(NativeSystem, CheckParameterDistinguishesMissingFlagAndValue)
{
    EXPECT_EQ(Call(35, 0, "-missing"), 0U);
    EXPECT_STREQ(reinterpret_cast<char*>(Call(35, 0, "-flag")), "");
    EXPECT_STREQ(reinterpret_cast<char*>(Call(35, 0, "-value")), "actual-value");
}
TEST_F(NativeSystem, ExecutesActualEngineCommandsAndFilesWithPairedFree)
{
    Call(32, 0, "echo direct\n");
    Call(33, 0, "fixture.cfg");
    EXPECT_EQ(executed, "echo direct\necho fixture\n");
    EXPECT_EQ(loads, 1);
    EXPECT_EQ(frees, 1);
}
TEST_F(NativeSystem, RegisterFailureAndForeignCollisionDoNotRemoveForeignCommands)
{
    void* module = AddModule();
    commands["foreign"] = [] {};
    EXPECT_EQ(Call(30, 1, "foreign", module), 0U);
    add_ok = false;
    EXPECT_EQ(Call(30, 1, "failed", module), 0U);
    EXPECT_FALSE(system.HasPendingCommands());
    system.Shutdown();
    EXPECT_EQ(commands.count("foreign"), 1U);
}
TEST_F(NativeSystem, EngineForwarderPreservesSpacesEmptyArgumentsAndSemicolons)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 91, "custom", module), 1U);
    args = {"custom", "two words", "", "semi;colon"};
    commands.at("custom")();
    EXPECT_EQ(count(module, 4), 1);
    EXPECT_EQ(count(module, 5), 91);
    EXPECT_STREQ(line(module), "custom \"two words\" \"\" \"semi;colon\"");
}
TEST_F(NativeSystem, UnrepresentableEmbeddedQuotesDoNotRunMalformedCommands)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 1, "custom", module), 1U);
    args = {"custom", "bad\"argument"};
    commands.at("custom")();
    EXPECT_EQ(count(module, 4), 0);
}
TEST_F(NativeSystem, ModuleMayRemoveItselfDuringCommandAndCallbackIsUnregistered)
{
    void* module = AddModule();
    hook_module = module;
    hook_event = 4;
    configure(module, 1, 1, Hook, this);
    ASSERT_EQ(Call(30, 1, "custom", module), 1U);
    args = {"custom"};
    commands.at("custom")();
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(count(module, 2), 1);
}
TEST_F(NativeSystem, FailedRemovalRetainsPendingCallbackUntilSuccessfulRetry)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 1, "custom", module), 1U);
    remove_ok = false;
    system.Shutdown();
    EXPECT_FALSE(system.IsReady());
    EXPECT_TRUE(system.HasPendingCommands());
    EXPECT_EQ(count(module, 2), 0);
    args = {"custom"};
    commands.at("custom")();
    EXPECT_EQ(count(module, 4), 0);
    remove_ok = true;
    system.Shutdown();
    EXPECT_FALSE(system.HasPendingCommands());
    EXPECT_EQ(count(module, 2), 1);
}
TEST_F(NativeSystem, NativeShutdownSlotCleansCommandsAndBorrowedModules)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 1, "custom", module), 1U);
    Call(15);
    EXPECT_FALSE(system.IsReady());
    EXPECT_EQ(count(module, 2), 1);
    EXPECT_TRUE(commands.empty());
}
TEST_F(NativeSystem, ErrorStopsDispatchAndDeliversActualMessageWithoutBuildClaims)
{
    Call(5, 999);
    EXPECT_NE(debug.find("999"), std::string::npos);
    Call(34, 4, "failure");
    EXPECT_EQ(error_text, "error:failure:4");
    EXPECT_EQ(Call(13), 4U);
}

TEST_F(NativeSystem, ThrowingRemovalHookRetainsPendingStateWithoutTerminating)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 1, "custom", module), 1U);
    remove_throws = true;
    EXPECT_NO_THROW(system.Shutdown());
    EXPECT_TRUE(system.HasPendingCommands());
    EXPECT_EQ(count(module, 2), 0);
    remove_throws = false;
    system.Shutdown();
    EXPECT_FALSE(system.HasPendingCommands());
    EXPECT_EQ(count(module, 2), 1);
}

TEST_F(NativeSystem, ShutdownRequestedInsideFrameDefersModuleShutdownUntilReturn)
{
    void* module = AddModule();
    bool entered = false;
    struct Context
    {
        csr::client::NativeGameUiSystem* system;
        int (*count)(void*, int);
        void* module;
        bool* entered;
    } context{&system, count, module, &entered};
    configure(
        module,
        1,
        0,
        [](void* value, int event) {
            if (event != 2)
            {
                return;
            }
            auto* context = static_cast<Context*>(value);
            context->system->Shutdown();
            EXPECT_EQ(context->count(context->module, 2), 0);
            *context->entered = true;
        },
        &context
    );
    system.Frame(0.1);
    EXPECT_TRUE(entered);
    EXPECT_FALSE(system.IsReady());
    EXPECT_EQ(count(module, 2), 1);
}

TEST_F(NativeSystem, ThrowingModuleFrameIsContainedAndClosesTheSystem)
{
    void* module = AddModule();
    configure(
        module,
        1,
        0,
        [](void*, int event) {
            if (event == 2)
            {
                throw std::runtime_error("fixture frame failed");
            }
        },
        nullptr
    );
    EXPECT_NO_THROW(system.Frame(0.1));
    EXPECT_FALSE(system.IsReady());
    EXPECT_EQ(count(module, 2), 1);
}

TEST_F(NativeSystem, ShutdownDuringSuccessfulInitializationStillPairsModuleShutdown)
{
    void* module = Module();
    configure(
        module,
        1,
        0,
        [](void* value, int event) {
            if (event == 1)
            {
                static_cast<csr::client::NativeGameUiSystem*>(value)->Shutdown();
            }
        },
        &system
    );
    EXPECT_EQ(Call(36, 0, "closing-init", module), 0U);
    EXPECT_EQ(count(module, 2), 1);
    EXPECT_FALSE(system.IsReady());
}

TEST_F(NativeSystem, TypeGetterSelfRemovalDoesNotReturnRetiredModuleOrCallItsName)
{
    void* module = AddModule();
    hook_module = module;
    hook_event = 6;
    configure(module, 1, 0, Hook, this);
    EXPECT_EQ(Call(37, 0, "fixture-module001", nullptr, const_cast<char*>("instance")), 0U);
    EXPECT_EQ(count(module, 2), 1);
    EXPECT_EQ(count(module, 7), 0);
}

TEST_F(NativeSystem, NameGetterSelfRemovalDoesNotReturnRetiredModule)
{
    void* module = AddModule();
    hook_module = module;
    hook_event = 7;
    configure(module, 1, 0, Hook, this);
    EXPECT_EQ(Call(37, 0, "fixture-module001", nullptr, const_cast<char*>("instance")), 0U);
    EXPECT_EQ(count(module, 2), 1);
}

TEST_F(NativeSystem, GetterRemovalOfOtherModuleSkipsThatSnapshotEntry)
{
    void* retired = AddModule("retired");
    void* caller = AddModule("caller");
    hook_module = retired;
    hook_event = 6;
    configure(caller, 1, 0, Hook, this);
    EXPECT_EQ(Call(37, 0, "fixture-module001", nullptr, const_cast<char*>("retired")), 0U);
    EXPECT_EQ(count(retired, 2), 1);
    EXPECT_EQ(count(retired, 6), 0);
    EXPECT_EQ(count(retired, 7), 0);
}

TEST_F(NativeSystem, ThrowingTypeAndNameGettersFailClosedWithoutEscapingAbi)
{
    void* module = AddModule();
    configure(
        module,
        1,
        0,
        [](void*, int event) {
            if (event == 6)
            {
                throw std::runtime_error("fixture type getter failed");
            }
        },
        nullptr
    );
    EXPECT_NO_THROW(EXPECT_EQ(Call(37, 0, "fixture-module001"), 0U));
    configure(
        module,
        1,
        0,
        [](void*, int event) {
            if (event == 7)
            {
                throw std::runtime_error("fixture name getter failed");
            }
        },
        nullptr
    );
    EXPECT_NO_THROW(EXPECT_EQ(Call(37, 0, "fixture-module001", nullptr, const_cast<char*>("instance")), 0U));
    EXPECT_EQ(count(module, 2), 0);
}

TEST_F(NativeSystem, EngineForwarderAcceptsCaseVariedCommandInvocation)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 91, "fixture_command", module), 1U);
    args = {"FIXTURE_COMMAND", "two words"};
    commands.at("fixture_command")();
    EXPECT_EQ(count(module, 4), 1);
    EXPECT_EQ(count(module, 5), 91);
    EXPECT_STREQ(line(module), "FIXTURE_COMMAND \"two words\"");
}

TEST_F(NativeSystem, RejectsMixedCaseForeignCommandCollision)
{
    void* module = AddModule();
    commands["foreign_command"] = [] {};
    EXPECT_EQ(Call(30, 1, "FoReIgN_cOmMaNd", module), 0U);
    EXPECT_EQ(commands.size(), 1U);
    system.Shutdown();
    EXPECT_EQ(commands.count("foreign_command"), 1U);
}

TEST_F(NativeSystem, RejectsCaseVariedDuplicateOwnedCommand)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 1, "fixture_command", module), 1U);
    EXPECT_EQ(Call(30, 2, "FiXtUrE_cOmMaNd", module), 0U);
    EXPECT_EQ(commands.size(), 1U);
}

TEST_F(NativeSystem, TerminalSealSuppressesAllCachedHostServiceCalls)
{
    void* module = AddModule();
    int size{};
    void* bytes = reinterpret_cast<void*>(Call(23, 0, "before-seal", &size));
    ASSERT_NE(bytes, nullptr);
    system.EndNativeLifetime();
    EXPECT_FALSE(system.IsReady());
    EXPECT_EQ(system.Interface(), nullptr);
    Call(19, 1, "ignored");
    Call(20, 1, "ignored");
    Call(5, 1, "ignored");
    Call(24, 0, "", bytes);
    Call(25, 0, "ignored");
    Call(27, 1);
    Call(28);
    Call(31, 0, "ignored", &size);
    Call(32, 0, "ignored");
    Call(33, 0, "ignored");
    Call(34, 1, "ignored");
    EXPECT_EQ(Call(35, 0, "-value"), 0U);
    EXPECT_EQ(Call(22), 0U);
    EXPECT_EQ(Call(23, 0, "ignored", &size), 0U);
    EXPECT_EQ(Call(36, 0, "ignored", module), 0U);
    EXPECT_EQ(Call(37, 0, "fixture-module001"), 0U);
    Call(4, 1, "", module);
    Call(15);
    system.Frame(0.1);
    EXPECT_EQ(printed, "");
    EXPECT_EQ(debug, "");
    EXPECT_EQ(executed, "");
    EXPECT_EQ(title, "");
    EXPECT_FALSE(console);
    EXPECT_EQ(error_text, "");
    EXPECT_EQ(loads, 1);
    EXPECT_EQ(frees, 0);
    EXPECT_EQ(clear_count, 0);
    EXPECT_EQ(count(module, 2), 0);
    EXPECT_EQ(count(module, 1), 0);
    char untouched[] = "safe";
    Call(21, sizeof(untouched), "", untouched);
    EXPECT_STREQ(untouched, "safe");
    std::string error;
    EXPECT_FALSE(system.Bind(host, error));
    std::free(bytes);
}

TEST_F(NativeSystem, TerminalSealDuringModuleFrameDoesNotCallRetiredModules)
{
    void* module = AddModule();
    configure(
        module,
        1,
        0,
        [](void* value, int event) {
            if (event == 2)
            {
                static_cast<csr::client::NativeGameUiSystem*>(value)->EndNativeLifetime();
            }
        },
        &system
    );
    system.Frame(0.1);
    EXPECT_FALSE(system.IsReady());
    EXPECT_EQ(count(module, 1), 1);
    EXPECT_EQ(count(module, 2), 0);
    system.Shutdown();
    EXPECT_EQ(count(module, 2), 0);
}

TEST_F(NativeSystem, TerminalSealDuringGetterPreventsFurtherGettersAndDiagnostics)
{
    void* module = AddModule();
    configure(
        module,
        1,
        0,
        [](void* value, int event) {
            if (event == 6)
            {
                static_cast<csr::client::NativeGameUiSystem*>(value)->EndNativeLifetime();
            }
        },
        &system
    );
    EXPECT_EQ(Call(37, 0, "fixture-module001", nullptr, const_cast<char*>("instance")), 0U);
    EXPECT_EQ(count(module, 7), 0);
    EXPECT_EQ(count(module, 2), 0);
    EXPECT_TRUE(debug.empty());
}

TEST_F(NativeSystem, TerminalSealDuringInitializationDoesNotInvokeNativeShutdownAfterward)
{
    void* module = Module();
    configure(
        module,
        1,
        0,
        [](void* value, int event) {
            if (event == 1)
            {
                static_cast<csr::client::NativeGameUiSystem*>(value)->EndNativeLifetime();
            }
        },
        &system
    );
    EXPECT_EQ(Call(36, 0, "sealed-init", module), 0U);
    EXPECT_EQ(count(module, 2), 0);
}

TEST_F(NativeSystem, TerminalSealDuringCommandRemovalPreventsSubsequentModuleShutdown)
{
    void* module = AddModule();
    ASSERT_EQ(Call(30, 7, "custom", module), 1U);
    seal_on_remove = true;
    EXPECT_EQ(Call(38, 0, "", module), 1U);
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(count(module, 2), 0);
    EXPECT_FALSE(system.IsReady());
    EXPECT_FALSE(system.HasPendingCommands());
}
