#include "gpu.h"
#include "font.h"

//Set frameskip to 0,1,2,3...
int skipCount = 0;
int linesInterlace = 0;

bool skipFrame = false;
bool skipGPU = false;

bool frameLimit = false

u8 BLEND_MODE;
u8 TEXT_MODE;
u8 Masking;
