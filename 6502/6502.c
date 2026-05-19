#include "6502.h"

#include "log.c"

typedef struct Instruction Instruction;
struct Instruction {
    char name[4];
    u8 cycles;
    u8 (*op)(Cpu *cpu);
    u8 (*addr_mode)(Cpu *cpu);
};


static Instruction
dispatch(u8 opcode);

extern void
bus_init(Bus *bus)
{
    cpu_connect_bus(&bus->cpu, bus);

    // Clear Bus contents just in case.
    for (size_t i = 0; i < BUS_RAM_SIZE; i += 1) {
        bus->ram[i] = 0;
    }
}

extern u8
bus_read_mode(Bus *bus, u16 addr, bool is_readonly)
{
    unused(is_readonly);
    log_assertf(0x0000 <= addr && addr <= 0xFFFF, "index %u is out of bounds", addr);
    return bus->ram[addr];
}

extern void
bus_write(Bus *bus, u16 addr, u8 data)
{
    // Useless, but just so we can begin getting used to it.
    log_assertf(0x0000 <= addr && addr <= 0xFFFF, "index %u is out of bounds", addr);
    bus->ram[addr] = data;
}

extern void
cpu_connect_bus(Cpu *cpu, Bus *bus)
{
    cpu->bus = bus;
}

extern u8
cpu_read(Cpu *cpu, u16 addr)
{
    return bus_read(cpu->bus, addr);
}

extern void
cpu_write(Cpu *cpu, u16 addr, u8 data)
{
    return bus_write(cpu->bus, addr, data);
}

extern u8
get_flag(Cpu *cpu, Flag flag)
{
    return cast(u8)((cpu->status & cast(u8)flag) != 0);
}

extern void
set_flag(Cpu *cpu, Flag flag, bool toggle)
{
    if (toggle) {
        cpu->status |= cast(u8)flag;
    } else {
        cpu->status &= ~cast(u8)flag;
    }
}

extern void
cpu_clock(Cpu *cpu)
{
    if (cpu->cycles == 0) {
        cpu->opcode = cpu_read(cpu, cpu->pc++);
        Instruction i = dispatch(cpu->opcode);

        // Get starting number of cycles.
        cpu->cycles = i.cycles;

        u8 extra_cycle1 = i.addr_mode(cpu);
        u8 extra_cycle2 = i.op(cpu);

        // Both calls returned 1, meaning we need an extra cycle?
        cpu->cycles += (extra_cycle1 & extra_cycle2);
    }
    cpu->cycles -= 1;
}

// ADDRESSING MODES {{{

// Implied addressing mode. The address containing the operand is implictly
// stated in the operation code of the instruction.
static u8
IMP(Cpu *cpu)
{
    cpu->fetched = cpu->a;
    return 0;
}

// Immediate addressing mode. The operand is contained in the second (2nd)
// byte of the instructino. No further memory addressing is required.
static u8
IMM(Cpu *cpu)
{
    cpu->addr_abs = cpu->pc++;
    return 0;
}

// Zero page addressing. Working memory tends to be located here as it
// requires less bytes to encode and less cycles to run.
static u8
ZP0(Cpu *cpu)
{
    // Get an absolute address in the range [0x0000, 0x00FF].
    cpu->addr_abs  = cpu_read(cpu, cpu->pc++);
    cpu->addr_abs &= 0x00FF;
    return 0;
}

static u8
ZPX(Cpu *cpu)
{
    cpu->addr_abs = cpu_read(cpu, cpu->pc++) + cpu->x;
    cpu->addr_abs &= 0x00FF;
    return 0;
}

static u8
ZPY(Cpu *cpu)
{
    cpu->addr_abs = cpu_read(cpu, cpu->pc++) + cpu->y;
    cpu->addr_abs &= 0x00FF;
    return 0;
}

// Absolute addressing mode. The argument is a 16-bit absolute effective
// address constructed from two (2) 8-bit values in a little-endian fashion.
static u8
ABS(Cpu *cpu)
{
    u16 lo = cast(u16)cpu_read(cpu, cpu->pc++);
    u16 hi = cast(u16)cpu_read(cpu, cpu->pc++);

    cpu->addr_abs = (hi << 8) | lo;
    return 0;
}

static u8
ABX(Cpu *cpu)
{
    u16 lo = cast(u16)cpu_read(cpu, cpu->pc++);
    u16 hi = cast(u16)cpu_read(cpu, cpu->pc++);

    cpu->addr_abs  = (hi << 8) | lo;
    cpu->addr_abs += cpu->x;

    // Check if we cross a page boundary.
    // Return 1 f we need an extra cycle to read a new page, else 0.
    return cast(u8)((cpu->addr_abs & 0xFF00) != (hi << 8));
}

static u8
ABY(Cpu *cpu)
{
    u16 lo = cast(u16)cpu_read(cpu, cpu->pc++);
    u16 hi = cast(u16)cpu_read(cpu, cpu->pc++);

    cpu->addr_abs  = (hi << 8) | lo;
    cpu->addr_abs += cpu->y;

    // Page boundary crossing check.
    return cast(u8)((cpu->addr_abs & 0xFF00) != (hi << 8));
}


// Addresing mode: indirect.
static u8
IND(Cpu *cpu)
{
    // Assemble a 16-bit address, which points to another 16-bit address.
    u16 paddr_lo  = cast(u16)cpu_read(cpu, cpu->pc++);
    u16 paddr_hi  = cast(u16)cpu_read(cpu, cpu->pc++);
    u16 paddr     = (paddr_hi << 8) | paddr_lo;

    u16 lo, hi;
    // Need to simulate the page boundary hardware bug?
    if (paddr_lo == 0x00FF) {
        lo = cast(u16)cpu_read(cpu, paddr + 0);
        hi = cast(u16)cpu_read(cpu, paddr & 0xFF00);
    } else {
        lo = cast(u16)cpu_read(cpu, paddr + 0);
        hi = cast(u16)cpu_read(cpu, paddr + 1);
    }
    cpu->addr_abs = (hi << 8) | lo;
    return 0;
}

static u8
IZX(Cpu *cpu)
{
    u16 zp_addr = cpu_read(cpu, cpu->pc++);

    // Offset the zero (0) page address with the X register.
    zp_addr += cast(u16)cpu->x;

    // Read the pointer we need.
    u16 lo = cpu_read(cpu, zp_addr & 0x00FF);
    u16 hi = cpu_read(cpu, (zp_addr + 1) & 0x00FF);
    cpu->addr_abs = (hi << 8) | lo;
    return 0;
}

static u8
IZY(Cpu *cpu)
{
    u16 zp_addr = cpu_read(cpu, cpu->pc++);

    // Read the 16-bit actual address from the operand.
    u16 lo = cpu_read(cpu, zp_addr & 0x00FF);
    u16 hi = cpu_read(cpu, (zp_addr + 1) & 0x00FF);

    // Finally use the Y offset to get the actual address.
    cpu->addr_abs  = (hi << 8) | lo;
    cpu->addr_abs += cpu->y;

    // If we cross a page boundary, return 1 to indicate another cycle
    // is needed.
    return cast(u8)((cpu->addr_abs & 0xFF00) != (hi << 8));
}

// Addressing mode: relative.
static u8
REL(Cpu *cpu)
{
    cpu->addr_rel = cpu_read(cpu, cpu->pc++);
    // Sign bit is toggled for the 8-bit signed representation?
    if (cpu->addr_rel & 0x80) {
        // Sign extend for 16-bits.
        cpu->addr_rel |= 0xFF00;
    }
    return 0;
}


// }}}

// Fetch data for all instructions sans instructions with implied arguments.
static u8
fetch(Cpu *cpu)
{
    if (dispatch(cpu->opcode).addr_mode != &IMP) {
        cpu->fetched = cpu_read(cpu, cpu->addr_abs);
    }
    return cpu->fetched;
}

// OPCODES {{{

// Add memory to accumulator (A) with carry.
static u8
ADC(Cpu *cpu)
{
    unused(cpu);
    return 0;
}

// "AND" memory with accumulator (A).
static u8
AND(Cpu *cpu)
{
    fetch(cpu);
    cpu->a &= cpu->fetched;
    set_flag(cpu, FLAG_Z, cpu->a == 0x00);
    set_flag(cpu, FLAG_N, cpu->a & 0x00);

    // Could potentially require an extra cycle.
    return 1;
}

// Shift left one (1) bit for memory or accumulator (A).
static u8
ASL(Cpu *cpu)
{
    unused(cpu);
    return 0;
}

// Branch on Carry Clear.
static u8
BCC(Cpu *cpu)
{
    unused(cpu);
    return 0;
}

// Branch on Carry Set.
static u8
BCS(Cpu *cpu)
{
    if (get_flag(cpu, FLAG_C) == 1) {
        // Add another cycle because a branch was taken.
        cpu->cycles += 1;
        cpu->addr_abs = cpu->pc + cpu->addr_rel;

        // The branch needs to cross a page boundary?
        if ((cpu->addr_abs & 0xFF00) != (cpu->pc & 0xFF00)){
            cpu->cycles += 1;
        }
        cpu->pc = cpu->addr_abs;
    }
    unused(cpu);
    return 0;
}

// Branch on Result Zero.
static u8
BEQ(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
BIT(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
BMI(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
BNE(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
BPL(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
BRK(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
BVC(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
BVS(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
CLC(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
CLD(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
CLI(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
CLV(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
CMP(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
CPX(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
CPY(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
DEC(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
DEX(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
DEY(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
EOR(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
INC(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
INX(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
INY(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
JMP(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
JSR(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
LDA(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
LDX(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
LDY(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
LSR(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
NOP(Cpu *cpu)
{
    unused(cpu);
    return 0;
}

static u8
ORA(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
PHA(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
PHP(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
PLA(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
PLP(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
ROL(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
ROR(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
RTI(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
RTS(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
SBC(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
SEC(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
SED(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
SEI(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
STA(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
STX(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
STY(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
TAX(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
TAY(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
TSX(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
TXA(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
TXS(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

static u8
TYA(Cpu *cpu)
{
    unused(cpu);
	return 0;
}

// Invalid opcode!
static u8
XXX(Cpu *cpu)
{
    unused(cpu);
    return 0;
}

// }}}
// LOOKUP TABLE {{{
#define X(OP, ADDR_MODE, cycles) {#OP, cycles, &OP, &ADDR_MODE}
#define E(OP, cycles)            {"???", cycles, &OP, &IMP}


/** @link https://www.princeton.edu/~mae412/HANDOUTS/Datasheets/6502.pdf
 *  @link https://datasheets.chipdb.org/Rockwell/6502.pdf */
static const Instruction
DISPATCH[] = {
X(BRK, IMM, 7), X(ORA, IZX, 6), E(XXX, 2)     , E(XXX, 8)     , E(NOP, 3)     , X(ORA, ZP0, 3), X(ASL, ZP0, 5), E(XXX, 5)     , X(PHP, IMP, 3), X(ORA, IMM, 2), X(ASL, IMP, 2), E(XXX, 2)     , E(NOP, 4)     , X(ORA, ABS, 4), X(ASL, ABS, 6), E(XXX, 6),
X(BPL, REL, 2), X(ORA, IZY, 5), E(XXX, 2)     , E(XXX, 8)     , E(NOP, 4)     , X(ORA, ZPX, 4), X(ASL, ZPX, 6), E(XXX, 5)     , X(CLC, IMP, 2), X(ORA, ABY, 4), X(INC, IMP, 2), E(XXX, 7)     , E(NOP, 4)     , X(ORA, ABX, 4), X(ASL, ABX, 6), E(XXX, 7),
X(JSR, ABS, 6), X(AND, IZX, 6), E(XXX, 2)     , E(XXX, 8)     , X(BIT, ZP0, 3), X(AND, ZP0, 3), X(ROL, ZP0, 5), E(XXX, 5)     , X(PLP, IMP, 4), X(AND, IMM, 2), X(ROL, IMP, 2), E(XXX, 2)     , X(BIT, ABS, 4), X(AND, ABS, 4), X(ROL, ABS, 6), E(XXX, 5),
X(BMI, REL, 2), X(AND, IZY, 5), E(XXX, 2)     , E(XXX, 8)     , X(BIT, ZPX, 4), X(AND, ZPX, 4), X(ROL, ZPX, 6), E(XXX, 5)     , X(SEC, IMP, 2), X(AND, ABY, 4), X(DEC, IMP, 2), E(XXX, 2)     , X(BIT, ABX, 4), X(AND, ABX, 4), X(ROL, ABX, 7), E(XXX, 5),
X(RTI, IMP, 6), X(EOR, IZX, 6), E(XXX, 2)     , E(XXX, 8)     , E(XXX, 2)     , X(EOR, ZP0, 3), X(LSR, ZP0, 5), E(XXX, 5)     , X(PHA, IMP, 3), X(EOR, IMM, 2), X(LSR, IMP, 2), E(XXX, 2)     , X(JMP, ABS, 3), X(EOR, ABS, 4), X(LSR, ABS, 6), E(XXX, 5),
X(BVC, REL, 2), X(EOR, IZY, 5), E(XXX, 2)     , E(XXX, 8)     , E(XXX, 2)     , X(EOR, ZPX, 4), X(LSR, ZPX, 6), E(XXX, 5)     , X(CLI, IMP, 2), X(EOR, ABY, 4), E(XXX, 3)     , E(XXX, 2)     , E(XXX, 2)     , X(EOR, ABX, 4), X(LSR, ABX, 7), E(XXX, 5),
X(RTS, IMP, 6), X(ADC, IZX, 6), E(XXX, 2)     , E(XXX, 8)     , E(XXX, 2)     , X(ADC, ZP0, 3), X(ROR, ZP0, 5), E(XXX, 5)     , X(PLA, IMP, 4), X(ADC, IMM, 2), X(ROR, IMP, 2), E(XXX, 2)     , X(JMP, IND, 6), X(ADC, ABS, 4), X(ROR, ABS, 6), E(XXX, 5),
X(BVS, REL, 2), X(ADC, IZY, 5), E(XXX, 2)     , E(XXX, 8)     , E(XXX, 2)     , X(ADC, ZPX, 4), X(ROR, ZPX, 6), E(XXX, 5)     , X(SEI, IMP, 2), X(ADC, ABY, 4), E(XXX, 2)     , E(XXX, 2)     , E(XXX, 2)     , X(ADC, ABX, 4), X(ROR, ABX, 7), E(XXX, 5),
E(NOP, 6)     , X(STA, IZX, 6), E(NOP, 2)     , E(XXX, 6)     , X(STY, ZP0, 3), X(STA, ZP0, 3), X(STX, ZP0, 3), E(XXX, 5)     , X(DEY, IMP, 2), X(BIT, IMM, 2), X(TXA, IMP, 2), E(XXX, 2)     , X(STY, ABS, 4), X(STA, ABS, 4), X(STX, ABS, 4), E(XXX, 5),
X(BCC, REL, 2), X(STA, IZY, 6), E(XXX, 2)     , E(XXX, 6)     , X(STY, ZPX, 4), X(STA, ZPX, 4), X(STX, ZPY, 4), E(XXX, 5)     , X(TYA, IMP, 2), X(STA, ABY, 5), X(TXS, IMP, 2), E(XXX, 2)     , E(XXX, 2)     , X(STA, ABX, 5), E(XXX, 2)     , E(XXX, 5),
X(LDY, IMM, 2), X(LDA, IZX, 6), X(LDX, IMM, 2), E(XXX, 6)     , X(LDY, ZP0, 3), X(LDA, ZP0, 3), X(LDX, ZP0, 3), E(XXX, 5)     , X(TAY, IMP, 2), X(LDA, IMM, 2), X(TAX, IMP, 2), E(XXX, 2)     , X(LDY, ABS, 4), X(LDA, ABS, 4), X(LDX, ABS, 4), E(XXX, 5),
X(BCS, REL, 2), X(LDA, IZY, 5), E(XXX, 2)     , E(XXX, 5)     , X(LDY, ZPX, 4), X(LDA, ZPX, 4), X(LDX, ZPY, 4), E(XXX, 5)     , X(CLV, IMP, 2), X(LDA, ABY, 4), X(TSX, IMP, 2), E(XXX, 2)     , X(LDY, ABX, 4), X(LDA, ABX, 4), X(LDX, ABY, 4), E(XXX, 5),
X(CPY, IMM, 2), X(CMP, IZX, 6), E(NOP, 2)     , E(XXX, 8)     , X(CPY, ZP0, 3), X(CMP, ZP0, 3), X(DEC, ZP0, 5), E(XXX, 5)     , X(INY, IMP, 2), X(CMP, IMM, 2), X(DEX, IMP, 2), E(XXX, 2)     , X(CPY, ABS, 4), X(CMP, ABS, 4), X(DEC, ABS, 6), E(XXX, 5),
X(BNE, REL, 2), X(CMP, IZY, 5), E(XXX, 2)     , E(XXX, 8)     , E(NOP, 4)     , X(CMP, ZPX, 4), X(DEC, ZPX, 6), E(XXX, 5)     , X(CLD, IMP, 2), X(CMP, ABY, 4), E(XXX, 3)     , E(XXX, 2)     , E(XXX, 2)     , X(CMP, ABX, 4), X(DEC, ABX, 7), E(XXX, 5),
X(CPX, IMM, 2), X(SBC, IZX, 6), E(NOP, 2)     , E(XXX, 8)     , X(CPX, ZP0, 3), X(SBC, ZP0, 3), X(INC, ZP0, 5), E(XXX, 5)     , X(INX, IMP, 2), X(SBC, IMM, 2), X(NOP, IMP, 2), E(XXX, 2)     , X(CPX, ABS, 4), X(SBC, ABS, 4), X(INC, ABS, 6), E(XXX, 7),
X(BEQ, REL, 2), X(SBC, IZY, 5), E(XXX, 2)     , E(XXX, 8)     , E(NOP, 4)     , X(SBC, ZPX, 4), X(INC, ZPX, 6), E(XXX, 6)     , X(SED, IMP, 2), X(SBC, ABY, 4), X(NOP, IMP, 2), E(XXX, 7)     , E(NOP, 4)     , X(SBC, ABX, 4), X(INC, ABX, 7), E(XXX, 7),
};

#undef E
#undef X

// }}}


static Instruction
dispatch(u8 opcode)
{
    return DISPATCH[opcode];
}
