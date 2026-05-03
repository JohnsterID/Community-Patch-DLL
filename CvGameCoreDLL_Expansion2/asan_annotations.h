#pragma once
// ASan annotation helpers for the VP DLL.
//
r// All macros expand to nothing when not compiled with -fsanitize=address.
//
// ── Shadow memory pinning ────────────────────────────────────────────────────
// Problem: the pre-built clang_rt.asan_dynamic-i386.dll reserves its shadow
// at [0x30000000-0x4fffffff] (scale=3, offset=0x30000000 — fixed constants
// compiled into the DLL, not changeable at runtime or via -asan-mapping-scale).
// When the game engine calls FreeLibrary(CvGameCore_Expansion2.dll) and then
// LoadLibraryW() it again (the normal main-menu → new-game path), the runtime's
// ref-count hits zero, Windows releases the shadow, the heap grows into that
// range, and the second LoadLibraryW() aborts:
//   "Shadow memory range interleaves with an existing memory mapping. ABORTING."
//
// Fix: VP_ASAN_PIN_RUNTIME() calls LoadLibraryA("clang_rt.asan_dynamic-i386.dll")
// once in DLL_PROCESS_ATTACH, holding an extra reference so the runtime is
// never fully unloaded.  The shadow stays reserved across the FreeLibrary/
// LoadLibraryW cycle.  On the second load __asan_init() is a no-op because
// asan_inited == 1 in the still-loaded runtime.
//
// Call VP_ASAN_PIN_RUNTIME() once, early in DLL_PROCESS_ATTACH (CvGameCoreDLL.cpp).
//
// ── Pool / free-list allocators ──────────────────────────────────────────────
// FObjectPool and FFreeListTrashArray keep objects alive but logically "free".
// ASan cannot see those logical frees without annotations.  Use:
//
//   VP_ASAN_POOL_FREE(ptr, size)   // slot returned to pool  → poisons it
//   VP_ASAN_POOL_ALLOC(ptr, size)  // slot handed to caller  → unpoisons it
//
// Typical pool constructor: VP_ASAN_POOL_FREE each pre-allocated object once
// so that the entire pool is poisoned until GetFreeObject() is called.
//
// ── Reference ────────────────────────────────────────────────────────────────
// https://github.com/llvm/llvm-project/blob/main/compiler-rt/include/sanitizer/asan_interface.h

#if defined(__has_feature) && __has_feature(address_sanitizer)
#  define VP_ASAN_ENABLED 1
// clang-cl places the sanitizer headers under its resource directory:
//   C:\Program Files (x86)\LLVM\lib\clang\<ver>\include\sanitizer\
#  include <sanitizer/asan_interface.h>

// Memory region access control
#  define VP_ASAN_POISON_MEMORY_REGION(addr, size)   __asan_poison_memory_region((addr), (size))
#  define VP_ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))

// Semantic aliases for pool/free-list allocators (clearer call-sites)
#  define VP_ASAN_POOL_FREE(ptr, size)   __asan_poison_memory_region((ptr), (size))
#  define VP_ASAN_POOL_ALLOC(ptr, size)  __asan_unpoison_memory_region((ptr), (size))

// Query / debug helpers
#  define VP_ASAN_IS_POISONED(addr)      __asan_address_is_poisoned((addr))
#  define VP_ASAN_DESCRIBE_ADDRESS(addr) __asan_describe_address((addr))

// Shadow pinning — implementation in asan_annotations.cpp
void VP_Asan_PinRuntime();
#  define VP_ASAN_PIN_RUNTIME() VP_Asan_PinRuntime()

#else
#  define VP_ASAN_ENABLED 0
#  define VP_ASAN_POISON_MEMORY_REGION(addr, size)   ((void)(addr), (void)(size))
#  define VP_ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#  define VP_ASAN_POOL_FREE(ptr, size)               ((void)(ptr),  (void)(size))
#  define VP_ASAN_POOL_ALLOC(ptr, size)              ((void)(ptr),  (void)(size))
#  define VP_ASAN_IS_POISONED(addr)                  (false)
#  define VP_ASAN_DESCRIBE_ADDRESS(addr)             ((void)(addr))
#  define VP_ASAN_PIN_RUNTIME()                      ((void)0)
#endif
