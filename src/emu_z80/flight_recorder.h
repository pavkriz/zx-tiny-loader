#pragma once
#include <stdint.h>
#include "../yielding_rom_macros.h"

typedef enum {
    FR_NONE,    // No operation
    FR_MEM_WR,  // Memory write
    FR_MEM_RD,  // Memory read
    FR_IO_WR,   // I/O write
    FR_IO_RD,   // I/O read
    FR_FETCH_1, // Instruction fetch, fetching 1st byte of the opcode
    FR_FETCH_2, // Instruction fetch, fetching 2st byte of a prefixed opcode
    FR_FETCH_PARAMS, // Instruction fetch, fetching params of the instruction (eg. immediate value or address)
    FR_HALT_NOP, // NOP instruction (actually a memory read at PC) during HALTed state (not repeated in the log)
    FR_IRQ,     // IRQ entry
} fr_operation_t;

typedef struct {
    fr_operation_t operation;
    uint16_t  addr;
    uint8_t data;
    uint16_t waited_bus_start;
    uint16_t waited_bus_end;
} flight_recorder_rec_t;

#define FLIGHT_RECORDER_SIZE 1024

typedef struct {
    flight_recorder_rec_t records[FLIGHT_RECORDER_SIZE];
    int current_index;
} flight_recorder_t;

// Global flight recorder instance
extern flight_recorder_t flight_recorder;

#define flight_recorder_log_op(op, addr, data) { flight_recorder.records[flight_recorder.current_index] = (flight_recorder_rec_t){op, addr, data, zx_waited_bus_start, zx_waited_bus_end}; flight_recorder.current_index = (flight_recorder.current_index + 1) % FLIGHT_RECORDER_SIZE; }
#define flight_recorder_log_mem_wr(addr, data) { flight_recorder_log_op(FR_MEM_WR, addr, data); }
#define flight_recorder_log_mem_rd(addr, data, is_at_pc) { flight_recorder_log_op((is_at_pc) ? FR_FETCH : FR_MEM_RD, addr, data); }
#define flight_recorder_log_io_wr(addr, data) { flight_recorder_log_op(FR_IO_WR, addr, data); }
#define flight_recorder_log_io_rd(addr, data) { flight_recorder_log_op(FR_IO_RD, addr, data); }
#define flight_recorder_log_irq(addr) { flight_recorder_log_op(FR_IRQ, addr, 0); }
#define flight_recorder_log_halt_nop(addr) { \
    if (flight_recorder.records[(flight_recorder.current_index - 1) % FLIGHT_RECORDER_SIZE].operation != FR_HALT_NOP) { \
        flight_recorder_log_op(FR_HALT_NOP, addr, 0); \
    } \
}

void flight_recorder_dump(void);