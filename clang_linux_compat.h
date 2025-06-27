#ifndef CLANG_LINUX_COMPAT_H
#define CLANG_LINUX_COMPAT_H

// Compatibility header for cross-compiling Windows VC9 code with clang on Linux
// This provides stub declarations for functions that VC9 headers expect
// but clang's runtime doesn't provide in the global namespace

#include <stdarg.h>  // For va_list
#include <errno.h>   // For errno_t

// Windows-specific types that clang doesn't define
typedef unsigned char byte;

// SAL (Source Annotation Language) macro definitions
// Since we prevent DriverSpecs.h inclusion, we need to define these macros
// that Windows headers reference but are now undefined
#define __pre
#define __post  
#define __deref
#define __drv_functionClass(x)
#define __drv_when(cond,annotes)
#define __drv_in(annotes)
#define __drv_out(annotes)
#define __drv_deref(annotes)
#define __drv_in_deref(annotes)
#define __drv_out_deref(annotes)
#define __drv_at(expr,annotes)
#define __drv_interlocked
#define __drv_aliasesMem
#define __drv_freesMem(kind)
#define __drv_inTry
#define __drv_preferredFunction(func,why)
#define __drv_allocatesMem(kind)

// Note: VC9 headers already provide all the wide character functions we need
// as inline functions, so we don't need to declare them here

// Threading function stubs for missing VC9 runtime functions
// These are used by the C++ runtime for thread-local storage initialization
#ifdef __cplusplus
extern "C" {
#endif

// Thread initialization functions that are missing from cross-compilation
// These are normally provided by the MSVC runtime but need stubs for cross-compilation
void __Init_thread_header(int* pOnce);
void __Init_thread_footer(int* pOnce);
void __Init_thread_abort(int* pOnce);

#ifdef __cplusplus
}
#endif

#endif // CLANG_LINUX_COMPAT_H