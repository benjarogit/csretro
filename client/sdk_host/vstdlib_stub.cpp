// The product process does not load Valve's i386 libvstdlib.so.
// vgui2 imports KeyValuesSystem from this library. The object is only the
// symbol table the controls already expect. It is not a second scheme.

#include <vstdlib/IKeyValuesSystem.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

    class HostKeyValues final : public IKeyValuesSystem
    {
    public:
        void RegisterSizeofKeyValues(int) override {}

        void* AllocKeyValuesMemory(int size) override
        {
            if (size <= 0)
            {
                return nullptr;
            }
            return std::malloc(static_cast<size_t>(size));
        }

        void FreeKeyValuesMemory(void* memory) override
        {
            std::free(memory);
        }

        HKeySymbol GetSymbolForString(const char* name) override
        {
            if (name == nullptr)
            {
                return INVALID_KEY_SYMBOL;
            }
            for (size_t index = 0; index < names_.size(); ++index)
            {
                if (names_[index] == name)
                {
                    return static_cast<HKeySymbol>(index);
                }
            }
            names_.emplace_back(name);
            return static_cast<HKeySymbol>(names_.size() - 1);
        }

        const char* GetStringForSymbol(HKeySymbol symbol) override
        {
            if (symbol < 0 || static_cast<size_t>(symbol) >= names_.size())
            {
                return "";
            }
            return names_[static_cast<size_t>(symbol)].c_str();
        }

        void AddKeyValuesToMemoryLeakList(void*, HKeySymbol) override {}
        void RemoveKeyValuesFromMemoryLeakList(void*) override {}

    private:
        std::vector<std::string> names_;
    };

    HostKeyValues g_key_values;

} // namespace

extern "C" __attribute__((visibility("default"))) IKeyValuesSystem* KeyValuesSystem()
{
    return &g_key_values;
}
