#ifndef STLOGGER_H
#define STLOGGER_H

#ifdef __WINS__
	#define LOG_TO_FILE
#endif

#include <coemain.h>
#include "STDefs.h"

#define LOG CLogger::InstanceL()

const TUid KUidLoggerSingleton = { 0x10000EF4 };

class CLogger : public CCoeStatic
{
public:

	IMPORT_C static CLogger* InstanceL();

	IMPORT_C void WriteL(const TDesC& aText);

	IMPORT_C void WriteL(const TDesC8& aText);

	IMPORT_C void WriteLineL(const TDesC& aText);

	IMPORT_C void WriteLineL(const TDesC8& aText);

	IMPORT_C void WriteLineL();

	IMPORT_C void WriteL(TInt aInt);

	IMPORT_C void WriteLineL(TInt aInt);

protected:

	CLogger() : CCoeStatic(KUidLoggerSingleton) {}
	
	~CLogger();

	void ConstructL();

private:

	RFs iFsSession;

	RFile iLogFile;
};

#endif
