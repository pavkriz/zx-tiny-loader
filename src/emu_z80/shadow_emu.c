#if USE_EMU	

#include "shadow_emu.h"
#include "picolib_emu/emu.h"
#include "didaktik_gama_1989_rom.h"
#include <stdio.h>
#include "../yielding_rom_macros.h"
#include "hardware/timer.h"
#include "emu_z80_debug.h"


sZ80	z80cpu;
u8 Memory[Z80_MEMSIZE]; // memory 64 KB

u8 last_value_written_to_0xff4a = 0;
u8 last_value_written_to_0xff4a_at_pc = 0;
u8 last_value_written_to_0xff4a_hl = 0;
u8 last_value_written_to_0xff4a_hl2 = 0;
u8 last_value_written_to_0xff4a_processing_m1_opcode = 0;
u8 last_value_written_to_0xff4a_processing_m1_pc = 0;
u8 last_value_written_to_0xff4a_previous_m1_opcode = 0;
u8 last_value_written_to_0xff4a_previous_m1_pc = 0;
bool next_mem_wr_is_push_f = false;

void FASTCODE NOFLASH(EmuInitializeRealZ80)()
{
	gpio_put(PIN_NUMBER_ROMCS, 1); // keep /ROMCS high to disable the internal ROM
	gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_OUT);

	yield_di();
	yield_ld_a_n(0); // will be I and R
	yield_ld_i_a();
	yield_ld_r_a();
	yield_ld_sp_nn(0xffff); // SP
	yield_exx();
	yield_exx_af_af2();
	yield_hl_nn(0xFFFF); // will be AF'
	yield_push_hl();
	yield_pop_af();
	sniff_mem_rd(); // skip two reads from memory caused by POP AF
	sniff_mem_rd();
	yield_ld_bc_nn(0x0000); // will be BC'
	yield_ld_de_nn(0x0000); // will be DE'
	yield_ld_hl_nn(0x0000); // will be HL'
	yield_exx();
	yield_exx_af_af2();
	yield_hl_nn(0xFFFF); // will be AF
	yield_push_hl();
	yield_pop_af();
	sniff_mem_rd(); // skip two reads from memory caused by POP AF
	sniff_mem_rd();
	yield_ld_bc_nn(0x0000); // BC
	yield_ld_de_nn(0x0000); // DE
	yield_ld_hl_nn(0x0000); // HL
	yield_ld_ix_nn(0x0000); // IX
	yield_ld_iy_nn(0x0000); // IY
	yield_jp_nn(0x0000); // will start CPU again with registers defined and ROM will be enabled (see next function call)

	gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_IN);  // enable ROM
}

void FASTCODE NOFLASH(DumpSplitBrain)()
{
	printf("Processing M1 opcode: 0x%02X\n", z80cpu.processing_m1_opcode);
	printf("Previous M1 opcode: 0x%02X\n", z80cpu.previous_m1_opcode);
	printf("Processing M1 PC: 0x%04X\n", z80cpu.processing_m1_pc);
	printf("PC: 0x%04X\n", z80cpu.pc);
	printf("BC: 0x%04X\n", z80cpu.bc);
	printf("DE: 0x%04X\n", z80cpu.de);
	printf("HL: 0x%04X\n", z80cpu.hl);
	printf("A: 0x%02X\n", z80cpu.a);
	printf("F: 0x%02X\n", z80cpu.f);
	printf("Last value written to 0xff4a: %02X\n", last_value_written_to_0xff4a);
	printf("Last value written to 0xff4a at PC: 0x%04X\n", last_value_written_to_0xff4a_at_pc);
	printf("Last value written to 0xff4a processing M1 opcode: 0x%02X\n", last_value_written_to_0xff4a_processing_m1_opcode);
	printf("Last value written to 0xff4a previous M1 opcode: 0x%02X\n", last_value_written_to_0xff4a_previous_m1_opcode);
	printf("Last value written to 0xff4a processing M1 PC: 0x%04X\n", last_value_written_to_0xff4a_processing_m1_pc);
	printf("Last value written to 0xff4a previous M1 PC: 0x%04X\n", last_value_written_to_0xff4a_previous_m1_pc);
	printf("Last value written to 0xff4a HL: 0x%04X\n", last_value_written_to_0xff4a_hl);
	printf("Last value written to 0xff4a HL2: 0x%04X\n", last_value_written_to_0xff4a_hl2);
	printMemoryM1ReadCounter();
	while (1) { } // stop here
}

// read memory
u8 FASTCODE NOFLASH(EmuGetMem)(u16 addr)
{
	// here we may either read from our emulated memory (if we have ROM copy as well) or read from DATA bus
	// in future, we may measure time between two two consecutive memory reads/writes or io reads/writes to detect in real HW an additional delay accured due to transition to ISR (IRQ ack + PC push to SP make this delay)
	// remember to consider extra delay if we know the Z80 is doint MEM writes or IO writes that we do not sniff actually (so we may emulate them faster than they are done in real HW)	
	u8 real_val = (addr <= 10) ? sniff_mem_rd_fast() : sniff_mem_rd();
	//u8 real_val = sniff_mem_rd_fast();
	u8 val = Memory[addr];
	if (real_val != val) {
		if (real_val == 0xF5) {
			// we are about to read ISR from ROM at 0x0038: F5 MASK_INT  PUSH AF
			// update CPU state that we entered ISR
			// all the following have already been done in the real Z80 while we waited for read signal (in M1 state)
			// push PC (avoid calling writemem since it would wait for real memory write, but it had already been done in real Z80 before)
			Memory[--z80cpu.sp] = z80cpu.pc >> 8; // push PC high byte
			Memory[--z80cpu.sp] = z80cpu.pc & 0xFF; // push PC low byte
			// set PC to next address after 0x0038
			z80cpu.pc = 0x0038;
			// set registers iff1 and iff2 to 0
			z80cpu.iff1 = 0;
			z80cpu.iff2 = 0;
			// clear halted internal flag if set
			z80cpu.halted = 0;
			resetMemoryM1ReadSinceIrqCounter();
		} else {
			// this would fail if there is POP AF instruction and emulated F register stored on stack is different to the real F register due to undocumented flag bits 3 and 5
			// this ignore the difference if we are doing POP AF instruction
			// if (z80cpu.processing_m1_opcode != 0xF1) { // POP AF
				printf("Emulation split brain - read from memory: %04X = %02X (real value: %02X)\n", addr, val, real_val);
				DumpSplitBrain();
			// }
		}
	}
	return real_val;
}

// write memory
void FASTCODE NOFLASH(EmuSetMem)(u16 addr, u8 data)
{
	u8 real_val = sniff_mem_wr();
	bool ignoreDifference = false;
	if (z80cpu.processing_m1_opcode == 0xF5) {
		// doing PUSH AF
		if (next_mem_wr_is_push_f) {
			// this is the second byte write of the PUSH AF instruction, ie. the F register
			// compare F glags regardless of bits 3 and 5
			if ((data & 0b11010111) != (real_val & 0b11010111)) {
				printf("Emulation split brain FLAGS differ - write to memory: %04X <= %02X (real value: %02X)\n", addr, data, real_val);
				DumpSplitBrain();
			} else {
				// stored flags lookg good, but emulated flags may differ in bits 3 and 5 to real ones
				// update emulated F register to match the real one in order no to get splitbrain when doing POP AF (or POP HL or whartever from location where AF is stored) later
				z80cpu.f = real_val;
				ignoreDifference = true;
			}
			next_mem_wr_is_push_f = false;
		} else {
			// this is the first byte write of the PUSH AF instruction, ie. the A register
			next_mem_wr_is_push_f = true;
		}
	}

	if (data != real_val && !ignoreDifference) {
		printf("Emulation split brain - write to memory: %04X <= %02X (real value: %02X)\n", addr, data, real_val);
		DumpSplitBrain();
	}
	if (addr >= 0x4000) { // write only to RAM
		Memory[addr] = real_val;
		if (addr == 0xff4a) {			
			last_value_written_to_0xff4a = data;
			last_value_written_to_0xff4a_at_pc = z80cpu.pc;
			last_value_written_to_0xff4a_processing_m1_opcode = z80cpu.processing_m1_opcode;
			last_value_written_to_0xff4a_previous_m1_opcode = z80cpu.previous_m1_opcode;
			last_value_written_to_0xff4a_processing_m1_pc = z80cpu.processing_m1_pc;
			last_value_written_to_0xff4a_previous_m1_pc = z80cpu.previous_m1_pc;
			last_value_written_to_0xff4a_hl = z80cpu.hl;
			last_value_written_to_0xff4a_hl2 = z80cpu.hl2;
		}
	}
}

// read port
u8 FASTCODE NOFLASH(EmuGetPort)(u16 addr)
{
	return sniff_io_rd();
}

// write port
void FASTCODE NOFLASH(EmuSetPort)(u16 addr, u8 data)
{
	// do nothing
	if (((addr & 0xFF) == 127) || ((addr & 0xFF) == 95)) {
		if (data != 0x92) {
			printf("Gama RAM bank switch: %02X\n", data);
		}
	}
}

void shadow_emulator() {
	// for now set all GPIOs as inputs
	for (int i = 0; i < 32; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_disable_pulls(i);
	}

	// initialize Z80 table
	Z80_InitTab();

	// copy ROM to memory
	for (int i = 0; i < sizeof(didaktik_gama_1989_rom); i++) {
		Memory[i] = didaktik_gama_1989_rom[i];
	}

	// setup callback functions
	z80cpu.readmem = EmuGetMem;
	z80cpu.writemem = EmuSetMem;
	z80cpu.readport = EmuGetPort;
	z80cpu.writeport = EmuSetPort;

    // except /ROMCS and /RESET which will be output
    //gpio_init(PIN_NUMBER_ROMCS);
    // gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_OUT);
    gpio_init(PIN_NUMBER_RESET);
    gpio_set_dir(PIN_NUMBER_RESET, GPIO_OUT);
    // keep /ROMCS high to disable the internal ROM
    // gpio_put(PIN_NUMBER_ROMCS, 1);
    // keep /RESET low for a while to reset the CPU
    gpio_put(PIN_NUMBER_RESET, 0);
    busy_wait_ms(100); // wait some time
    // set /RESET high to let the CPU run
    gpio_put(PIN_NUMBER_RESET, 1);


	Z80_Start(&z80cpu, 0, 0);
}

#endif // USE_EMU