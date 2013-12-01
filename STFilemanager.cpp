#include "STFileManager.h"
#include "STFileManagerEntry.h"

const TInt KMaxFileManagerEntries = 10;


CSTFileManager::CSTFileManager(CSTTorrent& aTorrent, RFs& aFs)
 : iTorrent(aTorrent), iFs(aFs)
{

}

CSTFileManager::~CSTFileManager()
{
	iFiles.ResetAndDestroy();
}

CSTFileManager* CSTFileManager::NewLC(CSTTorrent& aTorrent, RFs& aFs)
{
	CSTFileManager* self = new (ELeave) CSTFileManager(aTorrent, aFs);
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
}

CSTFileManager* CSTFileManager::NewL(CSTTorrent& aTorrent, RFs& aFs)
{
	CSTFileManager* self=CSTFileManager::NewLC(aTorrent, aFs);
	CleanupStack::Pop();
	return self;
}

void CSTFileManager::ConstructL()
{
}

CSTFileManagerEntry* CSTFileManager::GetFileManagerEntryL(CSTFile* aFile)
{
	CSTFileManagerEntry* fileEntry = NULL;
	TInt fileEntryIndex = -1;

	for (TInt i=0; i<iFiles.Count(); i++)
	{
		if (&(iFiles[i]->FileId()) == aFile)
		{
			fileEntryIndex = i;
			break;
		}
	}
	
	if (fileEntryIndex < 0)
	{
		fileEntry = AddFileL(aFile);
	}
	else
		fileEntry = iFiles[fileEntryIndex];
	
	return fileEntry;
}


TInt CSTFileManager::WriteL(CSTFile* aFile, TInt aPosition, const TDesC8& aData)
{
	CSTFileManagerEntry* fileEntry = GetFileManagerEntryL(aFile);
	if (fileEntry)
		return fileEntry->Write(aPosition, aData);
	else
		return KErrGeneral;
}


void CSTFileManager::Close(CSTFile* aFile)
{
	TInt fileEntryIndex = -1;

	for (TInt i=0; i<iFiles.Count(); i++)
	{
		if (&(iFiles[i]->FileId()) == aFile)
		{
			fileEntryIndex = i;
			break;
		}
	}
	
	if (fileEntryIndex >= 0)
	{
		delete iFiles[fileEntryIndex];
		iFiles.Remove(fileEntryIndex);
	}
}


void CSTFileManager::CloseAll()
{
	iFiles.ResetAndDestroy();
}


HBufC8* CSTFileManager::ReadL(CSTFile* aFile, TInt aPos, TInt aLength)
{
	CSTFileManagerEntry* fileEntry = GetFileManagerEntryL(aFile);
	if (fileEntry)
		return fileEntry->ReadL(aPos, aLength);
	else
		return NULL;
}


CSTFileManagerEntry* CSTFileManager::AddFileL(CSTFile* aFile)
{
	if (iFiles.Count() >= KMaxFileManagerEntries)
	{
		TInt lastIndex = iFiles.Count() - 1;
		delete iFiles[lastIndex];
		iFiles.Remove(lastIndex);
	}

	CSTFileManagerEntry* file = CSTFileManagerEntry::NewLC(iFs, iTorrent, *aFile);
	if (err)
		return NULL;

	User::LeaveIfError(iFiles.Insert(file, 0));
	CleanupStack::Pop();

	return file;
}
