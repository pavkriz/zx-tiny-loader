#include "emu_z80_debug.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "../yielding_rom_macros.h"

uint32_t memoryM1ReadCounter = 0;


void FASTCODE NOFLASH(printMemoryM1ReadCounter)() {
	printf("Memory read counter: %u\n", memoryM1ReadCounter);
}

void FASTCODE NOFLASH(dumpRegisters)(sZ80* z80cpu) {
// now disconnect ROM (via ROMCS) and emulate instructions that read several registers from the real CPU
			gpio_put(PIN_NUMBER_ROMCS, 1); // keep /ROMCS high to disable the internal ROM
			gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_OUT);
			/*
	uint16_t emu_z80_pc = 0;

		z80_reset:
        	yield_di();  // disable interrupts to avoid CPU jumping to 0x0038 on its own     
		loop:
			// Set the border color to black
			yield_ld_a_n(0);  
			yield_out_n_a(0xfe);
			for (int i = 0; i < 500; i++) {
				yield_nop();
				yield_dummy_jump();  // dummy jump to avoid the CPU to increment PC too much
			}
			// Set the border color to red
			yield_ld_a_n(2);
			yield_out_n_a(0xfe);
			for (int i = 0; i < 500; i++) {
				yield_nop();
				yield_dummy_jump();  // dummy jump to avoid the CPU to increment PC too much
			}
			// loop
			yield_jump(loop);
			*/

			// for (int i = 0; i < 0x10; i++) {
			// 	yield_nop();
			// }

			// read PC from CPU indirectly (no direct way exists) by making dummy CALL to a fictitious address
			// doing CALL XXX which pushes PC to stack, SP was 0xffff after reset, PUSH = cpu->writemem(--cpu->sp, regH); cpu->writemem(--cpu->sp, regL); 
			yield_call(0x1234);
			// now read DATA bus on each write to get the PC value beeing pushed to stack (store to RAM)
			uint8_t pc_h = sniff_mem_wr();
			uint8_t pc_l = sniff_mem_wr();
			uint16_t pc = (((uint16_t)pc_h << 8) | (uint16_t)pc_l) - 3; // 3=length of CALL instruction that adds artificially 3 to PC
			yield_push_af();
			// now read DATA bus on each write to get the A and F value beeing pushed to stack (store to RAM)
			uint8_t a = sniff_mem_wr();
			uint8_t f = sniff_mem_wr();		
			yield_push_bc();
			uint8_t b = sniff_mem_wr();			
			uint8_t c = sniff_mem_wr();
			yield_push_de();
			uint8_t d = sniff_mem_wr();
			uint8_t e = sniff_mem_wr();
			yield_push_hl();
			uint8_t h = sniff_mem_wr();
			uint8_t l = sniff_mem_wr();
			printf("==========================\n");
			printMemoryM1ReadCounter();
			printf("Real PC: %04X\n", pc);
			printf("Real A: %02X\n", a);
			printf("Real F: %02X\n", f & 0b11010111);
			printf("Real BC: %04X\n", (b << 8) | c);
			printf("Real DE: %04X\n", (d << 8) | e);
			printf("Real HL: %04X\n", (h << 8) | l);
			// print our emulated Z80 registers and stop emulation
			printf("Emulation stopped\n");
			printf("PC:\t%04X\n", z80cpu->pc);
			printf("A:\t%02X\n", z80cpu->a);
			printf("F:\t%02X\n", z80cpu->f & 0b11010111);
			printf("BC:\t%04X\n", z80cpu->bc);
			printf("DE:\t%04X\n", z80cpu->de);
			printf("HL:\t%04X\n", z80cpu->hl);
			printf("IFF1:\t%02X\n", z80cpu->iff1);
			printf("IFF2:\t%02X\n", z80cpu->iff2);
			// and stop
			while (1) { }
}

void FASTCODE NOFLASH(EmuDebugHookPreM1)(sZ80* z80cpu) {
    //if (memoryM1ReadCounter > 458975) {
	//if (memoryM1ReadCounter > 10) {
	if (memoryM1ReadCounter >= 1000000) {
			dumpRegisters(z80cpu);
	} else {
		memoryM1ReadCounter++;
	}

}
