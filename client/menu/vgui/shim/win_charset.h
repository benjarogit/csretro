#pragma once
// Windows charset helpers for LocalizedStringTable on POSIX.
#include <cwchar>
#include <cstring>

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

inline int MultiByteToWideChar(int, unsigned long, const char *src, int srcLen, wchar_t *dst, int dstChars)
{
	if (!src || !dst || dstChars <= 0)
		return 0;
	if (srcLen < 0)
		srcLen = static_cast<int>(strlen(src)) + 1;
	mbstate_t st{};
	const char *p = src;
	int n = 0;
	while (n < dstChars - 1 && (srcLen < 0 || (p - src) < srcLen))
	{
		wchar_t wc;
		size_t r = mbrtowc(&wc, p, static_cast<size_t>((src + srcLen) - p), &st);
		if (r == static_cast<size_t>(-1) || r == static_cast<size_t>(-2))
			break;
		if (r == 0)
		{
			dst[n++] = L'\0';
			break;
		}
		dst[n++] = wc;
		p += r;
		if (wc == L'\0')
			break;
	}
	dst[n < dstChars ? n : dstChars - 1] = L'\0';
	return n;
}

inline int WideCharToMultiByte(int, unsigned long, const wchar_t *src, int srcLen, char *dst, int dstBytes, const char *, int *)
{
	if (!src || !dst || dstBytes <= 0)
		return 0;
	if (srcLen < 0)
		srcLen = static_cast<int>(wcslen(src)) + 1;
	mbstate_t st{};
	char *out = dst;
	char *end = dst + dstBytes - 1;
	for (int i = 0; i < srcLen && out < end; ++i)
	{
		char buf[8];
		size_t r = wcrtomb(buf, src[i], &st);
		if (r == static_cast<size_t>(-1))
			break;
		if (out + static_cast<int>(r) > end)
			break;
		memcpy(out, buf, r);
		out += r;
		if (src[i] == L'\0')
			break;
	}
	*out = '\0';
	return static_cast<int>(out - dst);
}
