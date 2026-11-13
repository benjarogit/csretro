#include <cstring>
#include <goldsrc/client_api.h>
#include <goldsrc/engine/cmd.h>
#include <goldsrc/engine/net.h>

#include "../engine.h"

int* p_cmd_argc;
char* (*p_cmd_argv)[80];

goldsrc::sizebuf_t cmd_text{};

void Cbuf_AddFilteredText(const char* text)
{
    // Process body: filter empty text and queue the rest on cmd_text.
    // Init stores this pointer and does not call it.
    if (text == nullptr || text[0] == '\0')
    {
        return;
    }
    if (cmd_text.data == nullptr || cmd_text.maxsize <= 0)
    {
        return;
    }
    const int len = static_cast<int>(std::strlen(text));
    if (len <= 0 || cmd_text.cursize + len > cmd_text.maxsize)
    {
        return;
    }
    std::memcpy(cmd_text.data + cmd_text.cursize, text, static_cast<size_t>(len));
    cmd_text.cursize += len;
}

void Cbuf_InsertText(const char* text)
{
    eng()->Cbuf_InsertText.InvokeChained(text);
}

void Cmd_AddCommand(const char* cmd_name, goldsrc::xcommand_t function)
{
    eng()->Cmd_AddCommand.InvokeChained(cmd_name, function);
}

int Cmd_Argc()
{
    p_g_engdstAddrs->Cmd_Argc();
    return *p_cmd_argc;
}

const char* Cmd_Argv(int arg)
{
    p_g_engdstAddrs->Cmd_Argv(&arg);

    if (arg < *p_cmd_argc)
        return (*p_cmd_argv)[arg];

    return "";
}
