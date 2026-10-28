// Force-included in the "unity" build of the logic tests. Unreal force-includes its own
// headers into every module .cpp, so pure code must survive these macros (and Windows ones).
#pragma once

#define UE_BUILD_SHIPPING 0
#define WITH_EDITOR 1
#define check(expr) GON_SHIM_check_macro_clash
#define checkf(expr, ...) GON_SHIM_checkf_macro_clash
#define verify(expr) GON_SHIM_verify_macro_clash
#define ensure(expr) GON_SHIM_ensure_macro_clash
#define TEXT(x) L##x
#define PI (3.1415926535897932f)
#define SMALL_NUMBER (1.e-8f)
#define KINDA_SMALL_NUMBER (1.e-4f)
#define BIG_NUMBER (3.4e+38f)
#define INDEX_NONE (-1)
#define IN
#define OUT
#define OPTIONAL
#define TRUE 1
#define FALSE 0
#define ERROR 0
#define DELETE (0x00010000L)
#define DrawText DrawTextW
#define GetMessage GetMessageW
#define GetObject GetObjectW
#define CreateFile CreateFileW
#define ReadFile GON_SHIM_ReadFile_macro_clash
#define Yield GON_SHIM_Yield_macro_clash
