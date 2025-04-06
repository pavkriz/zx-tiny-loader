#include "hardware/gpio.h"

#include "pinmap.h"


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

uint16_t emu_z80_pc = 0;  // this program counter is actually not used in the code

void __core0_func(emu_prog_border)() {
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
    clear_screen:
        for (int i = 16384; i < 23296; i++) {
            yield_ld_hl_nn(i);
            yield_ld_ref_hl_n(0xFF);
            yield_dummy_jump();  // dummy jump to avoid the CPU to increment PC too much
        }
    loop:
        // Set the border color to black
        yield_ld_a_n(ZX_COLOR_BLACK);  
        yield_out_n_a(ZX_ULA_IO_PORT);
        for (int i = 0; i < 500; i++) {
            yield_nop();
            yield_dummy_jump();  // dummy jump to avoid the CPU to increment PC too much
        }
        // Set the border color to red
        yield_ld_a_n(ZX_COLOR_RED);
        yield_out_n_a(ZX_ULA_IO_PORT);
        for (int i = 0; i < 500; i++) {
            yield_nop();
            yield_dummy_jump();  // dummy jump to avoid the CPU to increment PC too much
        }
        // loop
        yield_jump(loop);
}