#include "config.h";

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

    //graphics->Get("HardwareTransform", &bHardwareTransform, true);

}

void Config::Save()
{
    if (iniFilename_) { //TODO: && g_Config.bSaveSettings
        //TODO: CleanRecent();
        IniFile iniFile;
        if (!iniFile.Load(iniFilename_)) {
            ERROR_LOG(LOADER, "Error saving config - can't read ini %s", iniFilename_);
        }

    IniFile::Section *cpu = iniFile.GetOrCreateSection("CPU");
    cpu->Set("Jit", bJit);
    cpu->Set("FastMemory", bFastMemory);

    IniFile::Section *sound = iniFile.GetOrCreateSection("Sound");
    sound->Set("Enable", bEnableSound);
    sound->Set("VolumeBGM", iBGMVolume);
    sound->Set("VolumeSFX", iSFXVolume);

    if (!iniFile.Save(iniFilename_)) {
        ERROR_LOG(LOADER, "Error saving config - can't write ini %s", iniFilename_);
        return;
    }
    INFO_LOG(LOADER, "Config saved: %s", iniFilename_);
    } else {
    INFO_LOG(LOADER, "Not saving config");
    }
}
