/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Brian Ryner <bryner@brianryner.com> (original author)
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

#include "nsAppDirectoryServiceDefs.h"
#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "mozStorageHelper.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsIRemoteContainer.h"
#include "nsUnicharUtils.h"
#include "nsFaviconService.h"
#include "nsAnnotationService.h"

const PRInt32 nsNavBookmarks::kFindBookmarksIndex_ItemChild = 0;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_FolderChild = 1;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Parent = 2;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Position = 3;

const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_FolderID = 0;
const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_Title = 1;
const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_Type = 2;

// These columns sit to the right of the kGetInfoIndex_* columns.
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Position = 9;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_ItemChild = 10;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderChild = 11;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderTitle = 12;

nsNavBookmarks* nsNavBookmarks::sInstance = nsnull;

#define BOOKMARKS_ANNO_PREFIX "bookmarks/"
#define ANNO_FOLDER_READONLY BOOKMARKS_ANNO_PREFIX "readonly"

nsNavBookmarks::nsNavBookmarks()
  : mRoot(0), mBookmarksRoot(0), mToolbarRoot(0), mTagRoot(0), mBatchLevel(0),
    mBatchHasTransaction(PR_FALSE)
{
  NS_ASSERTION(!sInstance, "Multiple nsNavBookmarks instances!");
  sInstance = this;
}

nsNavBookmarks::~nsNavBookmarks()
{
  NS_ASSERTION(sInstance == this, "Expected sInstance == this");
  sInstance = nsnull;
}

NS_IMPL_ISUPPORTS2(nsNavBookmarks,
                   nsINavBookmarksService, nsINavHistoryObserver)

nsresult
nsNavBookmarks::Init()
{
  nsresult rv;

  nsNavHistory *history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT id, name, type FROM moz_bookmarks_folders WHERE id = ?1"),
                               getter_AddRefs(mDBGetFolderInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT folder_child FROM moz_bookmarks WHERE parent IS NULL"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (results) {
      mRoot = statement->AsInt64(0);
    }
  }

  nsCAutoString buffer;

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bundleService->CreateBundle(
      "chrome://browser/locale/places/places.properties",
      getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBFindURIBookmarks
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT a.* "
      "FROM moz_bookmarks a, moz_history h "
      "WHERE h.url = ?1 AND a.item_child = h.id"),
    getter_AddRefs(mDBFindURIBookmarks));
  NS_ENSURE_SUCCESS(rv, rv);

  // Construct a result where the first columns exactly match those returned by
  // mDBGetURLPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks.  This selects only
  // _item_ children which are in moz_history.
  // Results are kGetInfoIndex_*
  NS_NAMED_LITERAL_CSTRING(selectItemChildren,
    "SELECT h.id, h.url, h.title, h.user_title, h.rev_host, h.visit_count, "
      "(SELECT MAX(visit_date) FROM moz_historyvisit WHERE page_id = h.id), "
      "f.url, null, a.position, a.item_child, a.folder_child, null "
    "FROM moz_bookmarks a "
    "JOIN moz_history h ON a.item_child = h.id "
    "LEFT OUTER JOIN moz_favicon f ON h.favicon = f.id "
    "WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3 ");

  // Construct a result where the first columns are padded out to the width
  // of mDBGetVisitPageInfo, containing additional columns for position,
  // item_child, and folder_child from moz_bookmarks, and name from
  // moz_bookmarks_folders.  This selects only _folder_ children which are
  // in moz_bookmarks_folders. Results are kGetInfoIndex_* kGetChildrenIndex_*
  NS_NAMED_LITERAL_CSTRING(selectFolderChildren,
    "SELECT null, null, null, null, null, null, null, null, null, a.position, a.item_child, a.folder_child, c.name "
    "FROM moz_bookmarks a "
    "JOIN moz_bookmarks_folders c ON c.id = a.folder_child "
    "WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3");

  // Construct a result where the first columns are padded out to the width
  // of mDBGetVisitPageInfo, containing additional columns for position,
  // item_child, and folder_child from moz_bookmarks.  This selects only
  // _separator_ children which are in moz_bookmarks.  Results are
  // kGetInfoIndex_* kGetChildrenIndex_*.  item_child and folder_child will
  // be NULL for separators.
  NS_NAMED_LITERAL_CSTRING(selectSeparatorChildren,
    "SELECT null, null, null, null, null, null, null, null, null, a.position, null, null, null "
    "FROM moz_bookmarks a "
    "WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3 AND "
    "a.item_child ISNULL and a.folder_child ISNULL");

  NS_NAMED_LITERAL_CSTRING(orderByPosition, " ORDER BY a.position");

  // mDBGetChildren: select all children of a given folder, sorted by position
  rv = dbConn->CreateStatement(selectItemChildren +
                               NS_LITERAL_CSTRING(" UNION ALL ") +
                               selectFolderChildren +
                               NS_LITERAL_CSTRING(" UNION ALL ") +
                               selectSeparatorChildren + orderByPosition,
                               getter_AddRefs(mDBGetChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBFolderCount: count all of the children of a given folder
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT COUNT(*) FROM moz_bookmarks WHERE parent = ?1"),
                               getter_AddRefs(mDBFolderCount));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBIndexOfItem: find the position of an item within a folder
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT position FROM moz_bookmarks WHERE item_child = ?1 AND parent = ?2"),
                               getter_AddRefs(mDBIndexOfItem));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT position FROM moz_bookmarks WHERE folder_child = ?1 AND parent = ?2"),
                               getter_AddRefs(mDBIndexOfFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT item_child, folder_child FROM moz_bookmarks WHERE parent = ?1 AND position = ?2"),
                               getter_AddRefs(mDBGetChildAt));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetRedirectDestinations
  // input = page ID, time threshold; output = unique ID input has redirected to
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT dest_v.page_id "
      "FROM moz_historyvisit source_v "
      "LEFT JOIN moz_historyvisit dest_v ON dest_v.from_visit = source_v.visit_id "
      "WHERE source_v.page_id = ?1 "
      "AND source_v.visit_date >= ?2 "
      "AND (dest_v.visit_type = 5 OR dest_v.visit_type = 6) "
      "GROUP BY dest_v.page_id"),
    getter_AddRefs(mDBGetRedirectDestinations));
  NS_ENSURE_SUCCESS(rv, rv);

  FillBookmarksHash();

  // must be last: This may cause bookmarks to be imported, which will exercise
  // most of the bookmark system
  // keywords
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT k.keyword FROM moz_history h "
      "JOIN moz_keywords k ON h.id = k.page_id "
      "WHERE h.url = ?1"),
    getter_AddRefs(mDBGetKeywordForURI));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT url FROM moz_keywords k "
      "JOIN moz_history h ON k.page_id = h.id "
      "WHERE k.keyword = ?1"),
    getter_AddRefs(mDBGetURIForKeyword));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitRoots();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // allows us to notify on title changes. MUST BE LAST so it is impossible
  // to fail after this call, or the history service will have a reference to
  // us and we won't go away.
  history->AddObserver(this, PR_FALSE);

  // DO NOT PUT STUFF HERE that can fail. See observer comment above.

  return NS_OK;
}


// nsNavBookmarks::InitTables
//
//    All commands that initialize the schema of the DB go in here. This is
//    called from history init before the dummy DB connection is started that
//    will prevent us from modifying the schema.

nsresult // static
nsNavBookmarks::InitTables(mozIStorageConnection* aDBConn)
{
  nsresult rv;
  PRBool exists;
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks ("
        "item_child INTEGER, "
        "folder_child INTEGER, "
        "parent INTEGER, "
        "position INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);

    // this index will make it faster to determine if a given item is
    // bookmarked (used by history queries and vacuuming, for example)
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE INDEX moz_bookmarks_itemindex ON moz_bookmarks (item_child)"));
    NS_ENSURE_SUCCESS(rv, rv);

    // the most common operation is to find the children given a parent
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE INDEX moz_bookmarks_parentindex ON moz_bookmarks (parent)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_bookmarks_folders
  // Note the use of AUTOINCREMENT for the primary key. This ensures that each
  // new primary key is unique, reducing the chance that stale references to
  // bookmark folders will reference a random folder. This slows down inserts
  // a little bit, which is why we don't always use it, but bookmark folders
  // are not created very often. 
  // NOTE: The folder undo code depends on the autoincrement behavior because
  // it must be able to undo deleting of a folder back to the old folder ID,
  // which we assume has not been taken by something else.
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_folders"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_folders ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name LONGVARCHAR, "
        "type LONGVARCHAR)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_bookmarks_roots
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_roots"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_roots ("
        "root_name VARCHAR(16) UNIQUE, "
        "folder_id INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_keywords
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_keywords"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TABLE moz_keywords ("
        "keyword VARCHAR(32) UNIQUE,"
        "page_id INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);

    // it should be fast to go url->ID and ID->url
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE INDEX moz_keywords_keywordindex ON moz_keywords (keyword)"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE INDEX moz_keywords_pageindex ON moz_keywords (page_id)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


struct RenumberItem {
  PRInt64 folderChild;
  nsCOMPtr<nsIURI> itemURI;
  PRInt32 position;
};

struct RenumberItemsArray {
  nsVoidArray items;
  ~RenumberItemsArray();
};

RenumberItemsArray::~RenumberItemsArray()
{
  for (PRInt32 i = 0; i < items.Count(); ++i) {
    delete NS_STATIC_CAST(RenumberItem*, items[i]);
  }
}

// nsNavBookmarks::InitRoots
//
//    This locates and creates if necessary the root items in the bookmarks
//    folder hierarchy. These items are stored in a special roots table that
//    maps short predefined names to folder IDs.
//
//    Normally, these folders will exist already and we will save their IDs
//    which are exposed through the bookmark service interface.
//
//    If the root does not exist, a folder is created for it and the ID is
//    saved in the root table. No user-visible name is given to these folders
//    and they have no parent or other attributes.
//
//    These attributes are set when the default_places.html file is imported.
//    It defines the hierarchy, and has special attributes that tell us when
//    a folder is one of our well-known roots. We then insert the root in the
//    defined point in the hierarchy and set its attributes from this.
//
//    This should be called as the last part of the init process so that
//    all of the statements are set up and the service is ready to use.

nsresult
nsNavBookmarks::InitRoots()
{
  nsresult rv;
  nsCOMPtr<mozIStorageStatement> getRootStatement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("SELECT folder_id FROM moz_bookmarks_roots WHERE root_name = ?1"),
                                 getter_AddRefs(getRootStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool importDefaults = PR_FALSE;
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("places"), &mRoot, &importDefaults);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("menu"), &mBookmarksRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("toolbar"), &mToolbarRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("tags"), &mTagRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (importDefaults) {
    // when there is no places root, we should define the hierarchy by
    // importing the default one.
    nsCOMPtr<nsIURI> defaultPlaces;
    rv = NS_NewURI(getter_AddRefs(defaultPlaces),
                   NS_LITERAL_CSTRING("chrome://browser/locale/places/default_places.html"),
                   nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ImportBookmarksHTMLInternal(defaultPlaces, PR_TRUE, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    // migrate the user's old bookmarks
    // FIXME: move somewhere else to some profile migrator thingy
    nsCOMPtr<nsIFile> bookmarksFile;
    rv = NS_GetSpecialDirectory(NS_APP_BOOKMARKS_50_FILE,
                                getter_AddRefs(bookmarksFile));
    if (bookmarksFile) {
      PRBool bookmarksFileExists;
      rv = bookmarksFile->Exists(&bookmarksFileExists);
      if (NS_SUCCEEDED(rv) && bookmarksFileExists) {
        nsCOMPtr<nsIIOService> ioservice = do_GetService(
                                    "@mozilla.org/network/io-service;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIURI> bookmarksFileURI;
        rv = ioservice->NewFileURI(bookmarksFile,
                                   getter_AddRefs(bookmarksFileURI));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = ImportBookmarksHTMLInternal(bookmarksFileURI, PR_FALSE, 0);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  return NS_OK;
}


// nsNavBookmarks::CreateRoot
//
//    This gets or creates a root folder of the given type. aWasCreated
//    (optional) is true if the folder had to be created, false if we just used
//    an old one. The statement that gets a folder ID from a root name is
//    passed in so the DB only needs to parse the statement once, and we don't
//    have to have a global for this. Creation is less optimized because it
//    happens rarely.

nsresult
nsNavBookmarks::CreateRoot(mozIStorageStatement* aGetRootStatement,
                           const nsCString& name, PRInt64* aID,
                           PRBool* aWasCreated)
{
  PRBool hasResult = PR_FALSE;
  nsresult rv = aGetRootStatement->BindUTF8StringParameter(0, name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aGetRootStatement->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasResult) {
    if (aWasCreated)
      *aWasCreated = PR_FALSE;
    rv = aGetRootStatement->GetInt64(0, aID);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(*aID != 0, "Root is 0 for some reason, folders can't have 0 ID");
    return NS_OK;
  }
  if (aWasCreated)
    *aWasCreated = PR_TRUE;

  // create folder with no name or attributes
  nsCOMPtr<mozIStorageStatement> insertStatement;
  rv = CreateFolder(0, NS_LITERAL_STRING(""), -1, aID);
  NS_ENSURE_SUCCESS(rv, rv);

  // save root ID
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_roots (root_name,folder_id) VALUES (?1, ?2)"),
                                 getter_AddRefs(insertStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->BindUTF8StringParameter(0, name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->BindInt64Parameter(1, *aID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavBookmarks::FillBookmarksHash
//
//    This initializes the bookmarks hashtable that tells us which bookmark
//    a given URI redirects to. This hashtable includes all URIs that
//    redirect to bookmarks.
//
//    This is called from the bookmark init function and so is wrapped
//    in that transaction (for better performance).

nsresult
nsNavBookmarks::FillBookmarksHash()
{
  nsresult rv;
  PRBool hasMore;

  // first init the hashtable
  NS_ENSURE_TRUE(mBookmarksHash.Init(1024), NS_ERROR_OUT_OF_MEMORY);

  // first populate the table with all bookmarks
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id "
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_history h ON b.item_child = h.id where b.item_child IS NOT NULL"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 pageID;
    rv = statement->GetInt64(0, &pageID);
    NS_ENSURE_TRUE(mBookmarksHash.Put(pageID, pageID), NS_ERROR_OUT_OF_MEMORY);
  }

  // Find all pages h2 that have been redirected to from a bookmarked URI:
  //    bookmarked -> url (h1)         url (h2)
  //                    |                 ^
  //                    .                 |
  //                 visit (v1) -> destination visit (v2)
  // This should catch most redirects, which are only one level. More levels of
  // redirection will be handled separately.
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT v1.page_id, v2.page_id "
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_historyvisit v1 on b.item_child = v1.page_id "
      "LEFT JOIN moz_historyvisit v2 on v2.from_visit = v1.visit_id "
      "WHERE b.item_child IS NOT NULL "
      "AND v2.visit_type = 5 OR v2.visit_type = 6 " // perm. or temp. RDRs
      "GROUP BY v2.page_id"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 fromId, toId;
    statement->GetInt64(0, &fromId);
    statement->GetInt64(1, &toId);

    NS_ENSURE_TRUE(mBookmarksHash.Put(toId, fromId), NS_ERROR_OUT_OF_MEMORY);

    // handle redirects deeper than one level
    rv = RecursiveAddBookmarkHash(fromId, toId, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavBookmarks::AddBookmarkToHash
//
//    Given a bookmark that was potentially added, this goes through all
//    redirects that this page may have resulted in and adds them to our hash.
//    Note that this takes the ID of the URL in the history system, which we
//    generally have when calling this function and which makes it faster.
//
//    For better performance, this call should be in a DB transaction.
//
//    @see RecursiveAddBookmarkHash

nsresult
nsNavBookmarks::AddBookmarkToHash(PRInt64 aBookmarkId, PRTime aMinTime)
{
  // this function might be called before our hashtable is initialized (for
  // example, on history import), just ignore these, we'll pick up the add when
  // the hashtable is initialized later
  if (! mBookmarksHash.IsInitialized())
    return NS_OK;
  if (! mBookmarksHash.Put(aBookmarkId, aBookmarkId))
    return NS_ERROR_OUT_OF_MEMORY;
  return RecursiveAddBookmarkHash(aBookmarkId, aBookmarkId, aMinTime);
}


// nsNavBookmkars::RecursiveAddBookmarkHash
//
//    Used to add a new level of redirect information to the bookmark hash.
//    Given a source bookmark 'aBookmark' and 'aCurrentSouce' that has already
//    been added to the hashtable, this will add all redirect destinations of
//    'aCurrentSort'. Will call itself recursively to walk down the chain.
//
//    'aMinTime' is the minimum time to consider visits from. Visits previous
//    to this will not be considered. This allows the search to be much more
//    efficient if you know something happened recently. Use 0 for the min time
//    to search all history for redirects.

nsresult
nsNavBookmarks::RecursiveAddBookmarkHash(PRInt64 aBookmarkID,
                                         PRInt64 aCurrentSource,
                                         PRTime aMinTime)
{
  nsresult rv;
  nsTArray<PRInt64> found;

  // scope for the DB statement. The statement must be reset by the time we
  // recursively call ourselves again, because our recursive call will use the
  // same statement.
  {
    mozStorageStatementScoper scoper(mDBGetRedirectDestinations);
    rv = mDBGetRedirectDestinations->BindInt64Parameter(0, aCurrentSource);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetRedirectDestinations->BindInt64Parameter(1, aMinTime);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(mDBGetRedirectDestinations->ExecuteStep(&hasMore)) &&
           hasMore) {

      // add this newly found redirect destination to the hashtable
      PRInt64 curID;
      rv = mDBGetRedirectDestinations->GetInt64(0, &curID);
      NS_ENSURE_SUCCESS(rv, rv);

      // It is very important we ignore anything already in our hashtable. It
      // is actually pretty common to get loops of redirects. For example,
      // a restricted page will redirect you to a login page, which will
      // redirect you to the restricted page again with the proper cookie.
      PRInt64 alreadyExistingOne;
      if (mBookmarksHash.Get(curID, &alreadyExistingOne))
        continue;

      if (! mBookmarksHash.Put(curID, aBookmarkID))
        return NS_ERROR_OUT_OF_MEMORY;

      // save for recursion later
      found.AppendElement(curID);
    }
  }

  // recurse on each found item now that we're done with the statement
  for (PRUint32 i = 0; i < found.Length(); i ++) {
    rv = RecursiveAddBookmarkHash(aBookmarkID, found[i], aMinTime);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavBookmarks::UpdateBookmarkHashOnRemove
//
//    Call this when a bookmark is removed. It will see if the bookmark still
//    exists anywhere in the system, and, if not, remove all references to it
//    in the bookmark hashtable.
//
//    The callback takes a pointer to what bookmark is being removed (as
//    an Int64 history page ID) as the userArg and removes all redirect
//    destinations that reference it.

PR_STATIC_CALLBACK(PLDHashOperator)
RemoveBookmarkHashCallback(nsTrimInt64HashKey::KeyType aKey,
                           PRInt64& aBookmark, void* aUserArg)
{
  const PRInt64* removeThisOne = NS_REINTERPRET_CAST(const PRInt64*, aUserArg);
  if (aBookmark == *removeThisOne)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}
nsresult
nsNavBookmarks::UpdateBookmarkHashOnRemove(PRInt64 aBookmarkId)
{
  // note we have to use the DB version here since the hashtable may be
  // out-of-date
  PRBool inDB;
  nsresult rv = IsBookmarkedInDatabase(aBookmarkId, &inDB);
  NS_ENSURE_SUCCESS(rv, rv);
  if (inDB)
    return NS_OK; // bookmark still exists, don't need to update hashtable

  // remove it
  mBookmarksHash.Enumerate(RemoveBookmarkHashCallback,
                           NS_REINTERPRET_CAST(void*, &aBookmarkId));
  return NS_OK;
}


// nsNavBookmarks::IsBookmarkedInDatabase
//
//    This checks to see if the specified URI is actually bookmarked, bypassing
//    our hashtable. Normal IsBookmarked checks just use the hashtable.

nsresult
nsNavBookmarks::IsBookmarkedInDatabase(PRInt64 aBookmarkID,
                                       PRBool *aIsBookmarked)
{
  // we'll just select position since it's just an int32 and may be faster.
  // We don't actually care about the data, just whether there is any.
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT position FROM moz_bookmarks WHERE item_child = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, aBookmarkID);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->ExecuteStep(aIsBookmarked);
}


nsresult
nsNavBookmarks::AdjustIndices(PRInt64 aFolder,
                              PRInt32 aStartIndex, PRInt32 aEndIndex,
                              PRInt32 aDelta)
{
  NS_ASSERTION(aStartIndex <= aEndIndex, "start index must be <= end index");

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks SET position = position + ");
  buffer.AppendInt(aDelta);
  buffer.AppendLiteral(" WHERE parent = ");
  buffer.AppendInt(aFolder);

  if (aStartIndex != 0) {
    buffer.AppendLiteral(" AND position >= ");
    buffer.AppendInt(aStartIndex);
  }
  if (aEndIndex != PR_INT32_MAX) {
    buffer.AppendLiteral(" AND position <= ");
    buffer.AppendInt(aEndIndex);
  }

  nsresult rv = DBConn()->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  RenumberItemsArray itemsArray;
  nsVoidArray *items = &itemsArray.items;
  {
    mozStorageStatementScoper scope(mDBGetChildren);
 
    rv = mDBGetChildren->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBGetChildren->BindInt32Parameter(1, aStartIndex + aDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBGetChildren->BindInt32Parameter(2, aEndIndex + aDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&results)) && results) {
      RenumberItem *item = new RenumberItem();
      if (!item) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (mDBGetChildren->IsNull(kGetChildrenIndex_ItemChild)) {
        item->folderChild = mDBGetChildren->AsInt64(kGetChildrenIndex_FolderChild);
      } else {
        nsCAutoString spec;
        mDBGetChildren->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, spec);
        rv = NS_NewURI(getter_AddRefs(item->itemURI), spec, nsnull);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      item->position = mDBGetChildren->AsInt32(kGetChildrenIndex_Position);
      if (!items->AppendElement(item)) {
        delete item;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  nsBookmarksUpdateBatcher batch;

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetPlacesRoot(PRInt64 *aRoot)
{
  *aRoot = mRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarksRoot(PRInt64 *aRoot)
{
  *aRoot = mBookmarksRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetToolbarRoot(PRInt64 *aRoot)
{
  *aRoot = mToolbarRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetTagRoot(PRInt64 *aRoot)
{
  *aRoot = mTagRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::InsertItem(PRInt64 aFolder, nsIURI *aItem, PRInt32 aIndex)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = History()->GetUrlIdFor(aItem, &childID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // see if this item is already in the folder
  PRBool hasItem = PR_FALSE;
  PRInt32 previousIndex = -1;
  { // scope the mDBIndexOfItem statement
    mozStorageStatementScoper scoper(mDBIndexOfItem);
    rv = mDBIndexOfItem->BindInt64Parameter(0, childID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBIndexOfItem->BindInt64Parameter(1, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBIndexOfItem->ExecuteStep(&hasItem);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasItem)
      previousIndex = mDBIndexOfItem->AsInt32(0);
  }

  PRInt32 index = (aIndex == -1) ? FolderCount(aFolder) : aIndex;

  if (hasItem && index == previousIndex)
    return NS_OK; // item already at its desired position: nothing to do

  if (hasItem) {
    // remove any old items
    rv = RemoveItem(aFolder, aItem);
    NS_ENSURE_SUCCESS(rv, rv);

    // since we just removed the item, everything after it shifts back by one
    if (index > previousIndex)
      index --;
  }

  rv = AdjustIndices(aFolder, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks (item_child, parent, position) VALUES (");
  buffer.AppendInt(childID);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(aFolder);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(index);
  buffer.AppendLiteral(")");

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  AddBookmarkToHash(childID, 0);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemAdded(aItem, aFolder, index))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveItem(PRInt64 aFolder, nsIURI *aItem)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = History()->GetUrlIdFor(aItem, &childID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (childID == 0) {
    return NS_OK; // the item isn't in history at all
  }

  PRInt32 childIndex;
  nsCAutoString buffer;
  {
    mozStorageStatementScoper scope(mDBIndexOfItem);
    mDBIndexOfItem->BindInt64Parameter(0, childID);
    mDBIndexOfItem->BindInt64Parameter(1, aFolder);

    PRBool results;
    rv = mDBIndexOfItem->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);

    // We _should_ always have a result here, but maybe we don't if the table
    // has become corrupted.  Just silently skip adjusting indices.
    childIndex = results ? mDBIndexOfItem->AsInt32(0) : -1;
  }

  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE parent = ");
  buffer.AppendInt(aFolder);
  buffer.AppendLiteral(" AND item_child = ");
  buffer.AppendInt(childID);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (childIndex != -1) {
    rv = AdjustIndices(aFolder, childIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = UpdateBookmarkHashOnRemove(childID);
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemRemoved(aItem, aFolder, childIndex))

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::ReplaceItem(PRInt64 aFolder, nsIURI *aItem, nsIURI *aNewItem)
{
  mozIStorageConnection *dbConn = DBConn();
  nsNavHistory *history = History();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = history->GetUrlIdFor(aItem, &childID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (childID == 0) {
    return NS_ERROR_INVALID_ARG; // the item isn't in history at all
  }

  PRInt64 newChildID;
  rv = history->GetUrlIdFor(aNewItem, &newChildID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(newChildID != 0, "must have an item id");

  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks SET item_child = ");
  buffer.AppendInt(newChildID);
  buffer.AppendLiteral(" WHERE item_child = ");
  buffer.AppendInt(childID);
  buffer.AppendLiteral(" AND parent = ");
  buffer.AppendInt(aFolder);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // update the bookmark hash, something could have gone away, and something
  // else could have been created
  rv = UpdateBookmarkHashOnRemove(childID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = AddBookmarkToHash(newChildID, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemReplaced(aFolder, aItem, aNewItem))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::CreateFolder(PRInt64 aParent, const nsAString &aName,
                             PRInt32 aIndex, PRInt64 *aNewFolder)
{
  // CreateFolderWithID returns the index of the new folder, but that's not
  // used here.  To avoid any risk of corrupting data should this function
  // be changed, we'll use a local variable to hold it.  The PR_TRUE argument
  // will cause notifications to be sent to bookmark observers.
  PRInt32 localIndex = aIndex;
  return CreateFolderWithID(-1, aParent, aName, PR_TRUE, &localIndex, aNewFolder);
}

NS_IMETHODIMP
nsNavBookmarks::CreateContainer(PRInt64 aParent, const nsAString &aName,
                                const nsAString &aType, PRInt32 aIndex,
                                PRInt64 *aNewFolder)
{
  return CreateContainerWithID(-1, aParent, aName, aType, aIndex, aNewFolder);
}

nsresult
nsNavBookmarks::CreateFolderWithID(PRInt64 aFolder, PRInt64 aParent,
                                   const nsAString& aName,
                                   PRBool aSendNotifications,
                                   PRInt32* aIndex, PRInt64* aNewFolder)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (*aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt32 index = (*aIndex == -1) ? FolderCount(aParent) : *aIndex;

  nsresult rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<mozIStorageStatement> statement;
    if (aFolder == -1) {
      rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_folders (name, type) VALUES (?1, null)"),
                                   getter_AddRefs(statement));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = statement->BindStringParameter(0, aName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_folders (id, name, type) VALUES (?1, ?2, null)"),
                                   getter_AddRefs(statement));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = statement->BindInt64Parameter(0, aFolder);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindStringParameter(1, aName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt64 child;
  rv = dbConn->GetLastInsertRowID(&child);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks (folder_child, parent, position) VALUES (");
  buffer.AppendInt(child);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(aParent);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(index);
  buffer.AppendLiteral(")");
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // When creating a livemark container, we need to delay sending notifications
  // until the container type has been set.  In that case, they'll be sent by
  // CreateContainerWithID rather than here.
  if (aSendNotifications) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnFolderAdded(child, aParent, index))
  }

  *aIndex = index;
  *aNewFolder = child;
  return NS_OK;
}

nsresult 
nsNavBookmarks::CreateContainerWithID(PRInt64 aFolder, PRInt64 aParent, 
                                      const nsAString &aName, const nsAString &aType, 
                                      PRInt32 aIndex, PRInt64 *aNewFolder)
{
  // Containers are wrappers around read-only folders, with a specific type.
  // CreateFolderWithID will return the index of the newly created folder,
  // which we will need later on in order to send notifications.  The PR_FALSE
  // argument disables sending notifiactions, since we need to defer that until
  // the folder type has been set.
  PRInt32 localIndex = aIndex;
  nsresult rv = CreateFolderWithID(aFolder, aParent, aName, PR_FALSE, &localIndex, aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the type.
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks_folders SET type = ?2 WHERE id = ?1"),
                                 getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, *aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(1, aType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Send notifications after folder type has been set.
  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderAdded(*aNewFolder, aParent, localIndex))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::InsertSeparator(PRInt64 aParent, PRInt32 aIndex)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt32 index = (aIndex == -1) ? FolderCount(aParent) : aIndex;

  nsresult rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> statement;
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks "
                                          "(parent, position) VALUES (?1,?2)"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, index);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnSeparatorAdded(aParent, index))

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveChildAt(PRInt64 aParent, PRInt32 aIndex)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);
  nsresult rv;
  PRInt64 item, folder;

  {
    mozStorageStatementScoper scope(mDBGetChildAt);
    rv = mDBGetChildAt->BindInt64Parameter(0, aParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildAt->BindInt32Parameter(1, aIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    rv = mDBGetChildAt->ExecuteStep(&hasMore);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasMore) {
      // Child doesn't exist
      return NS_ERROR_INVALID_ARG;
    }

    if (mDBGetChildAt->IsNull(0)) {
      item = 0;
      folder = mDBGetChildAt->AsInt64(1);
    } else {
      folder = 0;
      item = mDBGetChildAt->AsInt64(0);
    }
  }

  if (item != 0) {
    // We're removing an item, go find its URI.
    mozIStorageStatement *pageInfo = History()->DBGetIdPageInfo();
    nsCOMPtr<nsIURI> uri;
    {
      mozStorageStatementScoper scope(pageInfo);
      rv = pageInfo->BindInt64Parameter(0, item);
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool hasMore;
      rv = pageInfo->ExecuteStep(&hasMore);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!hasMore) {
        return NS_ERROR_INVALID_ARG;
      }
      nsCAutoString spec;
      rv = pageInfo->GetUTF8String(1, spec);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_NewURI(getter_AddRefs(uri), spec);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Commit this transaction so that we don't notify observers mid-tranaction
    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    return RemoveItem(aParent, uri);
  }
  if (folder != 0) {
    // Commit this transaction so that we don't notify observers mid-tranaction
    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    return RemoveFolder(folder);
  }

  // No item or folder, so this is a separator.
  nsCOMPtr<mozIStorageStatement> statement;
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_bookmarks WHERE parent = ?1 AND position = ?2"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AdjustIndices(aParent, aIndex + 1, PR_INT32_MAX, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnSeparatorRemoved(aParent, aIndex))
  return NS_OK;
}

nsresult 
nsNavBookmarks::GetParentAndIndexOfFolder(PRInt64 aFolder, PRInt64* aParent, 
                                          PRInt32* aIndex)
{
  nsCAutoString buffer;
  buffer.AssignLiteral("SELECT parent, position FROM moz_bookmarks WHERE folder_child = ");
  buffer.AppendInt(aFolder);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn()->CreateStatement(buffer, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results) {
    return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
  }

  *aParent = statement->AsInt64(0);
  *aIndex = statement->AsInt32(1);
  
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveFolder(PRInt64 aFolder)
{
  // If this is a container bookmark, try to notify its service.
  nsresult rv;
  nsCAutoString folderType;
  rv = GetFolderType(aFolder, folderType);
  NS_ENSURE_SUCCESS(rv, rv);
  if (folderType.Length() > 0) {
    // There is a type associated with this folder; it's a livemark.
    nsCOMPtr<nsIRemoteContainer> bmcServ = do_GetService(folderType.get());
    if (bmcServ) {
      rv = bmcServ->OnContainerRemoving(aFolder);
      if (NS_FAILED(rv))
        NS_WARNING("Remove folder container notification failed.");
    }
  }

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 parent;
  PRInt32 index;
  rv = GetParentAndIndexOfFolder(aFolder, &parent, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove all of the folder's children
  RemoveFolderChildren(aFolder);

  // Remove the folder from its parent
  nsCAutoString buffer;
  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE folder_child = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // And remove the folder from the folder table
  buffer.AssignLiteral("DELETE FROM moz_bookmarks_folders WHERE id = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AdjustIndices(parent, index + 1, PR_INT32_MAX, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderRemoved(aFolder, parent, index))

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsNavBookmarks::RemoveFolderTransaction, nsITransaction)

NS_IMETHODIMP
nsNavBookmarks::GetRemoveFolderTransaction(PRInt64 aFolder, nsITransaction** aResult)
{
  // Create and initialize a RemoveFolderTransaction object that can be used to
  // recreate the folder safely later. 

  nsAutoString title;
  nsresult rv = GetFolderTitle(aFolder, title);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 parent;
  PRInt32 index;
  rv = GetParentAndIndexOfFolder(aFolder, &parent, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString type;
  rv = GetFolderType(aFolder, type);
  NS_ENSURE_SUCCESS(rv, rv);

  RemoveFolderTransaction* rft = 
    new RemoveFolderTransaction(aFolder, parent, title, index, type);
  if (!rft)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult = rft);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveFolderChildren(PRInt64 aFolder)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsTArray<PRInt32> separatorChildren; // separator indices
  nsTArray<PRInt64> folderChildren;
  nsCOMArray<nsIURI> itemChildren;
  nsresult rv;
  {
    mozStorageStatementScoper scope(mDBGetChildren);
    rv = mDBGetChildren->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildren->BindInt32Parameter(1, 0);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildren->BindInt32Parameter(2, PR_INT32_MAX);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&hasMore)) && hasMore) {
      PRBool isFolder = ! mDBGetChildren->IsNull(kGetChildrenIndex_FolderChild);
      if (isFolder) {
        // folder
        folderChildren.AppendElement(
            mDBGetChildren->AsInt64(kGetChildrenIndex_FolderChild));
      } else if (mDBGetChildren->IsNull(kGetChildrenIndex_ItemChild)) {
        // separator
        separatorChildren.AppendElement(mDBGetChildren->AsInt32(kGetChildrenIndex_Position));
      } else {
        // item (URI)
        nsCOMPtr<nsIURI> uri;
        nsCAutoString spec;
        mDBGetChildren->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, spec);
        rv = NS_NewURI(getter_AddRefs(uri), spec);
        NS_ENSURE_SUCCESS(rv, rv);
        PRBool success = itemChildren.AppendObject(uri);
        NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
      }
    }
  }

  // Remove separators.  The list of separators will already be sorted since
  // we order by position, so by enumerating it backwards, we avoid having to
  // deal with shifting indices.
  PRUint32 i;
  for (i = separatorChildren.Length() - 1; i != PRUint32(-1); --i) {
    rv = RemoveChildAt(aFolder, separatorChildren[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // remove folders
  for (i = 0; i < folderChildren.Length(); ++i) {
    rv = RemoveFolder(folderChildren[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // remove items
  for (i = 0; i < PRUint32(itemChildren.Count()); ++i) {
    rv = RemoveItem(aFolder, itemChildren[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  transaction.Commit();
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::MoveFolder(PRInt64 aFolder, PRInt64 aNewParent, PRInt32 aIndex)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT parent, position FROM moz_bookmarks WHERE folder_child = ?1"),
                                        getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 parent;
  PRInt32 oldIndex;
  {
    mozStorageStatementScoper scope(statement);
    rv = statement->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    parent = statement->AsInt64(0);
    oldIndex = statement->AsInt32(1);
  }

  // Make sure aNewParent is not aFolder or a subfolder of aFolder
  {
    mozStorageStatementScoper scope(statement);
    PRInt64 p = aNewParent;

    while (p) {
      if (p == aFolder) {
        return NS_ERROR_INVALID_ARG;
      }

      rv = statement->BindInt64Parameter(0, p);
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool results;
      rv = statement->ExecuteStep(&results);
      NS_ENSURE_SUCCESS(rv, rv);
      p = results ? statement->AsInt64(0) : 0;
    }
  }

  PRInt32 newIndex;
  if (aIndex == -1) {
    newIndex = FolderCount(aNewParent);
    // If the parent remains the same, then the folder is really being moved
    // to count - 1 (since it's being removed from the old position)
    if (parent == aNewParent) {
      --newIndex;
    }
  } else {
    newIndex = aIndex;

    if (parent == aNewParent && newIndex > oldIndex) {
      // when an item is being moved lower in the same folder, the new index
      // refers to the index before it was removed. Removal causes everything
      // to shift up.
      --newIndex;
    }
  }

  if (aNewParent == parent && newIndex == oldIndex) {
    // Nothing to do!
    return NS_OK;
  }

  // First we remove the item from its old position.
  nsCAutoString buffer;
  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE folder_child = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now renumber to account for the move
  if (parent == aNewParent) {
    // We can optimize the updates if moving within the same container.
    // We only shift the items between the old and new positions, since the
    // insertion will offset the deletion.
    if (oldIndex > newIndex) {
      rv = AdjustIndices(parent, newIndex, oldIndex - 1, 1);
    } else {
      rv = AdjustIndices(parent, oldIndex + 1, newIndex, -1);
    }
  } else {
    // We're moving between containers, so this happens in two steps.
    // First, fill the hole from the deletion.
    rv = AdjustIndices(parent, oldIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    // Now, make room in the new parent for the insertion.
    rv = AdjustIndices(aNewParent, newIndex, PR_INT32_MAX, 1);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
         "INSERT INTO moz_bookmarks (folder_child, parent, position) "
         "VALUES (?1, ?2, ?3)"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt64Parameter(1, aNewParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(2, newIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // notify bookmark observers
  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderMoved(aFolder, parent, oldIndex,
                                    aNewParent, newIndex))

  // notify remote container provider if there is one
  nsCAutoString type;
  rv = GetFolderType(aFolder, type);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! type.IsEmpty()) {
    nsCOMPtr<nsIRemoteContainer> container =
      do_GetService(type.get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = container->OnContainerMoved(aFolder, aNewParent, newIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetChildFolder(PRInt64 aFolder, const nsAString& aSubFolder,
                               PRInt64* _result)
{
  // note: we allow empty folder names
  nsresult rv;
  if (aFolder == 0)
    return NS_ERROR_INVALID_ARG;

  // If this gets used a lot, we'll want a precompiled statement
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("SELECT c.id FROM moz_bookmarks a JOIN moz_bookmarks_folders c ON a.folder_child = c.id WHERE a.parent = ?1 AND c.name = ?2"),
                                 getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  statement->BindInt64Parameter(0, aFolder);
  statement->BindStringParameter(1, aSubFolder);

  PRBool hasResult = PR_FALSE;
  rv = statement->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (! hasResult) {
    // item not found
    *_result = 0;
    return NS_OK;
  }

  return statement->GetInt64(0, _result);
}

NS_IMETHODIMP
nsNavBookmarks::SetItemTitle(nsIURI *aURI, const nsAString &aTitle)
{
  return History()->SetPageUserTitle(aURI, aTitle);
}


NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(nsIURI *aURI, nsAString &aTitle)
{
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  history->SyncDB();

  mozIStorageStatement *statement = DBGetURLPageInfo();
  nsresult rv = BindStatementURI(statement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatementScoper scope(statement);

  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    aTitle.SetIsVoid(PR_TRUE);
    return NS_OK;
  }

  // First check for a user title
  if (!statement->IsNull(nsNavHistory::kGetInfoIndex_UserTitle))
    return statement->GetString(nsNavHistory::kGetInfoIndex_UserTitle, aTitle);

  // If there is no user title, check for a history title.
  return statement->GetString(nsNavHistory::kGetInfoIndex_Title, aTitle);
}

NS_IMETHODIMP
nsNavBookmarks::SetFolderTitle(PRInt64 aFolder, const nsAString &aTitle)
{
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks_folders SET name = ?2 WHERE id = ?1"),
                                 getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(1, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderChanged(aFolder, NS_LITERAL_CSTRING("title")))

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetFolderTitle(PRInt64 aFolder, nsAString &aTitle)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  nsresult rv = mDBGetFolderInfo->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return mDBGetFolderInfo->GetString(kGetFolderInfoIndex_Title, aTitle);
}


nsresult
nsNavBookmarks::GetFolderType(PRInt64 aFolder, nsACString &aType)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  nsresult rv = mDBGetFolderInfo->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return mDBGetFolderInfo->GetUTF8String(kGetFolderInfoIndex_Type, aType);
}

nsresult
nsNavBookmarks::ResultNodeForFolder(PRInt64 aID,
                                    nsNavHistoryQueryOptions *aOptions,
                                    nsNavHistoryResultNode **aNode)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  mDBGetFolderInfo->BindInt64Parameter(0, aID);

  PRBool results;
  nsresult rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(results, "ResultNodeForFolder expects a valid folder id");

  // type (empty for normal ones, nonempty for container providers)
  nsCAutoString folderType;
  rv = mDBGetFolderInfo->GetUTF8String(kGetFolderInfoIndex_Type, folderType);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  nsCAutoString title;
  rv = mDBGetFolderInfo->GetUTF8String(kGetFolderInfoIndex_Title, title);

  *aNode = new nsNavHistoryFolderResultNode(title, aOptions, aID, folderType);
  if (! *aNode)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetFolderURI(PRInt64 aFolder, nsIURI **aURI)
{
  // Create a query for the folder; the URI is the querystring from that
  // query. We could create a proper query and serialize it, which might
  // make it less prone to breakage since we'd only have one code path.
  // However, this gets called a lot (every time we make a folder node)
  // and constructing fake queries and options each time just to
  // serialize them would be a waste. Therefore, we just synthesize the
  // correct string here.
  //
  // If you change this, change IsSimpleFolderURI which detects this string.
  nsCAutoString spec("place:folder=");
  spec.AppendInt(aFolder);
  spec.AppendLiteral("&group=3"); // GROUP_BY_FOLDER
  return NS_NewURI(aURI, spec);
}

NS_IMETHODIMP
nsNavBookmarks::GetFolderReadonly(PRInt64 aFolder, PRBool *aResult)
{
  // Ask the folder's nsIRemoteContainer for the readonly property.
  *aResult = PR_FALSE;
  nsCAutoString type;
  nsresult rv = GetFolderType(aFolder, type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type.IsEmpty()) {
    nsCOMPtr<nsIRemoteContainer> container =
      do_GetService(type.get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = container->GetChildrenReadOnly(aResult);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
nsNavBookmarks::QueryFolderChildren(PRInt64 aFolderId,
                                    nsNavHistoryQueryOptions *aOptions,
                                    nsCOMArray<nsNavHistoryResultNode> *aChildren)
{
  nsresult rv;
  mozStorageStatementScoper scope(mDBGetChildren);

  rv = mDBGetChildren->BindInt64Parameter(0, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBGetChildren->BindInt32Parameter(1, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBGetChildren->BindInt32Parameter(2, PR_INT32_MAX);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;

  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions, &rv);
  PRInt32 index = -1;
  while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&results)) && results) {

    // The results will be in order of index. Even if we don't add a node
    // because it was excluded, we need to count it's index, so do that
    // before doing anything else. Index was initialized to -1 above, so
    // it will start counting at 0 the first time through the loop.
    index ++;

    PRBool isFolder = !mDBGetChildren->IsNull(kGetChildrenIndex_FolderChild);
    nsCOMPtr<nsNavHistoryResultNode> node;
    if (isFolder) {
      PRInt64 folder = mDBGetChildren->AsInt64(kGetChildrenIndex_FolderChild);

      if (options->ExcludeReadOnlyFolders()) {
        // see if it's read only and skip it
        PRBool readOnly = PR_FALSE;
        GetFolderReadonly(folder, &readOnly);
        if (readOnly)
          continue; // skip
      }

      rv = ResultNodeForFolder(folder, aOptions, getter_AddRefs(node));
      if (NS_FAILED(rv))
        continue;
    } else if (mDBGetChildren->IsNull(kGetChildrenIndex_ItemChild)) {
      // separator
      if (aOptions->ExcludeItems()) {
        continue;
      }
      node = new nsNavHistorySeparatorResultNode();
      NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);
    } else {
      rv = History()->RowToResult(mDBGetChildren, options,
                                  getter_AddRefs(node));
      NS_ENSURE_SUCCESS(rv, rv);

      PRUint32 nodeType;
      node->GetType(&nodeType);
      if ((nodeType == nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
           aOptions->ExcludeQueries()) ||
          (nodeType != nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
           nodeType != nsINavHistoryResultNode::RESULT_TYPE_FOLDER &&
           aOptions->ExcludeItems())) {
        continue;
      }
    }

    // this method fills all bookmark queries, so we store the index of the
    // item in its parent
    node->mBookmarkIndex = index;

    NS_ENSURE_TRUE(aChildren->AppendObject(node), NS_ERROR_OUT_OF_MEMORY);
  }
  return NS_OK;
}

PRInt32
nsNavBookmarks::FolderCount(PRInt64 aFolder)
{
  mozStorageStatementScoper scope(mDBFolderCount);

  nsresult rv = mDBFolderCount->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, 0);

  PRBool results;
  rv = mDBFolderCount->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  return mDBFolderCount->AsInt32(0);
}

NS_IMETHODIMP
nsNavBookmarks::IsBookmarked(nsIURI *aURI, PRBool *aBookmarked)
{
  nsNavHistory* history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);

  // convert the URL to an ID
  PRInt64 urlID;
  nsresult rv = history->GetUrlIdFor(aURI, &urlID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! urlID) {
    // never seen this before, not even in history
    *aBookmarked = PR_FALSE;
    return NS_OK;
  }

  PRInt64 bookmarkedID;
  PRBool foundOne = mBookmarksHash.Get(urlID, &bookmarkedID);

  // IsBookmarked only tests if this exact URI is bookmarked, so we need to
  // check that the destination matches
  if (foundOne)
    *aBookmarked = (urlID == bookmarkedID);
  else
    *aBookmarked = PR_FALSE;

#ifdef DEBUG
  // sanity check for the bookmark hashtable
  PRBool realBookmarked;
  rv = IsBookmarkedInDatabase(urlID, &realBookmarked);
  NS_ASSERTION(realBookmarked == *aBookmarked,
               "Bookmark hash table out-of-sync with the database");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkedURIFor(nsIURI* aURI, nsIURI** _retval)
{
  *_retval = nsnull;

  nsNavHistory* history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);

  // convert the URL to an ID
  PRInt64 urlID;
  nsresult rv = history->GetUrlIdFor(aURI, &urlID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! urlID) {
    // never seen this before, not even in history, leave result NULL
    return NS_OK;
  }

  PRInt64 bookmarkID;
  if (mBookmarksHash.Get(urlID, &bookmarkID)) {
    // found one, convert ID back to URL. This statement is NOT refcounted
    mozIStorageStatement* statement = history->DBGetIdPageInfo();
    NS_ENSURE_TRUE(statement, NS_ERROR_UNEXPECTED);
    mozStorageStatementScoper scoper(statement);

    rv = statement->BindInt64Parameter(0, bookmarkID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    if (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
      nsCAutoString spec;
      statement->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, spec);
      return NS_NewURI(_retval, spec);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::ChangeBookmarkURI(nsIURI *aOldURI, nsIURI *aNewURI)
{
  nsresult rv;

  if (!aOldURI || !aNewURI)
    return NS_ERROR_NULL_POINTER;

  // This method is only meaningful if oldURI is actually bookmarked.
  PRBool bookmarked = PR_FALSE;
  rv = IsBookmarked(aOldURI, &bookmarked);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!bookmarked)
    return NS_ERROR_INVALID_ARG;

  PRBool equal = PR_FALSE;
  rv = aOldURI->Equals(aNewURI, &equal);
  NS_ENSURE_SUCCESS(rv, rv);
  if (equal) // no-op
    return NS_OK;

  // Now that we're satisfied with the quality of our input, the
  // actual work starts here.

  nsTArray<PRInt64> folders;
  rv = GetBookmarkFoldersTArray(aOldURI, &folders);
  NS_ENSURE_SUCCESS(rv, rv);

  // The bookmark operations within the area in which "batch" is in scope
  // will be within a batched operation.  This allows the batch to be
  // closed properly if we exit early due to an error condition.
  nsBookmarksUpdateBatcher batch;

  // in folders, replace all instances of old URI with new URI
  for (PRUint32 i = 0; i < folders.Length(); i++) {
    rv = ReplaceItem(folders[i], aOldURI, aNewURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // copy title from old URI to new URI
  nsAutoString title;
  rv = GetItemTitle(aOldURI, title);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!title.IsEmpty()) {
    rv = SetItemTitle(aNewURI, title);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // copy keyword (shortcut) from old URI to new URI
  nsAutoString keyword;
  rv = GetKeywordForURI(aOldURI, keyword);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!keyword.IsEmpty()) {
    rv = SetKeywordForURI(aNewURI, keyword);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // copy annotations from old URI to new URI
  nsAnnotationService* annoService =
    nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annoService, NS_ERROR_UNEXPECTED);
  rv = annoService->CopyAnnotations(aOldURI, aNewURI, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // associate favicon from old URI (if present) with new URI (if none already)
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_UNEXPECTED);
  nsCOMPtr<nsIURI> sourceFaviconURI;
  rv = faviconService->GetFaviconForPage(aOldURI, 
                                         getter_AddRefs(sourceFaviconURI));
  if (NS_SUCCEEDED(rv)) { // oldURI has favicon
    nsCOMPtr<nsIURI> destFaviconURI;
    rv = faviconService->GetFaviconForPage(aNewURI,
                                           getter_AddRefs(destFaviconURI));
    if (NS_FAILED(rv)) {  // newURI has no favicon
      rv = faviconService->SetFaviconUrlForPage(aNewURI, sourceFaviconURI);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkFoldersTArray(nsIURI *aURI,
                                         nsTArray<PRInt64> *aResult)
{
  mozStorageStatementScoper scope(mDBFindURIBookmarks);
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsresult rv = BindStatementURI(mDBFindURIBookmarks, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool more;
  while (NS_SUCCEEDED((rv = mDBFindURIBookmarks->ExecuteStep(&more))) && more) {
    if (! aResult->AppendElement(
        mDBFindURIBookmarks->AsInt64(kFindBookmarksIndex_Parent)))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  return transaction.Commit();
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkFolders(nsIURI *aURI, PRUint32 *aCount,
                                   PRInt64 **aFolders)
{
  *aCount = 0;
  *aFolders = nsnull;
  nsresult rv;
  nsTArray<PRInt64> folders;

  // Get the information from the DB as a TArray
  rv = GetBookmarkFoldersTArray(aURI, &folders);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the results into a new array for output
  if (folders.Length()) {
    *aFolders = NS_STATIC_CAST(PRInt64*,
                           nsMemory::Alloc(sizeof(PRInt64) * folders.Length()));
    if (! *aFolders)
      return NS_ERROR_OUT_OF_MEMORY;
    for (PRUint32 i = 0; i < folders.Length(); i ++)
      (*aFolders)[i] = folders[i];
  }
  *aCount = folders.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::IndexOfItem(PRInt64 aFolder, nsIURI *aItem, PRInt32 *aIndex)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  PRInt64 id;
  nsresult rv = History()->GetUrlIdFor(aItem, &id, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (id == 0) {
    *aIndex = -1;
    return NS_OK;
  }

  mozStorageStatementScoper scope(mDBIndexOfItem);
  mDBIndexOfItem->BindInt64Parameter(0, id);
  mDBIndexOfItem->BindInt64Parameter(1, aFolder);
  PRBool results;
  rv = mDBIndexOfItem->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results) {
    *aIndex = -1;
    return NS_OK;
  }

  *aIndex = mDBIndexOfItem->AsInt32(0);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::IndexOfFolder(PRInt64 aParent,
                              PRInt64 aFolder, PRInt32 *aIndex)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  mozStorageStatementScoper scope(mDBIndexOfFolder);
  mDBIndexOfFolder->BindInt64Parameter(0, aFolder);
  mDBIndexOfFolder->BindInt64Parameter(1, aParent);
  PRBool results;
  nsresult rv = mDBIndexOfFolder->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results) {
    *aIndex = -1;
    return NS_OK;
  }

  *aIndex = mDBIndexOfFolder->AsInt32(0);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::SetKeywordForURI(nsIURI* aURI, const nsAString& aKeyword)
{
  nsresult rv;
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsNavHistory *history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);
  PRInt64 pageId;
  nsCOMPtr<mozIStorageStatement> statement;

  // Shortcuts are always lowercased internally.
  nsAutoString kwd(aKeyword);
  ToLowerCase(kwd);

  // When we are clearing a keyword, don't force URI creation. If they give
  // us a brand new URI and an empty keyword, there is obviously nothing to do
  PRBool forceURICreation = ! kwd.IsEmpty();
  rv = history->GetUrlIdFor(aURI, &pageId, forceURICreation);
  if (! pageId)
    return NS_OK; // no keyword & URL not found: nothing to do

  if (aURI) {
    // delete any existing keyword for the given URI, only create the ID if
    rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM moz_keywords WHERE page_id = ?1"),
      getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt64Parameter(0, pageId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (! kwd.IsEmpty()) {
    rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM moz_keywords WHERE keyword = ?1"),
      getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(0, kwd);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // when either is empty, that was asking us to clear the value, and we're done
  if (! aURI || kwd.IsEmpty())
    return transaction.Commit();

  // otherwise, we have a URI/keyword pair and want to create it...

  // this statement will overwrite any old keyword with that value since the
  // keyword column is unique and we use "OR REPLACE" conflict resolution
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT OR REPLACE INTO moz_keywords (keyword, page_id) VALUES (?1, ?2)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(0, kwd);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(1, pageId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return transaction.Commit();
}

NS_IMETHODIMP
nsNavBookmarks::GetKeywordForURI(nsIURI* aURI, nsAString& aKeyword)
{
  aKeyword.Truncate(0);

  mozStorageStatementScoper scoper(mDBGetKeywordForURI);
  nsresult rv = BindStatementURI(mDBGetKeywordForURI, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetKeywordForURI->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || ! hasMore) {
    aKeyword.SetIsVoid(PR_TRUE);
    return NS_OK; // not found: return void keyword string
  }

  // found, get the keyword
  return mDBGetKeywordForURI->GetString(0, aKeyword);
}

NS_IMETHODIMP
nsNavBookmarks::GetURIForKeyword(const nsAString& aKeyword, nsIURI** aURI)
{
  *aURI = nsnull;
  if (aKeyword.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  // Shortcuts are always lowercased internally.
  nsAutoString kwd(aKeyword);
  ToLowerCase(kwd);

  mozStorageStatementScoper scoper(mDBGetURIForKeyword);
  nsresult rv = mDBGetURIForKeyword->BindStringParameter(0, kwd);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetURIForKeyword->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || ! hasMore)
    return NS_OK; // not found: leave URI null

  // found, get the URI
  nsCAutoString spec;
  rv = mDBGetURIForKeyword->GetUTF8String(0, spec);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_NewURI(aURI, spec);
}

NS_IMETHODIMP
nsNavBookmarks::BeginUpdateBatch()
{
  if (mBatchLevel++ == 0) {
    mozIStorageConnection* conn = DBConn();
    PRBool transactionInProgress = PR_TRUE; // default to no transaction on err
    conn->GetTransactionInProgress(&transactionInProgress);
    mBatchHasTransaction = ! transactionInProgress;
    if (mBatchHasTransaction)
      conn->BeginTransaction();

    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnBeginUpdateBatch())
  }
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::EndUpdateBatch()
{
  if (--mBatchLevel == 0) {
    if (mBatchHasTransaction)
      DBConn()->CommitTransaction();
    mBatchHasTransaction = PR_FALSE;
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnEndUpdateBatch())
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::AddObserver(nsINavBookmarkObserver *aObserver,
                            PRBool aOwnsWeak)
{
  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}

NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver *aObserver)
{
  return mObservers.RemoveWeakElement(aObserver);
}

// nsNavBookmarks::nsINavHistoryObserver

NS_IMETHODIMP
nsNavBookmarks::OnBeginUpdateBatch()
{
  // These aren't passed through to bookmark observers currently.
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnEndUpdateBatch()
{
  // These aren't passed through to bookmark observers currently.
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnVisit(nsIURI *aURI, PRInt64 aVisitID, PRTime aTime,
                        PRInt64 aSessionID, PRInt64 aReferringID,
                        PRUint32 aTransitionType)
{
  // If the page is bookmarked, we need to notify observers
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnItemVisited(aURI, aVisitID, aTime))
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnDeleteURI(nsIURI *aURI)
{
  // If the page is bookmarked, we need to notify observers
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnItemChanged(aURI, NS_LITERAL_CSTRING("cleartime"),
                                      EmptyString()))
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnClearHistory()
{
  // TODO(bryner): we should notify on visited-time change for all URIs
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,
                               const nsAString& aUserTitle,
                               PRBool aIsUserTitleChanged)
{
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    if (aUserTitle.IsVoid()) {
      // use "real" title because the user title is NULL. Either the user title
      // was "unset" or the page title changed.
      ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                          OnItemChanged(aURI, NS_LITERAL_CSTRING("title"),
                                        aPageTitle));
    } else if (aIsUserTitleChanged) {
      // there is a user title and it changed
      ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                          OnItemChanged(aURI, NS_LITERAL_CSTRING("title"),
                                        aUserTitle));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnPageChanged(nsIURI *aURI, PRUint32 aWhat,
                              const nsAString &aValue)
{
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    if (aWhat == nsINavHistoryObserver::ATTRIBUTE_FAVICON) {
      ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                          OnItemChanged(aURI, NS_LITERAL_CSTRING("favicon"),
                                        aValue));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnPageExpired(nsIURI* aURI, PRTime aVisitTime,
                              PRBool aWholeEntry)
{
  // pages that are bookmarks shouldn't expire, so we don't need to handle it
  return NS_OK;
}
