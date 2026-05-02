// UBSan handlers compatible with VS2008 runtime
// Provides descriptive error messages with value printing and deduplication
// without requiring the UCRT-dependent clang runtime library.
//
// Based on LLVM compiler-rt/lib/ubsan/ but simplified for VS2008 compatibility.
//
// CRITICAL: This entire file must not be instrumented by any sanitizer.
// The handler functions ARE the sanitizer runtime — instrumenting them causes
// infinite recursion (e.g. hashLocation's FNV-1a unsigned multiply triggers
// unsigned-integer-overflow handler which calls hashLocation again).

#include "CvGameCoreDLLPCH.h"

#ifdef VPDEBUG

// Disable all sanitizer instrumentation for every function in this file
#pragma clang attribute push(__attribute__((no_sanitize("undefined", "unsigned-integer-overflow", "implicit-conversion"))), apply_to = function)

// ============================================================================
// Type Descriptors (from LLVM ubsan_value.h)
// ============================================================================

struct SourceLocation {
    const char* filename;
    unsigned int line;
    unsigned int column;
};

// Type descriptor - matches LLVM's layout
struct TypeDescriptor {
    unsigned short typeKind;   // 0=integer, 1=float, 0xFFFF=unknown
    unsigned short typeInfo;   // For int: bit 0 = signed, bits 1-15 = bit width log2
    char typeName[1];          // Variable length, null-terminated
    
    bool isInteger() const { return typeKind == 0; }
    bool isFloat() const { return typeKind == 1; }
    bool isSigned() const { return typeInfo & 1; }
    unsigned getIntBitWidth() const { return 1 << (typeInfo >> 1); }
    unsigned getFloatBitWidth() const { return typeInfo; }
};

// Value handle - stores value inline if small, otherwise pointer
typedef uintptr_t ValueHandle;

// ============================================================================
// Data Structures (from LLVM ubsan_handlers.h)
// ============================================================================

struct TypeMismatchData {
    SourceLocation loc;
    const TypeDescriptor* type;
    unsigned char logAlignment;
    unsigned char typeCheckKind;
};

struct OverflowData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct ShiftOutOfBoundsData {
    SourceLocation loc;
    const TypeDescriptor* lhsType;
    const TypeDescriptor* rhsType;
};

struct OutOfBoundsData {
    SourceLocation loc;
    const TypeDescriptor* arrayType;
    const TypeDescriptor* indexType;
};

struct UnreachableData {
    SourceLocation loc;
};

struct VLABoundData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct InvalidValueData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct NonNullArgData {
    SourceLocation loc;
    SourceLocation attrLoc;
    int argIndex;
};

struct NonNullReturnData {
    SourceLocation attrLoc;
};

struct PointerOverflowData {
    SourceLocation loc;
};

// Legacy v1 layout emitted by very old clang (no source location):
//   struct FloatCastOverflowData { const TypeDescriptor *FromType, *ToType; };
// Current v2 layout (with source location) — always emitted by modern clang:
struct FloatCastOverflowDataV2 {
    SourceLocation loc;
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
};

struct InvalidBuiltinData {
    SourceLocation loc;
    unsigned char kind;
};

struct FunctionTypeMismatchData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct AlignmentAssumptionData {
    SourceLocation loc;
    SourceLocation assumptionLoc;
    const TypeDescriptor* type;
};

struct ImplicitConversionData {
    SourceLocation loc;
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
    // ImplicitConversionCheckKind (keep in sync with LLVM CGExprScalar.cpp):
    //   0 = ICCK_IntegerTruncation (legacy clang 7)
    //   1 = ICCK_UnsignedIntegerTruncation
    //   2 = ICCK_SignedIntegerTruncation
    //   3 = ICCK_IntegerSignChange
    //   4 = ICCK_SignedIntegerTruncationOrSignChange
    unsigned char kind;
    unsigned int BitfieldBits; // non-zero when source is a bitfield of this width
};

// ============================================================================
// Deduplication
// ============================================================================
//
// Each UBSan data struct (OverflowData, TypeMismatchData, etc.) is emitted by
// the compiler as a static const local variable — its address is permanently
// unique per source violation site.  Using the data pointer as key gives:
//   • No false collisions (unlike hashing filename+line+col strings)
//   • No string iteration (O(1) instead of O(filename length))
//   • Thread safety via InterlockedCompareExchange (Windows XP+, no CRT needed)
//
// Slot collision (two distinct sites mapping to the same index) means the
// secondary site loses dedup and fires repeatedly — acceptable in a 1024-slot
// table with the handful of active sites a typical debug session produces.

static const size_t DEDUP_TABLE_SIZE = 1024;
static volatile LONG g_dedupTable[DEDUP_TABLE_SIZE];

// Returns true (duplicate — suppress) if this data pointer has already been
// reported. Returns false (new — report) and claims the slot otherwise.
static bool isDuplicate(const void* data)
{
    LONG key   = (LONG)(uintptr_t)data;
    size_t idx = ((uintptr_t)data >> 4) % DEDUP_TABLE_SIZE;
    // CAS: if slot is 0 (empty), write key and return 0 → first report → NOT dup.
    //      if slot already holds key, return key                → IS dup.
    //      if slot holds another key (collision), return other  → NOT dup (report again).
    LONG prev  = InterlockedCompareExchange(&g_dedupTable[idx], key, 0);
    return prev == key;
}

// ============================================================================
// Value Formatting (extract and print actual values)
// ============================================================================

static void formatValue(char* buffer, size_t bufSize, const TypeDescriptor* type, ValueHandle value)
{
    if (!type) {
        sprintf_s(buffer, bufSize, "?");
        return;
    }
    
    if (type->isInteger()) {
        unsigned bits = type->getIntBitWidth();
        if (type->isSigned()) {
            if (bits <= 32) {
                sprintf_s(buffer, bufSize, "%d", (int)(intptr_t)value);
            } else {
                // On 32-bit builds sizeof(ValueHandle)==4, so 64-bit values are passed by pointer
                sprintf_s(buffer, bufSize, "%lld", *(long long*)value);
            }
        } else {
            if (bits <= 32) {
                sprintf_s(buffer, bufSize, "%u", (unsigned int)value);
            } else {
                // On 32-bit builds sizeof(ValueHandle)==4, so 64-bit values are passed by pointer
                sprintf_s(buffer, bufSize, "%llu", *(unsigned long long*)value);
            }
        }
    } else if (type->isFloat()) {
        unsigned bits = type->getFloatBitWidth();
        if (bits <= 32) {
            // Float is passed as bits in the handle
            union { unsigned int i; float f; } u;
            u.i = (unsigned int)value;
            sprintf_s(buffer, bufSize, "%g", u.f);
        } else {
            // Double is passed by pointer
            sprintf_s(buffer, bufSize, "%g", *(double*)value);
        }
    } else {
        sprintf_s(buffer, bufSize, "<unknown>");
    }
}

// ============================================================================
// Core Reporting
// ============================================================================

static void ubsan_output(const char* message)
{
    OutputDebugStringA(message);
    fprintf(stderr, "%s", message);
    fflush(stderr);
}

// Break into the debugger only when one is attached.
// Used by recoverable handlers so the game continues running without a debugger
// (accumulating all violations in output) while still breaking when debugging.
static void ubsan_break()
{
    if (IsDebuggerPresent())
        __debugbreak();
}

// Returns true if the violation was newly reported (caller should break).
// Returns false if it was a duplicate (caller should silently continue).
static bool ubsan_report(const void* key, const char* errorType, const SourceLocation& loc)
{
    if (isDuplicate(key)) return false;

    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: %s ***\n    at %s:%u:%u\n",
        errorType,
        loc.filename ? loc.filename : "<unknown>",
        loc.line,
        loc.column);

    ubsan_output(buffer);
    return true;
}

static bool ubsan_report_with_value(const void* key, const char* errorType, const SourceLocation& loc,
                                     const char* desc, const TypeDescriptor* type, ValueHandle value)
{
    if (isDuplicate(key)) return false;

    char valStr[64];
    formatValue(valStr, sizeof(valStr), type, value);

    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: %s ***\n    %s: %s (type: %s)\n    at %s:%u:%u\n",
        errorType,
        desc,
        valStr,
        type ? type->typeName : "<unknown>",
        loc.filename ? loc.filename : "<unknown>",
        loc.line,
        loc.column);

    ubsan_output(buffer);
    return true;
}

static bool ubsan_report_overflow(const void* key, const char* op, const SourceLocation& loc,
                                   const TypeDescriptor* type, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(key)) return false;

    char lhsStr[64], rhsStr[64];
    formatValue(lhsStr, sizeof(lhsStr), type, lhs);
    formatValue(rhsStr, sizeof(rhsStr), type, rhs);

    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: %s overflow ***\n    %s %s %s cannot be represented in type %s\n    at %s:%u:%u\n",
        type && type->isSigned() ? "signed integer" : "unsigned integer",
        lhsStr, op, rhsStr,
        type ? type->typeName : "<unknown>",
        loc.filename ? loc.filename : "<unknown>",
        loc.line,
        loc.column);

    ubsan_output(buffer);
    return true;
}

// ============================================================================
// Handler Implementations (extern "C" for linker compatibility)
// ============================================================================

extern "C" {

// ---- Type mismatch ----

static const char* getTypeCheckKindName(unsigned char kind)
{
    static const char* names[] = {
        "load of", "store to", "reference binding to", "member access within",
        "member call on", "constructor call on", "downcast of", "downcast of",
        "upcast of", "cast to virtual base of", "_Nonnull binding to",
        "dynamic operation on"
    };
    return kind < sizeof(names)/sizeof(names[0]) ? names[kind] : "access of";
}

static bool impl_type_mismatch(TypeMismatchData* data, ValueHandle pointer)
{
    if (isDuplicate(data)) return false;
    const char* typeName  = data->type ? data->type->typeName : "<unknown>";
    const char* checkKind = getTypeCheckKindName(data->typeCheckKind);
    char buffer[512];
    if (!pointer) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: null pointer access ***\n    %s null pointer of type %s\n    at %s:%u:%u\n",
            checkKind, typeName, data->loc.filename, data->loc.line, data->loc.column);
    } else if ((pointer & ((1u << data->logAlignment) - 1u)) != 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: misaligned address ***\n    %s misaligned address 0x%p for type %s (requires %u-byte alignment)\n    at %s:%u:%u\n",
            checkKind, (void*)pointer, typeName, 1u << data->logAlignment,
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: type mismatch ***\n    %s address 0x%p with insufficient space for type %s\n    at %s:%u:%u\n",
            checkKind, (void*)pointer, typeName,
            data->loc.filename, data->loc.line, data->loc.column);
    }
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_type_mismatch_v1(TypeMismatchData* data, ValueHandle pointer)
{
    if (impl_type_mismatch(data, pointer)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_type_mismatch_v1_abort(TypeMismatchData* data, ValueHandle pointer)
{
    if (impl_type_mismatch(data, pointer)) __debugbreak();
}

// ---- Integer overflow ----

__declspec(dllexport) void __ubsan_handle_add_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "+", data->loc, data->type, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_add_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "+", data->loc, data->type, lhs, rhs)) __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_sub_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "-", data->loc, data->type, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_sub_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "-", data->loc, data->type, lhs, rhs)) __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_mul_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "*", data->loc, data->type, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_mul_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "*", data->loc, data->type, lhs, rhs)) __debugbreak();
}

static bool impl_divrem_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(data)) return false;
    char lhsStr[64], rhsStr[64];
    formatValue(lhsStr, sizeof(lhsStr), data->type, lhs);
    formatValue(rhsStr, sizeof(rhsStr), data->type, rhs);
    char buffer[512];
    if (rhs == 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: division by zero ***\n    %s / 0 is undefined\n    at %s:%u:%u\n",
            lhsStr, data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: division overflow ***\n    %s / %s cannot be represented in type %s\n    at %s:%u:%u\n",
            lhsStr, rhsStr, data->type ? data->type->typeName : "<unknown>",
            data->loc.filename, data->loc.line, data->loc.column);
    }
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_divrem_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_divrem_overflow(data, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_divrem_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_divrem_overflow(data, lhs, rhs)) __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_negate_overflow(OverflowData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "negation overflow", data->loc, "cannot negate", data->type, val)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_negate_overflow_abort(OverflowData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "negation overflow", data->loc, "cannot negate", data->type, val)) __debugbreak();
}

// ---- Shift errors ----

static bool impl_shift_out_of_bounds(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(data)) return false;
    char lhsStr[64], rhsStr[64];
    formatValue(lhsStr, sizeof(lhsStr), data->lhsType, lhs);
    formatValue(rhsStr, sizeof(rhsStr), data->rhsType, rhs);
    char buffer[512];
    if (data->lhsType && data->lhsType->isInteger()) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: shift out of bounds ***\n    shift amount %s is invalid for %u-bit type %s (value: %s)\n    at %s:%u:%u\n",
            rhsStr, data->lhsType->getIntBitWidth(), data->lhsType->typeName, lhsStr,
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: shift out of bounds ***\n    shift amount %s is invalid for type %s (value: %s)\n    at %s:%u:%u\n",
            rhsStr, data->lhsType ? data->lhsType->typeName : "<unknown>", lhsStr,
            data->loc.filename, data->loc.line, data->loc.column);
    }
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_shift_out_of_bounds(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_shift_out_of_bounds(data, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_shift_out_of_bounds_abort(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_shift_out_of_bounds(data, lhs, rhs)) __debugbreak();
}

// ---- Array bounds ----

__declspec(dllexport) void __ubsan_handle_out_of_bounds(OutOfBoundsData* data, ValueHandle index)
{
    if (ubsan_report_with_value(data, "array index out of bounds", data->loc, "index", data->indexType, index)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_out_of_bounds_abort(OutOfBoundsData* data, ValueHandle index)
{
    if (ubsan_report_with_value(data, "array index out of bounds", data->loc, "index", data->indexType, index)) __debugbreak();
}

// ---- Unreachable / missing return (UNRECOVERABLE -- always fatal) ----

__declspec(dllexport) void __ubsan_handle_builtin_unreachable(UnreachableData* data)
{
    if (ubsan_report(data, "execution reached __builtin_unreachable()", data->loc))
        __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_builtin_unreachable_abort(UnreachableData* data)
{
    __ubsan_handle_builtin_unreachable(data);
}

__declspec(dllexport) void __ubsan_handle_missing_return(UnreachableData* data)
{
    if (ubsan_report(data, "execution reached end of non-void function without returning a value", data->loc))
        __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_missing_return_abort(UnreachableData* data)
{
    __ubsan_handle_missing_return(data);
}

// ---- VLA bound ----

__declspec(dllexport) void __ubsan_handle_vla_bound_not_positive(VLABoundData* data, ValueHandle bound)
{
    if (ubsan_report_with_value(data, "variable length array bound is not positive", data->loc, "bound", data->type, bound)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_vla_bound_not_positive_abort(VLABoundData* data, ValueHandle bound)
{
    if (ubsan_report_with_value(data, "variable length array bound is not positive", data->loc, "bound", data->type, bound)) __debugbreak();
}

// ---- Float cast overflow ----
// Takes void* to accommodate both legacy v1 (no SourceLocation) and current v2
// (with SourceLocation) layouts.  Modern clang always emits v2.

static bool impl_float_cast_overflow(void* dataPtr, ValueHandle val)
{
    FloatCastOverflowDataV2* data = (FloatCastOverflowDataV2*)dataPtr;
    if (isDuplicate(data)) return false;
    char valStr[64];
    formatValue(valStr, sizeof(valStr), data->fromType, val);
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: float cast overflow ***\n    %s (type %s) is outside the range of type %s\n    at %s:%u:%u\n",
        valStr,
        data->fromType ? data->fromType->typeName : "<unknown>",
        data->toType   ? data->toType->typeName   : "<unknown>",
        data->loc.filename, data->loc.line, data->loc.column);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_float_cast_overflow(void* data, ValueHandle val)
{
    if (impl_float_cast_overflow(data, val)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_float_cast_overflow_abort(void* data, ValueHandle val)
{
    if (impl_float_cast_overflow(data, val)) __debugbreak();
}

// ---- Load of invalid value (bad bool / unscoped enum) ----

__declspec(dllexport) void __ubsan_handle_load_invalid_value(InvalidValueData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "load of value outside valid range for type", data->loc, "value", data->type, val)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_load_invalid_value_abort(InvalidValueData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "load of value outside valid range for type", data->loc, "value", data->type, val)) __debugbreak();
}

// ---- Invalid builtin (__builtin_ctz/clz(0), __builtin_assume(false)) ----

static bool impl_invalid_builtin(InvalidBuiltinData* data)
{
    const char* msg;
    char buf[96];
    switch (data->kind) {
    case 0:  msg = "passing zero to __builtin_ctz(), which is undefined"; break;
    case 1:  msg = "passing zero to __builtin_clz(), which is undefined"; break;
    case 2:  msg = "__builtin_assume() evaluated to false";                break;
    default:
        sprintf_s(buf, sizeof(buf), "invalid use of builtin (kind=%u)", (unsigned)data->kind);
        msg = buf;
        break;
    }
    return ubsan_report(data, msg, data->loc);
}

__declspec(dllexport) void __ubsan_handle_invalid_builtin(InvalidBuiltinData* data)
{
    if (impl_invalid_builtin(data)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_invalid_builtin_abort(InvalidBuiltinData* data)
{
    if (impl_invalid_builtin(data)) __debugbreak();
}

// ---- Nonnull argument / return ----

static bool impl_nonnull_arg(NonNullArgData* data)
{
    if (isDuplicate(data)) return false;
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: null pointer passed to nonnull argument ***\n    argument index: %d\n    at %s:%u:%u\n",
        data->argIndex,
        data->loc.filename, data->loc.line, data->loc.column);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_nonnull_arg(NonNullArgData* data)
{
    if (impl_nonnull_arg(data)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_nonnull_arg_abort(NonNullArgData* data)
{
    if (impl_nonnull_arg(data)) __debugbreak();
}

// Dedup key uses loc (the return-statement SourceLocation*) -- a static const
// unique per return site, same dedup guarantee as using data*.
__declspec(dllexport) void __ubsan_handle_nonnull_return_v1(NonNullReturnData* data, SourceLocation* loc)
{
    if (ubsan_report(loc, "null returned from function declared to never return null", *loc)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_nonnull_return_v1_abort(NonNullReturnData* data, SourceLocation* loc)
{
    if (ubsan_report(loc, "null returned from function declared to never return null", *loc)) __debugbreak();
}

// ---- Pointer overflow ----

static bool impl_pointer_overflow(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    if (isDuplicate(data)) return false;
    char buffer[512];
    if (base == 0 && result == 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: pointer overflow ***\n    applying zero offset to null pointer\n    at %s:%u:%u\n",
            data->loc.filename, data->loc.line, data->loc.column);
    } else if (base == 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: pointer overflow ***\n    applying non-zero offset to null pointer (result: 0x%p)\n    at %s:%u:%u\n",
            (void*)result, data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: pointer overflow ***\n    pointer 0x%p with offset overflowed to 0x%p\n    at %s:%u:%u\n",
            (void*)base, (void*)result, data->loc.filename, data->loc.line, data->loc.column);
    }
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_pointer_overflow(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    if (impl_pointer_overflow(data, base, result)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_pointer_overflow_abort(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    if (impl_pointer_overflow(data, base, result)) __debugbreak();
}

// ---- Function type mismatch (indirect call through wrong-type pointer) ----

static bool impl_function_type_mismatch(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    if (isDuplicate(data)) return false;
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: indirect function call type mismatch ***\n    call through pointer 0x%p to function of wrong type %s\n    at %s:%u:%u\n",
        (void*)ptr,
        data->type ? data->type->typeName : "<unknown>",
        data->loc.filename, data->loc.line, data->loc.column);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_function_type_mismatch(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    if (impl_function_type_mismatch(data, ptr)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_function_type_mismatch_abort(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    if (impl_function_type_mismatch(data, ptr)) __debugbreak();
}

// ---- Alignment assumption ----

static bool impl_alignment_assumption(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    if (isDuplicate(data)) return false;
    char buffer[512];
    if (offset) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: alignment assumption violated ***\n    address 0x%p with offset %u does not meet alignment %u for type %s\n    at %s:%u:%u\n",
            (void*)ptr, (unsigned)offset, (unsigned)align,
            data->type ? data->type->typeName : "<unknown>",
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: alignment assumption violated ***\n    address 0x%p does not meet alignment %u for type %s\n    at %s:%u:%u\n",
            (void*)ptr, (unsigned)align,
            data->type ? data->type->typeName : "<unknown>",
            data->loc.filename, data->loc.line, data->loc.column);
    }
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_alignment_assumption(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    if (impl_alignment_assumption(data, ptr, align, offset)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_alignment_assumption_abort(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    if (impl_alignment_assumption(data, ptr, align, offset)) __debugbreak();
}

// ---- Implicit conversion (truncation, sign change) ----

static const char* getImplicitConversionKindName(unsigned char kind)
{
    static const char* names[] = {
        "integer truncation",              // 0 (legacy clang 7)
        "unsigned integer truncation",     // 1
        "signed integer truncation",       // 2
        "integer sign change",             // 3
        "signed truncation or sign change" // 4
    };
    return kind < sizeof(names)/sizeof(names[0]) ? names[kind] : "implicit conversion";
}

static bool impl_implicit_conversion(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    if (isDuplicate(data)) return false;
    char srcStr[64], dstStr[64];
    formatValue(srcStr, sizeof(srcStr), data->fromType, src);
    formatValue(dstStr, sizeof(dstStr), data->toType,   dst);
    char buffer[512];
    if (data->BitfieldBits) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: implicit conversion (%s) ***\n    value %s (type %s, bitfield %u bits) changed to %s (type %s)\n    at %s:%u:%u\n",
            getImplicitConversionKindName(data->kind),
            srcStr, data->fromType ? data->fromType->typeName : "<unknown>", data->BitfieldBits,
            dstStr, data->toType   ? data->toType->typeName   : "<unknown>",
            data->loc.filename ? data->loc.filename : "<unknown>",
            data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: implicit conversion (%s) ***\n    value %s (type %s) changed to %s (type %s)\n    at %s:%u:%u\n",
            getImplicitConversionKindName(data->kind),
            srcStr, data->fromType ? data->fromType->typeName : "<unknown>",
            dstStr, data->toType   ? data->toType->typeName   : "<unknown>",
            data->loc.filename ? data->loc.filename : "<unknown>",
            data->loc.line, data->loc.column);
    }
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_implicit_conversion(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    if (impl_implicit_conversion(data, src, dst)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_implicit_conversion_abort(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    if (impl_implicit_conversion(data, src, dst)) __debugbreak();
}

} // extern "C"


#pragma clang attribute pop

#endif // VPDEBUG
