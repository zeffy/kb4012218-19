#pragma once
#include "targetver.h"

#include <phnt_windows.h>
#include <phnt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <strsafe.h>
#include <shellapi.h>
#include <detours.h>
#include <patternfind.h>

#include "tracing.h"

extern IMAGE_DOS_HEADER __ImageBase;
#define PIMAGEBASE ((HMODULE)&__ImageBase)
