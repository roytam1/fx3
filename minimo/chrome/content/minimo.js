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

const imgICache                = Components.interfaces.imgICache;
const nsIBrowserDOMWindow      = Components.interfaces.nsIBrowserDOMWindow;
const nsIBrowserHistory        = Components.interfaces.nsIBrowserHistory;
const nsIClipboard             = Components.interfaces.nsIClipboard;
const nsIDeviceSupport         = Components.interfaces.nsIDeviceSupport;
const nsIDOMChromeWindow       = Components.interfaces.nsIDOMChromeWindow;
const nsIDOMDocument           = Components.interfaces.nsIDOMDocument;
const nsIDOMWindow             = Components.interfaces.nsIDOMWindow;
const nsIDocShellHistory       = Components.interfaces.nsIDocShellHistory;
const nsIDocShellTreeItem      = Components.interfaces.nsIDocShellTreeItem;
const nsIDocShellTreeNode      = Components.interfaces.nsIDocShellTreeNode;
const nsIInterfaceRequestor    = Components.interfaces.nsIInterfaceRequestor;
const nsIPhoneSupport          = Components.interfaces.nsIPhoneSupport;
const nsIPrefBranch            = Components.interfaces.nsIPrefBranch;
const nsIPrefService           = Components.interfaces.nsIPrefService;
const nsISHistory              = Components.interfaces.nsISHistory;
const nsISupportsString        = Components.interfaces.nsISupportsString;
const nsISupportsWeakReference = Components.interfaces.nsISupportsWeakReference;
const nsITransferable          = Components.interfaces.nsITransferable;
const nsIURIFixup              = Components.interfaces.nsIURIFixup;
const nsIWebNavigation         = Components.interfaces.nsIWebNavigation;
const nsIXULBrowserWindow      = Components.interfaces.nsIXULBrowserWindow;
const nsIWebProgress           = Components.interfaces.nsIWebProgress;
const nsIWebProgressListener   = Components.interfaces.nsIWebProgressListener;
const nsIWebProgressListener2  = Components.interfaces.nsIWebProgressListener2;
const nsIXULWindow             = Components.interfaces.nsIXULWindow;
const nsITransfer              = Components.interfaces.nsITransfer;

const NS_BINDING_ABORTED = 0x804b0002;

var gPanMode = null;
var appCore = null;
var gBrowser = null;
var gBookmarksDoc=null; 
var gURLBar = null;
var gClickSelectsAll = true;
var gIgnoreFocus = false;
var gIgnoreClick = false;
var gBrowserStatusHandler;
var gSelectedTab=null;
var gFullScreen=false;
var gRSSTag="minimo";
var gGlobalHistory = null;
var gURIFixup = null;
var gShowingMenuCurrent=null;
var gPopupNodeContextMenu=null;
var gDeckMode=0; // 0 = site, 1 = sb, 2= rss. Used for the URLBAR selector, DeckMode impl.
var gDeckMenuChecked=null; // to keep the state of the checked URLBAR selector mode. 
var gURLBarBoxObject = null; // stores the urlbar boxObject so the background loader can update itself based on actual urlbar size width;

var gSoftKeyAccessState=0;   // Accessibility and keyboard shortcuts. See BrowserMenuSpin functions. 

var gPref = null;                    // so far snav toggles on / off via direct access to pref.
                                     // See bugzilla.mozilla.org/show_bug.cgi?id=311287#c1
var gPrefAdded=false; // shall be used to flush the pref. 

var gMinimoBundle = null;  // Strings and such. 

function nsBrowserStatusHandler()
{
}

nsBrowserStatusHandler.prototype = 
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(nsIWebProgressListener) ||
        aIID.equals(nsIXULBrowserWindow) ||
        aIID.equals(nsISupportsWeakReference) ||
        aIID.equals(nsISupports))
    {
      return this;
    }
    throw Components.results.NS_NOINTERFACE;
  },
  
  init : function()
  {
    this.urlBar           = document.getElementById("urlbar");
    this.progressBGPosition = 0;  /* To be removed, fix in onProgressChange ... */ 

    var securityUI = gBrowser.securityUI;
    this.onSecurityChange(null, null, nsIWebProgressListener.STATE_IS_INSECURE);
  },
  
  destroy : function()
  {
    this.urlBar = null;
    this.progressBGPosition = null;  /* To be removed, fix in onProgressChange ... */ 
  },
  
  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
    var refBrowser=null;
    var tabItem=null;
    
    if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK)
    {
      
      if (aStateFlags & nsIWebProgressListener.STATE_START)
      {
        // disable and hides the nav-menu-button; and enables unhide the stop button
        document.getElementById("nav-menu-button").className="stop-button";
        document.getElementById("nav-menu-button").setAttribute("command","cmd_BrowserStop");

        // Notify anyone interested that we are loading.
        try {
          var os = Components.classes["@mozilla.org/observer-service;1"]
                             .getService(Components.interfaces.nsIObserverService);
          var host = aRequest.QueryInterface(Components.interfaces.nsIChannel).URI.host;
          os.notifyObservers(null, "loading-domain", host);
        }
        catch(e) {}

        document.getElementById("statusbar").hidden=false;


        //        document.getElementById("menu_NavPopup").



		if(aRequest && aWebProgress.DOMWindow == content) {
          this.startDocumentLoad(aRequest);
		}
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
        document.getElementById("statusbar").hidden=true;

        // gURLBarBoxObject width + 20 pixels.... so you cannot see it.         
        document.getElementById("urlbar").inputField.style.backgroundPosition=gURLBarBoxObject.width+20+"px 100%";
        
        if (aRequest) {
            if (aWebProgress.DOMWindow == content) this.endDocumentLoad(aRequest, aStatus);
        }

        // disable and hides the nav-stop-button; and enables unhides the nav-menu-button button
        document.getElementById("nav-menu-button").className="menu-button";
        document.getElementById("nav-menu-button").setAttribute("command","cmd_BrowserNavMenu");

        return;
      }
      return;
    }
    
    if (aStateFlags & nsIWebProgressListener.STATE_IS_DOCUMENT)
    { 
      if (aStateFlags & nsIWebProgressListener.STATE_START)
      {
        return;
      }
      
      if (aStateFlags & nsIWebProgressListener.STATE_STOP)
      {
        // 
        //        try {
        //          var imageCache = Components.classes["@mozilla.org/image/cache;1"]
        //                                   .getService(imgICache);
        //          imageCache.clearCache(false);
        //        }
        //        catch(e) {}
        
        
        return;
      }
      return;
    }
  },
  onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
  {

    //    document.getElementById("statusbar-text").label= "dbg:onProgressChange " + aCurTotalProgress + " " + aMaxTotalProgress;


    var percentage = parseInt((aCurTotalProgress/aMaxTotalProgress)*parseInt(gURLBarBoxObject.width));
    if(percentage<0) percentage=10;
    document.getElementById("urlbar").inputField.style.backgroundPosition=percentage+"px 100%";
  },
  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    /* Ideally we dont want to check this here.
       Better to have some other protocol view-rss in the chrome */
    
    const rssmask = "chrome://minimo/content/rssview/rssload.xhtml?url=";
    const sbmask = "chrome://minimo/content/rssview/rssload.xhtml?url=http://del.icio.us/rss/tag/";
    
    if(aLocation.spec.substr(0, rssmask .length) == rssmask ) {
      
      if(aLocation.spec.substr(0, sbmask .length) == sbmask ) {
        /* We trap the URL */ 
        this.urlBar.value="sb:"+gRSSTag; 
        
      } else {
        
        /* We trap the URL */ 
        this.urlBar.value="rss:"+gRSSTag; 
        
      }
      
    } else {
      domWindow = aWebProgress.DOMWindow;
      // Update urlbar only if there was a load on the root docshell
      if (domWindow == domWindow.top) {
        this.urlBar.value = aLocation.spec;
      }
    }
    
    BrowserUpdateBackForwardState();
    
    BrowserUpdateFeeds();

    BrowserPanRefresh();

  },
  
  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    document.getElementById("statusbar-text").label=aMessage;
  },
  startDocumentLoad : function(aRequest)
  {
    gBrowser.mCurrentBrowser.feeds = null;
  },
  endDocumentLoad : function(aRequest, aStatus)
  {
  },
  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
    /* Color is temporary. We shall dynamically assign a new class to the element and or to 
       evaluate access from another class rule, the security identity color has to be with the minimo.css */ 
    
    switch (aState) {
    case nsIWebProgressListener.STATE_IS_SECURE | nsIWebProgressListener.STATE_SECURE_HIGH:
    //this.urlBar.value="level high";
    document.styleSheets[1].cssRules[0].style.backgroundColor="yellow";
    document.getElementById("lock-icon").className="security-notbroken";
    break;	
    case nsIWebProgressListener.STATE_IS_SECURE | nsIWebProgressListener.STATE_SECURE_LOW:
    // this.urlBar.value="level low";
    document.styleSheets[1].cssRules[0].style.backgroundColor="lightyellow";
    document.getElementById("lock-icon").className="security-notbroken";
    break;
    case nsIWebProgressListener.STATE_IS_BROKEN:
    //this.urlBar.value="level broken";
    document.styleSheets[1].cssRules[0].style.backgroundColor="lightred";
    document.getElementById("lock-icon").className="security-broken";
    break;
    case nsIWebProgressListener.STATE_IS_INSECURE:
    default:
    document.styleSheets[1].cssRules[0].style.backgroundColor="white";
    document.getElementById("lock-icon").className="security-na";
    break;
    }   
  },
  
  setJSStatus : function(status)
  {
  },
  
  setJSDefaultStatus : function(status)
  {
  },
  
  setDefaultStatus : function(status)
  {
  },
  
  setOverLink : function(link, b)
  {
  }
}
  
/** 
 * Initial Minimo Startup 
 * 
 **/
  
/* moved this as global */ 
  
  function MiniNavStartup()
{
  var homepage = "http://www.mozilla.org";
  
  try {
    
    gBrowser = document.getElementById("content");
    
    gURLBar = document.getElementById("urlbar");
    gURLBar.setAttribute("completedefaultindex", "true");
    
    var currentTab=gBrowser.selectedTab;
    browserInit(currentTab);
    gSelectedTab=currentTab;
    
    gBrowserStatusHandler = new nsBrowserStatusHandler();
    gBrowserStatusHandler.init();
    
    window.XULBrowserWindow = gBrowserStatusHandler;
    window.QueryInterface(nsIInterfaceRequestor)
      .getInterface(nsIWebNavigation)
      .QueryInterface(nsIDocShellTreeItem).treeOwner
      .QueryInterface(nsIInterfaceRequestor)
      .getInterface(nsIXULWindow)
      .XULBrowserWindow = window.XULBrowserWindow;
    
    gBrowser.addProgressListener(gBrowserStatusHandler, nsIWebProgress.NOTIFY_ALL);
    
    window.QueryInterface(nsIDOMChromeWindow).browserDOMWindow =
      new nsBrowserAccess();

    gBrowser.webNavigation.sessionHistory = 
      Components.classes["@mozilla.org/browser/shistory;1"].createInstance(nsISHistory);
    
    gBrowser.docShell.QueryInterface(nsIDocShellHistory).useGlobalHistory = true;
    
    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
      .getService(nsIBrowserHistory);
    
    gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
      .getService(nsIURIFixup);
    
    var bookmarkstore=null; 
    
    try {
      gPref = Components.classes["@mozilla.org/preferences-service;1"]
        .getService(nsIPrefBranch);
      
      var page = gPref.getCharPref("browser.startup.homepage");
      var bookmarkstore = gPref.getCharPref("browser.bookmark.store");
      
      if (page != null)
      {
        var fixedUpURI = gURIFixup.createFixupURI(page, 2 /*fixup url*/ );
        homepage = fixedUpURI.spec;
      }
    } catch (ignore) {}
    
  } catch (e) {
    alert("Error trying to startup browser.  Please report this as a bug:\n" + e);
  }

  var reg = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  reg.registerFactory(Components.ID("{fe4d6bd5-e4cd-45f9-95bd-1e1796d2c7f7}"),
                       "Minimo Transfer Item",
                       "@mozilla.org/transfer;1",
                       new TransferItemFactory());

  loadURI(homepage);
  loadBookmarks(bookmarkstore);
  
  /*
   * Override the title attribute <title /> in this doc with a setter.
   * This is our workaround solution so that when the tabbrowser::updateTitle
   * tries to update this document's title, nothing happens. Bug 311564
   */ 
  
  document.__defineSetter__("title",function(x){}); // Stays with the titled defined by the XUL element. 
  
  gBrowser.addEventListener("DOMLinkAdded", BrowserLinkAdded, false);
  
  /*
   * We save the inputField (anonymous node within textbox urlbar) BoxObject, so we can measure its width 
   * on progress load
   */
  gURLBarBoxObject=(document.getBoxObjectFor(document.getElementById("urlbar").inputField));
  
  /* 
   * Homebar init...
   */
   
  bmInitXUL(document,document.getElementById("homebarcontainer"));
  document.getElementById("browserleftbar").style.display="block";
  document.getElementById("browserleftbar").addEventListener("mousedown",HomebarHandler,true);  

  /*
   * Local bundle repository
   */

  gMinimoBundle = document.getElementById("minimo_properties");

}

function HomebarHandler(e) {

  if(e.target.nodeName=="toolbarbutton") {

  } else {
	BrowserHomeBar();
  }

}

/* 
 * XUL > Menu > Tabs > Creates menuitems for each tab. 
 * When the XUL Nav menu > Tabs Item is selected,
 * meaning the MenuTabsContainer is show, 
 * contents are dynamically written. Check id="MenuTabsContainer"
 * 
 */
function BrowserMenuTabsActive() {
  for (var i = 0; i < gBrowser.mPanelContainer.childNodes.length; i++) {
    tabItem=gBrowser.mTabContainer.childNodes[i];
    var tabMenuElement=document.createElement("menuitem");
    tabMenuElement.setAttribute("label",tabItem.label);
    tabMenuElement.setAttribute("oncommand","BrowserTabFocus("+i+")");    
    document.getElementById("MenuTabsContainer").appendChild(tabMenuElement);	
  }
}

function BrowserTabFocus(i) {
  gBrowser.selectedTab=gBrowser.mTabContainer.childNodes[i];
  gBrowser.contentWindow.focus();
}

/* 
 * Menu > Tabs -> destroy tab reference elements.
 * When the XUL Nav menu > id="MenuTabsContainer" is hidden,
 * menuitems are removed from the menu. 
 */
function BrowserMenuTabsDestroy() {
  var refTabMenuContainer=document.getElementById("MenuTabsContainer");
  while(refTabMenuContainer.firstChild) {
    refTabMenuContainer.removeChild(refTabMenuContainer.firstChild);
  }
}

/*
 * Page's new Link tag handlers. This should be able to be smart about RSS, CSS, and maybe other Minimo stuff?  
 * So far we have this here, so we can experience and try some new stuff. To be tabrowsed.
 */
function BrowserLinkAdded(event) {
  // ref http://lxr.mozilla.org/mozilla/source/browser/base/content/browser.js#2070
  
  /* 
   * Taken from browser.js - yes this should be in tabbrowser
   */
  
  var erel = event.target.rel;
  var etype = event.target.type;
  var etitle = event.target.title;
  var ehref = event.target.href;
  
  const alternateRelRegex = /(^|\s)alternate($|\s)/i;
  const rssTitleRegex = /(^|\s)rss($|\s)/i;
  
  if (!alternateRelRegex.test(erel) || !etype) return;
  
  etype = etype.replace(/^\s+/, "");
  etype = etype.replace(/\s+$/, "");
  etype = etype.replace(/\s*;.*/, "");
  etype = etype.toLowerCase();
  
  if (etype == "application/rss+xml" || etype == "application/atom+xml" || (etype == "text/xml" || etype == "application/xml" || etype == "application/rdf+xml") && rssTitleRegex.test(etitle))
  {
    
    const targetDoc = event.target.ownerDocument;
    
    var browsers = gBrowser.browsers;
    var shellInfo = null;
    
    for (var i = 0; i < browsers.length; i++) {
      var shell = findChildShell(targetDoc, browsers[i].docShell, null);
      if (shell) shellInfo = { shell: shell, browser: browsers[i] };
    }
    
    //var shellInfo = this._getContentShell(targetDoc);
    
    var browserForLink = shellInfo.browser;
    
    if(!browserForLink) return;
    
    var feeds = [];
    if (browserForLink.feeds != null) feeds = browserForLink.feeds;
    var wrapper = event.target;
    feeds.push({ href: wrapper.href, type: etype, title: wrapper.title});
    browserForLink.feeds = feeds;
    
    if (browserForLink == gBrowser || browserForLink == gBrowser.mCurrentBrowser) {
      var feedButton = document.getElementById("feed-button");
      if (feedButton) {
        feedButton.setAttribute("feeds", "true");
        //				feedButton.setAttribute("tooltiptext", gNavigatorBundle.getString("feedHasFeeds"));	
        document.getElementById("feed-button-menu").setAttribute("onpopupshowing","DoBrowserRSS('"+ehref+"')");
      }
    }
  }
}

function BrowserUpdateFeeds() {
  var feedButton = document.getElementById("feed-button");
  if (!feedButton)
    return;
  
  var feeds = gBrowser.mCurrentBrowser.feeds;
  
  if (!feeds || feeds.length == 0) {
    if (feedButton.hasAttribute("feeds")) feedButton.removeAttribute("feeds");
    //		feedButton.setAttribute("tooltiptext",  gNavigatorBundle.getString("feedNoFeeds"));
  } else {
    feedButton.setAttribute("feeds", "true");
    document.getElementById("feed-button-menu").setAttribute("onpopupshowing","DoBrowserRSS('"+feeds[0].href+"')");
    
    //		feedButton.setAttribute("tooltiptext", gNavigatorBundle.getString("feedHasFeeds"));
  }
}

/* 
 * For now, this updates via DOM the top menu. Context menu should be here as well. 
 */
function BrowserUpdateBackForwardState() {

       if(gBrowser.webNavigation.canGoBack) {
            document.getElementById("command_back").setAttribute("disabled","false");
            document.getElementById("item-back").setAttribute("hidden","false");
        } else {
            document.getElementById("command_back").setAttribute("disabled","true");
            document.getElementById("item-back").setAttribute("hidden","true");


        }
        
        if(gBrowser.webNavigation.canGoForward) {
            document.getElementById("command_forward").setAttribute("disabled","false");
            document.getElementById("item-forward").setAttribute("hidden","false");
        } else {
            document.getElementById("command_forward").setAttribute("disabled","true");
            document.getElementById("item-forward").setAttribute("hidden","true");
        }

}


function findChildShell(aDocument, aDocShell, aSoughtURI) {
  aDocShell.QueryInterface(nsIWebNavigation);
  aDocShell.QueryInterface(nsIInterfaceRequestor);
  var doc = aDocShell.getInterface(nsIDOMDocument);
  if ((aDocument && doc == aDocument) || 
      (aSoughtURI && aSoughtURI.spec == aDocShell.currentURI.spec))
    return aDocShell;
  
  var node = aDocShell.QueryInterface(nsIDocShellTreeNode);
  for (var i = 0; i < node.childCount; ++i) {
    var docShell = node.getChildAt(i);
    docShell = findChildShell(aDocument, docShell, aSoughtURI);
    if (docShell) return docShell;
  }
  return null;
}


/** 
 * Init stuff
 * 
 **/
function browserInit(refTab)  
{
  /* 
   * addRule access navigational rule to each tab 
   */
  
  refTab.setAttribute("accessrule","focus_content");
  
  /*
   * 
   */
  var refBrowser=gBrowser.getBrowserForTab(refTab);
  
  try {
    refBrowser.markupDocumentViewer.textZoom = .90;
  } catch (e) {
    
  }
  gURLBar = document.getElementById("urlbar");
  
}

function MiniNavShutdown()
{
  if (gBrowserStatusHandler) gBrowserStatusHandler.destroy();
  if(gPrefAdded) {
	try {
      var psvc = Components.classes["@mozilla.org/preferences-service;1"]
        .getService(nsIPrefService);
      
      psvc.savePrefFile(null);
      
	} catch (e) { alert(e); }
  }
}

function loadURI(uri)
{
  gBrowser.webNavigation.loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
}

function BrowserHome()
{
  var homepage = "http://www.mozilla.org";

  var page = gPref.getCharPref("browser.startup.homepage");
  if (page != null)
  {
    var fixedUpURI = gURIFixup.createFixupURI(page, 2 /*fixup url*/ );
    homepage = fixedUpURI.spec;
  }

  loadURI(homepage);
}

function BrowserBack()
{
  gBrowser.webNavigation.goBack();
}

function BrowserForward()
{
  gBrowser.webNavigation.goForward();
}

function BrowserStop()
{
  gBrowser.webNavigation.stop(nsIWebNavigation.STOP_ALL);
}

function BrowserReload()
{
  gBrowser.webNavigation.reload(nsIWebNavigation.LOAD_FLAGS_NONE);
}

/* 
 * Combine the two following functions in one
 */
function BrowserOpenTab()
{
  try { 
    gBrowser.selectedTab = gBrowser.addTab('about:blank');
    browserInit(gBrowser.selectedTab);
  } catch (e) {
    alert(e);
  }
  //  if (gURLBar) setTimeout(function() { gURLBar.focus(); }, 0);  
}


/* 
 * Used by the Context Menu - Open link as Tab 
 */
function BrowserOpenLinkAsTab() 
{
  
  if(gPopupNodeContextMenu) {
    try { 
      gBrowser.selectedTab = gBrowser.addTab(gPopupNodeContextMenu);
      browserInit(gBrowser.selectedTab);
    } catch (e) {
      alert(e);
    }
  }
}

/*
 * Used by the Homebar - Open URL as Tab. 
 * WARNING: We need to validate this URL through an existing security mechanism. 
 */

function BrowserOpenURLasTab(tabUrl) {
  try {  
    gBrowser.selectedTab = gBrowser.addTab(tabUrl);   
    browserInit(gBrowser.selectedTab);
  } catch (e) {
  }  
}

/**
 * FOR - keyboard acessibility - context menu for tabbed area *** 
 * Launches the popup for the tabbed area / tabbrowser. Make sure to call this function 
 * when the tabbed panel is available. WARNING somehow need to inform which tab was lack clicked 
 * or mouse over.  
 *
 **/
function BrowserLaunchTabbedPopup() {
  var tabMenu = document.getAnonymousElementByAttribute(document.getElementById("content"),"anonid","tabContextMenu");
  tabMenu.showPopup(gBrowser.selectedTab,-1,-1,"popup","bottomleft", "topleft");
}

/**
 * Has to go through some other approach like a XML-based rule system. 
 * Those are constraints conditions and action. 
 **/

function BrowserViewOptions() {
  document.getElementById("toolbar-view").collapsed=!document.getElementById("toolbar-view").collapsed;
  if(document.getElementById("toolbar-view").collapsed &&  document.getElementById("command_ViewOptions").getAttribute("checked")=="true") {
	document.getElementById("command_ViewOptions").setAttribute("checked","false");
  }
}

/**
 * Has to go through some other approach like a XML-based rule system. 
 * Those are constraints conditions and action. 
 **/

function BrowserViewRSS() {
  document.getElementById("toolbar-rss").collapsed=!document.getElementById("toolbar-rss").collapsed;
  if(document.getElementById("toolbar-rss").collapsed &&  document.getElementById("command_ViewRSS").getAttribute("checked")=="true") {
	document.getElementById("command_ViewRSS").setAttribute("checked","false");
  }
}

/**
 * Deckmode urlbar selector. 
 * Toggles menu item and deckmode.
 */
function BrowserViewDeckSB() {
  BrowserSetDeck(1,document.getElementById("command_ViewDeckSB"));
}

function BrowserViewDeckSearch() {
  BrowserSetDeck(2,document.getElementById("command_ViewDeckSearch"));
}

function BrowserViewDeckDefault() {
  BrowserSetDeck(0,document.getElementById("command_ViewDeckDefault"));
}



/**
 * Has to go through some other approach like a XML-based rule system. 
 * Those are constraints conditions and action. 
 **/

function BrowserViewSearch() {
  document.getElementById("toolbar-search").collapsed=!document.getElementById("toolbar-search").collapsed;
  if(document.getElementById("toolbar-search").collapsed &&  document.getElementById("command_ViewSearch").getAttribute("checked")=="true") {
	document.getElementById("command_ViewSearch").setAttribute("checked","false");
  }
}


/**
 * Has to go through some other approach like a XML-based rule system. 
 * Those are constraints conditions and action. 
 **/

function BrowserViewFind() {
  document.getElementById("toolbar-find").collapsed=!document.getElementById("toolbar-find").collapsed;
  if(document.getElementById("toolbar-find").collapsed &&  document.getElementById("command_ViewFind").getAttribute("checked")=="true") {
	document.getElementById("command_ViewFind").setAttribute("checked","false");
  }
}

/**
 * Has to go through some other approach like a XML-based rule system. 
 * Those are constraints conditions and action. 
 **/

function BrowserViewHomebar() {
  document.getElementById("browserleftbar").collapsed=!document.getElementById("browserleftbar").collapsed;
}

/** 
 * urlbar indentity, style, progress indicator.
 **/ 
function urlbar() {
}


/* Reset the text size */ 
function BrowserResetZoomPlus() {
  gBrowser.selectedBrowser.markupDocumentViewer.textZoom+= .25;
}

function BrowserResetZoomMinus() {
  gBrowser.selectedBrowser.markupDocumentViewer.textZoom-= .25;
}


function MenuMainPopupShowing () {

   try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefBranch);
    if (pref.getBoolPref("snav.enabled"))
    {

      document.getElementById("snav_toggle").label = gMinimoBundle.getString("snavToggleEnableKeyScrolling");

    }
    else
    {

      document.getElementById("snav_toggle").label = gMinimoBundle.getString("snavToggleEnableJumpToLinks");

    }

    if (pref.getBoolPref("ssr.enabled"))
    {

      document.getElementById("ssr_toggle").label = gMinimoBundle.getString("ssrDesktopLayout");
     
    }
    else
    {

      document.getElementById("ssr_toggle").label = gMinimoBundle.getString("ssrSingleColumn"); 

    }
  }
  catch(ex) { alert(ex); }
}

function MenuNavPopupShowing () {

  /*  
  command_back
  command_forward
  command_go
  command_reload
  
    
  command_stop
  */

}

function isContentFrame(aFocusedWindow)
{
  if (!aFocusedWindow)
    return false;
  
  return (aFocusedWindow.top == window.content);
}

function BrowserContentAreaPopupShowing () {

  var selectedRange=gBrowser.selectedBrowser.contentDocument.getSelection();

  /* Enable Copy */
  
  if( selectedRange && selectedRange.toString() ) {
    document.getElementById("item-copy").hidden=false;
  } else {
    document.getElementById("item-copy").hidden=true;
  }
  
  /* Enable Paste - Can paste only if the focused element has a value attribute. :) 
     THis may affect XHTML nodes. Users may be able to paste things within XML nodes. 
  */

  var targetPopupNode = document.popupNode; 

  if( targetPopupNode instanceof HTMLInputElement || targetPopupNode instanceof HTMLTextAreaElement ) {
      if(DoClipCheckPaste()) {
        document.getElementById("item-paste").hidden=false;
        gPopupNodeContextMenu = targetPopupNode; 
      }
  } else {
    document.getElementById("item-paste").hidden=true;
  } 

  /*
   * Open Link as New Tab  
   */ 
  if( targetPopupNode.href ) { 
    gPopupNodeContextMenu = targetPopupNode.href;
    document.getElementById("link_as_new_tab").hidden=false;
  } else {
    document.getElementById("link_as_new_tab").hidden=true;
  }

  /*
   * Open Frame in new tab
   */

  var frameItem = document.getElementById("open_frame_in_tab");
  if (!content || !content.frames.length || !isContentFrame(document.commandDispatcher.focusedWindow))
    frameItem.setAttribute("hidden", "true");
  else
    frameItem.removeAttribute("hidden");

}

/* Bookmarks */ 

function BrowserBookmarkThis() {
 /* So far to force resync load bookmark from the pref, there are cases, bookmark is 
  * erased and we need to check
  */
  var bookmarkstore = gPref.getCharPref("browser.bookmark.store");
  loadBookmarks(bookmarkstore);
  
  var currentURI=gBrowser.selectedBrowser.webNavigation.currentURI.spec;
  var currentContentTitle=gBrowser.selectedBrowser.contentTitle;
  var currentIconURL=gBrowser.selectedBrowser.mIconURL;
  var newLi=gBookmarksDoc.createElement("li");
  newLi.setAttribute("title",currentContentTitle);
  if(currentIconURL) {
	newLi.setAttribute("iconsrc",currentIconURL); 
  } else {
	newLi.setAttribute("iconsrc","chrome://minimo/skin/m.gif");
  }
  var bmContent=gBookmarksDoc.createTextNode(currentURI);
  newLi.appendChild(bmContent);
  gBookmarksDoc.getElementsByTagName("bm")[0].appendChild(newLi);
  gPrefAdded=true;
  
  storeBookmarks();	
  refreshBookmarks();
}

function BrowserBookmark() {
  try {  
    gBrowser.selectedTab = gBrowser.addTab('chrome://minimo/content/bookmarks/bmview.xhtml');   
    browserInit(gBrowser.selectedTab);
  } catch (e) {
  }  
}

/* Toolbar specific code - to be removed from here */ 

function DoBrowserSearch() {
  BrowserViewSearch();
  try { 
    var vQuery=document.getElementById("toolbar-search-tag").value;
    if(vQuery!="") {
      gBrowser.selectedTab = gBrowser.addTab('http://www.google.com/xhtml?q='+vQuery+'&hl=en&lr=&safe=off&btnG=Search&site=search&mrestrict=xhtml');
      browserInit(gBrowser.selectedTab);
    }
  } catch (e) {
    
  }  
}

/* 
 * Search extension to urlbar, deckmode.
 * Called form the deckmode urlbar selector
 */

function DoBrowserSearchURLBAR(vQuery) {
  try { 
    if(vQuery!="") {
      gBrowser.selectedTab = gBrowser.addTab('http://www.google.com/xhtml?q='+vQuery+'&hl=en&lr=&safe=off&btnG=Search&site=search&mrestrict=xhtml');
      browserInit(gBrowser.selectedTab);
    }
  } catch (e) {
  }  
}

/* Toolbar specific code - to be removed from here */ 

function DoBrowserRSS(sKey) {
  
  if(!sKey) BrowserViewRSS(); // The toolbar is being used. Otherwise it is via the sb: trap protocol. 
  
  try { 
    
    if(sKey) {
      gRSSTag=sKey;
    } else if(document.getElementById("toolbar-rss-rsstag").value!="") {
      gRSSTag=document.getElementById("toolbar-rss-rsstag").value;
    }
    
    gBrowser.selectedTab = gBrowser.addTab('chrome://minimo/content/rssview/rssload.xhtml?url='+gRSSTag);
    
    browserInit(gBrowser.selectedTab);
  } catch (e) {
    
  }  
}

function DoBrowserGM(xmlRef) {
  
  try { 
      
    gBrowser.selectedTab = gBrowser.addTab('chrome://minimo/content/moduleview/moduleload.xhtml?url='+xmlRef);
    
    browserInit(gBrowser.selectedTab);
  } catch (e) {
    
  }  
}

/* Toolbar specific code - to be removed from here */ 

function DoBrowserSB(sKey) {
  
  if(!sKey) BrowserViewRSS(); // The toolbar is being used. Otherwise it is via the sb: trap protocol. 
  
  try { 
    if(sKey) {
      gRSSTag=sKey;
    } else if(document.getElementById("toolbar-rss-rsstag").value!="") {
      gRSSTag=document.getElementById("toolbar-rss-rsstag").value;
    }
    
    gBrowser.selectedTab = gBrowser.addTab('chrome://minimo/content/rssview/rssload.xhtml?url=http://del.icio.us/rss/tag/'+gRSSTag);
    browserInit(gBrowser.selectedTab);
  } catch (e) {
    
  }  
}

/* Toolbar specific code - to be removed from here */ 


function DoBrowserFind() {
  //  BrowserViewFind();
  try { 
    var vQuery=document.getElementById("toolbar-find-tag").value;
    if(vQuery!="") {
      gBrowser.contentWindow.focus();
      
      /* FIND DOCUMENTATION: 
         41 const FIND_NORMAL = 0;
         42 const FIND_TYPEAHEAD = 1;
         43 const FIND_LINKS = 2;
         http://lxr.mozilla.org/mozilla/source/toolkit/components/typeaheadfind/content/findBar.js
      */
      gBrowser.fastFind.find(vQuery,0);
    }
  } catch (e) {
    alert(e);
  }  
}

/* Toolbar specific code - to be removed from here */ 

function DoBrowserFindNext() {
  try { 
	gBrowser.fastFind.findNext();
  } catch (e) {
    alert(e);
  }  
}




function DoPanelPreferences() {
  window.openDialog("chrome://minimo/content/preferences/preferences.xul","preferences","modal,centerscreeen,chrome,resizable=no");
  // BrowserReload(); 
}

/* 
   Testing the SMS and Call Services 
*/
function DoTestSendCall(toCall) {
  var phoneInterface= Components.classes["@mozilla.org/phone/support;1"].createInstance(nsIPhoneSupport);
  phoneInterface.makeCall(toCall,"");
}

function DoGoogleToggle() {
  // marcio
  //google xhtml string call http://www.google.com/gwt/n?q=xml&site=mozilla_minimo&u=www.xml.com/
  
  var locationAddress="google.com";

  if(gURLBar.value.indexOf("http://")>-1) {
	locationAddress=gURLBar.value.split("http://")[1];	
  }

  try { 
        
    gBrowser.selectedTab = gBrowser.addTab('http://www.google.com/gwt/n?q=xml&site=mozilla_minimo&u='+locationAddress);
    
    browserInit(gBrowser.selectedTab);

  } catch (e) {
    
  }  
}

function DoSSRToggle()
{
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefBranch);
    pref.setBoolPref("ssr.enabled", !pref.getBoolPref("ssr.enabled"));
    
    gBrowser.webNavigation.reload(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
  }
  catch(ex) { alert(ex); }

}

function DoSNavToggle()
{
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefBranch);
    pref.setBoolPref("snav.enabled", !pref.getBoolPref("snav.enabled"));
    
    content.focus();    
  }
  catch(ex) { alert(ex); }

}

function DoToggleSoftwareKeyboard()
{
  try {

    var device = Components.classes["@mozilla.org/device/support;1"].getService(nsIDeviceSupport);

    if (device.has("hasSoftwareKeyboard") == "yes") {
      var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefBranch);
      pref.setBoolPref("skey.enabled", !pref.getBoolPref("skey.enabled"));
    }
    else {
      document.commandDispatcher.advanceFocus();
    }
  }
  catch(ex) { alert(ex); }
}

function OpenFrameInTab()
{
  var url = document.popupNode.ownerDocument.location.href;
  BrowserOpenURLasTab(url);
}


function DoFullScreen()
{
  gFullScreen = !gFullScreen;
  
  document.getElementById("nav-bar").hidden = gFullScreen;
  
  // Is this the simpler approach to count tabs? 
  if(gBrowser.mPanelContainer.childNodes.length>1) {
    gBrowser.setStripVisibilityTo(!gFullScreen);
  } 
  
  window.fullScreen = gFullScreen;  
}

/* 
   
 */
function DoClipCopy()
{
  var copytext=gBrowser.selectedBrowser.contentDocument.getSelection().toString();
  var str = Components.classes["@mozilla.org/supports-string;1"].createInstance(nsISupportsString);
  if (!str) return false;
  str.data = copytext;
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(nsITransferable);
  if (!trans) return false;
  trans.addDataFlavor("text/unicode");
  trans.setTransferData("text/unicode",str,copytext.length * 2);
  var clipid = nsIClipboard;

  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].getService(clipid);
  if (!clip) return false;
  clip.setData(trans,null,clipid.kGlobalClipboard);
}

/* 
   Currently supports text/unicode. 
*/
function DoClipCheckPaste()
{
  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].getService(nsIClipboard);
  if (!clip) return false;
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(nsITransferable);
  if (!trans) return false;
  trans.addDataFlavor("text/unicode");
  clip.getData(trans,clip.kGlobalClipboard);
  var str = new Object();
  var strLength = new Object();
  var pastetext = null;
  trans.getTransferData("text/unicode",str,strLength);
  if (str) str = str.value.QueryInterface(nsISupportsString);
  if (str) pastetext = str.data.substring(0,strLength.value / 2);
  if(pastetext) {
    return pastetext;
  } else return false;
}

function DoClipPaste()
{
  
  gPopupNodeContextMenu.focus();   // Hack. When the context menu goes open, then we store the Pastable element in gPopupNodeContextMenu
                                   // If the user clicks the element in the Context menu, the focused element changes, then is not pastable
                                   // anymore. 

  var disp = document.commandDispatcher;
  var cont = disp.getControllerForCommand("cmd_paste");
  cont.doCommand("cmd_paste");

}

function URLBarEntered()
{
  try
  {
    if (!gURLBar)
      return;
    
    var url = gURLBar.value;
    if (gURLBar.value == "" || gURLBar.value == null)
      return;
    
    /* Trap to SB 'protocol' */ 
    
    if(gURLBar.value.substring(0,3)=="sb:") {
      DoBrowserSB(gURLBar.value.split("sb:")[1]);
      return;
    }
    
    /* Trap to RSS 'protocol' */ 
    
    if(gURLBar.value.substring(0,4)=="pan:") {
      gPanMode=true;
      document.getElementById("button-icon-pan").hidden=false;
      return;
    }

    if(gURLBar.value.substring(0,4)=="rss:") {
      DoBrowserRSS(gURLBar.value.split("rss:")[1]);
      return;
    }
    
    if(gURLBar.value.substring(0,3)=="gm:") {
      DoBrowserGM(gURLBar.value.split("gm:")[1]);
      return;
    }
    
    // SB mode
    if(gDeckMode==1) {
      DoBrowserSB(gURLBar.value);
      BrowserSetDeck(0,document.getElementById("command_ViewDeckDefault"));
      return;
    }
    
    if(gDeckMode==2) {
      DoBrowserSearchURLBAR(gURLBar.value);
      BrowserSetDeck(0,document.getElementById("command_ViewDeckDefault"));
      return;
    }

    /* Other normal cases */ 
    
    if (gURLBar.value.indexOf(" ") == -1)
    {
      var fixedUpURI = gURIFixup.createFixupURI(url, 2 /*fixup url*/ );
      gGlobalHistory.markPageAsTyped(fixedUpURI);
      gURLBar.value = fixedUpURI.spec;
    }
    
    loadURI(gURLBar.value);

    content.focus();
  }
  catch(ex) {alert(ex);}
  
  
  return true;
}

function PageProxyClickHandler(aEvent) {
  document.getElementById("urlbarModeSelector").showPopup(document.getElementById("proxy-deck"),-1,-1,"popup","bottomleft", "topleft");
}


function URLBarFocusHandler(aEvent, aElt)
{
  
  if (gIgnoreFocus)
    gIgnoreFocus = false;
  else if (gClickSelectsAll)
    aElt.select();
  
  // gURLBar.setAttribute("open", "true"); 
  // gURLBar.showHistoryPopup();
  
  
}

function URLBarMouseDownHandler(aEvent, aElt)
{
  if (aElt.hasAttribute("focused")) { 
    gIgnoreClick = true;
  } else {
    gIgnoreFocus = true;
    gIgnoreClick = false;
    aElt.setSelectionRange(0, 0);
  } 
}

function URLBarClickHandler(aEvent, aElt)
{
  if (!gIgnoreClick && aElt.selectionStart == aElt.selectionEnd)
    aElt.select();
}

/* 
 * Main Menu 
 */ 

function BrowserMenuPopup() {
   ref=document.getElementById("menu_MainPopup");

   if(gShowingMenuCurrent==ref) {
	gShowingMenuCurrent.hidePopup();
	gShowingMenuCurrent=null;
   } else {
	if(!gShowingMenuCurrent) {
		gShowingMenuCurrent=ref;
	} 
      gShowingMenuCurrent.showPopup(document.getElementById("menu-button"),-1,-1,"popup","bottomleft", "topleft");
   }
}

function BrowserNavMenuPopup() {
   ref=document.getElementById("menu_NavPopup");

   if(gShowingMenuCurrent==ref) {
	gShowingMenuCurrent.hidePopup();
	gShowingMenuCurrent=null;
   } else {
	if(!gShowingMenuCurrent) {
		gShowingMenuCurrent=ref;
	} 
      gShowingMenuCurrent.showPopup(document.getElementById("nav-menu-button"),-1,-1,"popup","bottomright", "topright");
   }
}

function MenuMainPopupHiding() {
	gShowingMenuCurrent=null;
}

function MenuNavPopupHiding() {
	gShowingMenuCurrent=null;
}

function BrowserMenuPopupFalse() {
  document.getElementById("menu_MainPopup").hidePopup();
}


/*
 * BrowserMenu Accessibility Key Spin
 * You click the Softkey1 and rotates through some elements / focus. 
 * depends on gSoftKeyAccessState with initial state=0;
 */


function BrowserMenuSpin() {
  if(gSoftKeyAccessState==0||gSoftKeyAccessState==3) {
	if(gSoftKeyAccessState==3) {
	    document.getElementById("menu_NavPopup").hidePopup();
	    gSoftKeyAccessState=1;
	}
	document.getElementById("menu-button").focus();
	document.getElementById("menu_MainPopup").showPopup(document.getElementById("menu-button"),-1,-1,"popup","bottomleft", "topleft");
	gShowingMenuCurrent=document.getElementById("menu_MainPopup");
	gSoftKeyAccessState=1;
  } else if(gSoftKeyAccessState==1) {
	document.getElementById("menu_MainPopup").hidePopup();
	document.getElementById("urlbar").focus();
	gSoftKeyAccessState=2;	    
  } else if(gSoftKeyAccessState==2) {
	document.getElementById("menu_NavPopup").showPopup(document.getElementById("nav-menu-button"),-1,-1,"popup","bottomright", "topright");
	gShowingMenuCurrent=document.getElementById("menu_NavPopup");
	document.getElementById("nav-menu-button").focus();
	gSoftKeyAccessState=3;
  } 
}

function MenuEnableEscapeKeys() {

	// we remove the focus from the toolbar button to avoid a command_action (keyboard event) to 
	// call the menu again. 
	
	document.getElementById("menu_MainPopup").focus();

	// When popups are on, <key /> not working...bugs like https://bugzilla.mozilla.org/show_bug.cgi?id=55495 

	document.addEventListener("keypress",MenuHandleMenuEscape,true); 
	
}

function MenuDisableEscapeKeys() {
  document.removeEventListener("keypress",MenuHandleMenuEscape,true); 
}

function MenuHandleMenuEscape(e) {
  /* This applies because our <key /> handlers would not work when Menu popups are active */ 
  if( gShowingMenuCurrent &&  e.keyCode==e.DOM_VK_F9 ) {
    BrowserMenuSpin();
  }
}


/*
 * The URLBAR Deck mode selector 
 */

function BrowserSetDeck(dMode,menuElement) {
  
  gDeckMode=dMode;
  if(dMode==2) document.getElementById("urlbar-deck").className='search';
  if(dMode==1) document.getElementById("urlbar-deck").className='sb';
  if(dMode==0) document.getElementById("urlbar-deck").className='';
  
}


// ripped from browser.js, this should be shared in toolkit.
function nsBrowserAccess()
{
}

nsBrowserAccess.prototype =
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(nsIBrowserDOMWindow) ||
        aIID.equals(nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
  
  openURI : function(aURI, aOpener, aWhere, aContext)
  {
    var url = aURI ? aURI.spec : "about:blank";
    var newTab     = gBrowser.addTab(url);
    var newWindow  = gBrowser.getBrowserForTab(newTab).docShell
                             .QueryInterface(nsIInterfaceRequestor)
                             .getInterface(nsIDOMWindow);
    return newWindow;
  },
  
  isTabContentWindow : function(aWindow)
  {
    var browsers = gBrowser.browsers;
    for (var ctr = 0; ctr < browsers.length; ctr++)
      if (browsers.item(ctr).contentWindow == aWindow)
        return true;
    return false;
  }
}

/*
 * Download Service - Work-in-progress not-for-long
 */

function BrowserViewDownload(cMode) {
  document.getElementById("toolbar-download").collapsed=!cMode;
}

function DownloadSet( aCurTotalProgress, aMaxTotalProgress ) {
  gInputBoxObject=(document.getBoxObjectFor(document.getElementById("toolbar-download-tag").inputField));
  var percentage = parseInt((aCurTotalProgress/aMaxTotalProgress)*parseInt(gInputBoxObject.width));
  if(percentage<0) percentage=2;
  document.getElementById("toolbar-download-tag").inputField.style.backgroundPosition=percentage+"px 100%";
}

function DownloadCancel(refId) {

  try {  document.getElementById("toolbar-download-tag").cachedCancelable.cancel(NS_BINDING_ABORTED) } catch (e) { alert(e) };
  document.getElementById("download-button-stop").disabled=false;
  BrowserViewDownload(false);

}

function DownloadReveal() {
 
  document.getElementById("toolbar-download-tag").value=document.getElementById("toolbar-download-tag").getAttribute("reveal");
  document.getElementById("download-button-stop").label="";
  document.getElementById("toolbar-download-tag").inputField.style.backgroundColor="lightgreen";
  document.getElementById("toolbar-download-tag").inputField.style.backgroundPosition=gInputBoxObject.width+20+"px 100%";

}

function TransferItemFactory() {
}

TransferItemFactory.prototype = {
   createInstance: function(delegate, iid) {
    return new TransferItem().QueryInterface(iid);
  },
  lockFactory: function(lock) {
  }
};

function TransferItem() {
}

TransferItem.prototype = {
  
  QueryInterface: function (iid) {
    if (iid.equals(nsITransfer) ||
        iid.equals(nsIWebProgressListener) ||
        iid.equals(nsIWebProgressListener2) ||
        iid.equals(nsISupports))
      return this;
    
    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },
  
  init: function (aSource, aTarget, aDisplayName, aMIMEInfo, startTime, aTempFile, aCancelable) {
    
   // document.getElementById("statusbar").hidden=false;
    BrowserViewDownload(true);
    document.getElementById("toolbar-download-tag").cachedCancelable=aCancelable;
    document.getElementById("download-button-stop").disabled=false;
    document.getElementById("toolbar-download-tag").value=aSource.spec; 
    document.getElementById("toolbar-download-tag").setAttribute("reveal",aTarget.spec);
    document.getElementById("toolbar-download-tag").setAttribute("sourcelocation",aSource.spec);
    document.getElementById("toolbar-download-tag").inputField.style.backgroundColor="lightgreen";
    document.getElementById("download-close").hidden=true;

  },
  
  onStateChange: function( aWebProgress, aRequest, aStateFlags, aStatus ) {

       if ( aStateFlags & nsIWebProgressListener.STATE_STOP ) {

          document.getElementById("download-button-stop").label=document.getElementById("minimo_properties").getString("downloadButtonReveal");
          document.getElementById("download-button-stop").setAttribute("oncommand","DownloadReveal()");
          document.getElementById("toolbar-download-tag").inputField.style.backgroundColor="lightgreen";
          document.getElementById("download-close").hidden=false;

       }
    
  },
  
  onProgressChange: function( aWebProgress,
                              aRequest,
                              aCurSelfProgress,
                              aMaxSelfProgress,
                              aCurTotalProgress,
                              aMaxTotalProgress ) {
    
    return onProgressChange64(aWebProgress, aRequest, aCurSelfProgress, 
                              aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
  },
  
  onProgressChange64: function( aWebProgress,
                                aRequest,
                                aCurSelfProgress,
                                aMaxSelfProgress,
                                aCurTotalProgress,
                                aMaxTotalProgress ) {
    
    //document.getElementById("statusbar-text").label= "dbg:onProgressChange " + aCurTotalProgress + " " + aMaxTotalProgress;

    DownloadSet( aCurTotalProgress, aMaxTotalProgress );

  },
  
  onStatusChange: function( aWebProgress, aRequest, aStatus, aMessage ) {
  },
  
  onLocationChange: function( aWebProgress, aRequest, aLocation ) {
  },
  
  onSecurityChange: function( aWebProgress, aRequest, state ) {
  },
}
  

function BrowserHomeBar()  {

    if(document.getElementById("homebarcontainer").style.display=="none") document.getElementById("homebarcontainer").style.display="block"; 
    else document.getElementById("homebarcontainer").style.display="none";

}


/* Prototype PAN */ 

  
function BrowserPan() {

	if(!gBrowser.contentWindow.gInPan) {  

		gBrowser.contentDocument.addEventListener("mousedown",BrowserPanMouseHandler,true);
		gBrowser.contentDocument.addEventListener("mouseup",BrowserPanMouseHandlerDestroy,true);
		gBrowser.contentDocument.addEventListener("click",BrowserPanMouseHandlerPanNull,true);
		document.getElementById("toolbar-pan").collapsed=false;
		gBrowser.contentWindow.gInPan=true;
	} else {
		gBrowser.contentDocument.removeEventListener("mousedown",BrowserPanMouseHandler,true);
		gBrowser.contentDocument.removeEventListener("mouseup",BrowserPanMouseHandlerDestroy,true);
		gBrowser.contentDocument.removeEventListener("click",BrowserPanMouseHandlerPanNull,true);
		document.getElementById("toolbar-pan").collapsed=true;
		gBrowser.contentWindow.gInPan=false;
	}
}

function BrowserPanRefresh() {
  if(gBrowser.contentWindow.gInPan) {
		document.getElementById("toolbar-pan").collapsed=false;

  } else {
		document.getElementById("toolbar-pan").collapsed=true;
  }
}
var gInPan=false;
var gPanY=-1;
var gPanX=-1;
var gInitialPanX=null;
var gInitialPanY=null;

function BrowserPanMouseHandler(e) {

  gBrowser.contentDocument.addEventListener("mousemove",BrowserPanMouseHandlerPan,true); 
  gPanY=e.clientY;
  gPanX=e.clientX;
  e.preventDefault();
  e.stopPropagation();
  gInitialPanY=gPanY;
  gInitialPanX=gPanX;

}

function BrowserPanMouseHandlerPan(e) {
  panDeltaY=gPanY-e.clientY;
  panDeltaX=gPanX-e.clientX;
  
  /* Workaround to bug 327934 */
  gBrowser.contentWindow.scrollBy(0,panDeltaY);
  gBrowser.contentWindow.scrollBy(panDeltaX,0);

  gPanY=e.clientY;
  gPanX=e.clientX;
}

function BrowserPanMouseHandlerPanNull(e) {
    e.preventDefault();
}

function BrowserPanMouseHandlerDestroy(e) {
  gBrowser.contentDocument.removeEventListener("mousemove",BrowserPanMouseHandlerPan,true);
  e.preventDefault();
  e.stopPropagation();
  if(e.clientY==gInitialPanY || e.clientX==gInitialPanX) {
	BrowserPan();
  }
}

