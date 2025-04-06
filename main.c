#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"


#define __core1_func(x) __scratch_y(__STRING(x)) x
#define __core0_func(x) __scratch_x(__STRING(x)) x



#ifdef RUN_LOGIC_ANALYZER
#include "logic_analyzer.c"
#else
#include "emu.c"
#endif

int main() {
    // curretly overclocking is actually not needed, the test program (border lines) runs at 150MHz perfectly
    bool success = set_sys_clock_khz(300000, true);
    if (!success) {
        // Failed to configure, fallback or halt
        while (true) {
            printf("Overclock failed!\n");
            sleep_ms(1000);
        }
    }


    stdio_init_all();
    printf("Hello, multicore!\n");
    printf("Sys clock: %u Hz\n", clock_get_hz(clk_sys));


    /// \tag::setup_multicore[]

    //multicore_launch_core1(logic_analyzer_core1_entry);

    #ifdef RUN_LOGIC_ANALYZER
    logic_analyzer_entry();
    #else
    emu_prog_border();
    #endif

    while (1) {
        // Main core loop
        // You can add your main program logic here
        tight_loop_contents(); // This is a placeholder for the main loop
    }

    /// \end::setup_multicore[]
}
