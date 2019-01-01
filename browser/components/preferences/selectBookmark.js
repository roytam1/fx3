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
 * The Original Code is Select Bookmark for Home Page Dialog.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

/**
 * SelectBookmarkDialog controls the user interface for the "Use Bookmark for
 * Home Page" dialog. 
 * 
 * The caller (gGeneralPane.setHomePageToBookmark in general.js) invokes this
 * dialog with a single argument - a reference to an object with a .url 
 * property. This dialog is responsible for updating the contents of the .url
 * property with the new value of the "Home Page" text field before it closes.
 */ 
var SelectBookmarkDialog = {
  init: function SBD_init() {
    // Initial update of the OK button.
    this.selectionChanged();
    
    var bookmarks = document.getElementById("bookmarks");
    bookmarks.excludeItems = false;
    bookmarks.excludeQueries = true;
    bookmarks.place = bookmarks.place;
  },
  
  /** 
   * Update the disabled state of the OK button as the user changes the 
   * selection within the view. 
   */
  selectionChanged: function SBD_selectionChanged() {
    var accept = document.documentElement.getButton("accept");
    var bookmarks = document.getElementById("bookmarks");
    accept.disabled = !bookmarks.hasSelection;
  },
  
  /**
   * The user has double clicked on a tree row that is a link. Take this to
   * mean that they want that link to be their homepage, and close the dialog.
   */
  linkChosen: function SBD_linkChosen() {
    var bookmarks = document.getElementById("bookmarks");
    if (bookmarks.hasSingleSelection && 
        PlacesController.nodeIsURI(bookmarks.selectedNode))
      document.documentElement.getButton("accept").click();
  },
  
  /**
   * User accepts their selection. Set all the selected URLs or the contents
   * of the selected folder as the list of homepages.
   */
  accept: function SBD_accept() {
    var bookmarks = document.getElementById("bookmarks");
    NS_ASSERT(bookmarks.hasSelection,
              "Should not be able to accept dialog if there is no selected URL!");
    var urls = [];
    var selectedNode = bookmarks.selectedNode;
    if (bookmarks.hasSingleSelection && 
        PlacesController.nodeIsFolder(selectedNode)) {
      var contents = PlacesController.getFolderContents(asFolder(selectedNode).folderId);
      var cc = contents.childCount;
      for (var i = 0; i < cc; ++i) {
        var node = contents.getChild(i);
        if (PlacesController.nodeIsURI(node));
          urls.push(node.uri);
      }
    }
    else {
      var nodes = bookmarks.getSelectionNodes();
      for (i = 0; i < nodes.length; ++i)
        urls.push(nodes[i].uri);
    }
    window.arguments[0].url = urls.join("|");
  },
};

#include ../../../toolkit/content/debug.js
