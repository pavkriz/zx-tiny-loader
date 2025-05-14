#include "shadow_emu.h"
#include "../global.h"
#include "didaktik_gama_1989_rom.h"
#include "../../rom/testrom.h"
#include <stdio.h>
#include "../yielding_rom_macros.h"
#include "hardware/timer.h"
#include "emu_z80_debug.h"
#include "../sna_loader.h"
#include "../../rom/bios.h"
#include "flight_recorder.h"
#include <Z/constants/pointer.h>
#include <Z/types/integral.h>
#include "emu_divide.h"

#include <Z80.h>

#define ZX_ROM_SIZE 0x4000
#define Z80_MEMSIZE 0x10000

typedef struct {
        zusize  cycles;
        zuint8  memory[Z80_MEMSIZE];
        Z80     cpu;
} Machine;

Machine machine;

typedef struct {
        uint8_t   val;
        uint16_t  real_val;
} Generic_mem_read_result;

bool next_mem_wr_is_push_f = false;

void FASTCODE NOFLASH(DumpSplitBrain)()
{
	flight_recorder_dump();
	printf("Waited bus start: %d\n", zx_waited_bus_start);
	printf("Interrupt mode: IM%d\n", machine.cpu.im);
	printf("HALT state: %d\n", machine.cpu.halt_line);
	printf("PC: 0x%04X\n", machine.cpu.pc.uint16_value);
	printf("SP: 0x%04X\n", machine.cpu.sp.uint16_value);
	printf("A: 0x%02X\n", machine.cpu.af.uint16_value >> 8);
	printf("F: 0x%02X\n", machine.cpu.af.uint16_value & 0xFF);
	printf("BC: 0x%04X\n", machine.cpu.bc.uint16_value);
	printf("DE: 0x%04X\n", machine.cpu.de.uint16_value);
	printf("HL: 0x%04X\n", machine.cpu.hl.uint16_value);
	printf("R: 0x%02X\n", (machine.cpu.r & 0b01111111) | (machine.cpu.r7 & 0b10000000));

	/*
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
	*/
	while (1) { } // stop here
}


static Generic_mem_read_result INLINE FASTCODE (machine_cpu_read_generic)(Machine *self, zuint16 addr, bool is_m1)
{
	uint16_t real_val;
	uint8_t val;

	// IRQ acknowledge cycle detection
	uint32_t control_pins = zx_bus_event_wait();
	if (control_pins & PIN_BIT_IORQ) {
		// IRQ acknowledge cycle happened in real HW instead of memory read, addr is irrelevant here
		real_val = zx_bus_get_data();
		val = real_val;
		real_val = (Z80_REQUEST_INT << 8) | real_val; // add IRQ flag
		return (Generic_mem_read_result) { .val = val, .real_val = real_val };
	} else if (control_pins & PIN_BIT_WR) {
		printf("Emulation split brain - Unexpected WR cycle at addr %04X\n", addr);
		DumpSplitBrain();
	}

	// normal memory read detected at addr

	// early divide mapping
	// if ((addr & 0xff00) == 0x3d00) {
    // 	divide_set_automap(true);
    // }

	if ((addr < ZX_ROM_SIZE) && snapshot_is_loading()) {
		// read from emulated ROM
		if (addr == ROM_ADDR_COMMAND_BYTE_BUFFER) {
			// This is the command byte buffer address, 
			// we need to read next byte from snapshot RAM to be loaded by the running Z80 code.
			// This way we mimmic something like INIR instruction behaviour but from the memory instead of I/O port.
			val = snapshot_get_next_byte();
		} else {
			val = self->memory[addr];
		}
		zx_bus_yield_data(val);
		real_val = val;
		if (addr == snapshot_get_rom_page_out_address()) {
			// this is the address where the "BIOS" ROM (that loads a snapshot) is mapped out and original ROM is mapped in
			// we do not have to map ZX ROM in since we disable (map out) it only during zx_bus_yield_data operation
			snapshot_loading_finished();
		}
	//} else if ((addr < ZX_ROM_SIZE) && divide_is_mapped()) {
		// TODO zx_bus_yield_data based on current divide mapping
	/*
	if (addr < ZX_ROM_SIZE) {
		val = self->memory[addr];
		real_val = yield_mem_or_sniff_iorq(val);
		*/
	} else {
		// here we may either read from our emulated memory (if we have ROM copy as well) or read from DATA bus
		// TODO check also unexpected memory write here indicating CALL (PUSH PC) due to entering NMI ISR
		real_val = zx_bus_get_data();
		if (addr >= ZX_ROM_SIZE) {
			// RAM, we shadow-emulate it
			val = self->memory[addr];
		} else {
			// ROM, we read from DATA bus (we may shadow emulate it in future, but now we don't do it yet)
			val = real_val;
		}
	}

	if ((addr & 0xfff8) == 0x1ff8) {
        divide_set_automap(false);
    } else if((addr == 0x0000) || (addr == 0x0008) || (addr == 0x0038)
      || (addr == 0x0066) || (addr == 0x04c6) || (addr == 0x0562)) {
        divide_set_automap(true);
    }

	return (Generic_mem_read_result) { .val = val, .real_val = real_val };
}

// read memory during first (or the only if it's not a prefixed opcode) m1 opcode fetch or detect interrupt acknowledge cycle
zuint16 FASTCODE NOFLASH(machine_cpu_fetch_1st_opcode_or_detect_interrupt_ack)(Machine *self, zuint16 addr)
{
	Generic_mem_read_result res = machine_cpu_read_generic(self, addr, true);
	if (res.real_val & (Z80_REQUEST_INT << 8)) {
		// this is IRQ acknowledge, the calling code will detect the flag in the higher byte
		flight_recorder_log_irq(self->cpu.pc.uint16_value);
	} else {
		flight_recorder_log_op(FR_FETCH_1, addr, res.val);
		if (res.real_val != res.val) {
			printf("Emulation split brain - read from memory: %04X = %02X (real value: %02X)\n", addr, res.val, res.real_val);
			DumpSplitBrain();
		}
	}
	return res.real_val;
}

// read memory or detect interrupt acknowledge cycle while doing NOP during HALTed state
zuint8 FASTCODE NOFLASH(machine_halt_nop)(Machine *self, zuint16 addr)
{
	Generic_mem_read_result res = machine_cpu_read_generic(self, addr, false);
	if (res.real_val & (Z80_REQUEST_INT << 8)) {
		// this is IRQ acknowledge
		flight_recorder_log_irq(self->cpu.pc.uint16_value);
		// notify the calling code by setting a flag in the CPU state
		self->cpu.request = Z80_REQUEST_INT;
	} else {
		flight_recorder_log_halt_nop(addr);
		if (res.real_val != res.val) {
			printf("Emulation split brain - read from memory: %04X = %02X (real value: %02X)\n", addr, res.val, res.real_val);
			DumpSplitBrain();
		}
	}
	return res.real_val;
}

// read memory during a second succesive (in case of a prefixed opcode) m1 opcode fetch
zuint16 FASTCODE NOFLASH(machine_cpu_fetch_2nd_opcode)(Machine *self, zuint16 addr)
{
	Generic_mem_read_result res = machine_cpu_read_generic(self, addr, true);
	if (res.real_val & (Z80_REQUEST_INT << 8)) {
		printf("Emulation split brain - unexpected IRQ acknowledge cycle detected\n");
		DumpSplitBrain();
	} else {
		flight_recorder_log_op(FR_FETCH_1, addr, res.val);
		if (res.real_val != res.val) {
			printf("Emulation split brain - read from memory: %04X = %02X (real value: %02X)\n", addr, res.val, res.real_val);
			DumpSplitBrain();
		}
	}
	return res.real_val;
}

// read memory
zuint8 FASTCODE NOFLASH(machine_cpu_read)(Machine *self, zuint16 addr)
{
	Generic_mem_read_result res = machine_cpu_read_generic(self, addr, false);
	if (res.real_val & (Z80_REQUEST_INT << 8)) {
		/*
		This is now handled via machine_halt_nop
		if (self->cpu.halt_line) {
			self->cpu.request = Z80_REQUEST_INT;
			return 0;
		}
		*/
		printf("Emulation split brain - unexpected IRQ acknowledge cycle detected\n");
		DumpSplitBrain();
	} else {
		int is_at_pc = (self->cpu.pc.uint16_value == addr);
		flight_recorder_log_op(is_at_pc ? FR_FETCH : FR_MEM_RD, addr, res.val);
		if (res.real_val != res.val) {
			printf("Emulation split brain - read from memory: %04X = %02X (real value: %02X)\n", addr, res.val, res.real_val);
			DumpSplitBrain();
		}
	}
	return res.real_val;
}

// write memory
void FASTCODE NOFLASH(machine_cpu_write)(Machine *self, zuint16 addr, zuint8 data)
{
	uint8_t real_val = sniff_mem_wr();
	flight_recorder_log_mem_wr(addr, data);

	if (data != real_val) {
		printf("Emulation split brain - write to memory: %04X <= %02X (real value: %02X)\n", addr, data, real_val);
		DumpSplitBrain();
	}
	if (addr >= 0x4000) { // write only to RAM
		self->memory[addr] = real_val;
	}
}

// read port
zuint8 FASTCODE NOFLASH(machine_cpu_in)(Machine *self, zuint16 addr)
{
	uint8_t val = sniff_io_rd();
	flight_recorder_log_io_rd(addr, val);
	return val;
}

// write port
void FASTCODE NOFLASH(machine_cpu_out)(Machine *self, zuint16 addr, zuint8 data)
{
	uint8_t real_val = sniff_io_wr();
	flight_recorder_log_io_wr(addr, data);
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

void FASTCODE NOFLASH(shadow_emulator)() {
	// for now set all GPIOs as inputs
	for (int i = 0; i < 32; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_disable_pulls(i);
	}

	// initialize Z80
	machine.cpu.context      = &machine;
	machine.cpu.fetch_opcode_or_detect_interrupt = (Z80ReadOrInterrupt)machine_cpu_fetch_1st_opcode_or_detect_interrupt_ack;
	machine.cpu.nop          = (Z80Read )machine_halt_nop;
	machine.cpu.fetch_opcode = (Z80Read )machine_cpu_fetch_2nd_opcode;
	machine.cpu.fetch        =
	machine.cpu.read         = (Z80Read )machine_cpu_read;
	machine.cpu.write        = (Z80Write)machine_cpu_write;
	machine.cpu.in           = (Z80Read )machine_cpu_in;
	machine.cpu.out          = (Z80Write)machine_cpu_out;
	machine.cpu.halt         = Z_NULL;
	machine.cpu.nmia         = Z_NULL;
	machine.cpu.inta         = Z_NULL;
	machine.cpu.int_fetch    = Z_NULL;
	machine.cpu.ld_i_a       = Z_NULL;
	machine.cpu.ld_r_a       = Z_NULL;
	machine.cpu.reti         = Z_NULL;
	machine.cpu.retn         = Z_NULL;
	machine.cpu.hook         = Z_NULL;
	machine.cpu.illegal      = Z_NULL;
	machine.cpu.options      = Z80_MODEL_ZILOG_NMOS;

	// copy ROM to memory
	for (int i = 0; i < sizeof(testrom_rom); i++) {
		//Memory[i] = didaktik_gama_1989_rom[i];
		//Memory[i] = testrom[i];
		//machine.memory[i] = testrom_rom[i];
		machine.memory[i] = bios_rom[i];
	}

	snapshot_init(machine.memory); // initialize snapshot loading

	z80_power(&machine.cpu, Z_FALSE);

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


	z80_run(&machine.cpu, 50000000);
	flight_recorder_dump();
	printf("Emulation finished\n");
	printf("Interrupt mode: IM%d\n", machine.cpu.im);
	printf("PC: 0x%04X\n", machine.cpu.pc.uint16_value);
	while (1) {
		// wait here
	}
}
