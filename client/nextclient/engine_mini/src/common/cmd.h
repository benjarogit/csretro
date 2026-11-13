#pragma once

#include <goldsrc/engine/cmd.h>
#include <goldsrc/engine/net.h>

void Cbuf_InsertText(const char* text);
void Cbuf_AddFilteredText(const char* text);
extern goldsrc::sizebuf_t cmd_text;
void Cmd_AddCommand(const char* cmd_name, goldsrc::xcommand_t function);
