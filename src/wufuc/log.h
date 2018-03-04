#pragma once

void logp_debug_write(const wchar_t *const format, ...);
#define log_debug_write(format, ...) \
        logp_debug_write(__FUNCTIONW__ L": " format L"\r\n", ##__VA_ARGS__)
#define trace log_debug_write
