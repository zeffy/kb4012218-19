#pragma once

#include <phnt_windows.h>

extern IMAGE_DOS_HEADER __ImageBase;
#define HMODULE_THISCOMPONENT ((HMODULE)&__ImageBase);

void trace_(const wchar_t *const format, ...);

#define STRINGIZEW_(x) L#x
#define STRINGIZEW(x) STRINGIZEW_(x)

#define LINEWSTR STRINGIZEW(__LINE__)
#define trace(format, ...) trace_(__FILEW__ L":" LINEWSTR L"(" __FUNCTIONW__ L"): " format L"\n", ##__VA_ARGS__)
