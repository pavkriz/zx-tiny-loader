#include "sna_loader.h"
#include "emu_z80/picolib_scaffold.h"
#include <stdbool.h>
#include "../test_snapshot_ram_hate.h"
#include "../test_snapshot_z80_hate.h"


#define COMMAND_NONE 0
#define COMMAND_MEM_COPY 1
#define COMMAND_JUMP 2
#define COMMAND_SNAP 3


#define ROM_ADDR_COMMAND 0x1F00
#define ROM_ADDR_COMMAND_DEST 0x1F02
#define ROM_ADDR_COMMAND_SIZE 0x1F04


u8 snapshot_loading = 0;
u16 snapshot_ram_index = 0;
u8* snapshot_ram_ptr;
u16 snapshot_rom_page_out_address = 0x0000;

typedef struct {
    uint8_t  a, f;
    uint8_t a_, f_;
    uint8_t i, r;

    uint16_t ix, iy;
    uint16_t sp, pc;
    uint16_t bc, de, hl;
    uint16_t bc_, de_, hl_;

    uint8_t iff1, iff2, im, out_ula;
} snap_state_t;

void read_snapshot_z80_header(uint8_t *buffer, snap_state_t *snap_state, bool *compressed) {
    snap_state->a = buffer[0];
    snap_state->f = buffer[1];
    snap_state->bc = buffer[2] + (buffer[3] << 8);
    snap_state->hl = buffer[4] + (buffer[5] << 8);
    snap_state->pc = buffer[6] + (buffer[7] << 8);
    snap_state->sp = buffer[8] + (buffer[9] << 8);
    snap_state->i = buffer[10];
    snap_state->r =  (buffer[11] & 0x7f)  +         // Refresh register (bit 7 is not significant)
			         ((buffer[12] & 0x01) << 7 );   // bit0 = R bit 7)
    snap_state->out_ula = (buffer[12] & 0x0e) >> 1; // bits1..3 = border
    snap_state->de = buffer[13] + (buffer[14] << 8);
    snap_state->bc_ = buffer[15] + (buffer[16] << 8);
    snap_state->de_ = buffer[17] + (buffer[18] << 8);
    snap_state->hl_ = buffer[19] + (buffer[20] << 8);
    snap_state->a_ = buffer[21];
    snap_state->f_ = buffer[22];
    snap_state->iy = buffer[23] + (buffer[24] << 8);
    snap_state->ix = buffer[25] + (buffer[26] << 8);
    snap_state->iff1 = buffer[27] ? 1 : 0;
    snap_state->iff2 = buffer[28] ? 1 : 0;
    snap_state->im = buffer[29] & 0x03;
    if (snap_state->pc == 0) {
        // z80 snapshot header version >-= 2
        uint16_t extra_header_length = buffer[30] + (buffer[31] << 8);
        snap_state->pc = buffer[32] + (buffer[33] << 8);
        *compressed = (buffer[12] & 0x20) ? 1 : 0;
    } else {
        // z80 snapshot header v1
        *compressed = 1;
    }
    // LIBSPECTRUM_Z80_HEADER_LENGTH = 30
    // TODO (*data) = buffer + LIBSPECTRUM_Z80_HEADER_LENGTH + 2 + extra_length; // >= v2
    // TODO (*data) = buffer + LIBSPECTRUM_Z80_HEADER_LENGTH; // v1
    /*
    Hereafter a number of memory blocks follow, each containing the compressed data of a 16K block. The compression is according to the old scheme, except for the end-marker, which is now absent. The structure of a memory block is:

        Byte    Length  Description
        ---------------------------
        0       2       Length of compressed data (without this 3-byte header)
                        If length=0xffff, data is 16384 bytes long and not compressed
        2       1       Page number of block
        3       [0]     Data

        https://worldofspectrum.org/faq/reference/z80format.htm
        In 48K mode, pages 4,5 and 8 are saved. 

        Compression
        v1  compressed if bit 5 of byte 12 is set.buffer
        v2  always compressed (?)
    */

   // TODO https://github.com/speccytools/libspectrum/blob/8ba1310586e906fb4bff202c59c8714b180430f4/z80.c#L207
   // TODO read_blocks https://github.com/speccytools/libspectrum/blob/8ba1310586e906fb4bff202c59c8714b180430f4/z80.c#L675

}

uint8_t lowByte(uint16_t n) {
    return (uint8_t)(n & 0x00FF);
}

uint8_t highByte(uint16_t n) {
    return (uint8_t)((n & 0xFF00) >> 8);
}


u8 FASTCODE NOFLASH(snapshot_init)(uint8_t* rom) { 
    snapshot_loading = 1; 
    snapshot_ram_index = 0; 
    snapshot_ram_ptr = test_snapshot_ram;
    // possibly load snapshot RAM from a storage to a 48k array, temporarily we reference the array directly from get_next_byte()

    // interpret z80 snapshot header
    bool compressed = false;
    snap_state_t snap_state;
    read_snapshot_z80_header(test_snapshot_z80, &snap_state, &compressed);

    // create a bootstrap code that sets all registers and jumps the the snapshot code at ROM_BUFFER_ADDR (that is where BIOS asm. jumps after loading the snapshot RAM)
    int pc = ROM_BUFFER_ADDR;
    rom[pc++] = 0xF3; /* DI */

    rom[pc++] = 0x3E; /* ld a, NN */
    rom[pc++] = snap_state.i;
    rom[pc++] = 0xED; /* ld i, a */
    rom[pc++] = 0x47;
    rom[pc++] = 0x3E; /* ld a, NN */
    rom[pc++] = snap_state.r;
    rom[pc++] = 0xED; /* ld r, a */
    rom[pc++] = 0x4F;
    
    rom[pc++] = 0x21; /* ld hl, NNNN */
    rom[pc++] = snap_state.f_;
    rom[pc++] = snap_state.a_;

    rom[pc++] = 0xE5; /* push hl */
    rom[pc++] = 0xF1; /* pop af */
    // TODO add 2 sniff_mem_rd() here somehow to skip two reads from memory caused by POP AF
    
    rom[pc++] = 0x01; /* ld bc, NNNN */
    rom[pc++] = lowByte(snap_state.bc_);
    rom[pc++] = highByte(snap_state.bc_);

    rom[pc++] = 0x11; /* ld de, NNNN */
    rom[pc++] = lowByte(snap_state.de_);
    rom[pc++] = highByte(snap_state.de_);

    rom[pc++] = 0x21; /* ld hl, NNNN */
    rom[pc++] = lowByte(snap_state.hl_);
    rom[pc++] = highByte(snap_state.hl_);

    rom[pc++] = 0xD9; /* exx */
    rom[pc++] = 0x08; /* ex af, af' */

    rom[pc++] = 0x21; /* ld hl, NNNN */
    rom[pc++] = snap_state.f;
    rom[pc++] = snap_state.a;

    rom[pc++] = 0xE5; /* push hl */
    rom[pc++] = 0xF1; /* pop af */
    
    rom[pc++] = 0x01; /* ld bc, NNNN */
    rom[pc++] = lowByte(snap_state.bc);
    rom[pc++] = highByte(snap_state.bc);

    rom[pc++] = 0x11; /* ld de, NNNN */
    rom[pc++] = lowByte(snap_state.de);
    rom[pc++] = highByte(snap_state.de);

    rom[pc++] = 0x21; /* ld hl, NNNN */
    rom[pc++] = lowByte(snap_state.hl);
    rom[pc++] = highByte(snap_state.hl);

    rom[pc++] = 0xDD; /* ld ix, NNNN */
    rom[pc++] = 0x21;
    rom[pc++] = lowByte(snap_state.ix);
    rom[pc++] = highByte(snap_state.ix);

    rom[pc++] = 0xFD; /* ld iy, NNNN */
    rom[pc++] = 0x21;
    rom[pc++] = lowByte(snap_state.iy);
    rom[pc++] = highByte(snap_state.iy);

    switch(snap_state.im){
    case 0:
        rom[pc++] = 0xED; /* IM 0 */
        rom[pc++] = 0x46;
        break;
    case 1:
        rom[pc++] = 0xED; /* IM 1 */
        rom[pc++] = 0x56;
        break;
    case 2:
        rom[pc++] = 0xED; /* IM 2 */
        rom[pc++] = 0x5E;
        break;
    }
    
    rom[pc++] = 0x31; /* ld sp, NNNN */
    rom[pc++] = lowByte(snap_state.sp);
    rom[pc++] = highByte(snap_state.sp);

    switch(snap_state.iff1){
    case 0:
        rom[pc++] = 0xF3; /* DI */
        break;
    case 1:
        rom[pc++] = 0xFB; /* EI */
        break;
    }

    rom[pc++] = 0xC3; /* jp NNNN */
    rom[pc++] = lowByte(snap_state.pc);
    rom[pc++] = highByte(snap_state.pc);

    snapshot_set_rom_page_out_address(pc - 1); // set the address where the "BIOS" ROM is mapped out and original ROM is mapped in (after reading from this address)

    dsb();

    rom[ROM_ADDR_COMMAND] = COMMAND_SNAP;  // indicate to "BIOS" assembly code that it should load a snapshot

    dsb();
}


