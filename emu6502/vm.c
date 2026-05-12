#include <stdio.h>  // printf
#include <string.h> // memset

#include "cpu.h"
#include "log.c"

#define printfln(fmt, ...)  printf(fmt "\n", __VA_ARGS__)
#define println(msg)        printfln("%s", msg)

#if EMU6502_DEBUG
#define assertfln(cond, fmt, ...)                                              \
do {                                                                           \
    if (!(cond)) {                                                             \
        log_fatalf("Assertion '" #cond "' failed! " fmt, __VA_ARGS__);         \
    }                                                                          \
} while (0)

#define panicfln(fmt, ...) log_fatalf("Runtime panic! " fmt, __VA_ARGS__)

#else

#define assertfln(...)
#define panicfln(...)

#endif // !NDEBUG

internal void
mem_reset(Mem *mem)
{
    memset(mem->data, 0, MEM_SIZE);
}

// Index-checked low level byte reader.
internal u8
mem_read_u8(const Mem *mem, usize index)
{
    assertfln(index < MEM_SIZE, "Index %zu / %zu is out of bounds", index, MEM_SIZE);
    // log_infof("mem[0x%04zx] -> %u", index, mem->data[index]);
    return mem->data[index];
}

// Index-checked low level word reader. Reads 2 bytes and combines them
// into a word in a little endian fashion.
internal u16
mem_read_u16(const Mem *mem, usize index)
{
    u16 lo = cast(u16)mem_read_u8(mem, index);
    u16 hi = cast(u16)mem_read_u8(mem, index + 1);
    return (hi << 8) | lo;
}

// Index-checked low level byte writer.
internal void
mem_write_u8(Mem *mem, usize index, u8 value)
{
    assertfln(index < MEM_SIZE, "Index %zu / %zu is out of bounds", index, MEM_SIZE);
    // log_infof("mem[0x%04zx] <- %u", index, value);
    mem->data[index] = value;
}

// Index-checked low level word writer. Splits the word into 2 bytes,
// storing them in a little endian fashion.
internal void
mem_write_u16(Mem *mem, usize index, u16 value)
{
    mem_write_u8(mem, index,     cast(u8)(value & 0xff));
    mem_write_u8(mem, index + 1, cast(u8)(value >> 8));
}

internal void
cpu_reset(Cpu *cpu)
{
    cpu->pc    = RESET_VECTOR;
    cpu->sp    = 0x0100;
    cpu->a     = 0;
    cpu->x     = 0;
    cpu->y     = 0;
    cpu->flags = 0;
    // log_infof("cpu->pc: 0x%04x", cpu->pc);
}

internal bool
cpu_has_flag(const Cpu *cpu, Flag flag)
{
    return (cpu->flags & cast(u8)flag) != 0;
}

internal void
cpu_set_flag(Cpu *cpu, Flag flag)
{
    cpu->flags |= cast(u8)flag;
}

internal void
cpu_set_flag_if(Cpu *cpu, Flag flag, bool cond)
{
    if (cond) {
        cpu_set_flag(cpu, flag);
    }
}

internal u8
cpu_read_u8(Cpu *cpu, const Mem *mem, u16 addr, usize *cycles)
{
    *cycles += 1;
    return mem_read_u8(mem, cast(usize)addr);
}

internal u16
cpu_read_u16(Cpu *cpu, const Mem *mem, u16 addr, usize *cycles)
{
    *cycles += 2;
    u16 lo = cast(u16)mem_read_u8(mem, cast(usize)addr);
    u16 hi = cast(u16)mem_read_u8(mem, cast(usize)addr + 1);
    return (hi << 8) | lo;
}


internal void
cpu_push_u8(Cpu *cpu, Mem *mem, u8 value, usize *cycles)
{
    *cycles += 1;
    mem_write_u8(mem, cpu->sp++, value);
}

internal void
cpu_push_u16(Cpu *cpu, Mem *mem, u16 value, usize *cycles)
{
    cpu->sp += 2;
    *cycles += 2;
    mem_write_u16(mem, cpu->sp, value);
}

// Read the current byte and advance the program counter.
internal u8
cpu_fetch_u8(Cpu *cpu, const Mem *mem, usize *cycles)
{
    // log_infof("cpu->pc: 0x%04x -> 0x%04x", cpu->pc, cpu->pc + 1);
    return cpu_read_u8(cpu, mem, cpu->pc++, cycles);
}

internal u16
cpu_fetch_u16(Cpu *cpu, const Mem *mem, usize *cycles)
{
    cpu->pc += 2;
    return cpu_read_u16(cpu, mem, cpu->pc - 2, cycles);
}

internal void
cpu_check_lda(Cpu *cpu)
{
    u8 a = cpu->a;
    cpu_set_flag_if(cpu, FLAG_ZERO, a == 0);
    cpu_set_flag_if(cpu, FLAG_NEGATIVE, (a & (1 << 7)) != 0);
}

internal usize
cpu_dispatch(Cpu *cpu, Mem *mem)
{
    usize cycles = 0;

    // Fetch the next instruction in memory.
    Opcode op = cast(Opcode)cpu_fetch_u8(cpu, mem, &cycles);
    switch (op) {
    case OP_LDA_IMM:
    {
        cpu->a = cpu_fetch_u8(cpu, mem, &cycles);
        printfln("A := %u", cpu->a);
        cpu_check_lda(cpu);
        break;
    }

    case OP_LDA_ZP:
    {
        // Zero page address. Must be in the inclusive range [0x0000,0x00FF].
        u8 addr = cpu_fetch_u8(cpu, mem, &cycles);
        cpu->a  = cpu_read_u8(cpu, mem, addr, &cycles);
        printfln("A := [0x%04x] ; %u", addr, cpu->a);
        cpu_check_lda(cpu);
        break;

    }

    case OP_LDA_ZPX:
    {
        // Zero page address. Must be in the inclusive range [0x0000,0x00FF].
        u8 addr = cpu_fetch_u8(cpu, mem, &cycles);
        cpu->a  = cpu_read_u8(cpu, mem, addr + cpu->x, &cycles);
        printfln("A := [0x%04x + %#x] ; %u", addr, cpu->x, cpu->a);
        cpu_check_lda(cpu);
        break;
    }

    case OP_JSR:
    {
        // Address we wish to jump to.
        u16 addr = cpu_fetch_u16(cpu, mem, &cycles);

        // Push the return address before actually performing the jump.
        // Remember that since the pc always points to the *next* instruction,
        // the current one is at the address minus one (1).
        cpu_push_u16(cpu, mem, cpu->pc - 1, &cycles);
        printfln("push 0x%04x, jmp 0x%04x", cpu->pc - 1, addr);

        // Performing the jump takes yet another cycle.
        cpu->pc = addr;
        cycles += 1;
        break;
    }

    default:
        panicfln("Unhandled case: Opcode(%i)", op);
        break;
    }
    return cycles;
}

internal void
cpu_execute(Cpu *cpu, Mem *mem, const usize cycles)
{
    usize total = 0;
    while (total < cycles) {
        total += cpu_dispatch(cpu, mem);
    }

    if (total != cycles) {
        log_errorf("Expected %zu cycles but got %zu", cycles, total);
    }
}

int
main(void)
{
    Cpu cpu;
    Mem mem;
    cpu_reset(&cpu);
    mem_reset(&mem);

    static const u8 program[] = {
        OP_LDA_IMM, 42,             // 2 cycles
        OP_LDA_ZP,  0x0042,         // 3 cycles
        OP_LDA_ZPX, 0x0042,         // 3 cycles
        OP_JSR,     0x0042, 0x0042, // 6 cycles
    };

    cpu.pc = 0x4000;
    for (u16 i = 0; i < cast(u16)sizeof(program); i += 1) {
        mem_write_u8(&mem, cpu.pc + i, program[i]);
    }
    mem_write_u8(&mem, 0x0042, 42);
    mem_write_u8(&mem, 0x4242, OP_LDA_IMM);
    mem_write_u8(&mem, 0x4243, 25);
    cpu_execute(&cpu, &mem, /*cycles=*/16);
    return 0;
}
