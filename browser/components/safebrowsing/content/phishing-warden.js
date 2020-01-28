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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// The warden checks request to see if they are for phishy pages. It
// does so by either querying a remote server with the URL (advanced
// protectoin mode) or querying our locally stored blacklists (privacy
// mode).
// 
// When the warden notices a problem, it queries all browser views
// (each of which corresopnds to an open browser window) to see
// whether one of them can handle it. A browser view can handle a
// problem if its browser window has an HTMLDocument loaded with the
// given URL and that Document hasn't already been flagged as a
// problem. For every problematic URL we notice loading, at most one
// Document is flagged as problematic. Otherwise you can get into
// trouble if multiple concurrent phishy pages load with the same URL.
//
// Since we check URLs very early in the request cycle (in a progress
// listener), the URL might not yet be associated with a Document when
// we determine that it is phishy. So the the warden retries finding
// a browser view to handle the problem until one can, or until it
// determines it should give up (see complicated logic below).
//
// The warden has displayers that the browser view uses to render
// different kinds of warnings (e.g., one that's shown before a page
// loads as opposed to one that's shown after the page has already
// loaded).
//
// Note: There is a single warden for the whole application.
//
// TODO better way to expose displayers/views to browser view
/**
 * Abtracts the checking of user/browser actions for signs of
 * phishing. 
 *
 * @constructor
 */
function PROT_PhishingWarden() {
  PROT_ListWarden.call(this);

  this.debugZone = "phishwarden";
  this.testing_ = false;
  this.browserViews_ = [];

  // Use this to query preferences
  this.prefs_ = new G_Preferences();

  // Only one displayer so far; perhaps we'll have others in the future
  this.displayers_ = {
    "afterload": PROT_PhishMsgDisplayer,
  };

  // We use this dude to do lookups on our remote server
  this.fetcher_ = new PROT_TRFetcher();

  // We need to know whether we're enabled and whether we're in advanced
  // mode, so reflect the appropriate preferences into our state.

  // Read state: should we be checking remote preferences?
  var checkRemotePrefName = PROT_GlobalStore.getServerCheckEnabledPrefName();
  this.checkRemote_ = this.prefs_.getPref(checkRemotePrefName, null);

  // Get notifications when the remote check preference changes
  var checkRemotePrefObserver = BindToObject(this.onCheckRemotePrefChanged,
                                             this);
  this.prefs_.addObserver(checkRemotePrefName, checkRemotePrefObserver);

  // Global preference to enable the phishing warden
  var phishWardenPrefName = PROT_GlobalStore.getPhishWardenEnabledPrefName();
  this.phishWardenEnabled_ = this.prefs_.getPref(phishWardenPrefName, null);

  // Get notifications when the phishing warden enabled pref changes
  var phishWardenPrefObserver = 
    BindToObject(this.onPhishWardenEnabledPrefChanged, this);
  this.prefs_.addObserver(phishWardenPrefName, phishWardenPrefObserver);

  // We have a hardcoded URLs we let people navigate to in order to 
  // check out the warning.
  this.testURLs_ = PROT_GlobalStore.getTestURLs();

  // hook up our browser listener
  this.progressListener_ = Cc["@mozilla.org/browser/safebrowsing/navstartlistener;1"]
      .getService(Ci.nsIDocNavStartProgressListener);
  this.progressListener_.callback = this;
  this.progressListener_.enabled = this.phishWardenEnabled_;

  G_Debug(this, "phishWarden initialized");
}

PROT_PhishingWarden.inherits(PROT_ListWarden);

/**
 * We implement nsIWebProgressListener
 */
PROT_PhishingWarden.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) || 
      iid.equals(Ci.nsIWebProgressListener) ||
      iid.equals(Ci.nsISupportsWeakReference))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

/**
 * Cleanup on shutdown.
 */
PROT_PhishingWarden.prototype.shutdown = function() {
  this.progressListener_.callback = null;
}

/**
 * When a preference (either advanced features or the phishwarden
 * enabled) changes, we might have to start or stop asking for updates. 
 * 
 * This is a little tricky; we start or stop management only when we
 * have complete information we can use to determine whether we
 * should.  It could be the case that one pref or the other isn't set
 * yet (e.g., they haven't opted in/out of advanced features). So do
 * nothing unless we have both pref values -- we get notifications for
 * both, so eventually we will start correctly.
 */ 
PROT_PhishingWarden.prototype.maybeToggleUpdateChecking = function() {
  if (this.testing_)
    return;

  var phishWardenPrefName = PROT_GlobalStore.getPhishWardenEnabledPrefName();
  var phishWardenEnabled = this.prefs_.getPref(phishWardenPrefName, null);

  var checkRemotePrefName = PROT_GlobalStore.getServerCheckEnabledPrefName();
  this.checkRemote_ = this.prefs_.getPref(checkRemotePrefName, null);

  G_Debug(this, "Maybe toggling update checking. " +
          "Warden enabled? " + phishWardenEnabled + " || " +
          "Check remote? " + this.checkRemote_);

  // Do nothing unless both prefs are set.  They can be null (unset), true, or
  // false.
  if (phishWardenEnabled === null || this.checkRemote_ === null)
    return;

  // We update and save to disk all tables if we don't have remote checking
  // enabled.  However, as long as the warden is enabled we always update the
  // whitelist, even if remote checking is disabled.  This gives a performance
  // benefit since we use the WL to suppress BL lookups.
  // 
  // phishEnabled remote WLupdates BLupdates
  // T            T      T         F
  // T            F      T         T
  // F            T      F         F
  // F            F      F         F

  if (phishWardenEnabled === true) {
    this.enableWhitelistTableUpdates();
    if (this.checkRemote_ === true) {
      this.disableBlacklistTableUpdates();
    } else if (this.checkRemote_ === false) {
      this.enableBlacklistTableUpdates();
    }
  } else if (phishWardenEnabled === false) {
    this.disableBlacklistTableUpdates();
    this.disableWhitelistTableUpdates();
  }
}

/**
 * Controllers register their browser views with us
 *
 * @param view Reference to a browser view 
 */
PROT_PhishingWarden.prototype.addBrowserView = function(view) {
  G_Debug(this, "New browser view registered.");
  this.browserViews_.push(view);
}

/**
 * Controllers unregister their views when their window closes
 *
 * @param view Reference to a browser view 
 */
PROT_PhishingWarden.prototype.removeBrowserView = function(view) {
  for (var i = 0; i < this.browserViews_.length; i++)
    if (this.browserViews_[i] === view) {
      G_Debug(this, "Browser view unregistered.");
      this.browserViews_.splice(i, 1);
      return;
    }
  G_Assert(this, false, "Tried to unregister non-existent browser view!");
}

/**
 * Deal with a user changing the pref that says whether we should check
 * the remote server (i.e., whether we're in advanced mode)
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 we should check remote server
 */
PROT_PhishingWarden.prototype.onCheckRemotePrefChanged = function(prefName) {
  this.checkRemote_ = this.prefs_.getBoolPrefOrDefault(prefName,
                                                       this.checkRemote_);
  this.maybeToggleUpdateChecking();
}

/**
 * Deal with a user changing the pref that says whether we should 
 * enable the phishing warden (i.e., that SafeBrowsing is active)
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 we should enable the phishing warden
 */
PROT_PhishingWarden.prototype.onPhishWardenEnabledPrefChanged = function(
                                                                    prefName) {
  this.phishWardenEnabled_ = 
    this.prefs_.getBoolPrefOrDefault(prefName, this.phishWardenEnabled_);
  this.maybeToggleUpdateChecking();
  this.progressListener_.enabled = this.phishWardenEnabled_;
}

/**
 * A request for a Document has been initiated somewhere. Check it!
 *
 * @param request
 * @param url
 */ 
PROT_PhishingWarden.prototype.onDocNavStart = function(request, url) {
  //G_Debug(this, "phishWarden: " + 
  //        (this.phishWardenEnabled_ ? "enabled" : "disabled"));
  //G_Debug(this, "checkRemote: " +
  //        (this.checkRemote_ ? "yes" : "no"));
  //G_Debug(this, "isTestURL: " +
  //        (this.isBlacklistTestURL(url) ? "yes" : "no"));

  // This logic is a bit involved. In some instances of SafeBrowsing
  // (the stand-alone extension, for example), the user might yet have
  // opted into or out of advanced protection mode. In this case we
  // would like to show them a modal dialog requiring them
  // to. However, there are links from that dialog to the test page,
  // and the warden starts out as disabled. So we want to show the
  // warning on the test page so long as the extension hasn't been
  // explicitly disabled.

  // If we're on the test page and we're not explicitly disabled
  // XXX Do we still need a test url or should each provider just put
  // it in their local list?
  // Either send a request off or check locally
  if (this.checkRemote_) {
    // First check to see if it's a blacklist url.
    if (this.isBlacklistTestURL(url)) {
      this.houstonWeHaveAProblem_(request);
    } else {
      // TODO: Use local whitelists to suppress remote BL lookups. 
      this.fetcher_.get(url,
                        BindToObject(this.onTRFetchComplete,
                                     this,
                                     request));
    }
  } else {
    // Check the local lists for a match.
    // XXX This is to not slow down Tp.  The real solution is to
    // move all the logic in isEvilURL_ to the background thread.
    // This involves moving the method into the listmanager.
    var evilCallback = BindToObject(this.localListMatch_,
                                    this,
                                    url,
                                    request);
    new G_Alarm(BindToObject(this.checkUrl_, this, url, evilCallback), 1000);
  }
}

/**
 * Invoked with the result of a lookupserver request.
 *
 * @param request The nsIRequest in which we're interested
 *
 * @param trValues Object holding name/value pairs parsed from the
 *                 lookupserver's response
 */
PROT_PhishingWarden.prototype.onTRFetchComplete = function(request,
                                                           trValues) {
  var callback = BindToObject(this.houstonWeHaveAProblem_, this, request);
  this.checkRemoteData(callback, trValues);
}

/**
 * One of our Check* methods found a problem with a request. Why do we
 * need to keep the nsIRequest (instead of just passing in the URL)? 
 * Because we need to know when to stop looking for the URL its
 * fetching, and to know this we need the nsIRequest.isPending flag.
 *
 * @param request nsIRequest that is problematic
 */
PROT_PhishingWarden.prototype.houstonWeHaveAProblem_ = function(request) {

  // We have a problem request that might or might not be associated
  // with a Document that's currently in a browser. If it is, we 
  // want that Document. If it's not, we want to give it a chance to 
  // be loaded. See below for complete details.

  if (this.maybeLocateProblem_(request))       // Cases 1 and 2 (see below)
    return;

  // OK, so the request isn't associated with any currently accessible
  // Document, and we want to give it the chance to be. We don't want
  // to retry forever (e.g., what if the Document was already displayed
  // and navigated away from?), so we'll use nsIRequest.isPending to help
  // us decide what to do.
  //
  // A�complication arises because there is a lag between when a
  // request transitions from pending to not-pending and when it's
  // associated with a Document in a browser. The transition from
  // pending to not occurs just before the notification corresponding
  // to NavWatcher.DOCNAVSTART (see NavWatcher), but the association
  // occurs afterwards. Unfortunately, we're probably in DOCNAVSTART.
  // 
  // Diagnosis by Darin:
  // ---------------------------------------------------------------------------
  // Here's a summary of what happens:
  //
  //   RestorePresentation() {
  //     Dispatch_OnStateChange(dummy_request, STATE_START)
  //     PostCompletionEvent()
  //   }
  //
  //   CompletionEvent() {
  //     ReallyRestorePresentation()
  //     Dispatch_OnStateChange(dummy_request, STATE_STOP)
  //   }
  //
  // So, now your code receives that initial OnStateChange event and sees
  // that the dummy_request is not pending and not loaded in any window.
  // So, you put a timeout(0) event in the queue.  Then, the CompletionEvent
  // is added to the queue.  The stack unwinds....
  //
  // Your timeout runs, and you find that the dummy_request is still not
  // pending and not loaded in any window.  Then the CompletionEvent
  // runs, and it hooks up the cached presentation.
  // 
  // https://bugzilla.mozilla.org/show_bug.cgi?id=319527
  // ---------------------------------------------------------------------------
  //
  // So the logic is:
  //
  //         request     found an unhandled          
  //  case   pending?    doc with the url?         action
  //  ----------------------------------------------------------------
  //   1      yes             yes           Use that doc (handled above)
  //   2      no              yes           Use that doc (handled above)
  //   3      yes             no            Retry
  //   4      no              no            Retry twice (case described above)
  //
  // We don't get into trouble with Docs with the same URL "stealing" the 
  // warning because there is exactly one warning signaled per nav to 
  // a problem URL, and each Doc can be marked as problematic at most once.

  if (request.isPending()) {        // Case 3

    G_Debug(this, "Can't find problem Doc; Req pending. Retrying.");
    new G_Alarm(BindToObject(this.houstonWeHaveAProblem_, 
                             this, 
                             request), 
                200 /*ms*/);

  } else {                          // Case 4

    G_Debug(this, 
            "Can't find problem Doc; Req completed. Retrying at most twice.");
    new G_ConditionalAlarm(BindToObject(this.maybeLocateProblem_, 
                                        this, 
                                        request),
                           0 /* next event loop */, 
                           true /* repeat */, 
                           2 /* at most twice */);
  }
}

/**
 * Query all browser views we know about and offer them the chance to
 * handle the problematic request.
 *
 * @param request nsIRequest that is problematic
 * 
 * @returns Boolean indicating if someone decided to handle it
 */
PROT_PhishingWarden.prototype.maybeLocateProblem_ = function(request) {
  G_Debug(this, "Trying to find the problem.");

  G_Debug(this, this.browserViews_.length + " browser views to check.");
  for (var i = 0; i < this.browserViews_.length; i++) {
    if (this.browserViews_[i].tryToHandleProblemRequest(this, request)) {
      G_Debug(this, "Found browser view willing to handle problem!");
      return true;
    }
    G_Debug(this, "wrong browser view");
  }
  return false;
}

/**
 * Indicates if this URL is one of the possible blacklist test URLs.
 * These test URLs should always be considered as phishy.
 *
 * @param url URL to check 
 * @return A boolean indicating whether this is one of our blacklist
 *         test URLs
 */
PROT_PhishingWarden.prototype.isBlacklistTestURL = function(url) {
  for (var i = 0, testURL = null; testURL = this.testURLs_[i]; ++i) {
    if (testURL === url) {
      return true;
    }
  }
  return false;
}

/**
 * Check to see if the url is in the blacklist.
 *
 * @param url String
 * @param callback Function
 */
PROT_PhishingWarden.prototype.checkUrl_ = function(url, callback) {
  // First check to see if it's a blacklist url.
  if (this.isBlacklistTestURL(url)) {
    callback();
    return;
  }
  if (!this.isSpurious_(url))
    this.isEvilURL_(url, callback);
}

/**
 * Callback for found local blacklist match.  First we report that we have
 * a blacklist hit, then we bring up the warning dialog.
 */
PROT_PhishingWarden.prototype.localListMatch_ = function(url, request) {
  // Maybe send a report
  (new PROT_Reporter).report("phishblhit", url);
  this.houstonWeHaveAProblem_(request);
}

/**
 * Examine data fetched from a lookup server for evidence of a
 * phishing problem. 
 *
 * @param callback Function to invoke if there is a problem. 
 * @param trValues Object containing name/value pairs the server returned
 */
PROT_PhishingWarden.prototype.checkRemoteData = function(callback, 
                                                         trValues) {

  if (!trValues) {
    G_Debug(this, "Didn't get TR values from the server.");
    return;
  }
  
  G_Debug(this, "Page has phishiness " + trValues["phishy"]);

  if (trValues["phishy"] == 1) {     // It's on our blacklist 
    G_Debug(this, "Remote blacklist hit");
    callback(this);
  } else {
    G_Debug(this, "Remote blacklist miss");
  }
}

/**
 * Helper function to determine whether a given URL is "spurious" for some
 * definition of "spurious".
 *
 * @param url String containing the URL to check
 * 
 * @returns Boolean indicating whether Fritz thinks it's too boring to notice
 */ 
PROT_PhishingWarden.prototype.isSpurious_ = function(url) {
  return (url == "about:blank" ||
          url == "about:config" ||  
          url.startsWith("chrome://") ||
          url.startsWith("file://") ||
          url.startsWith("jar:") ||
          url.startsWith("javascript:"));
}
