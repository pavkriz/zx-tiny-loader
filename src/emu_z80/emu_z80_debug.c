#include "emu_z80_debug.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "../yielding_rom_macros.h"

uint32_t memoryM1ReadCounter = 0;
uint32_t memoryM1ReadSinceIrqCounter = 0;
uint32_t irqCounter = 0;
bool irqAppeared = false;


void FASTCODE NOFLASH(printMemoryM1ReadCounter)() {
	printf("Memory read counter: %u\n", memoryM1ReadCounter);
	printf("Memory read since IRQ counter: %u\n", memoryM1ReadSinceIrqCounter);
	printf("IRQ counter: %u\n", irqCounter);
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

			yield_push_af();
			// now read DATA bus on each write to get the A and F value beeing pushed to stack (store to RAM)
			uint8_t a = sniff_mem_wr();
			uint8_t f = sniff_mem_wr();		
			yield_ld_a_r();
			yield_push_af();
			uint8_t r = sniff_mem_wr();	// here R is actually incremented artificially by 2 due to previous 2 instruction involved in "dump registers" process
			r = (r - 2) & 0b01111111 + (r & 0b10000000); // R is 7 bits counter, 8th bit is always kept as is
			uint8_t dummy = sniff_mem_wr();		
			// read PC from CPU indirectly (no direct way exists) by making dummy CALL to a fictitious address
			// doing CALL XXX which pushes PC to stack, SP was 0xffff after reset, PUSH = cpu->writemem(--cpu->sp, regH); cpu->writemem(--cpu->sp, regL); 
			yield_call(0x1234);
			// now read DATA bus on each write to get the PC value beeing pushed to stack (store to RAM)
			uint8_t pc_h = sniff_mem_wr();
			uint8_t pc_l = sniff_mem_wr();
			uint16_t pc = (((uint16_t)pc_h << 8) | (uint16_t)pc_l) - 7; // 3=length of CALL instruction that adds artificially 3 to PC + 1 PUSH AF + 2 LD A,R + 1 PUSH AF
			yield_ld_nn_sp(0x8000);  // store SP to "some" address and sniff the value
			uint8_t sp_l = sniff_mem_wr();  // here is LOW first
			uint8_t sp_h = sniff_mem_wr();
			uint16_t sp = (((uint16_t)sp_h << 8) | (uint16_t)sp_l) + 2;  // 2=length of return adress of CALL instruction that is used few lines above
			yield_push_bc();
			uint8_t b = sniff_mem_wr();			
			uint8_t c = sniff_mem_wr();
			yield_push_de();
			uint8_t d = sniff_mem_wr();
			uint8_t e = sniff_mem_wr();
			yield_push_hl();
			uint8_t h = sniff_mem_wr();
			uint8_t l = sniff_mem_wr();
			yield_exx();
			yield_push_bc();
			uint8_t b2 = sniff_mem_wr();			
			uint8_t c2 = sniff_mem_wr();
			yield_push_de();
			uint8_t d2 = sniff_mem_wr();
			uint8_t e2 = sniff_mem_wr();
			yield_push_hl();
			uint8_t h2 = sniff_mem_wr();
			uint8_t l2 = sniff_mem_wr();
			printf("==========================\n");
			printMemoryM1ReadCounter();
			printf("Real PC: %04X\n", pc);
			printf("Real SP: %04X\n", sp);
			printf("Real A: %02X\n", a);
			//printf("Real F: %02X\n", f & 0b11010111);
			printf("Real F: %02X\n", f);
			printf("Real R: %02X\n", r);
			printf("Real BC: %04X\n", (b << 8) | c);
			printf("Real DE: %04X\n", (d << 8) | e);
			printf("Real HL: %04X\n", (h << 8) | l);
			printf("Real BC': %04X\n", (b2 << 8) | c2);
			printf("Real DE': %04X\n", (d2 << 8) | e2);
			printf("Real HL': %04X\n", (h2 << 8) | l2);
			// print our emulated Z80 registers and stop emulation
			printf("Emulation stopped\n");
			printf("PC:\t%04X\n", z80cpu->pc);
			printf("SP:\t%04X\n", z80cpu->sp);
			printf("A:\t%02X\n", z80cpu->a);
			//printf("F:\t%02X\n", z80cpu->f & 0b11010111);
			printf("F:\t%02X\n", z80cpu->f);
			printf("R:\t%02X\n", (z80cpu->r & 0b01111111) | (z80cpu->r7 & 0b10000000));
			printf("BC:\t%04X\n", z80cpu->bc);
			printf("DE:\t%04X\n", z80cpu->de);
			printf("HL:\t%04X\n", z80cpu->hl);
			printf("BC':\t%04X\n", z80cpu->bc2);
			printf("DE':\t%04X\n", z80cpu->de2);
			printf("HL':\t%04X\n", z80cpu->hl2);
			printf("IFF1:\t%02X\n", z80cpu->iff1);
			printf("IFF2:\t%02X\n", z80cpu->iff2);
			printf("Processing M1 PC: %04X\n", z80cpu->processing_m1_pc);			
			printf("Processing M1 opcode: 0x%02X\n", z80cpu->processing_m1_opcode);
			// and stop
			while (1) { }
}

void FASTCODE NOFLASH(EmuDebugHookPreM1)(sZ80* z80cpu) {
    //if (memoryM1ReadCounter > 458975) {
	//if (memoryM1ReadCounter > 10) {
	//if (memoryM1ReadCounter >= 1000000 || (irqCounter > 2 && (memoryM1ReadSinceIrqCounter >= 98))) {
	//if (memoryM1ReadCounter >= 1000000 || (irqCounter > 0 && (memoryM1ReadSinceIrqCounter >= 1))) {
	//if (memoryM1ReadCounter >= 907001) {
	//if (memoryM1ReadCounter >= 901394) {
	//if (memoryM1ReadCounter >= 10000000) {
	//if (memoryM1ReadCounter >= 1000000) {
	//if (memoryM1ReadCounter >= 904000) {
	if (memoryM1ReadCounter >= 10000000) {
	// //if (z80cpu->pc == 0x1299) {
	// //if (z80cpu->pc == 0x0c0e) {
	 		dumpRegisters(z80cpu);
	} else {
		memoryM1ReadCounter++;
		if (irqAppeared) {
			memoryM1ReadSinceIrqCounter++;
		}
	}

}

void FASTCODE NOFLASH(resetMemoryM1ReadSinceIrqCounter)() {
	memoryM1ReadSinceIrqCounter = 0;
	irqAppeared = true;
	irqCounter++;
}
