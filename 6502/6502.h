#ifndef E6502_H
#define E6502_H

#include <stdbool.h>
#include <stdint.h>

#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)

typedef uint8_t  u8;
typedef uint16_t u16;

typedef struct Bus Bus;
typedef struct Cpu Cpu;
struct Cpu {
    Bus *bus;

    // Program counter. Always points to the *next* instruction (located in
    // memory) to be executed.
    u16 pc;
    u16 sp;     // Stack pointer. Points to a location on the bus.
    u8 a;       // Accumulator register.
    u8 x;       // X register. Often used as a counter or an index.
    u8 y;       // Y register. Often used as a counter or an index.
    u8 status;  // Status register.

    // Execution state.
    u8 opcode;
    u8 fetched;
    u16 addr_abs, addr_rel;
    u8 cycles;
};

// Status register bit flags.
enum Flag {
    FLAG_C = (1 << 0), // Carry bit.
    FLAG_Z = (1 << 1), // Zero. Toggled when an operation resulted in zero.
    FLAG_I = (1 << 2), // Disable interrupts.
    FLAG_D = (1 << 3), // Decimal mode. Unused in this implementation.
    FLAG_B = (1 << 4), // Break operation has been called.
    FLAG_U = (1 << 5), // Unused.
    FLAG_V = (1 << 6), // Overflow, Unsigned.
    FLAG_N = (1 << 7), // Negative. Signed.
};

typedef enum Flag Flag;

#define BUS_RAM_SIZE    (64 * 1024)

// Written to and read from, usually by the Cpu.
struct Bus {
    // Devices on the bus.
    Cpu cpu;

    // Fake RAM for now. 64 kilobytes.
    u8 ram[BUS_RAM_SIZE];
};

extern void
bus_init(Bus *bus);

extern u8
bus_read_mode(Bus *bus, u16 addr, bool is_readonly);

#define bus_read(bus, addr)   bus_read_mode(bus, addr, /*is_readonly=*/false)

extern void
bus_write(Bus *bus, u16 addr, u8 data);

extern void
cpu_connect_bus(Cpu *cpu, Bus *bus);

extern u8
cpu_read(Cpu *cpu, u16 addr);

extern void
cpu_write(Cpu *cpu, u16 addr, u8 data);

extern void
cpu_clock(Cpu *cpu);

/** @brief Reset signal.
 *
 * @note Must behave asynchronously.
 * @note Will interrupt the processor.
 */
extern void
cpu_reset(Cpu *cpu);

/** @brief Interrupt request signal.
 *
 * @note Must behave asynchronously.
 * @note Will interrupt the processor.
 */
extern void
cpu_irq(Cpu *cpu);

/** Non-maskable interrupt signal.
 *
 * @note Must behave asynchronously.
 * @note Will interrupt the processor.
 * @note Can never be disabled.
 */
extern void
cpu_nmi(Cpu *cpu);

#endif // !E6502_H
