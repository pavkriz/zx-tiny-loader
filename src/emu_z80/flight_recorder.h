#pragma once
#include <stdint.h>

typedef enum {
    FR_NONE,    // No operation
    FR_MEM_WR,  // Memory write
    FR_MEM_RD,  // Memory read
    FR_IO_WR,   // I/O write
    FR_IO_RD,   // I/O read
    FR_FETCH,   // Instruction fetch
    FR_FETCH_1, // Instruction fetch, fetching 1st byte of the opcode
    FR_IRQ,     // IRQ entry
} fr_operation_t;

typedef struct {
    fr_operation_t operation;
    uint16_t  addr;
    uint8_t data;
} flight_recorder_rec_t;

#define FLIGHT_RECORDER_SIZE 1024

typedef struct {
    flight_recorder_rec_t records[FLIGHT_RECORDER_SIZE];
    int current_index;
} flight_recorder_t;

// Global flight recorder instance
extern flight_recorder_t flight_recorder;

#define flight_recorder_log_op(op, addr, data) { flight_recorder.records[flight_recorder.current_index] = (flight_recorder_rec_t){op, addr, data}; flight_recorder.current_index = (flight_recorder.current_index + 1) % FLIGHT_RECORDER_SIZE; }
#define flight_recorder_log_mem_wr(addr, data) { flight_recorder_log_op(FR_MEM_WR, addr, data); }
#define flight_recorder_log_mem_rd(addr, data, is_at_pc) { flight_recorder_log_op((is_at_pc) ? FR_FETCH : FR_MEM_RD, addr, data); }
#define flight_recorder_log_io_wr(addr, data) { flight_recorder_log_op(FR_IO_WR, addr, data); }
#define flight_recorder_log_io_rd(addr, data) { flight_recorder_log_op(FR_IO_RD, addr, data); }
#define flight_recorder_log_irq(addr) { flight_recorder_log_op(FR_IRQ, addr, 0); }

void flight_recorder_dump(void);