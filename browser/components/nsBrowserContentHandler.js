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
 * The Original Code is the Mozilla Firefox browser.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

const nsISupports            = Components.interfaces.nsISupports;

const nsIBrowserDOMWindow    = Components.interfaces.nsIBrowserDOMWindow;
const nsIBrowserHandler      = Components.interfaces.nsIBrowserHandler;
const nsIBrowserHistory      = Components.interfaces.nsIBrowserHistory;
const nsIChannel             = Components.interfaces.nsIChannel;
const nsICommandLine         = Components.interfaces.nsICommandLine;
const nsICommandLineHandler  = Components.interfaces.nsICommandLineHandler;
const nsIContentHandler      = Components.interfaces.nsIContentHandler;
const nsIDocShellTreeItem    = Components.interfaces.nsIDocShellTreeItem;
const nsIDOMChromeWindow     = Components.interfaces.nsIDOMChromeWindow;
const nsIDOMWindow           = Components.interfaces.nsIDOMWindow;
const nsIFactory             = Components.interfaces.nsIFactory;
const nsIFileURL             = Components.interfaces.nsIFileURL;
const nsIHttpProtocolHandler = Components.interfaces.nsIHttpProtocolHandler;
const nsIInterfaceRequestor  = Components.interfaces.nsIInterfaceRequestor;
const nsIPrefBranch          = Components.interfaces.nsIPrefBranch;
const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;
const nsISupportsString      = Components.interfaces.nsISupportsString;
const nsIURIFixup            = Components.interfaces.nsIURIFixup;
const nsIWebNavigation       = Components.interfaces.nsIWebNavigation;
const nsIWindowMediator      = Components.interfaces.nsIWindowMediator;
const nsIWindowWatcher       = Components.interfaces.nsIWindowWatcher;
const nsICategoryManager     = Components.interfaces.nsICategoryManager;
const nsIWebNavigationInfo   = Components.interfaces.nsIWebNavigationInfo;

const NS_BINDING_ABORTED = 0x804b0002;
const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;
const NS_ERROR_ABORT = Components.results.NS_ERROR_ABORT;

function shouldLoadURI(aURI) {
  if (aURI && !aURI.schemeIs("chrome"))
    return true;

  dump("*** Preventing external load of chrome: URI into browser window\n");
  dump("    Use -chrome <uri> instead\n");
  return false;
}

function resolveURIInternal(aCmdLine, aArgument) {
  var uri = aCmdLine.resolveURI(aArgument);

  if (!(uri instanceof nsIFileURL)) {
    return uri;
  }

  try {
    if (uri.file.exists())
      return uri;
  }
  catch (e) {
    Components.utils.reportError(e);
  }

  // We have interpreted the argument as a relative file URI, but the file
  // doesn't exist. Try URI fixup heuristics: see bug 290782.
 
  try {
    var urifixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                             .getService(nsIURIFixup);

    uri = urifixup.createFixupURI(aArgument, 0);
  }
  catch (e) {
    Components.utils.reportError(e);
  }

  return uri;
}

function needHomepageOverride(prefb) {
  var savedmstone = null;
  try {
    savedmstone = prefb.getCharPref("browser.startup.homepage_override.mstone");
  } catch (e) {}

  var mstone = Components.classes["@mozilla.org/network/protocol;1?name=http"]
                         .getService(nsIHttpProtocolHandler).misc;

  if (mstone != savedmstone)
    prefb.setCharPref("browser.startup.homepage_override.mstone", mstone);

  // Return true if the pref didn't exist (i.e. new profile)
  return !savedmstone;
}

function openWindow(parent, url, target, features, args) {
  var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(nsIWindowWatcher);

  var argstring;
  if (args) {
    argstring = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(nsISupportsString);
    argstring.data = args;
  }
  return wwatch.openWindow(parent, url, target, features, argstring);
}

function openPreferences() {
  var features = "chrome,titlebar,toolbar,centerscreen,dialog=no";
  var url = "chrome://browser/content/preferences/preferences.xul";

  var win = getMostRecentWindow("Browser:Preferences");
  if (win) {
    win.focus();
  } else {
    openWindow(null, url, "_blank", features);
  }
}

function getMostRecentWindow(aType) {
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(nsIWindowMediator);
  return wm.getMostRecentWindow(aType);
}

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define BROKEN_WM_Z_ORDER
#endif
#endif

// this returns the most recent non-popup browser window
function getMostRecentBrowserWindow() {
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);

#ifdef BROKEN_WM_Z_ORDER
  var win = wm.getMostRecentWindow("navigator:browser", true);

  // if we're lucky, this isn't a popup, and we can just return this
  if (win && !win.toolbar.visible) {
    var windowList = wm.getEnumerator("navigator:browser", true);
    // this is oldest to newest, so this gets a bit ugly
    while (windowList.hasMoreElements()) {
      var nextWin = windowList.getNext();
      if (nextWin.toolbar.visible)
        win = nextWin;
    }
  }
#else
  var windowList = wm.getZOrderDOMWindowEnumerator("navigator:browser", true);
  if (!windowList.hasMoreElements())
    return null;

  var win = windowList.getNext();
  while (!win.toolbar.visible) {
    if (!windowList.hasMoreElements()) 
      return null;

    win = windowList.getNext();
  }
#endif

  return win;
}

var nsBrowserContentHandler = {
  /* helper functions */

  mChromeURL : null,

  get chromeURL() {
    if (this.mChromeURL) {
      return this.mChromeURL;
    }

    var prefb = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(nsIPrefBranch);
    this.mChromeURL = prefb.getCharPref("browser.chromeURL");

    return this.mChromeURL;
  },

  /* nsISupports */
  QueryInterface : function bch_QI(iid) {
    if (!iid.equals(nsISupports) &&
        !iid.equals(nsICommandLineHandler) &&
        !iid.equals(nsIBrowserHandler) &&
        !iid.equals(nsIContentHandler) &&
        !iid.equals(nsIFactory))
      throw Components.errors.NS_ERROR_NO_INTERFACE;

    return this;
  },

  /* nsICommandLineHandler */
  handle : function bch_handle(cmdLine) {
    if (cmdLine.handleFlag("browser", false)) {
      openWindow(null, this.chromeURL, "_blank",
                 "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                 this.defaultArgs);
      cmdLine.preventDefault = true;
    }

    try {
      var remoteCommand = cmdLine.handleFlagWithParam("remote", true);
    }
    catch (e) {
      throw NS_ERROR_ABORT;
    }

    if (remoteCommand != null) {
      try {
        var a = /^\s*(\w+)\(([^\)]*)\)\s*$/.exec(remoteCommand);
        var remoteVerb = a[1].toLowerCase();
        var remoteParams = [];
        var sepIndex = a[2].lastIndexOf(",");
        if (sepIndex == -1)
          remoteParams[0] = a[2];
        else {
          remoteParams[0] = a[2].substring(0, sepIndex);
          remoteParams[1] = a[2].substring(sepIndex + 1);
        }

        switch (remoteVerb) {
        case "openurl":
        case "openfile":
          // openURL(<url>)
          // openURL(<url>,new-window)
          // openURL(<url>,new-tab)

          var uri = resolveURIInternal(cmdLine, remoteParams[0]);

          var location = nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW;
          if (/new-window/.test(remoteParams[1]))
            location = nsIBrowserDOMWindow.OPEN_NEWWINDOW;
          else if (/new-tab/.test(remoteParams[1]))
            location = nsIBrowserDOMWindow.OPEN_NEWTAB;

          handURIToExistingBrowser(uri, location);
          break;

        case "xfedocommand":
          // xfeDoCommand(openBrowser)
          if (remoteParams[0].toLowerCase() != "openbrowser")
            throw NS_ERROR_ABORT;

          openWindow(null, this.chromeURL, "_blank",
                     "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                     this.defaultArgs);
          break;

        default:
          // Somebody sent us a remote command we don't know how to process:
          // just abort.
          throw NS_ERROR_ABORT;
        }

        cmdLine.preventDefault = true;
      }
      catch (e) {
        // If we had a -remote flag but failed to process it, throw
        // NS_ERROR_ABORT so that the xremote code knows to return a failure
        // back to the handling code.
        throw NS_ERROR_ABORT;
      }
    }

    var uriparam;
    try {
      while ((uriparam = cmdLine.handleFlagWithParam("new-window", false))) {
        var uri = resolveURIInternal(cmdLine, uriparam);
        if (!shouldLoadURI(uri))
          continue;
        openWindow(null, this.chromeURL, "_blank",
                   "chrome,dialog=no,all" + this.getFeatures(cmdLine),
                   uri.spec);
        cmdLine.preventDefault = true;
      }
    }
    catch (e) {
      Components.utils.reportError(e);
    }

    try {
      while ((uriparam = cmdLine.handleFlagWithParam("new-tab", false))) {
        var uri = resolveURIInternal(cmdLine, uriparam);
        handURIToExistingBrowser(uri, nsIBrowserDOMWindow.OPEN_NEWTAB);
        cmdLine.preventDefault = true;
      }
    }
    catch (e) {
      Components.utils.reportError(e);
    }

    var chromeParam = cmdLine.handleFlagWithParam("chrome", false);
    if (chromeParam) {

      // Handle the old preference dialog URL separately (bug 285416)
      if (chromeParam == "chrome://browser/content/pref/pref.xul") {
        openPreferences();
      } else {
        var features = "chrome,dialog=no,all" + this.getFeatures(cmdLine);
        openWindow(null, chromeParam, "_blank", features, "");
      }

      cmdLine.preventDefault = true;
    }
    if (cmdLine.handleFlag("preferences", false)) {
      openPreferences();
      cmdLine.preventDefault = true;
    }
    if (cmdLine.handleFlag("silent", false))
      cmdLine.preventDefault = true;
  },

  helpInfo : "  -browser            Open a browser window.\n",

  /* nsIBrowserHandler */

  get defaultArgs() {
    var prefb = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(nsIPrefBranch);

    if (needHomepageOverride(prefb)) {
      try {
        return prefb.getComplexValue("startup.homepage_override_url",
                                     nsIPrefLocalizedString).data;
      }
      catch (e) {
      }
    }

    try {
      var choice = prefb.getIntPref("browser.startup.page");
      if (choice == 1)
        return this.startPage;

      if (choice == 2)
        return Components.classes["@mozilla.org/browser/global-history;2"]
                         .getService(nsIBrowserHistory).lastPageVisited;
    }
    catch (e) {
    }

    return "about:blank";
  },

  get startPage() {
    var prefb = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(nsIPrefBranch);

    var uri = prefb.getComplexValue("browser.startup.homepage",
                                    nsIPrefLocalizedString).data;

    if (!uri) {
      prefb.clearUserPref("browser.startup.homepage");
      uri = prefb.getComplexValue("browser.startup.homepage",
                                  nsIPrefLocalizedString).data;
    }
                                
    var count;
    try {
      count = prefb.getIntPref("browser.startup.homepage.count");
    }
    catch (e) {
      return uri;
    }

    for (var i = 1; i < count; ++i) {
      try {
        var page = prefb.getComplexValue("browser.startup.homepage." + i,
                                         nsIPrefLocalizedString).data;
        uri += "\n" + page;
      }
      catch (e) {
      }
    }

    return uri;
  },

  mFeatures : null,

  getFeatures : function bch_features(cmdLine) {
    if (this.mFeatures === null) {
      this.mFeatures = "";

      try {
        var width = cmdLine.handleFlagWithParam("width", false);
        var height = cmdLine.handleFlagWithParam("height", false);

        if (width)
          this.mFeatures += ",width=" + width;
        if (height)
          this.mFeatures += ",height=" + height;
      }
      catch (e) {
      }
    }

    return this.mFeatures;
  },

  /* nsIContentHandler */

  handleContent : function bch_handleContent(contentType, context, request) {
    try {
      var webNavInfo = Components.classes["@mozilla.org/webnavigation-info;1"]
                                 .getService(nsIWebNavigationInfo);
      if (!webNavInfo.isTypeSupported(contentType, null)) {
        throw NS_ERROR_WONT_HANDLE_CONTENT;
      }
    } catch (e) {
      throw NS_ERROR_WONT_HANDLE_CONTENT;
    }

    var parentWin;
    try {
      parentWin = context.getInterface(nsIDOMWindow);
    }
    catch (e) {
    }

    request.QueryInterface(nsIChannel);
    
    openWindow(parentWin, request.URI, "_blank", null, null);
    request.cancel(NS_BINDING_ABORTED);
  },

  /* nsIFactory */
  createInstance: function bch_CI(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return this.QueryInterface(iid);
  },
    
  lockFactory : function bch_lock(lock) {
    /* no-op */
  }
};

const bch_contractID = "@mozilla.org/browser/clh;1";
const bch_CID = Components.ID("{5d0ce354-df01-421a-83fb-7ead0990c24e}");
const CONTRACTID_PREFIX = "@mozilla.org/uriloader/content-handler;1?type=";

function handURIToExistingBrowser(uri, location)
{
  if (!shouldLoadURI(uri))
    return;

  var navWin = getMostRecentBrowserWindow();
  if (!navWin) {
    // if we couldn't load it in an existing window, open a new one
    openWindow(null, nsBrowserContentHandler.chromeURL, "_blank",
               "chrome,dialog=no,all" + nsBrowserContentHandler.getFeatures(cmdLine),
               uri.spec);
    return;
  }

  var navNav = navWin.QueryInterface(nsIInterfaceRequestor)
                     .getInterface(nsIWebNavigation);
  var rootItem = navNav.QueryInterface(nsIDocShellTreeItem).rootTreeItem;
  var rootWin = rootItem.QueryInterface(nsIInterfaceRequestor)
                        .getInterface(nsIDOMWindow);
  var bwin = rootWin.QueryInterface(nsIDOMChromeWindow).browserDOMWindow;
  bwin.openURI(uri, null, location,
               nsIBrowserDOMWindow.OPEN_EXTERNAL);
}


var nsDefaultCommandLineHandler = {
  /* nsISupports */
  QueryInterface : function dch_QI(iid) {
    if (!iid.equals(nsISupports) &&
        !iid.equals(nsICommandLineHandler) &&
        !iid.equals(nsIFactory))
      throw Components.errors.NS_ERROR_NO_INTERFACE;

    return this;
  },

  /* nsICommandLineHandler */
  handle : function dch_handle(cmdLine) {
    var urilist = [];

    try {
      var ar;
      while ((ar = cmdLine.handleFlagWithParam("url", false))) {
        urilist.push(resolveURIInternal(cmdLine, ar));
      }
    }
    catch (e) {
      Components.utils.reportError(e);
    }

    var count = cmdLine.length;

    for (var i = 0; i < count; ++i) {
      var curarg = cmdLine.getArgument(i);
      if (curarg.match(/^-/)) {
        Components.utils.reportError("Warning: unrecognized command line flag " + curarg + "\n");
        // To emulate the pre-nsICommandLine behavior, we ignore
        // the argument after an unrecognized flag.
        ++i;
      } else {
        try {
          urilist.push(resolveURIInternal(cmdLine, curarg));
        }
        catch (e) {
          Components.utils.reportError("Error opening URI '" + curarg + "' from the command line: " + e + "\n");
        }
      }
    }

    if (urilist.length) {
      if (cmdLine.state != nsICommandLine.STATE_INITIAL_LAUNCH &&
          urilist.length == 1) {
        // Try to find an existing window and load our URI into the
        // current tab, new tab, or new window as prefs determine.
        try {
          handURIToExistingBrowser(urilist[0], nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW);
          return;
        }
        catch (e) {
        }
      }

      var speclist = [];
      for (var uri in urilist) {
        if (shouldLoadURI(urilist[uri]))
          speclist.push(urilist[uri].spec);
      }

      if (speclist.length) {
        openWindow(null, nsBrowserContentHandler.chromeURL, "_blank",
                   "chrome,dialog=no,all" + nsBrowserContentHandler.getFeatures(cmdLine),
                   speclist.join("|"));
      }

    }
    else if (!cmdLine.preventDefault) {
      openWindow(null, nsBrowserContentHandler.chromeURL, "_blank",
                 "chrome,dialog=no,all" + nsBrowserContentHandler.getFeatures(cmdLine),
                 nsBrowserContentHandler.defaultArgs);
    }
  },

  // XXX localize me... how?
  helpInfo : "Usage: firefox [-flags] [<url>]\n",

  /* nsIFactory */
  createInstance: function dch_CI(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return this.QueryInterface(iid);
  },
    
  lockFactory : function dch_lock(lock) {
    /* no-op */
  }
};

const dch_contractID = "@mozilla.org/browser/final-clh;1";
const dch_CID = Components.ID("{47cd0651-b1be-4a0f-b5c4-10e5a573ef71}");

var Module = {
  /* nsISupports */
  QueryInterface: function mod_QI(iid) {
    if (iid.equals(Components.interfaces.nsIModule) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIModule */
  getClassObject: function mod_getco(compMgr, cid, iid) {
    if (cid.equals(bch_CID))
      return nsBrowserContentHandler.QueryInterface(iid);

    if (cid.equals(dch_CID))
      return nsDefaultCommandLineHandler.QueryInterface(iid);

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
    
  registerSelf: function mod_regself(compMgr, fileSpec, location, type) {
    var compReg =
      compMgr.QueryInterface( Components.interfaces.nsIComponentRegistrar );

    compReg.registerFactoryLocation( bch_CID,
                                     "nsBrowserContentHandler",
                                     bch_contractID,
                                     fileSpec,
                                     location,
                                     type );
    compReg.registerFactoryLocation( dch_CID,
                                     "nsDefaultCommandLineHandler",
                                     dch_contractID,
                                     fileSpec,
                                     location,
                                     type );

    function registerType(contentType) {
      compReg.registerFactoryLocation( bch_CID,
				       "Browser Cmdline Handler",
				       CONTRACTID_PREFIX + contentType,
				       fileSpec,
				       location,
				       type );
    }

    registerType("text/html");
    registerType("application/vnd.mozilla.xul+xml");
#ifdef MOZ_SVG
    registerType("image/svg+xml");
#endif
    registerType("text/rdf");
    registerType("text/xml");
    registerType("application/xhtml+xml");
    registerType("text/css");
    registerType("text/plain");
    registerType("image/gif");
    registerType("image/jpeg");
    registerType("image/jpg");
    registerType("image/png");
    registerType("image/bmp");
    registerType("image/x-icon");
    registerType("image/vnd.microsoft.icon");
    registerType("image/x-xbitmap");
    registerType("application/http-index-format");

    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(nsICategoryManager);

    catMan.addCategoryEntry("command-line-handler",
                            "m-browser",
                            bch_contractID, true, true);
    catMan.addCategoryEntry("command-line-handler",
                            "x-default",
                            dch_contractID, true, true);
  },
    
  unregisterSelf : function mod_unregself(compMgr, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);
    compReg.unregisterFactoryLocation(bch_CID, location);
    compReg.unregisterFactoryLocation(dch_CID, location);

    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(nsICategoryManager);

    catMan.deleteCategoryEntry("command-line-handler",
                               "m-browser", true);
    catMan.deleteCategoryEntry("command-line-handler",
                               "x-default", true);
  },

  canUnload: function(compMgr) {
    return true;
  }
};

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
  return Module;
}
