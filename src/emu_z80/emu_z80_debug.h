#pragma once

#include "picolib_scaffold.h"
#include "picolib_emu/emu.h"
#include "picolib_emu/emu_z80.h"


void FASTCODE NOFLASH(printMemoryM1ReadCounter)();
void FASTCODE NOFLASH(EmuDebugHookPreM1)(sZ80* z80cpu);