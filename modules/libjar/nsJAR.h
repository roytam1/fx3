/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Don Bragg <dbragg@netscape.com>
 *   Samir Gehani <sgehani@netscape.com>
 *   Mitch Stoltz <mstoltz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef nsJAR_h__
#define nsJAR_h__

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"
#include "prtypes.h"
#include "prinrval.h"

#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsIEnumerator.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsAutoLock.h"
#include "nsIZipReader.h"
#include "nsIJAR.h"
#include "nsZipArchive.h"
#include "zipfile.h"
#include "nsIPrincipal.h"
#include "nsISignatureVerifier.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"

class nsIInputStream;
class nsJARManifestItem;
class nsZipReaderCache;

/*-------------------------------------------------------------------------
 * Class nsJAR declaration. 
 * nsJAR serves as an XPCOM wrapper for nsZipArchive with the addition of 
 * JAR manifest file parsing. 
 *------------------------------------------------------------------------*/
class nsJAR : public nsIZipReader, public nsIJAR
{
  // Allows nsJARInputStream to call the verification functions
  friend class nsJARInputStream;

  public:

    nsJAR();
    virtual ~nsJAR();
    
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_ZIPREADER_CID );
  
    NS_DECL_ISUPPORTS

    NS_DECL_NSIZIPREADER

    NS_DECL_NSIJAR

    PRIntervalTime GetReleaseTime() {
        return mReleaseTime;
    }
    
    PRBool IsReleased() {
        return mReleaseTime != PR_INTERVAL_NO_TIMEOUT;
    }

    void SetReleaseTime() {
      mReleaseTime = PR_IntervalNow();
    }
    
    void ClearReleaseTime() {
      mReleaseTime = PR_INTERVAL_NO_TIMEOUT;
    }
    
    void SetZipReaderCache(nsZipReaderCache* cache) {
      mCache = cache;
    }

    PRFileDesc* OpenFile();
  protected:
    //-- Private data members
    nsCOMPtr<nsIFile>        mZipFile;        // The zip/jar file on disk
    nsZipArchive             mZip;            // The underlying zip archive
    nsObjectHashtable        mManifestData;   // Stores metadata for each entry
    PRBool                   mParsedManifest; // True if manifest has been parsed
    nsCOMPtr<nsIPrincipal>   mPrincipal;      // The entity which signed this file
    PRInt16                  mGlobalStatus;   // Global signature verification status
    PRIntervalTime           mReleaseTime;    // used by nsZipReaderCache for flushing entries
    nsZipReaderCache*        mCache;          // if cached, this points to the cache it's contained in
	PRLock*					 mLock;	
    PRInt32                  mTotalItemsInManifest;
    PRFileDesc*              mFd;
    
    //-- Private functions
    nsresult ParseManifest(nsISignatureVerifier* verifier);
    void     ReportError(const char* aFilename, PRInt16 errorCode);
    nsresult LoadEntry(const char* aFilename, char** aBuf, 
                       PRUint32* aBufLen = nsnull);
    PRInt32  ReadLine(const char** src); 
    nsresult ParseOneFile(nsISignatureVerifier* verifier,
                          const char* filebuf, PRInt16 aFileType);
    nsresult VerifyEntry(nsISignatureVerifier* verifier,
                         nsJARManifestItem* aEntry, const char* aEntryData, 
                         PRUint32 aLen);

    nsresult CalculateDigest(const char* aInBuf, PRUint32 aInBufLen,
                             char** digest);

    //-- Debugging
    void DumpMetadata(const char* aMessage);
};

/**
 * nsJARItem
 *
 * An individual JAR entry. A set of nsJARItems macthing a
 * supplied pattern are returned in a nsJAREnumerator.
 */
class nsJARItem : public nsIZipEntry
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIZIPENTRY
    
    void Init(nsZipItem* aZipItem);

    nsJARItem();
    virtual ~nsJARItem();

    private:
    nsZipItem* mZipItem;
};

/**
 * nsJAREnumerator
 *
 * Enumerates a list of files in a zip archive 
 * (based on a pattern match in its member nsZipFind).
 */
class nsJAREnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    nsJAREnumerator(nsZipFind *aFind);
    virtual ~nsJAREnumerator();

protected:
    nsZipArchive *mArchive; // pointer extracted from mFind for efficiency
    nsZipFind    *mFind;
    nsZipItem    *mCurr;    // raw pointer to an nsZipItem owned by mArchive -- DON'T delete
    PRBool        mIsCurrStale;
};

////////////////////////////////////////////////////////////////////////////////

#if defined(DEBUG_warren) || defined(DEBUG_jband)
#define ZIP_CACHE_HIT_RATE
#endif

class nsZipReaderCache : public nsIZipReaderCache, public nsIObserver,
                         public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIZIPREADERCACHE
  NS_DECL_NSIOBSERVER

  nsZipReaderCache();
  virtual ~nsZipReaderCache();

  nsresult ReleaseZip(nsJAR* reader);

protected:
  PRLock*               mLock;
  PRInt32               mCacheSize;
  nsSupportsHashtable   mZips;

#ifdef ZIP_CACHE_HIT_RATE
  PRUint32              mZipCacheLookups;
  PRUint32              mZipCacheHits;
  PRUint32              mZipCacheFlushes;
  PRUint32              mZipSyncMisses;
#endif

};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsJAR_h__ */

