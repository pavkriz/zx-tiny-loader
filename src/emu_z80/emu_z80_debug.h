#pragma once

#include "../global.h"


void FASTCODE NOFLASH(printMemoryM1ReadCounter)();
void FASTCODE NOFLASH(resetMemoryM1ReadSinceIrqCounter)();
//void FASTCODE NOFLASH(EmuDebugHookPreM1)(sZ80* z80cpu);