#include "hardware/gpio.h"

#include "pinmap.h"
#include "test_snapshot_z80_hate.h"
#include "test_snapshot_ram_hate.h"


// ROM 0x0038 addr = IM1 IRQ ISR
// ROM 0x0066 addr = NMI ISR

// this is where we make Z80 thinks he jumps from time to time to not increment PC too much 
// to not reach RAM address region (but actually we do not take ADDRESS bus into account at all when yilding instructions)
#define SAVE_ROM_ADDR_FOR_LOOPS 0x0100  
// yield operations wait for CPU for memory or IO read and put the data to DATA bus (regardless of actual ADDRESS bus state)
static inline void yield_mem(uint32_t n) {
    while (gpio_get(PIN_NUMBER_RD) == 1) { }    /* wait for RD to go low (indicating a read operation) */ \
    gpio_put_masked(PIN_BITS_DATA, n);          /* put data to DATA bus */ \
    gpio_set_dir_out_masked(PIN_BITS_DATA);     /* set DATA bus to output */ \
    while (gpio_get(PIN_NUMBER_RD) == 0) { }    /* wait for RD to go high (indicating the end of read operation) */ \
    gpio_set_dir_in_masked(PIN_BITS_DATA);      /* set DATA bus to input (nout output) */
}
#define yield_m1(n) yield_mem(n); 
#define yield_ld_a_n(n) { yield_m1(0x3E); yield_mem(n); }
#define yield_out_n_a(n) { yield_m1(0xD3); yield_mem(n); }
#define yield_jp_nn(nn) { yield_m1(0xC3); yield_mem(nn); yield_mem(nn >> 8); }
#define yield_ld_hl_nn(nn) { yield_m1(0x21); yield_mem(nn); yield_mem(nn >> 8); }
#define yield_ld_ref_hl_n(n) { yield_m1(0x36); yield_mem(n); }
#define yield_di() yield_m1(0xF3);
#define yield_nop() yield_m1(0x00);
#define yield_dummy_jump() {yield_jp_nn(SAVE_ROM_ADDR_FOR_LOOPS); emu_z80_pc = SAVE_ROM_ADDR_FOR_LOOPS;}
#define yield_jump(label) {yield_dummy_jump(); goto label;}
#define ZX_ULA_IO_PORT 0xFE
#define ZX_COLOR_BLACK 0
#define ZX_COLOR_BLUE 1
#define ZX_COLOR_RED 2

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

snap_state_t snap_state;
uint16_t emu_z80_pc = 0;  // this program counter is actually not used in the code
uint8_t z80mc[100];  // buffer for Z80 snapshot bootstrap machine code

int create_bootstrap_machine_code(snap_state_t *snap_state);
void read_snapshot_z80_header(uint8_t *buffer, snap_state_t *snap_state, bool *compressed);

void __core0_func(snapshot_loader)() {
    // decode z80 snapshot header
    bool compressed = false;
    read_snapshot_z80_header(test_snapshot_z80, &snap_state, &compressed);
    // create bootstrap machine code
    int mc_code_size = create_bootstrap_machine_code(&snap_state);

    for (int i = 0; i < 32; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_disable_pulls(i);
    }
    // except /ROMCS and /RESET which will be output
    gpio_init(PIN_NUMBER_ROMCS);
    gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_OUT);
    gpio_init(PIN_NUMBER_RESET);
    gpio_set_dir(PIN_NUMBER_RESET, GPIO_OUT);
    // keep /ROMCS high to disable the internal ROM
    gpio_put(PIN_NUMBER_ROMCS, 1);
    // keep /RESET low for a while to reset the CPU
    gpio_put(PIN_NUMBER_RESET, 0);
    busy_wait_ms(10); // wait for 10ms
    // set /RESET high to let the CPU run
    gpio_put(PIN_NUMBER_RESET, 1);

    
    z80_reset:
        emu_z80_pc = 0;
        // assume the RESET has been deactivated (is high now to let CPU run) and CPU starts from 0x0000
        yield_di();  // disable interrupts to avoid CPU jumping to 0x0038 on its own     
    
        // clear screen (test)
        for (int i = 0; i < 6912; i++) {
            yield_ld_hl_nn(i+16384);
            yield_ld_ref_hl_n(0xFF);
            yield_dummy_jump();  // dummy jump to avoid the CPU to increment PC too much
        }
        // load whole decompressed RAM
        for (int i = 0; i < 3*16*1024; i++) {
            yield_ld_hl_nn(i+16384);
            yield_ld_ref_hl_n(test_snapshot_ram[i]);
            yield_dummy_jump();  // dummy jump to avoid the CPU to increment PC too much
        }
        // rum z80mc code
        for (int i = 0; i < mc_code_size; i++) {
            yield_mem(z80mc[i]);
        }
        // allow mapping ROM back to the CPU (ULA will control ROMCS now)
        gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_IN);
        // infinite loop, not to intefere with the CPU
        while (1) {
            tight_loop_contents();
        }

}

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

int create_bootstrap_machine_code(snap_state_t *snap_state)
{
    // TODO restore border from out_ula
  int pc=0;
  z80mc[pc++] = 0xF3; /* DI */

  z80mc[pc++] = 0x3E; /* ld a, NN */
  z80mc[pc++] = snap_state->i;
  z80mc[pc++] = 0xED; /* ld i, a */
  z80mc[pc++] = 0x47;
  z80mc[pc++] = 0x3E; /* ld a, NN */
  z80mc[pc++] = snap_state->r;
  z80mc[pc++] = 0xED; /* ld r, a */
  z80mc[pc++] = 0x4F;
  
  z80mc[pc++] = 0x21; /* ld hl, NNNN */
  z80mc[pc++] = snap_state->f_;
  z80mc[pc++] = snap_state->a_;

  z80mc[pc++] = 0xE5; /* push hl */
  z80mc[pc++] = 0xF1; /* pop af */
  
  z80mc[pc++] = 0x01; /* ld bc, NNNN */
  z80mc[pc++] = lowByte(snap_state->bc_);
  z80mc[pc++] = highByte(snap_state->bc_);

  z80mc[pc++] = 0x11; /* ld de, NNNN */
  z80mc[pc++] = lowByte(snap_state->de_);
  z80mc[pc++] = highByte(snap_state->de_);

  z80mc[pc++] = 0x21; /* ld hl, NNNN */
  z80mc[pc++] = lowByte(snap_state->hl_);
  z80mc[pc++] = highByte(snap_state->hl_);

  z80mc[pc++] = 0xD9; /* exx */
  z80mc[pc++] = 0x08; /* ex af, af' */

  z80mc[pc++] = 0x21; /* ld hl, NNNN */
  z80mc[pc++] = snap_state->f;
  z80mc[pc++] = snap_state->a;

  z80mc[pc++] = 0xE5; /* push hl */
  z80mc[pc++] = 0xF1; /* pop af */
  
  z80mc[pc++] = 0x01; /* ld bc, NNNN */
  z80mc[pc++] = lowByte(snap_state->bc);
  z80mc[pc++] = highByte(snap_state->bc);

  z80mc[pc++] = 0x11; /* ld de, NNNN */
  z80mc[pc++] = lowByte(snap_state->de);
  z80mc[pc++] = highByte(snap_state->de);

  z80mc[pc++] = 0x21; /* ld hl, NNNN */
  z80mc[pc++] = lowByte(snap_state->hl);
  z80mc[pc++] = highByte(snap_state->hl);

  z80mc[pc++] = 0xDD; /* ld ix, NNNN */
  z80mc[pc++] = 0x21;
  z80mc[pc++] = lowByte(snap_state->ix);
  z80mc[pc++] = highByte(snap_state->ix);

  z80mc[pc++] = 0xFD; /* ld iy, NNNN */
  z80mc[pc++] = 0x21;
  z80mc[pc++] = lowByte(snap_state->iy);
  z80mc[pc++] = highByte(snap_state->iy);

  switch(snap_state->im){
  case 0:
    z80mc[pc++] = 0xED; /* IM 0 */
    z80mc[pc++] = 0x46;
    break;
  case 1:
    z80mc[pc++] = 0xED; /* IM 1 */
    z80mc[pc++] = 0x56;
    break;
  case 2:
    z80mc[pc++] = 0xED; /* IM 2 */
    z80mc[pc++] = 0x5E;
    break;
  }
  
  z80mc[pc++] = 0x31; /* ld sp, NNNN */
  z80mc[pc++] = lowByte(snap_state->sp);
  z80mc[pc++] = highByte(snap_state->sp);

  switch(snap_state->iff1){
  case 0:
    z80mc[pc++] = 0xF3; /* DI */
    break;
  case 1:
    z80mc[pc++] = 0xFB; /* EI */
    break;
  }

  z80mc[pc++] = 0xC3; /* jp NNNN */
  z80mc[pc++] = lowByte(snap_state->pc);
  z80mc[pc++] = highByte(snap_state->pc);

  return pc;
}