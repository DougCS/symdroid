#ifndef STFILEMANAGERENTRY_H
#define STFILEMANAGERENTRY_H

#include <f32file.h>
#include "STDefs.h"

class CSTFile;
class CSTData;

class CSTFileManagerEntry : public CBase
{
public:

	~CSTFileManagerEntry();

	static CSTFileManagerEntry* NewL(RFs& aFs, CSTData& aData, CSTFile& aFile);

	static CSTFileManagerEntry* NewLC(RFs& aFs, CSTData& aData, CSTFile& aFile);

	inline CSTFile& FileId() const;

	TInt Write(TInt aPosition, const TDesC8& aData);
	
	HBufC8* ReadL(TInt aPos, TInt aLength);


private:

	CSTFileManagerEntry(CSTFile& aFile);

	void ConstructL(RFs& aFs, CSTData& aData);

private:

	RFile iFileHandle;

	CSTFile& iFile;

};


inline CSTFile& CSTFileManagerEntry::FileId() const {
	return iFile;
}

#endif
