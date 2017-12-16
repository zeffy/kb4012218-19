#pragma once

#include "targetver.h"

#include <phnt_windows.h>
#include <phnt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <strsafe.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <detours.h>

#include "patternfind.h"
#include "tracing.h"

extern IMAGE_DOS_HEADER __ImageBase;
#define PIMAGEBASE ((HMODULE)&__ImageBase)
