#include "stdafx.h"
#include "tracing.h"

void trace_(const wchar_t *const format, ...)
{
        va_list argptr;
        int count;
        wchar_t *buffer;

        va_start(argptr, format);

        count = _vscwprintf(format, argptr) + 1;
        buffer = calloc(count, sizeof *buffer);
        vswprintf_s(buffer, count, format, argptr);

        va_end(argptr);
        OutputDebugStringW(buffer);
        free(buffer);
}
