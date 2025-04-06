#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/structs/systick.h"
#include "pinmap.h"


#define BUFFER_SIZE 1024*2  // 2k samples = (4+8)*2k bytes
#define TRIGGER_PATTERN PIN_BIT_RESET  // Pattern to wait for before starting
#define TRIGGER_MASK PIN_BIT_RESET // Mask RESET pin only
#define PINS_MASK (PIN_BITS_DATA | PIN_BIT_RESET | PIN_BIT_RD | PIN_BIT_M1 ) // Mask for all pins we are interested in

typedef struct {
    uint32_t value;
    uint32_t systicks;
    uint32_t timestamp;
} Sample;

volatile Sample buffer[BUFFER_SIZE];
volatile uint32_t buffer_index = 0;
volatile bool logging_active = false;

//void static inline __not_in_flash_func(logic_analyzer_entry)() {    // Initialize all GPIOs as inputs
void __core1_func(logic_analyzer_entry)() {    // Initialize all GPIOs as inputs
    printf("Logic Analyzer started\n");
    for (int i = 0; i < NUM_IRQS; i++) {
        if (irq_is_enabled(i)) {
            printf("IRQ %d is enabled\n", i);
        }
    }

    for (int i = 0; i < 32; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_disable_pulls(i);
    }
    // enable pull-up on /RESET pin to avoid false low values
    gpio_pull_up(PIN_NUMBER_RESET);
    // enable pull-up on /RD pin to avoid false low values
    gpio_pull_up(PIN_NUMBER_RD);
    

    // Configure systick with maximum reload value and enable it
    systick_hw->rvr = 0xFFFFFF; // Set reload value to maximum (24 bits)
    systick_hw->cvr = 0;        // Clear current value
    systick_hw->csr = 0x5;      // Enable systick timer, source = cpu clk
    
    // Wait a bit to ensure the timer has started counting
    busy_wait_us(10);
    
    // test systick
    __dmb(); // Ensure all previous memory accesses are complete
    uint32_t systick_values[10];
    for (int i = 0; i < 10; i++) {
        systick_values[i] = systick_hw->cvr;
        //__dmb(); // Ensure all previous memory accesses are complete
    }
    printf("Systick values:\n");
    for (int i = 0; i < 10; i++) {
        printf("%u\n", systick_values[i]);
    }

    uint32_t last_value = gpio_get_all() & PINS_MASK;

    // Wait for the trigger pattern
    printf("Waiting for trigger pattern 0x%04x...\n", TRIGGER_PATTERN);
    while (true) {
        uint32_t current_value = gpio_get_all() & TRIGGER_MASK;
        
        // Check if the trigger pattern is detected
        if ((current_value) == TRIGGER_PATTERN) {
            //printf("!\n");
            logging_active = true;
            break;
        }
    }

    while (true) {
        uint32_t current_value = gpio_get_all() & PINS_MASK;
        if (current_value != last_value) {
            if (buffer_index < BUFFER_SIZE) {
                buffer[buffer_index].value = current_value;
                buffer[buffer_index].systicks = systick_hw->cvr; // Get current systick value (only 24bits are valid)
                buffer[buffer_index].timestamp = time_us_32(); // Get timestamp in microseconds
                buffer_index++;
            } else if (logging_active) {
                printf("Buffer full! Captured %d samples\n", BUFFER_SIZE);
                logging_active = false;  // Stop logging when buffer is full
                
                double prev_systick = 0;

                // Dump the entire buffer
                printf("Timestamp(us)\tSysticks\tDiff[ns]\tValue\tFlags\n");
                for (int i = 0; i < BUFFER_SIZE; i++) {
                    double idiff = (i < BUFFER_SIZE - 1) ? (buffer[i].systicks - buffer[i+1].systicks) : 0;
                    double time_diff = idiff * 3.333333f; // Convert to nanoseconds
                    printf("%10lu\t0x%08x\t%.1f\t0x%08x\t", buffer[i].timestamp, buffer[i].systicks, time_diff, buffer[i].value);
                    if (!(buffer[i].value & PIN_BIT_RESET)) {
                        printf("RESET ");
                    }
                    if (!(buffer[i].value & PIN_BIT_RD)) {
                        printf("RD ");
                    }
                    if (!(buffer[i].value & PIN_BIT_M1)) {
                        printf("M1 ");
                    }
                    printf("\n");
                }
                
                printf("Buffer dump complete\n");
            }
            last_value = current_value;
        }
    }
}
