/*	-------------------------------------------------------------------------------------------------------
	© 2025 JohnsterID. All rights reserved.
	Community Patch DLL - Memory Debug System
	------------------------------------------------------------------------------------------------------- */

#include "CvGameCoreDLLPCH.h"
#include "CvMemoryDebug.h"

#ifdef VPDEBUG

//=============================================================
// Static Member Definitions
//=============================================================
HeapTracker::AllocationMap HeapTracker::s_allocations;
CRITICAL_SECTION HeapTracker::s_lock;
bool HeapTracker::s_initialized = false;
bool HeapTracker::s_in_operation = false;

// Global cleanup instance
HeapTrackerCleanup g_heapTrackerCleanup;

//=============================================================
// Private Helper Methods
//=============================================================
void HeapTracker::EnsureInitialized() {
    if (!s_initialized) {
        InitializeCriticalSection(&s_lock);
        s_initialized = true;
        
        // Print initialization message
        if (!s_in_operation) {
            s_in_operation = true;
            std::printf("[HEAP DEBUG] Memory tracking initialized\n");
            s_in_operation = false;
        }
    }
}

bool HeapTracker::CheckRedZones(const BlockInfo& info, void* userPtr) {
    bool corrupted = false;
    
    // Check front red zone
    for (size_t i = 0; i < HEAP_REDZONE_SIZE; ++i) {
        if (info.base[i] != HEAP_REDZONE_PATTERN) {
            corrupted = true;
            break;
        }
    }
    
    // Check back red zone
    if (!corrupted) {
        unsigned char* backZone = info.base + HEAP_REDZONE_SIZE + info.size;
        for (size_t i = 0; i < HEAP_REDZONE_SIZE; ++i) {
            if (backZone[i] != HEAP_REDZONE_PATTERN) {
                corrupted = true;
                break;
            }
        }
    }
    
    if (corrupted && !s_in_operation) {
        s_in_operation = true;
        std::printf("[ERROR] Buffer overrun detected: %p (%zu bytes) allocated at %s:%d\n",
                    userPtr, info.size, info.file ? info.file : "unknown", info.line);
        s_in_operation = false;
    }
    
    return !corrupted;
}

void* HeapTracker::AlignPointer(void* ptr, size_t alignment) {
    if (alignment <= 1) return ptr;
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

size_t HeapTracker::CalculateAlignedSize(size_t size, size_t alignment) {
    if (alignment <= 1) return size;
    return (size + alignment - 1) & ~(alignment - 1);
}

//=============================================================
// Public Interface Methods
//=============================================================
void* HeapTracker::Alloc(size_t size, const char* file, int line, size_t alignment) {
    if (size == 0) return NULL;
    if (s_in_operation) return malloc(size); // Prevent recursion
    
    EnsureInitialized();
    
    // Calculate total size needed: red zones + user data + alignment padding
    size_t alignedSize = CalculateAlignedSize(size, alignment);
    size_t totalSize = alignedSize + (2 * HEAP_REDZONE_SIZE) + alignment;
    
    unsigned char* rawPtr = static_cast<unsigned char*>(malloc(totalSize));
    if (!rawPtr) return NULL;
    
    // Set up red zones and user area
    unsigned char* frontZone = rawPtr;
    unsigned char* userArea = rawPtr + HEAP_REDZONE_SIZE;
    
    // Apply alignment to user area if needed
    if (alignment > 1) {
        userArea = static_cast<unsigned char*>(AlignPointer(userArea, alignment));
    }
    
    unsigned char* backZone = userArea + size;
    
    // Initialize memory patterns
    memset(frontZone, HEAP_REDZONE_PATTERN, HEAP_REDZONE_SIZE);
    memset(userArea, HEAP_ALLOC_PATTERN, size);
    memset(backZone, HEAP_REDZONE_PATTERN, HEAP_REDZONE_SIZE);
    
    // Store allocation info
    EnterCriticalSection(&s_lock);
    try {
        BlockInfo info(size, file, line, rawPtr, alignment);
        s_allocations[userArea] = info;
    } catch (...) {
        // Handle map allocation failure
        LeaveCriticalSection(&s_lock);
        free(rawPtr);
        return NULL;
    }
    LeaveCriticalSection(&s_lock);
    
    // Log allocation (with recursion protection)
    if (!s_in_operation) {
        s_in_operation = true;
        std::printf("[ALLOC] %p (%zu bytes) at %s:%d\n", 
                    userArea, size, file ? file : "unknown", line);
        s_in_operation = false;
    }
    
    return userArea;
}

void HeapTracker::Free(void* ptr, const char* file, int line) {
    if (!ptr) return;
    if (s_in_operation) { free(ptr); return; } // Prevent recursion
    
    EnsureInitialized();
    
    EnterCriticalSection(&s_lock);
    AllocationMap::iterator it = s_allocations.find(ptr);
    if (it == s_allocations.end()) {
        LeaveCriticalSection(&s_lock);
        
        if (!s_in_operation) {
            s_in_operation = true;
            std::printf("[ERROR] Invalid or double free at %s:%d -> %p\n",
                        file ? file : "unknown", line, ptr);
            s_in_operation = false;
        }
        return;
    }
    
    BlockInfo info = it->second;
    s_allocations.erase(it);
    LeaveCriticalSection(&s_lock);
    
    // Check for buffer overruns
    CheckRedZones(info, ptr);
    
    // Overwrite memory with free pattern before releasing
    size_t totalSize = info.size + (2 * HEAP_REDZONE_SIZE);
    memset(info.base, HEAP_FREE_PATTERN, totalSize);
    free(info.base);
    
    // Log deallocation
    if (!s_in_operation) {
        s_in_operation = true;
        std::printf("[FREE ] %p (%zu bytes) at %s:%d\n",
                    ptr, info.size, file ? file : "unknown", line);
        s_in_operation = false;
    }
}

void HeapTracker::DumpLeaks() {
    if (!s_initialized) return;
    if (s_in_operation) return; // Prevent recursion
    
    s_in_operation = true;
    
    EnterCriticalSection(&s_lock);
    if (s_allocations.empty()) {
        std::printf("[LEAK CHECK] No memory leaks detected.\n");
    } else {
        std::printf("[LEAK CHECK] %u allocations not freed:\n",
                    static_cast<unsigned>(s_allocations.size()));
        
        size_t totalLeaked = 0;
        for (AllocationMap::const_iterator it = s_allocations.begin();
             it != s_allocations.end(); ++it) {
            const BlockInfo& info = it->second;
            std::printf("  Leak: %p (%zu bytes) allocated at %s:%d\n",
                        it->first, info.size, 
                        info.file ? info.file : "unknown", info.line);
            totalLeaked += info.size;
        }
        std::printf("[LEAK CHECK] Total leaked: %zu bytes\n", totalLeaked);
    }
    LeaveCriticalSection(&s_lock);
    
    s_in_operation = false;
}

void HeapTracker::DumpStats() {
    if (!s_initialized) return;
    if (s_in_operation) return;
    
    s_in_operation = true;
    
    EnterCriticalSection(&s_lock);
    size_t count = s_allocations.size();
    size_t totalBytes = 0;
    
    for (AllocationMap::const_iterator it = s_allocations.begin();
         it != s_allocations.end(); ++it) {
        totalBytes += it->second.size;
    }
    LeaveCriticalSection(&s_lock);
    
    std::printf("[HEAP STATS] Active allocations: %zu, Total bytes: %zu\n", 
                count, totalBytes);
    
    s_in_operation = false;
}

bool HeapTracker::ValidateHeap() {
    if (!s_initialized) return true;
    if (s_in_operation) return true; // Prevent recursion
    
    bool isValid = true;
    s_in_operation = true;
    
    EnterCriticalSection(&s_lock);
    for (AllocationMap::const_iterator it = s_allocations.begin();
         it != s_allocations.end(); ++it) {
        if (!CheckRedZones(it->second, it->first)) {
            isValid = false;
        }
    }
    LeaveCriticalSection(&s_lock);
    
    if (isValid) {
        std::printf("[HEAP VALIDATE] All allocations are valid\n");
    } else {
        std::printf("[HEAP VALIDATE] Heap corruption detected!\n");
    }
    
    s_in_operation = false;
    return isValid;
}

void HeapTracker::Cleanup() {
    if (!s_initialized) return;
    if (s_in_operation) return;
    
    s_in_operation = true;
    std::printf("[HEAP DEBUG] Performing final leak check...\n");
    s_in_operation = false;
    
    DumpLeaks();
    
    if (s_initialized) {
        DeleteCriticalSection(&s_lock);
        s_initialized = false;
    }
}

size_t HeapTracker::GetAllocationCount() {
    if (!s_initialized) return 0;
    
    EnterCriticalSection(&s_lock);
    size_t count = s_allocations.size();
    LeaveCriticalSection(&s_lock);
    
    return count;
}

size_t HeapTracker::GetTotalAllocatedBytes() {
    if (!s_initialized) return 0;
    
    size_t total = 0;
    EnterCriticalSection(&s_lock);
    for (AllocationMap::const_iterator it = s_allocations.begin();
         it != s_allocations.end(); ++it) {
        total += it->second.size;
    }
    LeaveCriticalSection(&s_lock);
    
    return total;
}

bool HeapTracker::IsValidPointer(void* ptr) {
    if (!ptr || !s_initialized) return false;
    
    EnterCriticalSection(&s_lock);
    bool found = s_allocations.find(ptr) != s_allocations.end();
    LeaveCriticalSection(&s_lock);
    
    return found;
}

#endif // VPDEBUG