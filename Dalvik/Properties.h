#ifndef PROPERTIES
#define PROPERTIES

bool dvmPropertiesStartup(int maxProps);
void dvmPropertiesShutdown(void);

bool dvmAddCommandLineProperty(const char* argStr);

void dvmCreateDefaultProperties(Object* propObj);
void dvmSetCommandLineProperties(Object* propObj);

char* dvmGetProperty(const char* key);

#endif
