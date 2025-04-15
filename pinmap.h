#ifndef PINMAP_H
#define PINMAP_H
// pin mapping

// Bits for DATA bus pins
#define PIN_BITS_DATA 255
#define PIN_NUMBER_RESET 8
#define PIN_BIT_RESET (1 << PIN_NUMBER_RESET) // Bit for /RESET pin
#define PIN_NUMBER_RD 9
#define PIN_BIT_RD (1 << PIN_NUMBER_RD) // Bit for /RD pin
#define PIN_NUMBER_ROMCS 10
#define PIN_BIT_ROMCS (1 << PIN_NUMBER_ROMCS) // Bit for ROMCS pin
#define PIN_NUMBER_M1 11
#define PIN_BIT_M1 (1 << PIN_NUMBER_M1) // Bit for M1 pin
#define PIN_NUMBER_WR 12
#define PIN_BIT_WR (1 << PIN_NUMBER_WR) // Bit for WR pin

#endif