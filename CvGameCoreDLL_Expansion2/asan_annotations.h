#pragma once
// ASan annotation helpers for the VP DLL.
//
// These macros forward to the official ASan interface when the translation unit
// is compiled with -fsanitize=address, and expand to nothing otherwise.
// They are useful for custom memory management (pools, arenas, free-lists) where
// ASan cannot infer the reachability of individual slots on its own.
//
// Usage example (pool allocator):
//
//   // Mark entire pool as poisoned (inaccessible) at construction:
//   VP_ASAN_POISON_MEMORY_REGION(m_buffer, sizeof(m_buffer));
//
//   // Unpoison a slot when handing it out:
//   VP_ASAN_UNPOISON_MEMORY_REGION(ptr, slotSize);
//
//   // Re-poison a slot when it is returned to the pool:
//   VP_ASAN_POISON_MEMORY_REGION(ptr, slotSize);
//
// Reference: https://github.com/llvm/llvm-project/blob/main/compiler-rt/include/sanitizer/asan_interface.h

#if defined(__has_feature) && __has_feature(address_sanitizer)
#  define VP_ASAN_ENABLED 1
// clang-cl puts the sanitizer interface headers under the resource directory;
// the LLVM install at C:\Program Files (x86)\LLVM\lib\clang\<ver>\include.
#  include <sanitizer/asan_interface.h>
#  define VP_ASAN_POISON_MEMORY_REGION(addr, size)   __asan_poison_memory_region((addr), (size))
#  define VP_ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
#  define VP_ASAN_ENABLED 0
#  define VP_ASAN_POISON_MEMORY_REGION(addr, size)   ((void)(addr), (void)(size))
#  define VP_ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif
