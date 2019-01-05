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
 * The Original Code is Google Suggest Autocomplete Implementation for Firefox.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *   Mike Connor <mconnor@mozilla.com>
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
 * Metadata describing the Web Search suggest mode
 */
const SEARCH_SUGGEST_CONTRACTID = "@mozilla.org/autocomplete/search;1?name=remote-url-suggestions";
const SEARCH_SUGGEST_CLASSNAME = "Remote Search Suggestions";
const SEARCH_SUGGEST_CLASSID = Components.ID("{aa892eb4-ffbf-477d-9f9a-06c995ae9f27}");

/**
 * SuggestAutoCompleteResult contains the results returned by the Suggest 
 * service - it implements nsIAutoCompleteResult and is used by the auto-
 * complete controller to populate the front end.
 * @constructor
 */
function SuggestAutoCompleteResult(searchString, 
                                   searchResult, 
                                   defaultIndex, 
                                   errorDescription,
                                   results,
                                   comments) {
  this._searchString = searchString;
  this._searchResult = searchResult;
  this._defaultIndex = defaultIndex;
  this._errorDescription = errorDescription;
  this._results = results;
  this._comments = comments;
}
SuggestAutoCompleteResult.prototype = {
  /** 
   * The user's query string
   * @private
   */
  _searchString: "",
  /** 
   * The result code of this result object, see |get searchResult| for possible
   * values.
   * @private
   */
  _searchResult: 0,
  /** 
   * The default item that should be entered if none is selected
   * @private
   */
  _defaultIndex: 0,
  /** 
   * The reason the search failed
   * @private
   */
  _errorDescription: "",
  /** 
   * The list of URLs returned by the Suggest Service
   * @private
   */
  _results: [],
  /** 
   * The list of Comments (number of results - or page titles) returned by the 
   * Suggest Service. 
   * @private
   */
  _comments: [],

  /** 
   * @return the user's query string   
   */
  get searchString() {
    return this._searchString;
  },
  /** 
   * @return the result code of this result object, either:
   *         RESULT_IGNORED   (invalid searchString)
   *         RESULT_FAILURE   (failure)
   *         RESULT_NOMATCH   (no matches found)
   *         RESULT_SUCCESS   (matches found)
   */
  get searchResult() {
    return this._searchResult;
  },
  /** 
   * @return the default item that should be entered if none is selected
   */
  get defaultIndex() {
    return this._defaultIndex;
  },
  /** 
   * @return the reason the search failed
   */
  get errorDescription() {
    return this._errorDescription;
  },
  /** 
   * @return the number of results
   */ 
  get matchCount() {
    return this._results.length;
  },
  /** 
   * Retrieves a result
   * @param  index    the index of the result requested
   * @return          the result at the specified index
   */
  getValueAt: function(index) {
    return this._results[index];
  },
  /** 
   * Retrieves a comment (metadata instance)
   * @param  index    the index of the comment requested
   * @return          the comment at the specified index
   */
  getCommentAt: function(index) {
    return this._comments[index];
  },
  /** 
   * Retrieves a style hint specific to a particular index.
   * @param  index    the index of the style hint requested
   * @return          the style hint at the specified index
   */
  getStyleAt: function(index) {
    return null;
  },
  /** 
   * Removes a result from the resultset
   * @param  index    the index of the result to remove
   */
  removeValueAt: function(index, removeFromDatabase) {
    this._results.splice(index, 1);
    this._comments.splice(index, 1);
  },
  
  /**
   * Part of nsISupports implementation.
   * @param   iid     requested interface identifier
   * @return  this object (XPConnect handles the magic of telling the caller that
   *                       we're the type it requested)
   */
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIAutoCompleteResult) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }  
};

/**
 * SuggestAutoComplete is a base class that implements nsIAutoCompleteSearch
 * and can collect results for a given search by using the search URL supplied
 * by the subclass. We do it this way since the AutoCompleteController in 
 * Mozilla requires a unique XPCOM Service for every search provider, even if
 * the logic for two providers is identical. 
 * @constructor
 */
function SuggestAutoComplete() { }
SuggestAutoComplete.prototype = {
  /** 
   * The XMLHttpRequest object.
   * @private
   */
  _request: null,
  
  /** 
   * The object implementing nsIAutoCompleteObserver that we notify when 
   * we have found results
   * @private
   */
  _listener: null,
  
  /** 
   * Called when the 'readyState' of the XMLHttpRequest changes. We only care 
   * about state 4 (COMPLETED) - handle the response data.
   * @private
   */
  onReadyStateChange: function() {
    // xxx use the real const here
    if (this._request && this._request.readyState == 4) {
      try {
        var status = this._request.status;
      }
      catch (e) {
        // The XML HttpRequest can throw NS_ERROR_NOT_AVAILABLE.
        return;
      }
      var responseText = this._request.responseText;
      if (status == 200 && responseText != "") {
        var searchString, results, comments, queryURLs;
        var searchService = 
          Components.classes["@mozilla.org/browser/search-service;1"]
                    .getService(Components.interfaces.nsIBrowserSearchService);
        var sandboxHost = "http://" + searchService.currentEngine.suggestionURI.host;
        var sandbox = new Components.utils.Sandbox(sandboxHost);
        var results2 = Components.utils.evalInSandbox(responseText, sandbox);
        
        if (results2[0]) {
          searchString = results2[0] ? results2[0] : "";
          results = results2[1] ? results2[1] : [];
          comments = results2[2] ? results2[2] : [];
        }
        else { 
          // this is backwards compat code for Google Suggest, to be removed
          // once they shift to the new format
          // The responseText is formatted like so:
          // searchString\n"r1","r2","r3"\n"c1","c2","c3"\n"p1","p2","p3"
          // ... where all values are escaped:
          //  rX = result  (search term or URL)
          //  cX = comment (number of results or page title)
          //  pX = prefix
          var parts = responseText.split("\n");
          results = parts[1] ? parts[1].split(",") : [];
          comments = parts[2] ? parts[2].split(",") : [];
          for (var i = 0; i < results.length; ++i) {
            results[i] = unescape(results[i]);
            results[i] = results[i].substr(1, results[i].length - 2);
            comments[i] = unescape(comments[i]);
            comments[i] = comments[i].substr(1, comments[i].length - 2);
          }
        }
        // Notify the FE of our new results
        this.onResultsReady(searchString, results, comments);

        // Reset our state for next time. 
        this._request = null;
        this._listener = null;
      }
    }
  },
  
  /** 
   * Notifies the front end of new results.
   * @param searchString  the user's query string
   * @param results       an array of results to the search
   * @param comments      an array of metadata corresponding to the results
   * @private
   */
  onResultsReady: function(searchString, results, comments) {
    if (this._listener) {
      var result = new SuggestAutoCompleteResult(searchString, 
                                                 Components.interfaces.nsIAutoCompleteResult.RESULT_SUCCESS,
                                                 0, "", results, comments);
      this._listener.onSearchResult(this, result);
    }
  },
  
  /** 
   * Called when there is an error loading the request. 
   * @private
   */
  onError: function() {
    if (this._listener) { 
      var result = new SuggestAutoCompleteResult("", 
                                                 Components.interfaces.nsIAutoCompleteResult.RESULT_FAILURE,
                                                 0, "", [], []);
      this._listener.onSearchResult(this, result);
    }
  },

  /**
   * Initiates the search result gathering process. Part of 
   * nsIAutoCompleteSearch implementation.
   * @param searchString    the user's query string
   * @param searchParam     unused, "an extra parameter"
   * @param previousResult  unused, a client-cached store of the previous 
   *                        generated resultset for faster searching.
   * @param listener        object implementing nsIAutoCompleteObserver which 
   *                        we notify when results are ready.
   */
  startSearch: function(searchString, searchParam, previousResult, listener) {
    var searchService = 
      Components.classes["@mozilla.org/browser/search-service;1"]
                .getService(Components.interfaces.nsIBrowserSearchService);
    // If the service URL is empty, bail.
    var serviceURL = searchService.currentEngine.suggestionURI.spec;
    if (serviceURL == "") 
      return;
      
    // If there's an existing request, stop it. There is no smart filtering here
    // as there is when looking through history/form data because the result set
    // returned by the server is different for every typed value - "ocean breathes"
    // does not return a subset of the results returned for "ocean", for example.
    if (this._request)
      this.stopSearch();
    
    // Actually do the search
    this._request = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                              .createInstance(Components.interfaces.nsIXMLHttpRequest);
    this._request.open("GET", serviceURL + searchString, true);
    
    this._listener = listener;
    
    var self = this;
    function onReadyStateChange() {
      self.onReadyStateChange();
    }
    function onError() {
      self.onError();
    }
    this._request.onreadystatechange = onReadyStateChange;
    this._request.onerror = onError;
    this._request.send(null);
  },
  
  /**
   * Ends the search result gathering process. Part of nsIAutoCompleteSearch
   * implementation.
   */
  stopSearch: function() {
    if (this._request) {
      this._request.abort();
      this._request = null;
      this._listener = null;
    }
  },

  /**
   * Part of nsISupports implementation.
   * @param   iid     requested interface identifier
   * @return  this object (XPConnect handles the magic of telling the caller that
   *                       we're the type it requested)
   */
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIAutoCompleteSearch) &&
        !iid.equals(Components.interfaces.nsIAutoCompleteObserver) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * SearchSuggestAutoComplete is a service implementation that handles suggest
 * results specific to web searches. 
 * @constructor
 */
function SearchSuggestAutoComplete() {
}
SearchSuggestAutoComplete.prototype = {
  __proto__: SuggestAutoComplete.prototype,
  serviceURL: ""
};

var gModule = {
  /**
   * Registers all the components supplied by this module. Part of nsIModule
   * implementation.
   * @param componentManager  the XPCOM component manager
   * @param location          the location of the module on disk
   * @param loaderString      opaque loader specific string
   * @param type              loader type being used to load this module
   */
  registerSelf: function(componentManager, location, loaderString, type) {
    if (this._firstTime) {
      this._firstTime = false;
      throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
    }
    componentManager = componentManager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    
    for (var key in this.objects) {
      var obj = this.objects[key];
      componentManager.registerFactoryLocation(obj.CID, obj.className, obj.contractID,
                                               location, loaderString, type);
    }
  },
  
  /**
   * Retrieves a Factory for the given ClassID. Part of nsIModule 
   * implementation.
   * @param componentManager  the XPCOM component manager
   * @param cid               the ClassID of the object for which a factory 
   *                          has been requested
   * @param iid               the IID of the interface requested
   */
  getClassObject: function(componentManager, cid, iid) {
    if (!iid.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    for (var key in this.objects) {
      if (cid.equals(this.objects[key].CID))
        return this.objects[key].factory;
    }
    
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  
  /**
   * Create a Factory object that can construct an instance of an object.
   * @param constructor   the constructor used to create the object
   * @private
   */
  _makeFactory: function(constructor) {
    function createInstance(outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return (new constructor()).QueryInterface(iid);
    }
    return { createInstance: createInstance };
  },

  /**
   * Determines whether or not this module can be unloaded.
   * @return returning true indicates that this module can be unloaded.
   */
  canUnload: function(componentManager) {
    return true;
  }
};

/**
 * Entry point for registering the components supplied by this JavaScript
 * module. 
 * @param componentManager  the XPCOM component manager
 * @param location          the location of this module on disk
 */
function NSGetModule(componentManager, location) {
  // Metadata about the objects this module can construct
  gModule.objects = {
    search: { 
      CID: SEARCH_SUGGEST_CLASSID,
      contractID: SEARCH_SUGGEST_CONTRACTID,
      className: SEARCH_SUGGEST_CLASSNAME,
      factory: gModule._makeFactory(SearchSuggestAutoComplete)
    },
  };
  return gModule;
}

