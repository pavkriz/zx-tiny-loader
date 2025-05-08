#pragma once
#include "global.h"
#include <stdint.h>


#define ROM_BUFFER_ADDR 0x2000
#define ROM_BUFFER_SIZE 0x2000
#define ROM_ADDR_COMMAND_BYTE_BUFFER 0x1F01

extern uint8_t snapshot_loading;
extern uint16_t snapshot_ram_index; // byte to be loaded next
extern uint8_t* snapshot_ram_ptr; // pointer to the snapshot RAM to be loaded
extern uint16_t snapshot_rom_page_out_address; // address where the "BIOS" ROM is mapped out and original ROM is mapped in, usually the last byte of the bootstrap code that finally jumps to the snapshot code
#define snapshot_is_loading() (snapshot_loading == 1)
#define snapshot_loading_finished() { snapshot_loading = 0; }
#define snapshot_get_rom_page_out_address() (snapshot_rom_page_out_address)
#define snapshot_set_rom_page_out_address(addr) (snapshot_rom_page_out_address = addr)
#define snapshot_get_next_byte() (snapshot_ram_ptr[snapshot_ram_index++])
uint8_t FASTCODE NOFLASH(snapshot_init)(uint8_t* rom);

