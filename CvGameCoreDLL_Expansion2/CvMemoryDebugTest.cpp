/*	-------------------------------------------------------------------------------------------------------
	© 2025 JohnsterID. All rights reserved.
	Community Patch DLL - Memory Debug System Test
	------------------------------------------------------------------------------------------------------- */

#include "CvGameCoreDLLPCH.h"

#ifdef VPDEBUG

//=============================================================
// Memory Debug Test Functions
//=============================================================

/// Test basic allocation and deallocation
void TestBasicMemoryOperations() {
    printf("[TEST] Starting basic memory operations test...\n");
    
    // Test normal allocation/deallocation
    void* ptr1 = DEBUG_MALLOC(100);
    if (ptr1) {
        memset(ptr1, 0x42, 100); // Write some data
        DEBUG_FREE(ptr1);
        printf("[TEST] Basic allocation/deallocation: PASSED\n");
    } else {
        printf("[TEST] Basic allocation/deallocation: FAILED - allocation returned NULL\n");
    }
    
    // Test zero-size allocation
    void* ptr2 = DEBUG_MALLOC(0);
    if (ptr2 == NULL) {
        printf("[TEST] Zero-size allocation: PASSED\n");
    } else {
        DEBUG_FREE(ptr2);
        printf("[TEST] Zero-size allocation: FAILED - should return NULL\n");
    }
    
    // Test NULL free (should be safe)
    DEBUG_FREE(NULL);
    printf("[TEST] NULL free: PASSED\n");
}

/// Test memory leak detection
void TestMemoryLeakDetection() {
    printf("[TEST] Starting memory leak detection test...\n");
    
    // Intentionally leak some memory for testing
    void* leak1 = DEBUG_MALLOC(50);
    void* leak2 = DEBUG_MALLOC(200);
    
    // Use the memory to avoid compiler warnings
    if (leak1) memset(leak1, 0x11, 50);
    if (leak2) memset(leak2, 0x22, 200);
    
    printf("[TEST] Created intentional leaks for testing\n");
    
    // Check current allocation count
    size_t count = HeapTracker::GetAllocationCount();
    size_t bytes = HeapTracker::GetTotalAllocatedBytes();
    
    printf("[TEST] Current allocations: %zu blocks, %zu bytes\n", count, bytes);
    
    if (count >= 2 && bytes >= 250) {
        printf("[TEST] Leak detection tracking: PASSED\n");
    } else {
        printf("[TEST] Leak detection tracking: FAILED\n");
    }
}

/// Test buffer overrun detection
void TestBufferOverrunDetection() {
    printf("[TEST] Starting buffer overrun detection test...\n");
    
    // Allocate a small buffer
    char* buffer = static_cast<char*>(DEBUG_MALLOC(10));
    if (!buffer) {
        printf("[TEST] Buffer overrun test: FAILED - allocation failed\n");
        return;
    }
    
    // Write within bounds (should be safe)
    for (int i = 0; i < 10; ++i) {
        buffer[i] = static_cast<char>('A' + i);
    }
    
    printf("[TEST] Writing within bounds completed\n");
    
    // Validate heap should pass at this point
    if (HeapTracker::ValidateHeap()) {
        printf("[TEST] Heap validation after normal write: PASSED\n");
    } else {
        printf("[TEST] Heap validation after normal write: FAILED\n");
    }
    
    // Intentionally write past the end (buffer overrun)
    // WARNING: This will corrupt the red zone and should be detected
    buffer[10] = 'X'; // Write one byte past the end
    buffer[11] = 'Y'; // Write another byte past the end
    
    printf("[TEST] Intentional buffer overrun performed\n");
    
    // Free the buffer - this should detect the corruption
    DEBUG_FREE(buffer);
    
    printf("[TEST] Buffer overrun detection test completed\n");
}

/// Test double-free detection
void TestDoubleFreeDetection() {
    printf("[TEST] Starting double-free detection test...\n");
    
    void* ptr = DEBUG_MALLOC(100);
    if (!ptr) {
        printf("[TEST] Double-free test: FAILED - allocation failed\n");
        return;
    }
    
    // First free (should be normal)
    DEBUG_FREE(ptr);
    printf("[TEST] First free completed normally\n");
    
    // Second free (should be detected as error)
    DEBUG_FREE(ptr);
    printf("[TEST] Double-free detection test completed\n");
}

/// Test invalid pointer free detection
void TestInvalidPointerFree() {
    printf("[TEST] Starting invalid pointer free test...\n");
    
    // Try to free a stack variable (invalid)
    int stackVar = 42;
    DEBUG_FREE(&stackVar);
    
    // Try to free a random pointer (invalid)
    void* randomPtr = reinterpret_cast<void*>(0x12345678);
    DEBUG_FREE(randomPtr);
    
    printf("[TEST] Invalid pointer free test completed\n");
}

/// Run all memory debug tests
void RunMemoryDebugTests() {
    printf("\n=== MEMORY DEBUG SYSTEM TESTS ===\n");
    
    TestBasicMemoryOperations();
    printf("\n");
    
    TestMemoryLeakDetection();
    printf("\n");
    
    TestBufferOverrunDetection();
    printf("\n");
    
    TestDoubleFreeDetection();
    printf("\n");
    
    TestInvalidPointerFree();
    printf("\n");
    
    // Final heap validation and stats
    printf("=== FINAL HEAP STATUS ===\n");
    HeapTracker::DumpStats();
    HeapTracker::ValidateHeap();
    
    printf("\n=== MEMORY DEBUG TESTS COMPLETED ===\n\n");
}

#else // !VPDEBUG

void RunMemoryDebugTests() {
    printf("[INFO] Memory debug tests are only available in VPDEBUG builds\n");
}

#endif // VPDEBUG