#ifndef SYMTORRENT_STFILE_H
#define SYMTORRENT_STFILE_H

#include <e32base.h>
#include <f32file.h>
#include "STDefs.h"

class CSTTorrent;
class CSTFile;

class CSTFile : public CBase
{
public:

	IMPORT_C static CSTFile* NewL(const TDesC& aPath, TUint aSize);
	
	IMPORT_C static CSTFile* NewLC(const TDesC& aPath, TUint aSize);	
	
	IMPORT_C ~CSTFile();
	
	inline TUint Size() const;
	
	inline const TDesC& Path() const;
	
	inline TPtrC FileName() const;
	
	inline TBool IsDownloaded() const;
	
	inline void SetDownloaded(TBool aDownloaded = ETrue);
	
	IMPORT_C HBufC* CreateFileInfoL() const;

private:

	CSTFile(TUint aSize);

	void ConstructL(const TDesC& aPath);
	
private:

	RFile iFile;

	TUint iSize;

	HBufC* iPath;
	
	TBool iDownloaded;
	
	TInt iFileNameLength;
};


inline TUint CSTFile::Size() const {
	return iSize;	
}

inline const TDesC& CSTFile::Path() const {
	return *iPath;
}

inline TBool CSTFile::IsDownloaded() const {
	return iDownloaded;
}
	
inline void CSTFile::SetDownloaded(TBool aDownloaded) {
	iDownloaded = aDownloaded;
}

inline TPtrC CSTFile::FileName() const {
	return iPath->Right(iFileNameLength);
}



#endif
