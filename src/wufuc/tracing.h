#pragma once

#include <phnt_windows.h>

extern HANDLE g_hTracingMutex;

void itrace_(const wchar_t *const format, ...);
void trace_(const wchar_t *const format, ...);

#define STRINGIZEW_(x) L#x
#define STRINGIZEW(x) STRINGIZEW_(x)

#define LINEWSTR STRINGIZEW(__LINE__)
#define itrace(format, ...) itrace_(__FILEW__ L":" LINEWSTR L"(" __FUNCTIONW__ L"): " format L"\r\n", ##__VA_ARGS__)
#define trace(format, ...) trace_(__FILEW__ L":" LINEWSTR L"(" __FUNCTIONW__ L"): " format L"\r\n", ##__VA_ARGS__)
