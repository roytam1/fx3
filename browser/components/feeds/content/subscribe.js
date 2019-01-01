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
 * The Original Code is the Feed Subscribe Handler.
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const URI_BUNDLE = "chrome://browser/content/feeds/subscribe.properties";

const PREF_SELECTED_APP = "browser.feeds.handlers.application";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_HANDLER = "browser.feeds.handler";
const PREF_SKIP_PREVIEW_PAGE = "browser.feeds.skip_preview_page";

function LOG(str) {
  dump("*** " + str + "\n");
}

var SubscribeHandler = {
  _getPropertyAsBag: function FH__getPropertyAsBag(container, property) {
    return container.fields.getProperty(property).
                     QueryInterface(Ci.nsIPropertyBag2);
  },
  
  _getPropertyAsString: function FH__getPropertyAsString(container, property) {
    try {
      return container.fields.getPropertyAsAString(property);
    }
    catch (e) {
    }
    return "";
  },
  
  _setContentText: function SH__setContentText(id, text) {
    var element = document.getElementById(id);
    while (element.hasChildNodes())
      element.removeChild(element.firstChild);
    element.appendChild(document.createTextNode(text));
  },
  
  get _bundle() {
    var sbs = 
        Cc["@mozilla.org/intl/stringbundle;1"].
        getService(Ci.nsIStringBundleService);
    return sbs.createBundle(URI_BUNDLE);
  },
  
  _getFormattedString: function SH__getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },
  
  _getString: function SH__getString(key) {
    return this._bundle.GetStringFromName(key);
  },
  
  init: function SH_init() {
    LOG("window.location.href = " + window.location.href);
    
    var feedService = 
        Cc["@mozilla.org/browser/feeds/result-service;1"].
        getService(Ci.nsIFeedResultService);
        
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var feedURI = ios.newURI(window.location.href, null, null);
    try {
      var result = feedService.getFeedResult(feedURI);
    }
    catch (e) {
      LOG("feed not available?!");
    }
    
    if (result.bozo) {
      LOG("feed result is bozo?!");
    }
    
    // Set up the displayed handler
    this._initSelectedHandler();
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch2);
    prefs.addObserver(PREF_SELECTED_HANDLER, this, false);
    prefs.addObserver(PREF_SELECTED_APP, this, false);
    
    var container = result.doc;
    this._setContentText("feedTitleText", container.title);
    this._setContentText("feedSubtitleText", 
                         this._getPropertyAsString(container, "description"));
    document.title = container.title;
                         
    try {
      var parts = this._getPropertyAsBag(container, "image");
      
      // Set up the title image (supplied by the feed)
      var feedTitleImage = document.getElementById("feedTitleImage");
      feedTitleImage.setAttribute("src", parts.getPropertyAsAString("url"));
      
      // Set up the title image link
      var feedTitleLink = document.getElementById("feedTitleLink");
      
      var titleText = 
        this._getFormattedString("linkTitleTextFormat", 
                                 [parts.getPropertyAsAString("title")]);
      feedTitleLink.setAttribute("title", titleText);
      feedTitleLink.setAttribute("href", parts.getPropertyAsAString("link"));

      // Fix the margin on the main title, so that the image doesn't run over
      // the underline
      var feedTitleText = document.getElementById("feedTitleText");
      var titleImageWidth = parseInt(parts.getPropertyAsAString("width")) + 15;
      feedTitleText.style.marginRight = titleImageWidth + "px";
    }
    catch (e) {
      LOG("E: " + e);
    }
    
    // Build the actual feed content
    var feedContent = document.getElementById("feedContent");
    var feed = container.QueryInterface(Ci.nsIFeed);
    for (var i = 0; i < feed.items.length; ++i) {
      var entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
      var title = document.createElementNS(XUL_NS, "label");
      title.value = entry.summary(true, 100);
      title.className = "feedEntryTitle";
      var body = document.createElementNS(XUL_NS, "description");
      body.appendChild(document.createTextNode(entry.content(true)));
      body.className = "feedEntryContent";
      feedContent.appendChild(title);
      feedContent.appendChild(body);
    }    
  },
  
  uninit: function SH_uninit() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch2);
    prefs.removeObserver(PREF_SELECTED_HANDLER, this);
    prefs.removeObserver(PREF_SELECTED_APP, this);
  },
  
  _getFileDisplayName: function SH__getFileDisplayName(file) {
#ifdef XP_WIN
    if (file instanceof Ci.nsILocalFileWin) {
      try {
        return file.getVersionInfoField("FileDescription");
      }
      catch (e) {
      }
    }
#endif
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var url = ios.newFileURI(file).QueryInterface(Ci.nsIURL);
    return url.fileName;
  },
  
  _initSelectedHandler: function SH__initSelectedHandler() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
    var chosen = 
        document.getElementById("feedSubscribeLineHandlerChosen");
    var unchosen = 
        document.getElementById("feedSubscribeLineHandlerUnchosen");
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    try {
      var iconURI = "chrome://browser/skin/places/livemarkItem.png";
      var handler = prefs.getCharPref(PREF_SELECTED_HANDLER);
      switch (handler) {
      case "client":
        var selectedApp = 
            prefs.getComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile);      
        var displayName = this._getFileDisplayName(selectedApp);
        this._setContentText("feedSubscribeHandleText", displayName);
        
        var url = ios.newFileURI(selectedApp).QueryInterface(Ci.nsIURL);
        iconURI = "moz-icon://" + url.spec;
        break;
      case "web":
        var webURI = prefs.getCharPref(PREF_SELECTED_WEB);
        var wccr = 
            Cc["@mozilla.org/web-content-handler-registrar;1"].
            getService(Ci.nsIWebContentConverterRegistrar);
        var title ="Unknown";
        var handler = 
            wccr.getWebContentHandlerByURI(TYPE_MAYBE_FEED, webURI);
        if (handler)
          title = handler.name;
        var uri = ios.newURI(webURI, null, null);
        iconURI = uri.prePath + "/favicon.ico";
        
        this._setContentText("feedSubscribeHandleText", title);
        break;
      case "bookmarks":
        this._setContentText("feedSubscribeHandleText", 
                             this._getString("liveBookmarks"));
        break;
      }
      unchosen.setAttribute("hidden", "true");
      chosen.removeAttribute("hidden");
      
      var displayArea = 
          document.getElementById("feedSubscribeHandleText");
      displayArea.style.setProperty("background-image", 
                                    "url(\"" + iconURI + "\")", "");
    }
    catch (e) {
      LOG("EEEE: " + e);
      // No selected handlers yet! Make the user choose...
      chosen.setAttribute("hidden", "true");
      unchosen.removeAttribute("hidden");
      document.getElementById("feedHeader").setAttribute("firstrun", "true");
    }
  },
  
  observe: function SH_observe(subject, topic, data) {
    if (topic == "nsPref:changed")
      this._initSelectedHandler();
  },  
  
  changeOptions: function SH_changeOptions() {
    openDialog("chrome://browser/content/feeds/options.xul", "", "modal,centerscreen");
  },
  
  subscribe: function FH_subscribe() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
    var handler = prefs.getCharPref(PREF_SELECTED_HANDLER);
    if (handler == "web") {
      var webURI = prefs.getCharPref(PREF_SELECTED_WEB);
      var wccr = 
          Cc["@mozilla.org/web-content-handler-registrar;1"].
          getService(Ci.nsIWebContentConverterRegistrar);
      var handler = 
          wccr.getWebContentHandlerByURI(TYPE_MAYBE_FEED, webURI);
      window.location.href = handler.getHandlerURI(window.location.href);
    }
    else {
      var feedService = 
          Cc["@mozilla.org/browser/feeds/result-service;1"].
          getService(Ci.nsIFeedResultService);
      feedService.addToClientReader(window.location.href);
    }
  },
};

#include ../../../../toolkit/content/debug.js

