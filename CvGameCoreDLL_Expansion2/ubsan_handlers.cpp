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

struct FloatCastOverflowData {
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
    unsigned char kind; // 0=integer truncation, 1=unsigned integer truncation, 2=sign change, 3=signed truncation/sign change
};

// ============================================================================
// Deduplication (simple hash set to avoid reporting same error repeatedly)
// ============================================================================

static const size_t DEDUP_TABLE_SIZE = 1024;
static unsigned int g_dedupTable[DEDUP_TABLE_SIZE] = {0};

static unsigned int hashLocation(const SourceLocation& loc)
{
    // Simple FNV-1a hash
    unsigned int hash = 2166136261u;
    if (loc.filename) {
        for (const char* p = loc.filename; *p; ++p) {
            hash ^= (unsigned char)*p;
            hash *= 16777619u;
        }
    }
    hash ^= loc.line;
    hash *= 16777619u;
    hash ^= loc.column;
    hash *= 16777619u;
    return hash;
}

static bool isDuplicate(const SourceLocation& loc)
{
    unsigned int hash = hashLocation(loc);
    size_t index = hash % DEDUP_TABLE_SIZE;
    
    if (g_dedupTable[index] == hash) {
        return true;  // Already reported
    }
    g_dedupTable[index] = hash;
    return false;
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
                sprintf_s(buffer, bufSize, "%lld", (long long)(intptr_t)value);
            }
        } else {
            if (bits <= 32) {
                sprintf_s(buffer, bufSize, "%u", (unsigned int)value);
            } else {
                sprintf_s(buffer, bufSize, "%llu", (unsigned long long)value);
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

static void ubsan_report(const char* errorType, const SourceLocation& loc, bool doBreak = true)
{
    if (isDuplicate(loc)) return;
    
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer), 
        "\n*** UBSAN: %s ***\n    at %s:%u:%u\n",
        errorType,
        loc.filename ? loc.filename : "<unknown>",
        loc.line,
        loc.column);
    
    ubsan_output(buffer);
    if (doBreak) __debugbreak();
}

static void ubsan_report_with_value(const char* errorType, const SourceLocation& loc,
                                     const char* desc, const TypeDescriptor* type, ValueHandle value,
                                     bool doBreak = true)
{
    if (isDuplicate(loc)) return;
    
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
    if (doBreak) __debugbreak();
}

static void ubsan_report_overflow(const char* op, const SourceLocation& loc,
                                   const TypeDescriptor* type, ValueHandle lhs, ValueHandle rhs,
                                   bool doBreak = true)
{
    if (isDuplicate(loc)) return;
    
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
    if (doBreak) __debugbreak();
}

// ============================================================================
// Handler Implementations (extern "C" for linker compatibility)
// ============================================================================

extern "C" {

// Type check kinds for type mismatch
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

// Type mismatch (null pointer, misaligned, wrong vptr)
__declspec(dllexport) void __ubsan_handle_type_mismatch_v1(TypeMismatchData* data, ValueHandle pointer)
{
    if (isDuplicate(data->loc)) return;
    
    const char* typeName = data->type ? data->type->typeName : "<unknown>";
    const char* checkKind = getTypeCheckKindName(data->typeCheckKind);
    
    char buffer[512];
    if (!pointer) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: null pointer access ***\n    %s null pointer of type %s\n    at %s:%u:%u\n",
            checkKind, typeName, data->loc.filename, data->loc.line, data->loc.column);
    } else if ((pointer & ((1 << data->logAlignment) - 1)) != 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: misaligned address ***\n    %s misaligned address 0x%p for type %s (requires %u byte alignment)\n    at %s:%u:%u\n",
            checkKind, (void*)pointer, typeName, 1u << data->logAlignment,
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: type mismatch ***\n    %s address 0x%p with insufficient space for type %s\n    at %s:%u:%u\n",
            checkKind, (void*)pointer, typeName, data->loc.filename, data->loc.line, data->loc.column);
    }
    
    ubsan_output(buffer);
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_type_mismatch_v1_abort(TypeMismatchData* data, ValueHandle pointer)
{
    __ubsan_handle_type_mismatch_v1(data, pointer);
}

// Integer overflow handlers
__declspec(dllexport) void __ubsan_handle_add_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    ubsan_report_overflow("+", data->loc, data->type, lhs, rhs);
}

__declspec(dllexport) void __ubsan_handle_add_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    __ubsan_handle_add_overflow(data, lhs, rhs);
}

__declspec(dllexport) void __ubsan_handle_sub_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    ubsan_report_overflow("-", data->loc, data->type, lhs, rhs);
}

__declspec(dllexport) void __ubsan_handle_sub_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    __ubsan_handle_sub_overflow(data, lhs, rhs);
}

__declspec(dllexport) void __ubsan_handle_mul_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    ubsan_report_overflow("*", data->loc, data->type, lhs, rhs);
}

__declspec(dllexport) void __ubsan_handle_mul_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    __ubsan_handle_mul_overflow(data, lhs, rhs);
}

__declspec(dllexport) void __ubsan_handle_divrem_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(data->loc)) return;
    
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
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_divrem_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    __ubsan_handle_divrem_overflow(data, lhs, rhs);
}

__declspec(dllexport) void __ubsan_handle_negate_overflow(OverflowData* data, ValueHandle val)
{
    ubsan_report_with_value("negation overflow", data->loc, "cannot negate", data->type, val);
}

__declspec(dllexport) void __ubsan_handle_negate_overflow_abort(OverflowData* data, ValueHandle val)
{
    __ubsan_handle_negate_overflow(data, val);
}

// Shift errors
__declspec(dllexport) void __ubsan_handle_shift_out_of_bounds(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(data->loc)) return;
    
    char lhsStr[64], rhsStr[64];
    formatValue(lhsStr, sizeof(lhsStr), data->lhsType, lhs);
    formatValue(rhsStr, sizeof(rhsStr), data->rhsType, rhs);
    
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: shift out of bounds ***\n    shift amount %s is too large for %s-bit type %s (value: %s)\n    at %s:%u:%u\n",
        rhsStr, 
        data->lhsType ? (data->lhsType->isInteger() ? (char[16]){0} : "?") : "?",
        data->lhsType ? data->lhsType->typeName : "<unknown>",
        lhsStr,
        data->loc.filename, data->loc.line, data->loc.column);
    
    // Fix: fill in bit width
    if (data->lhsType && data->lhsType->isInteger()) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: shift out of bounds ***\n    shift amount %s is invalid for %u-bit type %s (value: %s)\n    at %s:%u:%u\n",
            rhsStr, data->lhsType->getIntBitWidth(),
            data->lhsType->typeName, lhsStr,
            data->loc.filename, data->loc.line, data->loc.column);
    }
    
    ubsan_output(buffer);
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_shift_out_of_bounds_abort(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    __ubsan_handle_shift_out_of_bounds(data, lhs, rhs);
}

// Array bounds
__declspec(dllexport) void __ubsan_handle_out_of_bounds(OutOfBoundsData* data, ValueHandle index)
{
    ubsan_report_with_value("array index out of bounds", data->loc, "index", data->indexType, index);
}

__declspec(dllexport) void __ubsan_handle_out_of_bounds_abort(OutOfBoundsData* data, ValueHandle index)
{
    __ubsan_handle_out_of_bounds(data, index);
}

// Unreachable code
__declspec(dllexport) void __ubsan_handle_builtin_unreachable(UnreachableData* data)
{
    ubsan_report("execution reached __builtin_unreachable()", data->loc);
}

__declspec(dllexport) void __ubsan_handle_builtin_unreachable_abort(UnreachableData* data)
{
    __ubsan_handle_builtin_unreachable(data);
}

// Missing return
__declspec(dllexport) void __ubsan_handle_missing_return(UnreachableData* data)
{
    ubsan_report("execution reached end of non-void function without returning a value", data->loc);
}

__declspec(dllexport) void __ubsan_handle_missing_return_abort(UnreachableData* data)
{
    __ubsan_handle_missing_return(data);
}

// VLA bound
__declspec(dllexport) void __ubsan_handle_vla_bound_not_positive(VLABoundData* data, ValueHandle bound)
{
    ubsan_report_with_value("variable length array bound is not positive", data->loc, "bound", data->type, bound);
}

__declspec(dllexport) void __ubsan_handle_vla_bound_not_positive_abort(VLABoundData* data, ValueHandle bound)
{
    __ubsan_handle_vla_bound_not_positive(data, bound);
}

// Float cast overflow
__declspec(dllexport) void __ubsan_handle_float_cast_overflow(FloatCastOverflowData* data, ValueHandle val)
{
    if (isDuplicate(data->loc)) return;
    
    char valStr[64];
    formatValue(valStr, sizeof(valStr), data->fromType, val);
    
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: float cast overflow ***\n    %s (type %s) is outside the range of representable values of type %s\n    at %s:%u:%u\n",
        valStr,
        data->fromType ? data->fromType->typeName : "<unknown>",
        data->toType ? data->toType->typeName : "<unknown>",
        data->loc.filename, data->loc.line, data->loc.column);
    
    ubsan_output(buffer);
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_float_cast_overflow_abort(FloatCastOverflowData* data, ValueHandle val)
{
    __ubsan_handle_float_cast_overflow(data, val);
}

// Load invalid value (bad bool/enum)
__declspec(dllexport) void __ubsan_handle_load_invalid_value(InvalidValueData* data, ValueHandle val)
{
    ubsan_report_with_value("load of value outside valid range for type", data->loc, "value", data->type, val);
}

__declspec(dllexport) void __ubsan_handle_load_invalid_value_abort(InvalidValueData* data, ValueHandle val)
{
    __ubsan_handle_load_invalid_value(data, val);
}

// Invalid builtin (e.g., __builtin_clz(0))
__declspec(dllexport) void __ubsan_handle_invalid_builtin(InvalidBuiltinData* data)
{
    const char* builtinName = data->kind == 0 ? "__builtin_ctz" : "__builtin_clz";
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "passing zero to %s, which is undefined", builtinName);
    ubsan_report(buffer, data->loc);
}

__declspec(dllexport) void __ubsan_handle_invalid_builtin_abort(InvalidBuiltinData* data)
{
    __ubsan_handle_invalid_builtin(data);
}

// Nonnull argument
__declspec(dllexport) void __ubsan_handle_nonnull_arg(NonNullArgData* data)
{
    if (isDuplicate(data->loc)) return;
    
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: null pointer passed to nonnull argument ***\n    argument index: %d\n    at %s:%u:%u\n",
        data->argIndex,
        data->loc.filename, data->loc.line, data->loc.column);
    
    ubsan_output(buffer);
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_nonnull_arg_abort(NonNullArgData* data)
{
    __ubsan_handle_nonnull_arg(data);
}

// Nonnull return
__declspec(dllexport) void __ubsan_handle_nonnull_return_v1(NonNullReturnData* data, SourceLocation* loc)
{
    ubsan_report("null returned from function declared to never return null", *loc);
}

__declspec(dllexport) void __ubsan_handle_nonnull_return_v1_abort(NonNullReturnData* data, SourceLocation* loc)
{
    __ubsan_handle_nonnull_return_v1(data, loc);
}

// Pointer overflow
__declspec(dllexport) void __ubsan_handle_pointer_overflow(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    if (isDuplicate(data->loc)) return;
    
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
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_pointer_overflow_abort(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    __ubsan_handle_pointer_overflow(data, base, result);
}

// Function type mismatch (indirect call)
__declspec(dllexport) void __ubsan_handle_function_type_mismatch(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    if (isDuplicate(data->loc)) return;
    
    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: indirect function call type mismatch ***\n    call through pointer 0x%p to function of wrong type %s\n    at %s:%u:%u\n",
        (void*)ptr,
        data->type ? data->type->typeName : "<unknown>",
        data->loc.filename, data->loc.line, data->loc.column);
    
    ubsan_output(buffer);
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_function_type_mismatch_abort(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    __ubsan_handle_function_type_mismatch(data, ptr);
}

// Alignment assumption
__declspec(dllexport) void __ubsan_handle_alignment_assumption(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    if (isDuplicate(data->loc)) return;
    
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
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_alignment_assumption_abort(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    __ubsan_handle_alignment_assumption(data, ptr, align, offset);
}

// Implicit conversion (integer truncation, sign change)
static const char* getImplicitConversionKindName(unsigned char kind)
{
    static const char* names[] = {
        "integer truncation",
        "unsigned integer truncation",
        "sign change",
        "signed truncation or sign change"
    };
    return kind < sizeof(names)/sizeof(names[0]) ? names[kind] : "implicit conversion";
}

__declspec(dllexport) void __ubsan_handle_implicit_conversion(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    if (isDuplicate(data->loc)) return;

    char srcStr[64], dstStr[64];
    formatValue(srcStr, sizeof(srcStr), data->fromType, src);
    formatValue(dstStr, sizeof(dstStr), data->toType, dst);

    char buffer[512];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: implicit conversion (%s) ***\n    value %s (type %s) changed to %s (type %s)\n    at %s:%u:%u\n",
        getImplicitConversionKindName(data->kind),
        srcStr,
        data->fromType ? data->fromType->typeName : "<unknown>",
        dstStr,
        data->toType ? data->toType->typeName : "<unknown>",
        data->loc.filename ? data->loc.filename : "<unknown>",
        data->loc.line,
        data->loc.column);

    ubsan_output(buffer);
    __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_implicit_conversion_abort(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    __ubsan_handle_implicit_conversion(data, src, dst);
}

} // extern "C"

#pragma clang attribute pop

#endif // VPDEBUG
