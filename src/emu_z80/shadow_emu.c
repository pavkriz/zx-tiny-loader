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


// read memory
u8 FASTCODE NOFLASH(EmuGetMem)(u16 addr)
{
	// here we may either read from our emulated memory (if we have ROM copy as well) or read from DATA bus
	u8 real_val = sniff_mem_rd();
	u8 val = Memory[addr];
	if (real_val != val) {
		printf("Emulation brain split - read from memory: %04X = %02X (real value: %02X)\n", addr, val, real_val);
		printMemoryM1ReadCounter();
		while (1) { } // stop here
	}
	return val;
}

// write memory
void FASTCODE NOFLASH(EmuSetMem)(u16 addr, u8 data)
{
	if (addr >= 0x4000) { // write only to RAM
		Memory[addr] = data;
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