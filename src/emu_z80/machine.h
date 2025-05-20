#pragma once

#define Z80_MEMSIZE 0x10000

typedef struct {
        zusize  cycles;
        zuint8  memory[Z80_MEMSIZE];
        Z80_t     cpu;
} Machine;

extern Machine z80machine;

 // implemented in shadow_emu.c but used in z80.c:
extern zuint16 machine_cpu_fetch_1st_opcode_or_detect_interrupt_ack(zuint16 addr);
extern zuint16 machine_cpu_fetch_2nd_opcode(zuint16 addr);
extern zuint16 machine_cpu_fetch_params(zuint16 addr);
extern zuint8 machine_cpu_read(zuint16 addr);
extern void machine_cpu_write(zuint16 addr, zuint8 data);