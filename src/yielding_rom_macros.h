#pragma once

#include "../pinmap.h"
#include "hardware/gpio.h"
#include "emu_z80/picolib_scaffold.h"

// ROM 0x0038 addr = IM1 IRQ ISR
// ROM 0x0066 addr = NMI ISR

// this is where we make Z80 thinks he jumps from time to time to not increment PC too much 
// to not reach RAM address region (but actually we do not take ADDRESS bus into account at all when yilding instructions)
#define SAVE_ROM_ADDR_FOR_LOOPS 0x0100  
// yield operations wait for CPU for memory or IO read and put the data to DATA bus (regardless of actual ADDRESS bus state)
static INLINE void yield_mem(uint32_t n) {
    while (gpio_get(PIN_NUMBER_RD) != 0) { }    /* wait for RD to go low (indicating a read operation) */ \
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
#define yield_call(nn) {yield_m1(0xCD); yield_mem(nn); yield_mem(nn >> 8);}
#define yield_push_af() {yield_m1(0xF5);}
#define yield_push_bc() {yield_m1(0xC5);}
#define yield_push_de() {yield_m1(0xD5);}
#define yield_push_hl() {yield_m1(0xE5);}

static INLINE uint8_t sniff_mem_wr() {
    while (gpio_get(PIN_NUMBER_WR) != 0) { }    /* wait for WR to go low (indicating a write operation) */ \
    uint8_t val = gpio_get_all() & PIN_BITS_DATA; // read data from DATA bus
    while (gpio_get(PIN_NUMBER_WR) == 0) { }    /* wait for WR to go high (indicating the end of write operation) */ \
    return val;
}