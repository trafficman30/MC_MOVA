// stdafx.h is for project specific frequently used include files 
// stdafx.h should not be included from:
// MOVA Kernel, Utils, Comms.

#pragma once

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "MVTypes.h"
#include "MVDebug.h"
#include "MVMath.h"
#include "MVTrace.h"

#include "PCError.h"
#include "pctypes.h"

#define xg_APP_CONFIG_FILE_NAME     ("PCMOVA.exe.config")