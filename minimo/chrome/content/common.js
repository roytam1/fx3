/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marcio S. Galli - mgalli@geckonnection.com
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



/* XUL zoom in sync with pref service 
 * and also WARNING, this assumes styleSheets[1] and cssRules [1] is: 
 * defined in minimo.css and is: 
 * toolbar *, #appcontent *, * {
 * font-size: 10px;
 * padding: 0px ! important;
 *  margin: 0px ! important;
 * } 
 */

function syncUIZoom() {
  try
  {
    var currentUILevel=gPref.getIntPref("browser.display.zoomui");
    document.styleSheets[1].cssRules[1].style.fontSize=currentUILevel+"px";
  }
  catch (e) {}
}


/* All these Bookmarks  to be renamed to Bookmarks*Stuff function name space */ 

function loadBookmarks(storeStr) {

	var aDOMParser = new DOMParser();
	
	if(storeStr) {
		gBookmarksDoc = aDOMParser.parseFromString(storeStr,"text/xml");
	}
	
	if(storeStr&&gBookmarksDoc&&gBookmarksDoc.firstChild&&gBookmarksDoc.firstChild.nodeName=="bm") {
		refreshBookmarks();
	} else {
		var bookmarkEmpty="<bm></bm>";
		gPref.setCharPref("browser.bookmark.store",bookmarkEmpty);

		gBookmarksDoc = aDOMParser.parseFromString(bookmarkEmpty,"text/xml");
		refreshBookmarks();	
	}
}

function refreshBookmarks() {
	if(gBookmarksDoc.getElementsByTagName("li").length>0) {
		document.getElementById("command_BrowserBookmark").hidden=false;
	} 
}

function storeBookmarks() {
	var bmSerializer = new XMLSerializer();
	var encodedList=bmSerializer.serializeToString(gBookmarksDoc)
      gPref.setCharPref("browser.bookmark.store",encodedList);
}

function BookmarksDeleteAllAndSync() {
    var bookmarkEmpty="<bm></bm>";
    gPref.setCharPref("browser.bookmark.store",bookmarkEmpty);
}