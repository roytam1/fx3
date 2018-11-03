/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "pratom.h"
#include "unicpriv.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsUConvDll.h"
#include "nsIMappingCache.h"
#include "nsMappingCache.h"

//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [declaration]

/**
 * The actual implementation of the nsIUnicodeEncodeHelper interface.
 *
 * @created         22/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeEncodeHelper : public nsIUnicodeEncodeHelper
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsUnicodeEncodeHelper();

  /**
   * Class destructor.
   */
  virtual ~nsUnicodeEncodeHelper();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncodeHelper [declaration]

  NS_IMETHOD ConvertByTable(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength, uShiftTable * aShiftTable, 
      uMappingTable  * aMappingTable);

  NS_IMETHOD ConvertByMultiTable(const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uShiftTable ** aShiftTable, uMappingTable  ** aMappingTable);

  NS_IMETHOD CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult);

  NS_IMETHOD DestroyCache(nsIMappingCache aCache);
 
  NS_IMETHOD FillInfo(PRUint32* aInfo, uMappingTable  * aMappingTable);
  NS_IMETHOD FillInfo(PRUint32* aInfo, PRInt32 aTableCount, uMappingTable  ** aMappingTable);
};

//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [implementation]

NS_IMPL_ISUPPORTS(nsUnicodeEncodeHelper, kIUnicodeEncodeHelperIID);

nsUnicodeEncodeHelper::nsUnicodeEncodeHelper() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsUnicodeEncodeHelper::~nsUnicodeEncodeHelper() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsIUnicodeEncodeHelper [implementation]

NS_IMETHODIMP nsUnicodeEncodeHelper::ConvertByTable(
                                     const PRUnichar * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     char * aDest, 
                                     PRInt32 * aDestLength, 
                                     uShiftTable * aShiftTable, 
                                     uMappingTable  * aMappingTable)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  PRInt32 destLen = *aDestLength;

  PRUnichar med;
  PRInt32 bcw; // byte count for write;
  nsresult res = NS_OK;

  while (src < srcEnd) {
    if (!uMapCode((uTable*) aMappingTable, *(src++), &med)) {
      res = NS_ERROR_UENC_NOMAPPING;
      break;
    }

    if (!uGenerate(aShiftTable, 0, med, (PRUint8 *)dest, destLen, 
      (PRUint32 *)&bcw)) { 
      src--;
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }

    dest += bcw;
    destLen -= bcw;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

NS_IMETHODIMP nsUnicodeEncodeHelper::ConvertByMultiTable(
                                     const PRUnichar * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     char * aDest, 
                                     PRInt32 * aDestLength, 
                                     PRInt32 aTableCount, 
                                     uShiftTable ** aShiftTable, 
                                     uMappingTable  ** aMappingTable)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  PRInt32 destLen = *aDestLength;

  PRUnichar med;
  PRInt32 bcw; // byte count for write;
  nsresult res = NS_OK;
  PRInt32 i;

  while (src < srcEnd) {
    for (i=0; i<aTableCount; i++) 
      if (uMapCode((uTable*) aMappingTable[i], *src, &med)) break;

    src++;
    if (i == aTableCount) {
      res = NS_ERROR_UENC_NOMAPPING;
      break;
    }

    if (!uGenerate(aShiftTable[i], 0, med, (PRUint8 *)dest, destLen, 
      (PRUint32 *)&bcw)) { 
      src--;
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }

    dest += bcw;
    destLen -= bcw;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}


NS_IMETHODIMP nsUnicodeEncodeHelper::CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult)
{
   return nsMappingCache::CreateCache(aType, aResult);
}

NS_IMETHODIMP nsUnicodeEncodeHelper::DestroyCache(nsIMappingCache aCache)
{
   return nsMappingCache::DestroyCache(aCache);
}

NS_IMETHODIMP nsUnicodeEncodeHelper::FillInfo(PRUint32 *aInfo, uMappingTable  * aMappingTable)
{
   uFillInfo((uTable*) aMappingTable, aInfo);
   return NS_OK;
}
NS_IMETHODIMP nsUnicodeEncodeHelper::FillInfo(PRUint32 *aInfo, PRInt32 aTableCount, uMappingTable  ** aMappingTable)
{
   for (PRInt32 i=0; i<aTableCount; i++) 
      uFillInfo((uTable*) aMappingTable[i], aInfo);
   return NS_OK;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewUnicodeEncodeHelper(nsISupports* aOuter, 
                          const nsIID &aIID,
                          void **aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsUnicodeEncodeHelper* inst = new nsUnicodeEncodeHelper();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}
