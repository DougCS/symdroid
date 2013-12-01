#ifndef STFILEMANAGER_H
#define STFILEMANAGER_H

#include <e32std.h>
#include <e32base.h>
#include <f32file.h>
#include "STDefs.h"

class CSTFileManagerEntry;
class CSTTorrent;
class CSTFile;

class CSTFileManager : public CBase
{
public:

	~CSTFileManager();

	static CSTFileManager* NewL(CSTTorrent& aTorrent, RFs& aFs);

	static CSTFileManager* NewLC(CSTTorrent& aTorrent, RFs& aFs);
	
	TInt WriteL(CSTFile* aFile, TInt aPosition, const TDesC8& aData);

	TInt ReadL(CSTFile* aFile, TInt aPosition, TInt aLength, TDes8& aBuffer);

	HBufC8* ReadL(CSTFile* aFile, TInt aPos, TInt aLength);

	void Close(CSTFile* aFile);

	void CloseAll();

private:

	CSTFileManager(CSTTorrent& aTorrent, RFs& aFs);

	void ConstructL();

private:

	CSTFileManagerEntry* AddFileL(CSTFile* aFile);
	
	CSTFileManagerEntry* GetFileManagerEntryL(CSTFile* aFile);

private:

	RFs& iFs;

	CSTTorrent& iTorrent;

	RPointerArray<CSTFileManagerEntry> iFiles;

};

#endif
