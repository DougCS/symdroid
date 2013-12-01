#include "STFileManagerEntry.h"
#include "STFile.h"
#include "STLogger.h"

_LIT(KLitOpeningFileFailed, "Open/replace failed");


CSTFileManagerEntry::CSTFileManagerEntry(CSTFile& aFile)
 : iFile(aFile)
{
}


CSTFileManagerEntry::~CSTFileManagerEntry()
{
	iFileHandle.Close();
}

CSTFileManagerEntry* CSTFileManagerEntry::NewLC(RFs& aFs, CSTData& aData, CSTFile& aFile)
{
	CSTFileManagerEntry* self = new (ELeave)CSTFileManagerEntry(aFile);
	CleanupStack::PushL(self);
	self->ConstructL(aFs, aData);
	return self;
}

CSTFileManagerEntry* CSTFileManagerEntry::NewL(RFs& aFs, CSTData& aData, CSTFile& aFile)
{
	CSTFileManagerEntry* self = CSTFileManagerEntry::NewLC(aFs, aData, aFile);
	CleanupStack::Pop();
	return self;
}

void CSTFileManagerEntry::ConstructL(RFs& aFs, CSTData& aData)
{
	HBufC* fileName = HBufC::NewLC(aData.Path().Length() + iFile.Path().Length());
	TPtr fileNamePtr(fileName->Des());
	fileNamePtr.Copy(aData.Path());
	fileNamePtr.Append(iFile.Path());

	TPtrC fileNamePtr = iFile.Path();
	aFs.MkDirAll(iFile.Path());

	LOG->WriteL(_L("[File] Opening "));
	LOG->WriteLineL(fileNamePtr);

	if (iFileHandle.Open(aFs, fileNamePtr, EFileWrite|EFileShareAny) != KErrNone)
	{
		LOG->WriteLineL(_L("[File] Cannot open file, trying to create/replace"));
		if (iFileHandle.Replace(aFs, fileNamePtr, EFileWrite|EFileShareAny) != KErrNone)
		{
			LOG->WriteLineL(_L("[File] Create/replace failed"));
			User::Leave(0);
		}
		else
			LOG->WriteLineL(_L("[File] File created"));
	}
	else
		LOG->WriteLineL(_L("[File] File opened"));
}

TInt CSTFileManagerEntry::Write(TInt aPosition, const TDesC8& aData)
{
	TInt fileSize;
	TInt res;
	if ((res = iFileHandle.Size(fileSize)) != KErrNone)
		return res;

	if (fileSize < aPosition)
	{
		if ((res = iFileHandle.SetSize(aPosition)) != KErrNone)
		{
			LOG->WriteLineL(_L("File SetSize error"));
			return res;
		}				
	}

	return iFileHandle.Write(aPosition, aData);
}


HBufC8* CSTFileManagerEntry::ReadL(TInt aPos, TInt aLength)
{
	HBufC8* buf = HBufC8::NewLC(aLength);
	TPtr8 bufPtr(buf->Des());
	
	TInt res;
	
	res = iFileHandle.Read(aPos, bufPtr, aLength);
	
	if (res == KErrNone)
	{
		CleanupStack::Pop();
		return buf;
	}
	else
	{
		CleanupStack::PopAndDestroy();
		return NULL;
		
	}
}
