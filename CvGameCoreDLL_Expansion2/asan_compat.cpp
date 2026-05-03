// ASan annotation support — compiled into the VP DLL.
// See asan_compat.h for full documentation.

#include "CvGameCoreDLLPCH.h"

#if defined(__has_feature) && __has_feature(address_sanitizer)
#include "asan_compat.h"

// Disable sanitizer instrumentation for this file: it runs during DLL init,
// before ASan's own shadow is fully committed, so any instrumented access
// here would fault.
#pragma clang attribute push(__attribute__((no_sanitize("address"))), apply_to = function)

void VP_Asan_PinRuntime()
{
	// Hold an extra reference to the ASan dynamic runtime so it is never
	// fully unloaded when the game engine calls FreeLibrary() on the VP DLL.
	//
	// Background: the pre-built clang_rt.asan_dynamic-i386.dll reserves its
	// shadow memory at [0x30000000-0x4fffffff] during __asan_init().  When
	// FreeLibrary(CvGameCore_Expansion2.dll) is called (the normal main-menu
	// → new-game path), the runtime's ref-count drops to zero, Windows
	// releases the shadow, and the heap grows into that range.  The next
	// LoadLibraryW() then aborts:
	//   "Shadow memory range interleaves with an existing mapping. ABORTING."
	//
	// This call increments the runtime's ref-count to 2.  FreeLibrary() on
	// the VP DLL only drops it to 1, so the runtime (and its shadow) survive.
	// On the second LoadLibraryW() __asan_init() is a no-op because
	// asan_inited == 1 in the still-loaded runtime.
	static HMODULE s_hRuntime = LoadLibraryA("clang_rt.asan_dynamic-i386.dll");
	(void)s_hRuntime;
}

#pragma clang attribute pop

#endif // address_sanitizer
