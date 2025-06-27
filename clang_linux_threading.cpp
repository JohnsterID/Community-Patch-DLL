// Threading function implementations for cross-compilation compatibility
// These provide minimal implementations of VC9 runtime threading functions

extern "C" {

// Forward declaration to avoid header complications
void exit(int status);

// Simple thread-local storage initialization functions
// These are used by the C++ runtime for static local variable initialization
// in multi-threaded environments. For cross-compilation, we provide minimal stubs.

void _Init_thread_header(int* pOnce) {
    // In a real implementation, this would use atomic operations
    // For cross-compilation, we provide a minimal stub
    if (pOnce && *pOnce == 0) {
        *pOnce = 1;  // Mark as initializing
    }
}

void _Init_thread_footer(int* pOnce) {
    // Mark initialization as complete
    if (pOnce) {
        *pOnce = 2;  // Mark as initialized
    }
}

void _Init_thread_abort(int* pOnce) {
    // Reset initialization state on abort
    if (pOnce) {
        *pOnce = 0;  // Reset to uninitialized
    }
}

// Thread epoch variable for synchronization
// This is used by the C++ runtime for thread-local storage management
int _Init_thread_epoch = 0;

// Standard terminate function stub
void __std_terminate() {
    // In a real implementation, this would call terminate handlers
    // For cross-compilation, we provide a minimal stub that exits
    exit(1);
}

}