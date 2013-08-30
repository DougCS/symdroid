bool bHardwareTransform;
bool bTrueColor;

bool bBufferedRendering;
bool bFrameSkip;
int iSkipCount;

bool bJit;
bool bFastMemory;

bool bSaveSettings;

//TODO: Implement all features below or remove them from config.cpp

class Config {
	void Load(const char *iniFileName = "symdroid.ini");
	void Save();
	const char *iniFilename_;
};
int LOADER=1;
void INFO_LOG(int type,const char *arg1, const char *arg2="");
void ERROR_LOG(int type,const char *arg1, const char *arg2);

class IniFile {
	class Section {
		bool Get(const char* key, bool* value, bool defaultValue = false);
		bool Get(const char* key, int* value, int defaultValue = 0);
		void Set(const char* key, bool newValue);
	};
public:
	bool Load(const char *filename);
	bool Save(const char* filename);
	Section* GetOrCreateSection(const char* section);
};
