#include "config.h"

Config::Config() { }
Config::~Config() { }

void Config::Load(const char *iniFileName)
{
    iniFilename_ = iniFileName;
    INFO_LOG(LOADER, "Loading config: %s", iniFileName);
    bSaveSettings = true;

    IniFile iniFile;
    if (!iniFile.Load(iniFileName)) {
    ERROR_LOG(LOADER, "Failed to read %s. Setting config to default.", iniFileName);
    }

    IniFile::Section *cpu = iniFile.GetOrCreateSection("CPU");
    cpu->Get("Jit", &bJit, true);
    cpu->Get("FastMemory", &bFastMemory, false);

    graphics->Get("HardwareTransform", &bHardwareTransform, true);

IniFile::Section *sound = iniFile.GetOrCreateSection("Sound");
sound->Get("Enable", &bEnableSound, false);
sound->Get("VolumeBGM", &iBGMVolume, 4);
sound->Get("VolumeSFX", &iSFXVolume, 4);

void Config::Save()
{
    if (iniFilename_.size() && g_Config.bSaveSettings) {
        CleanRecent();
        IniFile iniFile;
        if (!iniFile.Load(iniFilename_.c_str())) {
            ERROR_LOG(LOADER, "Error saving config - can't read ini %s", iniFilename_.c_str());
        }

    IniFile::Section *cpu = iniFile.GetOrCreateSection("CPU");
    cpu->Set("Jit", bJit);
    cpu->Set("FastMemory", bFastMemory);

    IniFile::Section *sound = iniFile.GetOrCreateSection("Sound");
    sound->Set("Enable", bEnableSound);
    sound->Set("VolumeBGM", iBGMVolume);
    sound->Set("VolumeSFX", iSFXVolume);

    if (!iniFile.Save(iniFilename_.c_str())) {
        ERROR_LOG(LOADER, "Error saving config - can't write ini %s", iniFilename_.c_str());
        return;
    }
    INFO_LOG(LOADER, "Config saved: %s", iniFilename_.c_str());
    } else {
    INFO_LOG(LOADER, "Not saving config");
    }
}
