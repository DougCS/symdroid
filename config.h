bool bEnableSound;
int iSFXVolume;
int iBGMVolume;

bool bHardwareTransform;
bool bTrueColor

bool bBufferedRendering
bool bFrameSkip
int iSkipCount

bool bJit;
bool bFastMemory;

bool bSaveSettings;

void Load(const char *iniFileName = "symdroid.ini");
void Save();
