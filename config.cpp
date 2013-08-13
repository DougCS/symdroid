#include "config.h"

IniFile::Section *cpu = iniFile.GetOrCreateSection("CPU");
cpu->Set("Jit", bJit);
cpu->Set("FastMemory", bFastMemory);
