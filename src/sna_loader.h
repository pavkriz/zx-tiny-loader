#pragma once
#include "emu_z80/picolib_scaffold.h"

#define ROM_BUFFER_ADDR 0x2000
#define ROM_BUFFER_SIZE 0x2000
#define ROM_ADDR_COMMAND_BYTE_BUFFER 0x1F01

extern u8 snapshot_loading;
extern u16 snapshot_ram_index; // byte to be loaded next
extern u8* snapshot_ram_ptr; // pointer to the snapshot RAM to be loaded
extern u16 snapshot_rom_page_out_address; // address where the "BIOS" ROM is mapped out and original ROM is mapped in, usually the last byte of the bootstrap code that finally jumps to the snapshot code
#define snapshot_is_loading() (snapshot_loading == 1)
#define snapshot_loading_finished() { snapshot_loading = 0; }
#define snapshot_get_rom_page_out_address() (snapshot_rom_page_out_address)
#define snapshot_set_rom_page_out_address(addr) (snapshot_rom_page_out_address = addr)
#define snapshot_get_next_byte() (snapshot_ram_ptr[snapshot_ram_index++])
u8 FASTCODE NOFLASH(snapshot_init)(uint8_t* rom);

