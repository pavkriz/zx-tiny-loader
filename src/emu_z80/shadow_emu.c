#if USE_EMU	

#include "shadow_emu.h"
#include "picolib_emu/emu.h"
#include "didaktik_gama_1989_rom.h"
#include "testrom.h"
#include <stdio.h>
#include "../yielding_rom_macros.h"
#include "hardware/timer.h"
#include "emu_z80_debug.h"
#include "../sna_loader.h"
#include "../bios_rom.h"

#define ZX_ROM_SIZE 0x4000

sZ80	z80cpu;
u8 Memory[Z80_MEMSIZE]; // memory 64 KB
// ROM pointer, default to "Memory"
u8* ROM = Memory;

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
	yield_ld_a_n(0); // will be I
	yield_ld_i_a();
	// will be R, we make it the value that increments due to following instructions make it wrap to 0 after call 0x0000 (ie. "reset")
	// 22 refreshcycles follows, only 7 bits are used for R
	yield_ld_a_n((0-22) & 0b01111111); 
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

	//gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_IN);  // enable ROM
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
	printf("I: 0x%02X\n", z80cpu.i);
	printf("R: 0x%02X\n", z80cpu.r);
	printf("IM mode: 0x%01X\n", z80cpu.mode);	
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
	s16 real_val;
	u8 val;

	if ((addr < ZX_ROM_SIZE) && snapshot_is_loading()) {
		// read from emulated ROM
		if (addr == ROM_ADDR_COMMAND_BYTE_BUFFER) {
			// This is the command byte buffer address, 
			// we need to read next byte from snapshot RAM to be loaded by the running Z80 code.
			// This way we mimmic something like INIR instruction behaviour but from the memory instead of I/O port.
			val = snapshot_get_next_byte();
		} else {
			val = ROM[addr];
		}
		real_val = yield_mem_or_sniff_iorq(val);
		if (addr == snapshot_get_rom_page_out_address()) {
			// this is the address where the "BIOS" ROM (that loads a snapshot) is mapped out and original ROM is mapped in
			enable_zx_rom();
			snapshot_loading_finished();			
		}
	} else {
		// here we may either read from our emulated memory (if we have ROM copy as well) or read from DATA bus
		// TODO check also unexpected memory write here indicating CALL (PUSH PC) due to entering NMI ISR
		real_val = sniff_mem_rd_or_iorq();
		if (addr >= ZX_ROM_SIZE) {
			// RAM, we shadow-emulate it
			val = Memory[addr];
		} else {
			// ROM, we read from DATA bus (we may shadow emulate it in future, but now we don't do it yet)
			val = real_val;
		}
	}

	if (real_val < 0) {
		resetMemoryM1ReadSinceIrqCounter();
		// this is IRQ acknowledge
		u8 opcode_at_isr;
		s16 data_bus_val = -real_val-1; // convert to positive value		
		// depending on the IM mode, use or not use the value from the DATA bus
		// update CPU state that we entered ISR
		// push PC
		z80cpu.writemem(--z80cpu.sp, z80cpu.pc >> 8); // push PC high byte
		z80cpu.writemem(--z80cpu.sp, z80cpu.pc & 0xFF); // push PC low byte
		if (z80cpu.mode == Z80_INTMODE0) {
			// read instruction opcode from DATA bus
			opcode_at_isr = data_bus_val;
		} else if (z80cpu.mode == Z80_INTMODE1) {
			// set PC to 0x0038
			z80cpu.pc = 0x0038;
			opcode_at_isr = z80cpu.readmem(z80cpu.pc);
		} else {	// IM2
			// calculate the pointer to vector table
			u16 isr_l_pointer = (z80cpu.i << 8) | (data_bus_val & 0xFF);
			// read from vector table the ISR address
			u8 isr_pc_l = z80cpu.readmem(isr_l_pointer);
			u8 isr_pc_h = z80cpu.readmem(isr_l_pointer + 1);
			// set PC to ISR address and fetch the instruction
			z80cpu.pc = (isr_pc_h << 8) | isr_pc_l;
			opcode_at_isr = z80cpu.readmem(z80cpu.pc);
		}
		// set registers iff1 and iff2 to 0
		z80cpu.iff1 = 0;
		z80cpu.iff2 = 0;
		// clear halted internal flag if set
		z80cpu.halted = 0;		
		z80cpu.r += 1; // sync R register with real Z80 (why this??)
		return opcode_at_isr;
	} else {
		if (real_val != val) {
			printf("Emulation split brain - read from memory: %04X = %02X (real value: %02X)\n", addr, val, real_val);
			DumpSplitBrain();
		}
		return real_val;
	}
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
				// stored flags look good, but emulated flags may differ in bits 3 and 5 to real ones
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
	u8 real_val = sniff_io_wr();
	if (data != real_val) {
		printf("Emulation split brain - write to IO: %04X <= %02X (real value: %02X)\n", addr, data, real_val);
		DumpSplitBrain();
	}
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
	for (int i = 0; i < sizeof(testrom); i++) {
		//Memory[i] = didaktik_gama_1989_rom[i];
		//Memory[i] = testrom[i];
		Memory[i] = bios_rom[i];
	}

	snapshot_init(Memory); // initialize snapshot loading


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
    //gpio_put(PIN_NUMBER_RESET, 1);


	Z80_Start(&z80cpu, 0, 0);
}

#endif // USE_EMU