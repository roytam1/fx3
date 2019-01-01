/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Annie Sullivan <annie.sullivan@gmail.com> (original author)
 *   Joe Hughes <joe@retrovirus.com>
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

#include "nsIRDFService.h"
#include "nsLivemarkService.h"
#include "nsIServiceManager.h"
#include "nsNavBookmarks.h"
#include "nsNavHistoryResult.h"
#include "nsIAnnotationService.h"
#include "nsFaviconService.h"
#include "nsNetUtil.h"
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIObserverService.h"
#include "nsCRT.h"
#include "nsXPCOM.h"

#define LIVEMARK_TIMEOUT          15000       // fire every 15 seconds
#define PLACES_STRING_BUNDLE_URI  "chrome://browser/locale/places/places.properties"
#define LIVEMARK_ICON_URI         "chrome://browser/skin/places/livemarkItem.png"

// Constants for parsing RDF Livemarks
// These are used in nsBookmarksFeedHandler.cpp, but because there is
// no initialization function in the nsLivemarkLoadListener class, they
// are initialized by the nsLivemarkService::Init() function in this file.
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
#ifndef RSS09_NAMESPACE_URI
#define RSS09_NAMESPACE_URI "http://my.netscape.com/rdf/simple/0.9/"
#endif
#ifndef RSS10_NAMESPACE_URI
#define RSS10_NAMESPACE_URI "http://purl.org/rss/1.0/"
#endif
#ifndef DC_NAMESPACE_URI
#define DC_NAMESPACE_URI "http://purl.org/dc/elements/1.1/"
#endif 


nsLivemarkService* nsLivemarkService::sInstance = nsnull;


nsLivemarkService::nsLivemarkService()
  : mTimer(nsnull)
{
  NS_ASSERTION(!sInstance, "Multiple nsLivemarkService instances!");
  sInstance = this;
}

nsLivemarkService::~nsLivemarkService()
{
  NS_ASSERTION(sInstance == this, "Expected sInstance == this");
  sInstance = nsnull;
}

NS_IMPL_ISUPPORTS3(nsLivemarkService,
                   nsILivemarkService,
                   nsIRemoteContainer,
                   nsIObserver)

nsresult
nsLivemarkService::Init()
{
  nsresult rv;

  // Get string bundle for livemark messages
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bundleService->CreateBundle(
      PLACES_STRING_BUNDLE_URI,
      getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the folder icon uri.
  rv = NS_NewURI(getter_AddRefs(mIconURI), NS_LITERAL_STRING(LIVEMARK_ICON_URI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the localized strings for the names of the dummy
  // "livemark loading..." and "livemark failed to load" bookmarks
  rv = mBundle->GetStringFromName(NS_LITERAL_STRING("bookmarksLivemarkLoading").get(),
                                  getter_Copies(mLivemarkLoading));
  if (NS_FAILED(rv)) {
    mLivemarkLoading.Assign(NS_LITERAL_STRING("Live Bookmark loading..."));
  }

  nsXPIDLString lmfailedName;
  rv = mBundle->GetStringFromName(NS_LITERAL_STRING("bookmarksLivemarkFailed").get(),
                                  getter_Copies(mLivemarkFailed));
  if (NS_FAILED(rv)) {
    mLivemarkFailed.Assign(NS_LITERAL_STRING("Live Bookmark feed failed to load."));
  }

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);

  // Create timer to check whether to update livemarks
  if (!mTimer) {
      mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a timer");
      if (NS_FAILED(rv)) return rv;
      mTimer->InitWithFuncCallback(nsLivemarkService::FireTimer, this, LIVEMARK_TIMEOUT, 
                                   nsITimer::TYPE_REPEATING_SLACK);
      // Note: don't addref "this" as we'll cancel the timer in the nsLivemarkService destructor
  }

  // Use the annotations service to store data about livemarks
  mAnnotationService = do_GetService("@mozilla.org/browser/annotation-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the constants used to parse RDF livemark feeds
  nsCOMPtr<nsIRDFService> pRDF;
  pRDF = do_GetService(kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  pRDF->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                    getter_AddRefs(mLMRDF_type));
  pRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "channel"),
                    getter_AddRefs(mLMRSS09_channel));
  pRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "item"),
                    getter_AddRefs(mLMRSS09_item));
  pRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "title"),
                    getter_AddRefs(mLMRSS09_title));
  pRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "link"),
                    getter_AddRefs(mLMRSS09_link));

  pRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "channel"),
                    getter_AddRefs(mLMRSS10_channel));
  pRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "items"),
                    getter_AddRefs(mLMRSS10_items));
  pRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "title"),
                    getter_AddRefs(mLMRSS10_title));
  pRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "link"),
                    getter_AddRefs(mLMRSS10_link));

  pRDF->GetResource(NS_LITERAL_CSTRING(DC_NAMESPACE_URI "date"),
                    getter_AddRefs(mLMDC_date));

  // Initialize the list of livemarks from the list of URIs
  // that have a feed uri annotation.
  nsCOMArray<nsIURI> pLivemarks;
  rv = mAnnotationService->GetPagesWithAnnotationCOMArray(
      NS_LITERAL_CSTRING(LMANNO_FEEDURI), &pLivemarks);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < pLivemarks.Count(); ++i) {

    // Get the feed URI from the annotation.
    nsAutoString feedURIString;
    rv = mAnnotationService->GetAnnotationString(pLivemarks[i],
                                                 NS_LITERAL_CSTRING(LMANNO_FEEDURI),
                                                 feedURIString);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIURI> feedURI;
    rv = NS_NewURI(getter_AddRefs(feedURI), NS_ConvertUTF16toUTF8(feedURIString));
    NS_ENSURE_SUCCESS(rv, rv);

    // Use QueryStringToQueryArray() to get the folderId from the place:uri.
    // (It ends up in folders[0] since we know that this place: uri was
    // generated by the bookmark service to uniquely identify the folder)
    nsCAutoString folderQueryString;
    rv = pLivemarks[i]->GetSpec(folderQueryString);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMArray<nsNavHistoryQuery> queries;
    nsCOMPtr<nsNavHistoryQueryOptions> options;
    rv = History()->QueryStringToQueryArray(folderQueryString, &queries,
                                            getter_AddRefs(options));
    NS_ENSURE_SUCCESS(rv, rv);
    if (queries.Count() != 1 || queries[0]->Folders().Length() != 1)
      continue; // invalid folder URI (should identify exactly one folder)

    // Create the livemark and add it to the list.
    LivemarkInfo *li = new LivemarkInfo(queries[0]->Folders()[0],
                                        pLivemarks[i], feedURI);
    NS_ENSURE_TRUE(li, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_TRUE(mLivemarks.AppendElement(li), NS_ERROR_OUT_OF_MEMORY);
  }

  return rv;
}

NS_IMETHODIMP
nsLivemarkService::Observe(nsISupports *aSubject, const char *aTopic,
                           const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    nsresult rv;
    
    // Clear timer and pending loads to prevent leaks of the livemark service 
    // during shutdown.
    if (mTimer) {
        // be sure to cancel the timer, as it holds a
        // weak reference back to nsLivemarkService
        mTimer->Cancel();
        mTimer = nsnull;
    }
    // Cancel any pending loads
    for (PRUint32 i = 0; i < mLivemarks.Length(); ++i) {
      LivemarkInfo *li = mLivemarks[i];
      if (li->loadGroup) {
        li->loadGroup->Cancel(NS_BINDING_ABORTED);
      }
    }

    // Remove our xpcom-shutdown observer so we don't leak the observer 
    // service.
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsLivemarkService::CreateLivemark(PRInt64 aFolder,
                                  const nsAString &aName,
                                  nsIURI *aSiteURI,
                                  nsIURI *aFeedURI,
                                  PRInt32 aIndex,
                                  PRInt64 *aNewLivemark)
{
  // Create the livemark as a bookmark container
  nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
  nsresult rv = bookmarks->CreateContainer(aFolder, aName,
                                           NS_LITERAL_STRING(NS_LIVEMARKSERVICE_CONTRACTID),
                                           aIndex, aNewLivemark);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the livemark URI
  nsCOMPtr<nsIURI> livemarkURI;
  rv = bookmarks->GetFolderURI(*aNewLivemark, getter_AddRefs(livemarkURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Add an annotation to map the folder URI to the livemark feed URI
  nsCAutoString feedURISpec;
  rv = aFeedURI->GetSpec(feedURISpec);
  NS_ENSURE_SUCCESS(rv, rv);
  mAnnotationService->SetAnnotationString(livemarkURI,
                                          NS_LITERAL_CSTRING(LMANNO_FEEDURI),
                                          NS_ConvertUTF8toUTF16(feedURISpec),
                                          0,
                                          nsIAnnotationService::EXPIRE_NEVER);

  // Set the icon for the livemark
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);
  faviconService->SetFaviconUrlForPage(livemarkURI, mIconURI);

  if (aSiteURI) {
    // Add an annotation to map the folder URI to the livemark site URI
    nsCAutoString siteURISpec;
    rv = aSiteURI->GetSpec(siteURISpec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mAnnotationService->SetAnnotationString(livemarkURI,
                                                 NS_LITERAL_CSTRING(LMANNO_SITEURI),
                                                 NS_ConvertUTF8toUTF16(siteURISpec),
                                                 0,
                                                 nsIAnnotationService::EXPIRE_NEVER);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Store the livemark info in our array.
  LivemarkInfo *info = new LivemarkInfo(*aNewLivemark, livemarkURI, aFeedURI);
  NS_ENSURE_TRUE(info, NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mLivemarks.AppendElement(info), NS_ERROR_OUT_OF_MEMORY);

  UpdateLivemarkChildren(mLivemarks.Length() - 1, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsLivemarkService::IsLivemark(PRInt64 aFolder, PRBool *aResult)
{
  nsresult rv;
  *aResult = PR_FALSE;

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> folderURI;
  rv = bookmarks->GetFolderURI(aFolder, getter_AddRefs(folderURI));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mAnnotationService->HasAnnotation(
      folderURI, NS_LITERAL_CSTRING(LMANNO_FEEDURI), aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsLivemarkService::GetSiteURI(PRInt64 aContainer, nsIURI **aURI)
{
  nsresult rv;

  // First off, make sure we're dealing with a livemark ID.
  PRBool isLivemark = PR_FALSE;
  rv = IsLivemark(aContainer, &isLivemark);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isLivemark)
    return NS_ERROR_INVALID_ARG;

  // Now convert the container ID to a container URI for annotation operations
  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> containerURI;
  rv = bookmarks->GetFolderURI(aContainer, getter_AddRefs(containerURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // If there's no site URI annotation, return null
  nsAutoString siteURIString;
  rv = mAnnotationService->GetAnnotationString(
      containerURI, NS_LITERAL_CSTRING(LMANNO_SITEURI), siteURIString);
  if (NS_FAILED(rv)) {
    *aURI = nsnull;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> siteURI;
  rv = NS_NewURI(getter_AddRefs(siteURI), NS_ConvertUTF16toUTF8(siteURIString));
  NS_ENSURE_SUCCESS(rv, rv);
  *aURI = siteURI.get();
  NS_IF_ADDREF(*aURI);

  return NS_OK;
}

NS_IMETHODIMP
nsLivemarkService::SetSiteURI(PRInt64 aContainer, nsIURI *aSiteURI)
{
  nsresult rv;

  // First off, make sure we're dealing with a livemark ID.
  PRBool isLivemark = PR_FALSE;
  rv = IsLivemark(aContainer, &isLivemark);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isLivemark)
    return NS_ERROR_INVALID_ARG;

  // Now convert the container ID to a container URI for annotation operations
  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> containerURI;
  rv = bookmarks->GetFolderURI(aContainer, getter_AddRefs(containerURI));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aSiteURI) { // clear any existing site URI
    rv = mAnnotationService->RemoveAnnotation(
        containerURI, NS_LITERAL_CSTRING(LMANNO_SITEURI));
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  nsCAutoString siteURISpec;
  rv = aSiteURI->GetSpec(siteURISpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mAnnotationService->SetAnnotationString(
      containerURI,
      NS_LITERAL_CSTRING(LMANNO_SITEURI),
      NS_ConvertUTF8toUTF16(siteURISpec),
      0,
      nsIAnnotationService::EXPIRE_NEVER);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsLivemarkService::GetFeedURI(PRInt64 aContainer, nsIURI **aURI)
{
  nsresult rv;

  // Convert the container ID to a container URI for annotation operations
  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> containerURI;
  rv = bookmarks->GetFolderURI(aContainer, getter_AddRefs(containerURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // If there's no feed URI annotation, that means this isn't a livemark
  nsAutoString feedURIString;
  rv = mAnnotationService->GetAnnotationString(
      containerURI, NS_LITERAL_CSTRING(LMANNO_FEEDURI), feedURIString);
  if (NS_FAILED(rv))
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIURI> feedURI;
  rv = NS_NewURI(getter_AddRefs(feedURI), NS_ConvertUTF16toUTF8(feedURIString));
  NS_ENSURE_SUCCESS(rv, rv);
  *aURI = feedURI.get();
  NS_IF_ADDREF(*aURI);

  return NS_OK;
}

NS_IMETHODIMP
nsLivemarkService::SetFeedURI(PRInt64 aContainer, nsIURI *aFeedURI)
{
  nsresult rv;

  if (!aFeedURI) // a livemark folder with no feed URI is an illegal state
    return NS_ERROR_NULL_POINTER;

  // Convert the container ID to a container URI for annotation operations
  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> containerURI;
  rv = bookmarks->GetFolderURI(aContainer, getter_AddRefs(containerURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString feedURISpec;
  rv = aFeedURI->GetSpec(feedURISpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mAnnotationService->SetAnnotationString(
      containerURI,
      NS_LITERAL_CSTRING(LMANNO_FEEDURI),
      NS_ConvertUTF8toUTF16(feedURISpec),
      0,
      nsIAnnotationService::EXPIRE_NEVER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now update our internal table
  PRInt32 livemarkIndex = GetLivemarkIndex(aContainer);
  NS_ENSURE_TRUE(livemarkIndex > -1, NS_ERROR_FAILURE);

  mLivemarks[livemarkIndex]->feedURI = aFeedURI;

  return NS_OK;
}

#if 0
// These two container API functions are not currently in the remote container
// service because they are untested. We do not need them here.
NS_IMETHODIMP
nsLivemarkService::OnContainerOpening(
    nsINavHistoryContainerResultNode* container,
    nsINavHistoryQueryOptions* options)
{
  // Nothing to do here since the containers are filled in
  // as the livemarks are loaded through FireTimer().
  return NS_OK;
}


NS_IMETHODIMP
nsLivemarkService::OnContainerClosed(nsINavHistoryContainerResultNode* container)
{
  // Nothing to clean up
  return NS_OK;
}
#endif


NS_IMETHODIMP
nsLivemarkService::OnContainerRemoving(PRInt64 aContainer)
{
  nsresult rv;

  // Find the livemark id in the list of livemarks.
  PRInt32 lmIndex = -1;
  PRInt32 i;
  for (i = 0; i < PRInt32(mLivemarks.Length()); i++) {
    if (mLivemarks[i]->folderId == aContainer) {
      lmIndex = i;
      break;
    }
  }
  // If there livemark isn't in the list, it's not valid.
  if (lmIndex == -1)
    return NS_ERROR_INVALID_ARG;

  // Remove the annotations that link the folder URI to the 
  // Feed URI and Site URI
  LivemarkInfo *removedItem = mLivemarks[lmIndex];
  rv = mAnnotationService->RemoveAnnotation(removedItem->folderURI,
                                            NS_LITERAL_CSTRING(LMANNO_FEEDURI));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mAnnotationService->RemoveAnnotation(removedItem->folderURI,
                                            NS_LITERAL_CSTRING(LMANNO_SITEURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if there are any other livemarks pointing to this feed.
  // If not, remove the annotations for the feed.
  PRBool stillInUse = PR_FALSE;
  PRBool urisEqual = PR_FALSE;
  for (i = 0; i < PRInt32(mLivemarks.Length()); i++) {
    if (i != lmIndex && 
        (NS_OK == mLivemarks[i]->feedURI->Equals(removedItem->feedURI, &urisEqual)) &&
        urisEqual) {
      stillInUse = PR_TRUE;
      break;
    }
  }
  if (!stillInUse) {
    // No other livemarks use this feed. Clear all the annotations for it.
    rv = mAnnotationService->RemoveAnnotation(removedItem->feedURI,
                                              NS_LITERAL_CSTRING("livemark_expiration"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Cancel the load before we remove the element; this will ensure that
  // a LivemarkLoadListener won't try to add items for this load.
  if (removedItem->loadGroup) {
    removedItem->loadGroup->Cancel(NS_BINDING_ABORTED);
  }

  // Take the annotation out of the list of annotations.
  mLivemarks.RemoveElementAt(lmIndex);

  // Get rid of the children for this feed, clearing their annotations.
  DeleteLivemarkChildren(aContainer);

  return NS_OK;
}


NS_IMETHODIMP
nsLivemarkService::OnContainerMoved(PRInt64 aContainer,
                                    PRInt64 aNewFolder,
                                    PRInt32 aNewIndex)
{
  nsresult rv;
  
  // Find the livemark in the list.
  PRInt32 index = -1;
  for (PRInt32 i = 0; i < PRInt32(mLivemarks.Length()); i++) {
    if (mLivemarks[i]->folderId == aContainer) {
      index = i;
      break;
    }
  }
  if (index == -1)
    return NS_ERROR_INVALID_ARG;

  // Get the new uri
  nsCOMPtr<nsIURI> newURI;
  rv = nsNavBookmarks::GetBookmarksService()->GetFolderURI(aContainer,
                                                           getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> oldURI = mLivemarks[index]->folderURI;
  mLivemarks[index]->folderURI = newURI;

  // Update the annotation that maps the folder URI to the livemark feed URI
  nsAutoString feedURIString;
  rv = mAnnotationService->GetAnnotationString(oldURI,
                                               NS_LITERAL_CSTRING(LMANNO_FEEDURI),
                                               feedURIString);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mAnnotationService->RemoveAnnotation(oldURI,
                                            NS_LITERAL_CSTRING(LMANNO_FEEDURI));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mAnnotationService->SetAnnotationString(newURI,
                                               NS_LITERAL_CSTRING(LMANNO_FEEDURI),
                                               feedURIString,
                                               0,
                                               nsIAnnotationService::EXPIRE_NEVER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update the annotation that maps the folder URI to the livemark site URI
  nsAutoString siteURIString;
  rv = mAnnotationService->GetAnnotationString(oldURI,
                                               NS_LITERAL_CSTRING(LMANNO_SITEURI),
                                               siteURIString);
  // rv will be failure if no site URI annotation is present
  if (NS_SUCCEEDED(rv)) {
    rv = mAnnotationService->RemoveAnnotation(
        oldURI,
        NS_LITERAL_CSTRING(LMANNO_SITEURI));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mAnnotationService->SetAnnotationString(
        newURI,
        NS_LITERAL_CSTRING(LMANNO_SITEURI),
        siteURIString,
        0,
        nsIAnnotationService::EXPIRE_NEVER);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsLivemarkService::GetChildrenReadOnly(PRBool *aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}


void
nsLivemarkService::FireTimer(nsITimer* aTimer, void* aClosure)
{
  nsLivemarkService *lmks = NS_STATIC_CAST(nsLivemarkService *, aClosure);
  if (!lmks)  return;

  // Go through all of the livemarks, and check if each needs to be updated.
  for (PRUint32 i = 0; i < lmks->mLivemarks.Length(); i++) {
    lmks->UpdateLivemarkChildren(i, PR_FALSE);
  }
}

nsresult
nsLivemarkService::DeleteLivemarkChildren(PRInt64 aLivemarkFolderId)
{
  nsresult rv;
  nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
  nsNavHistory* history = History();
  
  nsCOMPtr<nsINavHistoryQuery> query;
  rv = history->GetNewQuery(getter_AddRefs(query));
  NS_ENSURE_TRUE(query, NS_ERROR_OUT_OF_MEMORY);

  rv = query->SetFolders(&aLivemarkFolderId, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINavHistoryQueryOptions> options;
  rv = history->GetNewQueryOptions(getter_AddRefs(options));
  NS_ENSURE_TRUE(options, NS_ERROR_OUT_OF_MEMORY);
  PRUint32 mode = nsINavHistoryQueryOptions::GROUP_BY_FOLDER;
  rv = options->SetGroupingMode(&mode, 1);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsINavHistoryResult> result;
  rv = history->ExecuteQuery(query, options, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsINavHistoryQueryResultNode> root;
  rv = result->GetRoot(getter_AddRefs(root));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 cc;
  root->SetContainerOpen(PR_TRUE);
  rv = root->GetChildCount(&cc);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < cc; i++) {
    nsCOMPtr<nsINavHistoryResultNode> node;
    rv = root->GetChild(i, getter_AddRefs(node));
    if (NS_FAILED(rv)) continue;
    nsCAutoString spec;
    rv = node->GetUri(spec);
    if (NS_FAILED(rv)) continue;
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), spec, nsnull);
    if (NS_FAILED(rv)) continue;
    rv = mAnnotationService->RemoveAnnotation(uri,
                                              NS_LITERAL_CSTRING(LMANNO_BMANNO));
    if (NS_FAILED(rv)) continue;
  }

  // Get the folder children.
  rv = bookmarks->RemoveFolderChildren(aLivemarkFolderId);
  return rv;
}

nsresult
nsLivemarkService::InsertLivemarkChild(PRInt64 aLivemarkFolderId, 
                                       nsIURI *aURI,
                                       const nsAString &aTitle,
                                       const nsAString &aFeedURI)
{
  nsresult rv;
  nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
  rv = bookmarks->InsertItem(aLivemarkFolderId, aURI, -1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bookmarks->SetItemTitle(aURI, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mAnnotationService->SetAnnotationString(aURI,
                                          NS_LITERAL_CSTRING(LMANNO_BMANNO),
                                          aFeedURI,
                                          0,
                                          nsIAnnotationService::EXPIRE_NEVER);
  return NS_OK;
}

nsresult
nsLivemarkService::InsertLivemarkLoadingItem(PRInt64 aFolder)
{
  nsresult rv;
  nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
  nsCOMPtr<nsIURI> loadingURI;
  rv = NS_NewURI(getter_AddRefs(loadingURI), NS_LITERAL_CSTRING("about:livemark-loading"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bookmarks->InsertItem(aFolder, loadingURI, -1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bookmarks->SetItemTitle(loadingURI, mLivemarkLoading);
  return rv;
}

nsresult
nsLivemarkService::InsertLivemarkFailedItem(PRInt64 aFolder)
{
  nsresult rv;
  nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
  nsCOMPtr<nsIURI> failedURI;
  rv = NS_NewURI(getter_AddRefs(failedURI), NS_LITERAL_CSTRING("about:livemark-failed"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bookmarks->InsertItem(aFolder, failedURI, -1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bookmarks->SetItemTitle(failedURI, mLivemarkFailed);
  return rv;
}

NS_IMETHODIMP
nsLivemarkService::ReloadAllLivemarks()
{
  for (PRUint32 i = 0; i < mLivemarks.Length(); i++) {
    UpdateLivemarkChildren(i, PR_TRUE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLivemarkService::ReloadLivemarkFolder(PRInt64 aFolderId)
{
  PRInt32 folderIndex = GetLivemarkIndex(aFolderId);
  if (folderIndex == -1)
    return NS_ERROR_INVALID_ARG;

  return UpdateLivemarkChildren(folderIndex, PR_TRUE);
}

// Returns the index into mLivemarks that corresponds to the given
// folder ID, or -1 if not found.
PRInt32
nsLivemarkService::GetLivemarkIndex(PRInt64 aFolderId)
{
  for (PRUint32 i = 0; i < mLivemarks.Length(); i++) {
    if (mLivemarks[i]->folderId == aFolderId) {
      return i;
    }
  }

  return -1;
}
