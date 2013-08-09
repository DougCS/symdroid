unsigned short opcode;
#include "opcodes.h"
//Symdroid will use 90mb ram
unsigned char memory[7550008];
//The screen size will 640X360 pixels
unsigned char gfx[640 * 360];
//CPU overclock is not good to use because it will need more battery energy and batter cooling in use of it.It's better to be turned of but if you want you can turn it on ON YOUR OWN RISK
bool CPUoverclock = flase;
int CPUoverclockrate = 667//in mhz

Bitu CPU_ArchitectureType = CPU_ARCHTYPE_MIXED;

void CPU_core_Full_init(void);
void CPU_core_Normal_init(void);
void CPU_core_Simple_init(void);
