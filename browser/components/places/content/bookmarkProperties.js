//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Places Bookmark Properties.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hughes <jhughes@google.com>
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

#include controller.js
#include ../../../../toolkit/content/debug.js

var BookmarkPropertiesPanel = {

  /** UI Text Strings */

  __strings: null,
  get _strings() {
    if (!this.__strings) {
      this.__strings = document.getElementById("stringBundle");
    }
    return this.__strings;
  },

  /**
   * The Bookmarks Service.
   */
  __bms: null,
  get _bms() {
    if (!this.__bms) {
      this.__bms =
        Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
        getService(Ci.nsINavBookmarksService);
    }
    return this.__bms;
  },

  /**
   * The Nav History Service.
   */
  __hist: null,
  get _hist() {
    if (!this.__hist) {
      this.__hist =
        Cc["@mozilla.org/browser/nav-history-service;1"].
        getService(Ci.nsINavHistoryService);
    }
    return this.__hist;
  },

  /**
   * The I/O Service, useful for creating nsIURIs from strings.
   */
  __ios: null,
  get _ios() {
    if (!this.__ios) {
      this.__ios =
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    }
    return this.__ios;
  },

  /**
   * The Live Bookmark service for dealing with syndication feed folders.
   */
  __livemarks: null,
  get _livemarks() {
    if (!this.__livemarks) {
      this.__livemarks =
        Cc["@mozilla.org/browser/livemark-service;1"].
        getService(Ci.nsILivemarkService);
    }
    return this.__livemarks;
  },

  /**
   * The Microsummary Service for displaying microsummaries.
   */
  __mss: null,
  get _mss() {
    if (!this.__mss)
      this.__mss = Cc["@mozilla.org/microsummary/service;1"].
                  getService(Ci.nsIMicrosummaryService);
    return this.__mss;
  },

  _bookmarkURI: null,
  _bookmarkTitle: "",
  _microsummaries: null,
  _dialogWindow: null,
  _parentWindow: null,
  _controller: null,

  EDIT_BOOKMARK_VARIANT: 0,
  ADD_BOOKMARK_VARIANT: 1,
  EDIT_FOLDER_VARIANT:  2,
  ADD_MULTIPLE_BOOKMARKS_VARIANT: 3,
  ADD_LIVEMARK_VARIANT: 4,
  EDIT_LIVEMARK_VARIANT: 5,

  /**
   * The variant identifier for the current instance of the dialog.
   * The possibilities are enumerated by the constants above.
   */
  _variant: null,

  _isVariant: function BPP__isVariant(variant) {
    return this._variant == variant;
  },

  /**
   * Returns true if this variant of the dialog uses a URI as a primary
   * identifier for the item being edited.
   */

  _identifierIsURI: function BPP__identifierIsURI() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if this variant of the dialog uses a folder ID  as a primary
   * identifier for the item being edited.
   */

  _identifierIsFolderID: function BPP__identifierIsFolderID() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return true;
    default:
      return false;
    }
  },

  /**
   * Returns true if the URI is editable in this variant of the dialog.
   */
  _isURIEditable: function BPP__isURIEditable() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if the shortcut field is visible in this
   * variant of the dialog.
   */
  _isShortcutVisible: function BPP__isShortcutVisible() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if the livemark feed and site URI fields are visible.
   */

  _areLivemarkURIsVisible: function BPP__areLivemarkURIsVisible() {
    switch(this._variant) {
    case this.ADD_LIVEMARK_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return true;
    default:
      return false;
    }
  },

  /**
   * Returns true if the microsummary field is visible in this variant
   * of the dialog.
   */
  _isMicrosummaryVisible: function BPP__isMicrosummaryVisible() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if bookmark deletion is possible from the current
   * variant of the dialog.
   */
  _isDeletePossible: function BPP__isDeletePossible() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.EDIT_FOLDER_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if the URI's folder is editable in this variant
   * of the dialog.
   */
  _isFolderEditable: function BPP__isFolderEditable() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
      return true;
    default:
      return false;
    }
  },

  /**
   * This method returns the correct label for the dialog's "accept"
   * button based on the variant of the dialog.
   */
  _getAcceptLabel: function BPP__getAcceptLabel() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
      return this._strings.getString("dialogAcceptLabelAdd");
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
      return this._strings.getString("dialogAcceptLabelAddMulti");
    default:
      return this._strings.getString("dialogAcceptLabelEdit");
    }
  },

  /**
   * This method returns the correct title for the current variant
   * of this dialog.
   */
  _getDialogTitle: function BPP__getDialogTitle() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
      return this._strings.getString("dialogTitleAdd");
    case this.EDIT_FOLDER_VARIANT:
      return this._strings.getString("dialogTitleFolderEdit");
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
      return this._strings.getString("dialogTitleAddMulti");
    case this.ADD_LIVEMARK_VARIANT:
      return this._strings.getString("dialogTitleAddLivemark");
    default:
      return this._strings.getString("dialogTitleBookmarkEdit");
    }
  },

  /**
   * Returns a string representing the folder tree selection type for
   * the given dialog variant.  This is either "single" when you can only
   * select one folder (usually because we're dealing with the location
   * of a child folder, which can only have one parent), or "multiple"
   * when you can select multiple folders (bookmarks can be in multiple
   * folders).
   */
  _getFolderSelectionType: function BPP__getFolderSelectionType() {
    switch(this._variant) {
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.EDIT_FOLDER_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return "single";
    default:
      return "multiple";
    }
  },

  /**
   * This method can be run on a URI parameter to ensure that it didn't
   * receive a string instead of an nsIURI object.
   */
  _assertURINotString: function BPP__assertURINotString(value) {
    NS_ASSERT((typeof(value) == "object") && !(value instanceof String),
    "This method should be passed a URI as a nsIURI object, not as a string.");
  },

  /**
   * Determines the correct variant of the dialog to display depending
   * on which action is passed in and the properties of the identifier value
   * (generally either a URI or a folder ID).
   *
   * NOTE: It's currently not possible to create the dialog with a folder
   *       id and "add" mode.
   *
   * @param identifier the URI or folder ID to display the properties for
   * @param action -- "add" if this is being triggered from an "add bookmark"
   *                  UI action; or "edit" if this is being triggered from
   *                  a "properties" UI action; or "addmulti" if we're
   *                  trying to create multiple bookmarks
   *
   * @returns one of the *_VARIANT constants
   */
  _determineVariant: function BPP__determineVariant(identifier, action) {
    if (action == "add") {
      this._assertURINotString(identifier);
      if (this._bms.isBookmarked(identifier)) {
        return this.EDIT_BOOKMARK_VARIANT;
      }
      else {
        return this.ADD_BOOKMARK_VARIANT;
      }
    }
    else if (action == "addmulti") {
      return this.ADD_MULTIPLE_BOOKMARKS_VARIANT;
    }
    else { /* Assume "edit" */
      if (typeof(identifier) == "number") {
        if (this._livemarks.isLivemark(identifier)) {
          return this.EDIT_LIVEMARK_VARIANT;
        }
        else {
          return this.EDIT_FOLDER_VARIANT;
        }
      }

      NS_ASSERT(this._bms.isBookmarked(identifier),
                "Bookmark Properties dialog instantiated with " +
                "non-bookmarked URI: \"" + identifier + "\"");
      return this.EDIT_BOOKMARK_VARIANT;
    }
  },

  /**
   * This method returns the title string corresponding to a given URI.
   * If none is available from the bookmark service (probably because
   * the given URI doesn't appear in bookmarks or history), we synthesize
   * a title from the first 100 characters of the URI.
   *
   * @param uri  a nsIURI object for which we want the title
   *
   * @returns a title string
   */

  _getURITitle: function BPP__getURITitle(uri) {
    this._assertURINotString(uri);

    var title = this._bms.getItemTitle(uri);

    /* If we can't get a title for a new bookmark, let's set it to
       be the first 100 characters of the URI. */
    if (title == null) {
      title = uri.spec;
      if (title.length > 100) {
        title = title.substr(0, 100);
      }
    }
    return title;
  },

  /**
   * This method should be called by the onload of the Bookmark Properties
   * dialog to initialize the state of the panel.
   *
   * @param identifier   a nsIURI object representing the bookmarked URI or
   *                     integer folder ID of the item that
   *                     we want to view the properties of
   * @param dialogWindow the window object of the Bookmark Properties dialog
   * @param controller   a PlacesController object for interacting with the
   *                     Places system
   */
  init: function BPP_init(dialogWindow, identifier, controller, action) {
    this._variant = this._determineVariant(identifier, action);

    if (this._identifierIsURI()) {
      this._assertURINotString(identifier);
      this._bookmarkURI = identifier;
    }
    else if (this._identifierIsFolderID()){
      this._folderId = identifier;
    }
    else if (this._isVariant(this.ADD_MULTIPLE_BOOKMARKS_VARIANT)) {
      this._URIList = identifier;
    }
    this._dialogWindow = dialogWindow;
    this._controller = controller;

    this._initFolderTree();
    this._populateProperties();
    this._updateSize();
  },


  /**
   * This method initializes the folder tree. 
   */

  _initFolderTree: function BPP__initFolderTree() {
    this._folderTree = this._dialogWindow.document.getElementById("folderTree");
    this._folderTree.peerDropTypes = [];
    this._folderTree.childDropTypes = [];
    this._folderTree.excludeItems = true;
    this._folderTree.setAttribute("seltype", this._getFolderSelectionType());

    var query = this._hist.getNewQuery();
    query.setFolders([this._bms.placesRoot], 1);
    var options = this._hist.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    options.excludeReadOnlyFolders = true;
    options.excludeQueries = true;
    this._folderTree.load([query], options);
  },

  /**
   * This is a shorter form of getElementById for the dialog document.
   * Given a XUL element ID from the dialog, returns the corresponding
   * DOM element.
   *
   * @param  XUL element ID
   * @returns corresponding DOM element, or null if none found
   */

  _element: function BPP__element(id) {
    return this._dialogWindow.document.getElementById(id);
  },

  /**
   * Hides the XUL element with the given ID.
   *
   * @param  string ID of the XUL element to hide
   */

  _hide: function BPP__hide(id) {
    this._element(id).setAttribute("hidden", "true");
  },

  /**
   * This method fills in the data values for the fields in the dialog.
   */
  _populateProperties: function BPP__populateProperties() {
    var document = this._dialogWindow.document;

    if (this._identifierIsURI()) {
      this._bookmarkTitle = this._getURITitle(this._bookmarkURI);
    }
    else if (this._identifierIsFolderID()){
      this._bookmarkTitle = this._bms.getFolderTitle(this._folderId);
    }

    this._dialogWindow.document.title = this._getDialogTitle();

    this._dialogWindow.document.documentElement.getButton("accept").label =
      this._getAcceptLabel();

    if (!this._isDeletePossible()) {
      this._dialogWindow.document.documentElement.getButton("extra1").hidden =
        "true";
    }

    var nurl = this._element("editURLBar");

    var titlebox = this._element("editTitleBox");

    titlebox.value = this._bookmarkTitle;

    if (this._isURIEditable()) {
      nurl.value = this._bookmarkURI.spec;
    }
    else {
      this._hide("locationRow");
    }

    if (this._areLivemarkURIsVisible()) {
      if (this._identifierIsFolderID()) {
        var feedURI = this._livemarks.getFeedURI(this._folderId);
        if (feedURI)
          this._element("editLivemarkFeedLocationBox").value = feedURI.spec;
        var siteURI = this._livemarks.getSiteURI(this._folderId);
        if (siteURI)
          this._element("editLivemarkSiteLocationBox").value = siteURI.spec;
      }
    } else {
      this._hide("livemarkFeedLocationRow");
      this._hide("livemarkSiteLocationRow");
    }

    if (this._isShortcutVisible()) {
      var shortcutbox = this._element("editShortcutBox");
      shortcutbox.value = this._bms.getKeywordForURI(this._bookmarkURI);
    }
    else {
      this._hide("shortcutRow");
    }

    if (this._isMicrosummaryVisible()) {
      this._microsummaries = this._mss.getMicrosummaries(this._bookmarkURI,
                                                         this._bookmarkURI);
      this._microsummaries.addObserver(this._microsummaryObserver);
      this._rebuildMicrosummaryPicker();
    }
    else {
      var microsummaryRow =
        this._dialogWindow.document.getElementById("microsummaryRow");
      microsummaryRow.setAttribute("hidden", "true");
    }

    if (this._isFolderEditable()) {
      this._folderTree.selectFolders([this._bms.bookmarksRoot]);
    }
    else {
      this._hide("folderRow");
    }
  },

  _rebuildMicrosummaryPicker: function BPP__rebuildMicrosummaryPicker() {
    var microsummaryMenuList = document.getElementById("microsummaryMenuList");
    var microsummaryMenuPopup = document.getElementById("microsummaryMenuPopup");

    // Remove old items from the menu, except the first item, which represents
    // "don't show a microsummary; show the page title instead".
    while (microsummaryMenuPopup.childNodes.length > 1)
      microsummaryMenuPopup.removeChild(microsummaryMenuPopup.lastChild);

    var enumerator = this._microsummaries.Enumerate();
    while (enumerator.hasMoreElements()) {
      var microsummary = enumerator.getNext().QueryInterface(Ci.nsIMicrosummary);

      var menuItem = document.createElement("menuitem");

      // Store a reference to the microsummary in the menu item, so we know
      // which microsummary this menu item represents when it's time to save
      // changes to the datastore.
      menuItem.microsummary = microsummary;

      // Content may have to be generated asynchronously; we don't necessarily
      // have it now.  If we do, great; otherwise, fall back to the generator
      // name, then the URI, and we trigger a microsummary content update.
      // Once the update completes, the microsummary will notify our observer
      // to rebuild the menu.
      // XXX Instead of just showing the generator name or (heaven forbid)
      // its URI when we don't have content, we should tell the user that we're
      // loading the microsummary, perhaps with some throbbing to let her know
      // it's in progress.
      if (microsummary.content != null)
        menuItem.setAttribute("label", microsummary.content);
      else {
        menuItem.setAttribute("label", microsummary.generator ? microsummary.generator.name
                                                               : microsummary.generatorURI.spec);
        microsummary.update();
      }

      microsummaryMenuPopup.appendChild(menuItem);

      // Select the item if this is the current microsummary for the bookmark.
      if (this._mss.isMicrosummary(this._bookmarkURI, microsummary))
        microsummaryMenuList.selectedItem = menuItem;
    }
  },

  _microsummaryObserver: {
    interfaces: [Ci.nsIMicrosummaryObserver, Ci.nsISupports],
  
    QueryInterface: function (iid) {
      //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      if (!iid.equals(Ci.nsIMicrosummaryObserver) &&
          !iid.equals(Ci.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
    },
  
    onContentLoaded: function(microsummary) {
      BookmarkPropertiesPanel._rebuildMicrosummaryPicker();
    },

    onElementAppended: function(microsummary) {
      BookmarkPropertiesPanel._rebuildMicrosummaryPicker();
    }
  },
  
  /**
   * Makes a URI from a spec.
   * @param   spec
   *          The string spec of the URI
   * @returns A URI object for the spec. 
   */
  _uri: function PC__uri(spec) {
    return this._ios.newURI(spec, null, null);
  },

  /**
   * Size the dialog to fit its contents.
   */
  _updateSize: function BPP__updateSize() {
    var width = this._dialogWindow.outerWidth;
    this._dialogWindow.sizeToContent();
    this._dialogWindow.resizeTo(width, this._dialogWindow.outerHeight);
  },

 /**
  * This method implements the "Delete Bookmark" action
  * in the Bookmark Properties dialog.
  */
  dialogDeleteBookmark: function BPP_dialogDeleteBookmark() {
    this.deleteBookmark(this._bookmarkURI);
    this._hideBookmarkProperties();
  },

 /**
  * This method implements the "Done" action
  * in the Bookmark Properties dialog.
  */
  dialogDone: function BPP_dialogDone() {
    if (this._isMicrosummaryVisible() && this._microsummaries)
      this._microsummaries.removeObserver(this._microsummaryObserver);
    this._saveChanges();
    this._hideBookmarkProperties();
  },

  /**
   * This method deletes the bookmark corresponding to the URI stored
   * in bookmarkURI.
   */
  deleteBookmark: function BPP_deleteBookmark(bookmarkURI) {
    this._assertURINotString(bookmarkURI);

    var folders = this._bms.getBookmarkFolders(bookmarkURI, {});
    if (folders.length == 0)
      return;

    var transactions = [];
    for (var i = 0; i < folders.length; i++) {
      var index = this._bms.indexOfItem(folders[i], bookmarkURI);
      var transaction = new PlacesRemoveItemTransaction(bookmarkURI,
                                                        folders[i], index)
      transactions.push(transaction);
    }

    var aggregate =
      new PlacesAggregateTransaction(this._getDialogTitle(), transactions);
    this._controller.tm.doTransaction(aggregate);
  },

  /**
   * This method checks the current state of the input fields in the
   * dialog, and if any of them are in an invalid state, it will disable
   * the submit button.  This method should be called after every
   * significant change to the input.
   */
  validateChanges: function BPP_validateChanges() {
    this._dialogWindow.document.documentElement.getButton("accept").disabled =
      !this._inputIsValid();
  },

  /**
   * This method checks to see if the input fields are in a valid state.
   *
   * @returns  true if the input is valid, false otherwise
   */
  _inputIsValid: function BPP__inputIsValid() {
    // When in multiple select mode, it's possible to deselect all rows,
    // but you have to file your bookmark in at least one folder.
    if (this._isFolderEditable()) {
      if (this._folderTree.getSelectionNodes().length == 0)
        return false;
    }

    if (this._isURIEditable() && !this._containsValidURI("editURLBar"))
      return false;

    // Feed Location has to be a valid URI;
    // Site Location has to be a valid URI or empty
    if (this._areLivemarkURIsVisible()) {
      if (!this._containsValidURI("editLivemarkFeedLocationBox"))
        return false;
      if (!this._containsValidURI("editLivemarkSiteLocationBox") &&
          (this._element("editLivemarkSiteLocationBox").value.length > 0))
        return false;
    }

    return true;
  },

  /**
   * Determines whether the XUL textbox with the given ID contains a
   * string that can be converted into an nsIURI.
   *
   * @param textboxID the ID of the textbox element whose contents we'll test
   *
   * @returns true if the textbox contains a valid URI string, false otherwise
   */
  _containsValidURI: function BPP__containsValidURI(textboxID) {
    try {
      var uri = this._uri(this._element(textboxID).value);
    } catch (e) {
      return false;
    }
    return true;
  },

  /**
   * Save any changes that might have been made while the properties dialog
   * was open.
   */
  _saveChanges: function BPP__saveChanges() {
    var transactions = [];
    var urlbox = this._element("editURLBar");
    var titlebox = this._element("editTitleBox");
    var newURI = this._bookmarkURI;
    if (this._identifierIsURI() && this._isURIEditable())
      newURI = this._uri(urlbox.value);

    if (this._isFolderEditable()) {
      var selected =  this._folderTree.getSelectionNodes();

      if (this._identifierIsURI()) {
        for (var i = 0; i < selected.length; i++) {
          var node = selected[i];
          if (node.type == node.RESULT_TYPE_FOLDER) {
            var folder = node.QueryInterface(Ci.nsINavHistoryFolderResultNode);
            if (!folder.childrenReadOnly) {
              transactions.push(
                new PlacesCreateItemTransaction(newURI, folder.folderId, -1));
            }
          }
        }
      }
      else if (this._isVariant(this.ADD_MULTIPLE_BOOKMARKS_VARIANT)) {
        var node = selected[0];
        var folder = node.QueryInterface(Ci.nsINavHistoryFolderResultNode);

        var newFolderTrans = new PlacesCreateFolderTransaction(
            titlebox.value, folder.folderId, -1);

        var childTransactions = [];
        for (var i = 0; i < this._URIList.length; ++i) {
          var uri = this._URIList[i];
          childTransactions.push(
              new PlacesCreateItemTransaction(uri, -1, -1));
          childTransactions.push(
              new PlacesEditItemTitleTransaction(uri,
                                                 this._getURITitle(uri)));
        }
        newFolderTrans.childTransactions = childTransactions;

        transactions.push(newFolderTrans);
      }
    }


    if (this._identifierIsURI())
      transactions.push(
        new PlacesEditItemTitleTransaction(newURI, titlebox.value));
    else if (this._identifierIsFolderID()) {
      if (this._areLivemarkURIsVisible()) {
        if (this._identifierIsFolderID()) {
          var feedURIString =
            this._element("editLivemarkFeedLocationBox").value;
          var feedURI = this._uri(feedURIString);
          transactions.push(
            new PlacesEditLivemarkFeedURITransaction(this._folderId, feedURI));

          // Site Location is empty, we can set its URI to null
          var siteURI = null;
          var siteURIString =
            this._element("editLivemarkSiteLocationBox").value;
          if (siteURIString.length > 0)
            siteURI = this._uri(siteURIString);
          transactions.push(
            new PlacesEditLivemarkSiteURITransaction(this._folderId, siteURI));
        }
      }


      transactions.push(
        new PlacesEditFolderTitleTransaction(this._folderId, titlebox.value));
    }

    if (this._isShortcutVisible()) {
      var shortcutbox =
        this._element("editShortcutBox");
      transactions.push(
        new PlacesEditBookmarkKeywordTransaction(this._bookmarkURI,
                                                 shortcutbox.value));
    }

    if (this._isVariant(this.EDIT_BOOKMARK_VARIANT) &&
        (newURI.spec != this._bookmarkURI.spec)) {
          this._controller.changeBookmarkURI(this._bookmarkURI, newURI);
    }

    if (this._isMicrosummaryVisible()) {
      var menuList = document.getElementById("microsummaryMenuList");

      // Something should always be selected in the microsummary menu,
      // but if nothing is selected, then conservatively assume we should
      // just display the bookmark title.
      if (menuList.selectedIndex == -1)
        menuList.selectedIndex = 0;
  
      // This will set microsummary == undefined if the user selected
      // the "don't display a microsummary" item.
      var newMicrosummary = menuList.selectedItem.microsummary;

      // Only add a microsummary update to the transaction if the microsummary
      // has actually changed, i.e. the user selected no microsummary,
      // but the bookmark previously had one, or the user selected a microsummary
      // which is not the one the bookmark previously had.
      if ((newMicrosummary == null &&
           this._mss.hasMicrosummary(this._bookmarkURI)) ||
          (newMicrosummary != null &&
           !this._mss.isMicrosummary(this._bookmarkURI, newMicrosummary))) {
        transactions.push(
          new PlacesEditBookmarkMicrosummaryTransaction(this._bookmarkURI,
                                                        newMicrosummary));
      }
    }

    // If we have any changes to perform, do them via the
    // transaction manager in the PlacesController so they can be undone.
    if (transactions.length > 0) {
      var aggregate =
        new PlacesAggregateTransaction(this._getDialogTitle(), transactions);
      this._controller.tm.doTransaction(aggregate);
    }
  },

  /**
   * This method is called to exit the Bookmark Properties panel.
   */
  _hideBookmarkProperties: function BPP__hideBookmarkProperties() {
    this._dialogWindow.close();
  }
};
