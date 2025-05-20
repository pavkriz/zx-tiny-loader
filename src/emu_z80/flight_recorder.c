#include "flight_recorder.h"
#include <stdio.h>

flight_recorder_t flight_recorder;

void flight_recorder_dump(void) {
    int index = flight_recorder.current_index;
    for (int i = 0; i < FLIGHT_RECORDER_SIZE; i++) {
        flight_recorder_rec_t *rec = &flight_recorder.records[index];
        index = (index + 1) % FLIGHT_RECORDER_SIZE;
        if (rec->operation == FR_NONE) {
            continue; // Skip empty records
        }
        switch (rec->operation) {
            case FR_MEM_WR:
                printf("MEM WR:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            case FR_MEM_RD:
                printf("MEM RD:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            case FR_IO_WR:
                printf("IO WR:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            case FR_IO_RD:
                printf("IO RD:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            case FR_FETCH_1:  
                printf("FETCH1:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            case FR_FETCH_2:  
                printf("FETCH2:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            case FR_FETCH_PARAMS:
                printf("FETCHP:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            case FR_IRQ:
                printf("IRQ:\t0x%04X (was PC pushed to SP when IRQ acknowloedged)", rec->addr);
                break;
            case FR_HALT_NOP:
                printf("HALT NOP:\t0x%04X = 0x%02X", rec->addr, rec->data);
                break;
            default:
                printf("UNKNOWN OPERATION: %d", rec->operation);
                break;
        }
        printf("\tws=%d\twe=%d\n", rec->waited_bus_start, rec->waited_bus_end);
    }
}