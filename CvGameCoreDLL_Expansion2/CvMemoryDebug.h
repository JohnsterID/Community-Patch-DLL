/*	-------------------------------------------------------------------------------------------------------
	© 2025 JohnsterID. All rights reserved.
	Community Patch DLL - Memory Debug System
	------------------------------------------------------------------------------------------------------- */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//!	 \file		CvMemoryDebug.h
//!  \brief     Lightweight ASan-style heap tracker for MSVC 2008 (C++03 / v90)
//!
//!		Compatible with /RTC1, /MD, and Debug builds. Provides heap debugging
//!		functionality to complement existing runtime checks.
//!
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#pragma once
#ifndef CVMEMORYDEBUG_H
#define CVMEMORYDEBUG_H

// Only enable in debug builds with VPDEBUG defined
#ifdef VPDEBUG

#include <crtdbg.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

//=============================================================
// Configuration Constants
//=============================================================
#define HEAP_REDZONE_SIZE 16         // Bytes before/after user buffer
#define HEAP_REDZONE_PATTERN 0xFD    // Pattern for guard bytes
#define HEAP_ALLOC_PATTERN  0xCD     // Pattern for new memory
#define HEAP_FREE_PATTERN   0xDD     // Pattern for freed memory
#define HEAP_DEFAULT_ALIGNMENT 8     // Default memory alignment

//=============================================================
// HeapTracker Class Declaration
//=============================================================
class HeapTracker {
private:
    struct BlockInfo {
        size_t size;
        const char* file;
        int line;
        unsigned char* base; // Pointer to start of full allocated block (with guards)
        size_t alignment;
        
        BlockInfo() : size(0), file(NULL), line(0), base(NULL), alignment(0) {}
        BlockInfo(size_t sz, const char* f, int l, unsigned char* b, size_t a)
            : size(sz), file(f), line(l), base(b), alignment(a) {}
    };

    typedef std::map<void*, BlockInfo> AllocationMap;
    static AllocationMap s_allocations;
    static CRITICAL_SECTION s_lock;
    static bool s_initialized;
    static bool s_in_operation; // Prevent recursive calls during printf/map operations

    // Internal helper methods
    static void EnsureInitialized();
    static bool CheckRedZones(const BlockInfo& info, void* userPtr);
    static void* AlignPointer(void* ptr, size_t alignment);
    static size_t CalculateAlignedSize(size_t size, size_t alignment);

public:
    // Core allocation/deallocation functions
    static void* Alloc(size_t size, const char* file, int line, size_t alignment = HEAP_DEFAULT_ALIGNMENT);
    static void Free(void* ptr, const char* file, int line);
    
    // Debugging and reporting functions
    static void DumpLeaks();
    static void DumpStats();
    static bool ValidateHeap();
    static void Cleanup();
    
    // Query functions
    static size_t GetAllocationCount();
    static size_t GetTotalAllocatedBytes();
    static bool IsValidPointer(void* ptr);
};

//=============================================================
// Helper Macros
//=============================================================
#define DEBUG_MALLOC(sz) HeapTracker::Alloc((sz), __FILE__, __LINE__)
#define DEBUG_MALLOC_ALIGNED(sz, align) HeapTracker::Alloc((sz), __FILE__, __LINE__, (align))
#define DEBUG_FREE(ptr) HeapTracker::Free((ptr), __FILE__, __LINE__)
#define DEBUG_VALIDATE_HEAP() HeapTracker::ValidateHeap()

// Compatibility with crtdbg.h expectations
#define _CrtDumpMemoryLeaks() HeapTracker::DumpLeaks()
#define _CrtCheckMemory() HeapTracker::ValidateHeap()

// Optional: Global malloc/free override (use with caution)
#ifdef CVMEMORY_DEBUG_OVERRIDE_GLOBAL
#define malloc(sz) DEBUG_MALLOC(sz)
#define free(ptr) DEBUG_FREE(ptr)
#endif

//=============================================================
// RAII Helper for Automatic Cleanup
//=============================================================
class HeapTrackerCleanup {
public:
    ~HeapTrackerCleanup() {
        HeapTracker::Cleanup();
    }
};

// Global cleanup instance
extern HeapTrackerCleanup g_heapTrackerCleanup;

#else // !VPDEBUG

// No-op macros for release builds
#define DEBUG_MALLOC(sz) malloc(sz)
#define DEBUG_MALLOC_ALIGNED(sz, align) malloc(sz)
#define DEBUG_FREE(ptr) free(ptr)
#define DEBUG_VALIDATE_HEAP() (true)
#define _CrtDumpMemoryLeaks() (0)
#define _CrtCheckMemory() (true)

#endif // VPDEBUG

#endif // CVMEMORYDEBUG_H