#pragma once

#include <stdint.h>

// define structs, macros etc. required by emu code

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed long int s32;
typedef unsigned long int u32;
typedef signed long long int s64;
typedef unsigned long long int u64;

// compile-time check
#ifdef __cplusplus
#define STATIC_ASSERT(c, msg) static_assert((c), msg)
#else
#define STATIC_ASSERT(c, msg) _Static_assert((c), msg)
#endif

// NULL
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

// request to use inline
#define INLINE __attribute__((always_inline)) inline

// avoid to use inline
#define NOINLINE __attribute__((noinline))

// place time critical function into RAM
#define NOFLASH(fnc) NOINLINE __attribute__((section(".time_critical." #fnc))) fnc

// fast function optimization
#define FASTCODE __attribute__ ((optimize("-Ofast")))

#define	B0 (1<<0)
#define	B1 (1<<1)
#define	B2 (1<<2)
#define	B3 (1<<3)
#define	B4 (1<<4)
#define	B5 (1<<5)
#define	B6 (1<<6)
#define	B7 (1<<7)
#define	B8 (1U<<8)
#define	B9 (1U<<9)
#define	B10 (1U<<10)
#define	B11 (1U<<11)
#define	B12 (1U<<12)
#define	B13 (1U<<13)
#define	B14 (1U<<14)
#define	B15 (1U<<15)
#define B16 (1UL<<16)
#define B17 (1UL<<17)
#define B18 (1UL<<18)
#define	B19 (1UL<<19)
#define B20 (1UL<<20)
#define B21 (1UL<<21)
#define B22 (1UL<<22)
#define B23 (1UL<<23)
#define B24 (1UL<<24)
#define B25 (1UL<<25)
#define B26 (1UL<<26)
#define B27 (1UL<<27)
#define B28 (1UL<<28)
#define B29 (1UL<<29)
#define B30 (1UL<<30)
#define B31 (1UL<<31)

#define BIT(pos) (1UL<<(pos))
#define BIT64(pos) (1ULL<<(pos))

typedef unsigned char Bool;
#define True 1
#define False 0

// some dummy implementations that do nothing (make sure we do not missed anything)
#define EmuInterSet(a,b) 0
#define PWM_Reset(a) 0
#define PWM_Clock(a,b) 0
#define PWM_Enable(a) 0
#define PWM_CTR(a) 0
#define PWM_Top(a,b) 0
#define PWM_GetClock(a) 0

// TODO these should do something but now do nothing
#define cb() 0
#define dsb() 0
// for now, we do not use the second core of RP2040 and run the emulator on core 0
#define Core1Exec(a) a()
#define Core1Reset() 0
