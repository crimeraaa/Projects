#include "6502.c"

int
main(void)
{
    Cpu cpu;
    Bus bus;

    bus_init(&bus);
    cpu_connect_bus(&cpu, &bus);
    return 0;
}
