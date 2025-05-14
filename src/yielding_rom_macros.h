#pragma once

#include "../pinmap.h"
#include "hardware/gpio.h"
#include <stdint.h>
#include "global.h"
#include <Z80.h>

// ROM 0x0038 addr = IM1 IRQ ISR
// ROM 0x0066 addr = NMI ISR

static uint16_t zx_waited_bus_start = 0;
static uint16_t zx_waited_bus_end = 0;

#define zx_rom_disable() { gpio_put(PIN_NUMBER_ROMCS, 1); gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_OUT); } // disable internal ZX ROM
#define zx_rom_enable() { gpio_set_dir(PIN_NUMBER_ROMCS, GPIO_IN); } // enable internal ZX ROM

static INLINE FASTCODE uint8_t sniff_mem_wr() {
    zx_waited_bus_start = 0;
    while (gpio_get(PIN_NUMBER_WR) != 0) { zx_waited_bus_start++; }    /* wait for WR to go low (indicating a write operation) */
    uint8_t val = gpio_get_all() & PIN_BITS_DATA; // read data from DATA bus
    zx_waited_bus_end = 0;
    while (gpio_get(PIN_NUMBER_WR) == 0) { zx_waited_bus_end++; }    /* wait for WR to go high (indicating the end of write operation) */
    return val;
}

static INLINE FASTCODE int16_t sniff_mem_rd() {
    zx_waited_bus_start = 0;
    while (gpio_get(PIN_NUMBER_RD) != 0) { zx_waited_bus_start++; }    /* wait for RD to go low (indicating a read operation) */
    uint8_t val = 0;
    uint8_t val_prev = 0;
    zx_waited_bus_end = 0;
    do {
        val_prev = val;
        val = gpio_get_all() & PIN_BITS_DATA; // read data from DATA bus
        zx_waited_bus_end++;
    } while (gpio_get(PIN_NUMBER_RD) == 0);    /* wait for RD to go high (indicating the end of read operation) */
    return val_prev;
}


static INLINE FASTCODE uint32_t zx_bus_event_wait() {
    uint32_t control_pins = 0;
    zx_waited_bus_start = 0;
    // wait for either RD or IORQ to go low (indicating currently a memory read operation or IRQ acknowledge)
    while ((control_pins = (gpio_get_all() & (PIN_BIT_WR | PIN_BIT_RD | PIN_BIT_IORQ)))  == (PIN_BIT_WR | PIN_BIT_RD | PIN_BIT_IORQ)) { zx_waited_bus_start++; }
    // invert the control pins to return more intuitive value
    control_pins = ~control_pins;
    return control_pins;
}

static INLINE FASTCODE uint8_t zx_bus_get_data() {
    uint8_t val = 0;
    uint8_t val_prev = 0;
    zx_waited_bus_end = 0;
    do {
        val_prev = val;
        val = gpio_get_all() & PIN_BITS_DATA; // read data from DATA bus
        zx_waited_bus_end++;
    } while (gpio_get(PIN_NUMBER_RD) == 0);    /* wait for RD to go high (indicating the end of read operation) */
    return val_prev;
}

static INLINE FASTCODE void zx_bus_yield_data(uint8_t data) {
    zx_rom_disable(); // make sure the internal ROM is disabled
    gpio_put_masked(PIN_BITS_DATA, data);          /* prepare data to DATA bus output buffer */
    gpio_set_dir_out_masked(PIN_BITS_DATA);     /* set DATA bus to output mode to provide emulated data to real Z80 */
    zx_waited_bus_end = 0;
    while (gpio_get(PIN_NUMBER_RD) == 0) { zx_waited_bus_end++; }    /* wait for RD to go high (indicating the end of read operation) */
    gpio_set_dir_in_masked(PIN_BITS_DATA);      /* set DATA bus to input mode to leave DATA bus in high impedance state */
    zx_rom_enable(); // enable the internal ROM again
}


static INLINE FASTCODE uint8_t sniff_io_rd() {
    zx_waited_bus_start = 0;
    while (gpio_get(PIN_NUMBER_RD) != 0) { zx_waited_bus_start++; }    /* wait for RD to go low (indicating a read operation) */
    //wait_z80_cycles(2.2); // wait for IO to respond, there is extra WAIT state in IO read
    // instead of waiting, read the DATA bus until RD goes high
    uint8_t val = 0;
    uint8_t val_prev = 0;
    zx_waited_bus_end = 0;
    do {
        val_prev = val;
        val = gpio_get_all() & PIN_BITS_DATA; // read data from DATA bus
        zx_waited_bus_end++;
    } while (gpio_get(PIN_NUMBER_RD) == 0);    /* wait for RD to go high (indicating the end of read operation) */
    //while (gpio_get(PIN_NUMBER_RD) == 0) { }    /* wait for RD to go high (indicating the end of read operation) */
    return val_prev;
}

static INLINE FASTCODE uint8_t sniff_io_wr() {
     uint32_t control_pins = 0;
    // wait for both WR and IORQ to go low
    zx_waited_bus_start = 0;
    while ((control_pins = (gpio_get_all() & (PIN_BIT_WR | PIN_BIT_IORQ)))  != 0) { zx_waited_bus_start++; }
    //wait_z80_cycles(1); // wait for RAM to respond
    // instead of waiting, read the DATA bus until RD goes high
    uint32_t val = 0;
    uint32_t val_prev = 0;
    zx_waited_bus_end = 0;
    do {
        val_prev = val;
        val = gpio_get_all() & PIN_BITS_DATA; // read data from DATA bus
        zx_waited_bus_end++;
    // wait for both RD anf IORQ to go high (indicating the end of the operation)
    } while ((control_pins = (gpio_get_all() & (PIN_BIT_WR | PIN_BIT_IORQ)))  != (PIN_BIT_WR | PIN_BIT_IORQ));
    return val_prev;
}