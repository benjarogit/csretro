#pragma once

#include <cstdint>
#include <tier1/utlvector.h>

constexpr int kSayTextMaxLines = 5;
constexpr int kSayTextMaxChars = 256;

struct TextRange
{
    int start;
    int end;
    float* color;
};

// A line of the stock chat (CHudSayText). The stock client keeps kSayTextMaxLines + 1 of them.
struct SayTextLine
{
    wchar_t m_line[kSayTextMaxChars];
    CUtlVector<TextRange> m_textRanges;
    int m_clientIndex;
    float* m_teamColor;
};

// The +28 tail is the i386 CUtlVector layout of the retail client. This process
// does not overlay that object.
#if UINTPTR_MAX <= UINT32_MAX
static_assert(sizeof(SayTextLine) == sizeof(wchar_t) * kSayTextMaxChars + 28);
#endif
