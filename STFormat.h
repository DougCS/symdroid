#ifndef STFORMAT_H
#define STFORMAT_H

#include <e32base.h>
#include "STTorrentManager.h"
#include "STNetworkManager.h"
#include "STTrackerConnection.h"
#include "STDefs.h"
#include "STHashChecker.h"

class CSTFilemanager;
class CSTBencode;
class CSTFile;
class CSTTrackerConnection;
class CSTPeer;
class CSTPiece;
class CSTBitField;
class TInetAddr;
class CSTFileManager;
class CSTPreferences;
class CSTAnnounceList;

enum TClosingState
{
	ETorrentNotClosing = 0,
	ETorrentClosing,
	ETorrentClosed
};


#ifdef __WINS__
 const TInt KMaxPeerConnectionCount = 1;
#else
 const TInt KMaxPeerConnectionCount = 15;
#endif

const TInt KIncomingConnectionSlots = 2;

class CSTFilemanager : public CBase, public MSTProxyConnectionObserver, public MSTHashCheckerObserver
{
public:

	IMPORT_C void StartL();

	IMPORT_C void StopL();
	
	IMPORT_C void CloseL();

	IMPORT_C TInt LoadL(const TDesC& aDataFileName);
	IMPORT_C TInt LoadL(const RFile& aDataFile);
	
	IMPORT_C TInt OpenSFileL(const TDesC& aDataFileName);
	
	IMPORT_C TBool HasEnoughDiskSpaceToDownload();
			
	IMPORT_C CSTPeer* Peer(const TInetAddr& aAddress);
	
	IMPORT_C TInt AddPeerL(CSTPeer* aPeer);
	
	IMPORT_C void CSTTorrent::AddPushPeerL(TInetAddr aAddress);
	
	IMPORT_C void SaveStateL();
	
	IMPORT_C void RemovePeer(const TInetAddr& aAddress);
	
	IMPORT_C void SetFailedL(const TDesC& aReason = KNullDesC);
	
	IMPORT_C void CheckHashL(CAknProgressDialog* aProgressDialog);

public:
	
	inline TUint BytesDownloaded() const;
	
	inline TUint BytesUploaded() const;
	
	inline TUint BytesLeft() const;
	
	inline const CSTAnnounceList* AnnounceList() const;
	
	inline const TDesC8& InfoHash() const;
	
	inline const CSTBitField* BitField() const;
	
	inline TInt PieceLength() const;
			
	inline TInt IndexOfPiece(const CSTPiece* aPiece) const;
	
	inline CSTPiece* Piece(TInt aIndex);

	inline const TDesC& Path() const;

	inline const TDesC& Name() const;

	inline const TDesC8& Comment() const;
	
	inline const TDesC8& CreatedBy() const;

	inline TInt Size() const;

	inline CSTFileManager& FileManager() const;

	inline TBool IsComplete() const;

	inline TInt PWConnectionCount() const;

	inline TReal DownloadPercent() const;
	
	inline TReal DownloadSpeed() const;
	
	inline TReal UploadSpeed() const;
	
	inline TSTTorrentStatusInfo StatusInfo() const;
	
	inline TInt PieceCount() const;
	
	inline TInt FileCount() const;
	
	inline const CSTFile* File(TInt aIndex);
	
	inline CSTTorrentManager* TorrentMgr() const;
	
	inline CSTPreferences* Preferences();
		
	inline const TDesC& SavedTorrent() const;
		
	inline TBool IsActive() const;
	
	inline const TDesC& FailReason() const;
	
	inline TBool IsFailed() const;
		
	inline TBool EndGame() const;
		
	inline TBool IsClosed();
	
	inline TInt ActiveConnectionCount() const;
	
	inline TTime LastTrackerConnectionTime() const;
	
	inline TInt TrackerRequestInterval() const;
	
	inline TInt NextTrackerConnectionIn();
	
	inline TInt PeerCount() const;

public:

	CSTTorrent(CSTTorrentManager* aTorrentMgr);
	
	void ConstructL();
	
	~CSTTorrent();
	
	void ProcessTrackerResponseL(CSTBencode* aResponse);
	
	void PieceDownloaded(CSTPiece* aPiece, TBool aNotifyTorrentObserver = ETrue);
	
	void PieceHashFailed(CSTPiece* aPiece);

	void UpdateBytesDownloadedL(TInt aBytesDownloaded, TBool aNotifyObserver = ETrue);
	
	void ResetDownloadsL(TBool aNotifyObserver);
	
	void UpdateBytesUploadedL(TInt aBytesUploaded, TBool aNotifyObserver = ETrue);
		
	void PeerDisconnectedL(CSTPeer* aPeer, TBool iPeerWireConnected);
	
	CSTPiece* GetPieceToDownloadL(CSTPeer* aPeer);
	
	void RemovePieceFromDownloading(CSTPiece* aPiece);
	
	void TrackerFailedL();
	
	void IncreasePWConnectionCountL();
	
	TBool HasTimeoutlessPeer();
	
	void IncreaseNumberOfPeersHavingPiece(TInt aPieceIndex);
	
	void DecreaseNumberOfPeersHavingPiece(TInt aPieceIndex);
	
	void EndGamePieceReceivedL(CSTPiece* aPiece, CSTPeer* aPeer);
	
private:

	void HashCheckFinishedL(CSTHashChecker* aHashChecker);

private:

	void ReportProxyConnectionL(TBool aProxyConnectionSucceeded);
	
private:

	void AddFileL(const TDesC& aRelativePath, TUint aSize);
	
	void AddFileL(const TDesC8& aRelativePath, TUint aSize);

	TInt ReadFromBencodedDataL(CSTBencode* aBencodedTorrent, const TDesC& aPath = KNullDesC);
	
	void ConnectToTrackerL(TTrackerConnectionEvent aEvent = ETrackerEventNotSpecified);

	void CalculateFileFragmentsL();
	
	void OnTimerL();

	void SetStatusInfoL(TSTTorrentStatusInfo aStatus);
	
	void SetComplete();
	
	void EnterEndGame();
	
private:

	HBufC* iStatusText;
	
	TUint iTrackerRequestInterval;

	TBool iValid;

	HBufC* iName;
	 
	HBufC* iPath;
	
	HBufC8* iAnnounce;

	CSTAnnounceList* iAnnounceList;
	
	HBufC8* iComment;
	
	HBufC8* iCreatedBy;
	
	TTime iCreationDate;
	
	TInt iPieceLength;
	
	CSTTorrentManager* iTorrentMgr;
	
	RPointerArray<CSTFile> iFiles;
	
	RPointerArray<CSTPiece> iPieces;
	
	TBuf8<20> iInfoHash;
	
	TInt iBytesUploaded;
	
	TInt iBytesDownloaded;
	
	TInt iBytesLeft;
	
	CSTTrackerConnection* iTrackerConnection;
	
	RPointerArray<CSTPeer> iPeers;
	
	RPointerArray<CSTPeer> iDisconnectedPeers;		
	
	TInt iEllapsedTime;
	
	CSTBitField* iBitField;
	
	TBool iActive;
	
	TInt iTrackerFailures;
	
	RPointerArray<CSTPiece> iDownloadingPieces;

	CSTFileManager* iFileManager;

	TBool iComplete;
DownloadedPieceCount;

	TInt iPeerWireConnectionCount;

	TReal iDownloadPercent;

	TInt iBytesPerSecond;

	TReal iAvarageBytesPerSecond;
	
	TInt iUploadedBytesPerSecond;

	TReal iAvarageUploadedBytesPerSecond;

	TSTTorrentStatusInfo iStatusInfo;
	
	HBufC* iSavedTorrent;
	
	TTrackerConnectionEvent iLastTrackerEvent;
	
	TBool iFailed;
	
	HBufC* iFailReason;
	
	TClosingState iClosingState;
	
	TInt iActiveConnectionCount;
	
	TInt64 iRandomSeed;
	
	TBool iEndGame;
iStatusInfoDelay;
	
	TTime iLastTrackerConnectionTime;
	
	RPointerArray<CSTPiece> iEndGamePiecesInTransit;
	
	CSTHashChecker* iHashChecker;
	
	friend class CSTTorrentManager;
	
	friend class CSTHashChecker;
};



inline const TDesC8& CSTTorrent::InfoHash() const {
	return iInfoHash;	
}

inline TUint CSTTorrent::BytesDownloaded() const {
	return iBytesDownloaded;
}

inline TUint CSTTorrent::BytesUploaded() const {
	return iBytesDownloaded;
}

inline TUint CSTTorrent::BytesLeft() const {
	return iBytesLeft;
}

inline const CSTBitField* CSTTorrent::BitField() const {
	return iBitField;	
}

inline TInt CSTTorrent::PieceLength() const {
	return iPieceLength;	
}

inline TInt CSTTorrent::IndexOfPiece(const CSTPiece* aPiece) const {
	return iPieces.Find(aPiece);	
}

inline const TDesC& CSTTorrent::Path() const {
	return *iPath;
}

inline const TDesC& CSTTorrent::Name() const {
	return *iName;
}

inline const TDesC8& CSTTorrent::Comment() const {
	if (iComment)
		return *iComment;
	return KNullDesC8;
}

inline const CSTAnnounceList* CSTTorrent::AnnounceList() const {
	return iAnnounceList;
}

inline const TDesC8& CSTTorrent::CreatedBy() const {
	if (iCreatedBy)
		return *iCreatedBy;
	return KNullDesC8;
}

inline TInt CSTTorrent::Size() const {
	return iBytesDownloaded + iBytesLeft;
}

inline CSTFileManager& CSTTorrent::FileManager() const {
	return *iFileManager;
}

inline TBool CSTTorrent::IsComplete() const {
	return iComplete;
}

inline TInt CSTTorrent::PWConnectionCount() const {
	return iPeerWireConnectionCount;
}

inline void CSTTorrent::NotifyTorrentManagerL() {
	iTorrentMgr->NotifyObserverL(this);
}

inline TReal CSTTorrent::DownloadPercent() const {
	return iDownloadPercent;
}

inline TReal CSTTorrent::DownloadSpeed() const {
	return iAvarageBytesPerSecond;
}

inline TReal CSTTorrent::UploadSpeed() const {
	return iAvarageUploadedBytesPerSecond;
}

inline TSTTorrentStatusInfo CSTTorrent::StatusInfo() const {
	return iStatusInfo;
}

inline TInt CSTTorrent::PieceCount() const {
	return iPieces.Count();
}

inline CSTPiece* CSTTorrent::Piece(TInt aIndex) {
	return iPieces[aIndex];
}

inline TInt CSTTorrent::FileCount() const {
	return iFiles.Count();
}
	
inline const CSTFile* CSTTorrent::File(TInt aIndex) {
	return iFiles[aIndex];
}

inline CSTTorrentManager* CSTTorrent::TorrentMgr() const {
	return iTorrentMgr;
}

inline const TDesC& CSTTorrent::SavedTorrent() const {
	if (iSavedTorrent)
		return *iSavedTorrent;
	
	return KNullDesC;
}

inline TBool CSTTorrent::IsActive() const {
	return iActive;
}

inline const TDesC& CSTTorrent::FailReason() const {
	if (iFailReason)
		return *iFailReason;
	
	return KNullDesC;
}

inline TBool CSTTorrent::IsFailed() const {
	return iFailed;
}

inline TBool CSTTorrent::IsClosed() {
	return (iClosingState == ETorrentClosed);
}

inline TInt CSTTorrent::ActiveConnectionCount() const {
	return iActiveConnectionCount;
}

inline CSTPreferences* CSTTorrent::Preferences() {
	return iTorrentMgr->Preferences();
}

inline TTime CSTTorrent::LastTrackerConnectionTime() const {
	return iLastTrackerConnectionTime;
}

inline TInt CSTTorrent::TrackerRequestInterval() const {
	return iTrackerRequestInterval;
}

inline TInt CSTTorrent::NextTrackerConnectionIn() {
	return (iTrackerRequestInterval - (iEllapsedTime % iTrackerRequestInterval));
}

inline TBool CSTTorrent::EndGame() const {
	return iEndGame;
}

inline TInt CSTTorrent::PeerCount() const {
	return iPeers.Count();
}

#endif
