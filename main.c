#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"



#define __core1_func(x) __scratch_y(__STRING(x)) x
#define __core0_func(x) __scratch_x(__STRING(x)) x

#define RUN_SNAPSHOT_LOADER

#if defined(USE_EMU_Z80)
#include "src/emu_z80/shadow_emu.h"
#elif defined(RUN_LOGIC_ANALYZER)
#include "logic_analyzer.c"
#elif defined(RUN_EMU_BORDER_SCREEN)
#include "emu_border_screen.c"
#elif defined(RUN_SNAPSHOT_LOADER)
#include "snapshot_loader.c"
#endif

int main() {
    // overclock
    volatile uint32_t *qmi_m0_timing=(uint32_t *)0x400d000c;
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_1_50);
    sleep_ms(10);
    *qmi_m0_timing = 0x60007204;
    //bool sucess = set_sys_clock_khz(432 * KHZ, true);
    bool sucess = set_sys_clock_khz(300 * KHZ, true);
    *qmi_m0_timing = 0x60007303;
    if (!sucess) {
        while (1) {
            printf("Failed to set system clock\n");
            sleep_ms(1000);
        }
    }
    

    stdio_init_all();
    printf("Hello, multicore!\n");
    printf("Sys clock: %u Hz\n", clock_get_hz(clk_sys));


    /// \tag::setup_multicore[]

    //multicore_launch_core1(logic_analyzer_core1_entry);

    #if defined(USE_EMU_Z80)
    shadow_emulator();
    #elif defined(RUN_LOGIC_ANALYZER)
    logic_analyzer_entry();
    #elif defined(RUN_EMU_BORDER_SCREEN)
    emu_border_screen();
    #elif defined(RUN_SNAPSHOT_LOADER)
    snapshot_loader();
    #endif

    while (1) {
        // Main core loop
        // You can add your main program logic here
        tight_loop_contents(); // This is a placeholder for the main loop
    }

    /// \end::setup_multicore[]
}
