#ifndef EMU6502_CPU_H
#define EMU6502_CPU_H

#include <stdbool.h>    // bool, true, false
#include <stdint.h>     // u?int[\d]+_t

#if defined(_DEBUG) || !defined(NDEBUG)

#ifndef EMU6502_DEBUG
#define EMU6502_DEBUG 1
#endif // !EMU6502_DEBUG

#else // ^^^_DEBUG || !NDEBUG, vvv !_DEBUG || NDEBUG

#ifndef EMU6502_DEBUG
#define EMU6502_DEBUG 0
#endif // !EMU6502_DEBUG

#endif // _DEBUG || !NDEBUG

// Because `static` is way too overloaded.
#define internal        static
#define local_persist   static
#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)

// CPU data size.
typedef uint8_t u8;

// CPU word size.
typedef uint16_t u16;

// Emulator word size.
typedef size_t usize;

enum Flag {
    // This flag bit is set is the last operation caused either:
    //
    //      1.) (Unsigned) Overflow from the 7th bit of the result.
    //      2.) (Signed) Underflow from bit 0.
    //
    FLAG_CARRY = 1 << 0,
    
    // This flag bit is set if the last operation resulted in a zero (0).
    //
    FLAG_ZERO = 1 << 1,

    // This flag bit is set if the program has executed a 'Set Interrupt
    // Disable' instruction, SEI.
    //
    FLAG_INTERRUPT = 1 << 2,

    // This flag bit is set explicitly using the 'Set Decimal Flag'
    // instruction, SED. When set the processor will obey the rules of
    // Binary Coded Decimal (BCD) arithmetic in addition and subtraction.
    //
    // It can be cleared with the 'Clear Decimal Flag' instruction, CLD.
    //
    FLAG_DECIMAL_MODE = 1 << 3,

    // This flag bit is set when a BRK instruction has been executed and an
    // interrupt has been generated to process it.
    //
    FLAG_BREAK = 1 << 4,

    // This flag bit is set when, during arithmetic operations, the result
    // yielded an invalid 2's complement result. It is determined by looking
    // at the carry between the following:
    //
    //      1.) Bit 6 and bit 7.
    //      2.) Bit 7 and the carry flag.
    //
    // It can be set by the following example:
    //
    //      1.) Adding to positive numbers but ending up with a negative
    //          result, as in `64 + 64 = -128`.
    //
    FLAG_OVERFLOW = 1 << 5,

    // This flag bit is set if the result of the last operation had its 7th
    // bit set to a one (1).
    //
    FLAG_NEGATIVE = 1 << 6,
};

// All opcodes must fit within a single byte.
enum Opcode {
    // | Byte Arguments   |  1  |  2  |  3  |  4  |*| Interpretation
    // |------------------|-----|-----|-----|-----|*|
    OP_LDA_IMM = 0xA9, // |  o  |  i  |-----|-----|*| r(a) = i
    OP_LDA_ZP  = 0xA5, // |  o  |  i  |-----|-----|*| r(a) = *[i]
    OP_LDA_ZPX = 0xB5, // |  o  |  i  |-----|-----|*| r(a) = *[i + r(x)]
    OP_JSR     = 0x20, // |  o  |  lo |  hi |-----|*| jmp u16(lo, hi)
};

typedef enum Opcode Opcode;
typedef enum Flag Flag;

typedef struct Cpu Cpu;
struct Cpu {
    // Program counter. Holds the address of the *next* instruction to be executes.
    // The address itself is merely a 16-bit index into the main memory.
    //
    u16 pc;

    // Stack pointer. Holds the address of the *first* freely available stack slot.
    // The address itself is merely a 16-bit index into the main memory.
    //
    u16 sp;

    // 8-bit processor registers.
    //  `a` is often used as the 'accumulator' register.
    //  `x` and `y` are often used as 'index' or 'counter' registers.
    //
    u8 a, x, y;

    // Status flag bit set. Use the `FLAG_*` constants.
    u8 flags;
};

// 64 kilobytes.
#define MEM_SIZE cast(usize)(64UL * 1024UL)

// Address of the reset vector, which is a u16 containing the address of
// the first instruction to be executed.
#define RESET_VECTOR 0xFFFC

typedef struct Mem Mem;
struct Mem {
    u8 data[MEM_SIZE];
};

#endif // !EMU6502_CPU_H
