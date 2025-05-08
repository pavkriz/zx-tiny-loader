#pragma once

// request to use inline
#define INLINE __attribute__((always_inline)) inline

// avoid to use inline
#define NOINLINE __attribute__((noinline))

// place time critical function into RAM
#define NOFLASH(fnc) NOINLINE __attribute__((section(".time_critical." #fnc))) fnc

#define NOFLASH_CONST(fnc) __attribute__((section(".time_critical." #fnc))) fnc

// fast function optimization
#define FASTCODE __attribute__ ((optimize("-Ofast")))

// data synchronization barrier
INLINE void dsb(void)
{
#if RISCV
	__asm volatile (" fence rw, rw\n" ::: "memory");
#else // ARM
	__asm volatile (" dsb\n" ::: "memory");
#endif
}