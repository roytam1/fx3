# -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is PrefWindow IV.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben "Count XULula" Goodger <ben@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# XXX Sigh. Yet Another Copy of the same file. sigh.

const _DEBUG = false; 
 
/** PrefWindow IV
 *  =============
 *  This is a general page switcher and pref loader.
 *  =>> CHANGES MUST BE REVIEWED BY ben@netscape.com!! <<=
 **/ 

var queuedTag; 
var queuedWindow;
function initPanel ( aPrefTag, aWindow )
  {
    if( hPrefWindow )
      hPrefWindow.onpageload( aPrefTag, aWindow )
    else {
      queuedTag = aPrefTag;
      queuedWindow = aWindow;
    }
  } 
 
window.doneLoading = false; 
 
function nsPrefWindow( frame_id )
{
  if ( !frame_id )
    throw "Error: frame_id not supplied!";

  this.contentFrame   = frame_id;
  this.wsm            = new nsWidgetStateManager( frame_id );
  this.wsm.attributes = ["preftype", "prefstring", "prefattribute", "disabled"];
  this.pref           = null;
  
  this.cancelHandlers = [];
  this.okHandlers     = [];  
    
  // set up window
  this.onload();
}

nsPrefWindow.prototype =
  {
    onload:
      function ()
        {
          try 
            {
              this.pref = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);
            }
          catch(e) 
            {
              dump("*** Failed to create prefs object\n");
              return;
            }
        },

      init: 
        function ()
          {        
            if( window.queuedTag )
              {
                this.onpageload( window.queuedTag, window.queuedWindow );
              }
          },
                  
      onOK:
        function ()
          {
            var tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("tag");
            if( tag == "" )
              {
                tag = document.getElementById( hPrefWindow.contentFrame ).getAttribute("src");
              }
            hPrefWindow.wsm.savePageData( tag, null );
            for( var i = 0; i < hPrefWindow.okHandlers.length; i++ )
              {
                hPrefWindow.okHandlers[i]();
              }
            hPrefWindow.savePrefs();
          },
        
      onCancel:
        function ()
          {
            for( var i = 0; i < hPrefWindow.cancelHandlers.length; i++ )
              {
                hPrefWindow.cancelHandlers[i]();
              }
          },

      registerOKCallbackFunc:
        function ( aFunctionReference )
          { 
            this.okHandlers[this.okHandlers.length] = aFunctionReference;
          },

      registerCancelCallbackFunc:
        function ( aFunctionReference )
          {
            this.cancelHandlers[this.cancelHandlers.length] = aFunctionReference;
          },
      getPrefIsLocked:
        function ( aPrefString )
          {
            return hPrefWindow.pref.PrefIsLocked(aPrefString);
          },
      getPref:
        function ( aPrefType, aPrefString, aDefaultFlag )
          {
            var pref = hPrefWindow.pref;
            try
              {
                switch ( aPrefType )
                  {
                    case "bool":
                      return !aDefaultFlag ? pref.GetBoolPref( aPrefString ) : pref.GetDefaultBoolPref( aPrefString );
                    case "int":
                      return !aDefaultFlag ? pref.GetIntPref( aPrefString ) : pref.GetDefaultIntPref( aPrefString );
                    case "localizedstring":
                      return pref.getLocalizedUnicharPref( aPrefString );
                    case "color":
                    case "string":
                    default:
                         return !aDefaultFlag ? pref.CopyUnicharPref( aPrefString ) : pref.CopyDefaultUnicharPref( aPrefString );
                  }
              }
            catch (e)
              {
                if( _DEBUG ) 
                  {
                    dump("*** no default pref for " + aPrefType + " pref: " + aPrefString + "\n");
                    dump(e + "\n");
                  }
              }
            return "!/!ERROR_UNDEFINED_PREF!/!";
          },

      setPref:
        function ( aPrefType, aPrefString, aValue )
          {
            try
              {
                switch ( aPrefType )
                  {
                    case "bool":
                      hPrefWindow.pref.SetBoolPref( aPrefString, aValue );
                      break;
                    case "int":
                      hPrefWindow.pref.SetIntPref( aPrefString, aValue );
                      break;
                    case "color":
                    case "string":
                    case "localizedstring":
                    default:
                      hPrefWindow.pref.SetUnicharPref( aPrefString, aValue );
                      break;
                  }
              }
            catch (e)
              {
                dump(e + "\n");
              }
          },
          
      savePrefs:
        function ()
          {
            for( var pageTag in this.wsm.dataManager.pageData )
              {
                var pageData = this.wsm.dataManager.getPageData( pageTag );
                if ("initialized" in pageData && pageData.initialized && "elementIDs" in pageData)
                  {
                for( var elementID in pageData.elementIDs )
                  {
                    var itemObject = pageData.elementIDs[elementID];
                    if ( "prefstring" in itemObject && itemObject.prefstring )
                      {
                        var elt = itemObject.localname;
                        var prefattribute = itemObject.prefattribute;
                        if (!prefattribute) {
                          if (elt == "checkbox")
                            prefattribute = "checked";
                          else if (elt == "button")
                            prefattribute = "disabled";
                          else
                            prefattribute = "value";
                        }
                        
                        var value = itemObject[prefattribute];
                        var preftype = itemObject.preftype;
                        if (!preftype) {
                          if (elt == "textbox")
                            preftype = "string";
                          else if (elt == "checkbox" || elt == "button")
                            preftype = "bool";
                          else if (elt == "radiogroup" || elt == "menulist")
                            preftype = "int";
                        }
                        switch( preftype )
                          {
                            case "bool":
                              if( value == "true" && typeof(value) == "string" )
                                value = true;
                              else if( value == "false" && typeof(value) == "string" )
                                value = false;
                              break;
                            case "int":
                              value = parseInt(value);                              
                              break;
                            case "color":
                              if( toString(value) == "" )
                                {
                                  dump("*** ERROR CASE: illegal attempt to set an empty color pref. ignoring.\n");
                                  break;
                                }
                            case "string":
                            case "localizedstring":
                            default:
                              if( typeof(value) != "string" )
                                {
                                  value = toString(value);
                                }
                              break;
                          }

                        if( value != this.getPref( preftype, itemObject.prefstring ) )
                          {
                            this.setPref( preftype, itemObject.prefstring, value );
                          }
                      }
                  }
              }
              }
              try 
                {
                  this.pref.savePrefFile(null);
                }
              catch (e)
                {
                  try
                    {
                      var prefUtilBundle = document.getElementById("bundle_prefutilities");
                      var alertText = prefUtilBundle.getString("prefSaveFailedAlert");
                      var titleText = prefUtilBundle.getString("prefSaveFailedTitle");
                      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                                    .getService(Components.interfaces.nsIPromptService);
                      promptService.alert(window, titleText, alertText);
                    }
                  catch (e)
                    {
                      dump(e + "\n");
                    }
                }
          },

      switchPage:
        function ( aNewURL, aNewTag )
          {
            var oldURL = document.getElementById( this.contentFrame ).getAttribute("tag");
            if( !oldURL )
              {
                oldURL = document.getElementById( this.contentFrame ).getAttribute("src");
              }
            this.wsm.savePageData( oldURL, null );      // save data from the current page. 
            if( aNewURL != oldURL )
              {
                document.getElementById( this.contentFrame ).setAttribute( "src", aNewURL );
                if( !aNewTag )
                  document.getElementById( this.contentFrame ).removeAttribute( "tag" );
                else
                  document.getElementById( this.contentFrame ).setAttribute( "tag", aNewTag );
              }
          },
              
      onpageload: 
        function ( aPageTag, aWindow )
          {
            if (!aWindow)
              aWindow = window.frames[this.contentFrame];
            
            // Only update the header title for panels that are loaded in the main dialog, 
            // not sub-dialogs. (Removing this check will cause the header display area to
            // be cleared whenever you open a sub-dialog that uses WSM)
            if (aWindow == window.frames[this.contentFrame]) {
              var header = document.getElementById("header");
              header.setAttribute("title",
                                  aWindow.document.documentElement.getAttribute("headertitle"));
            }
            
            var pageData = this.wsm.dataManager.getPageData(aPageTag);
            if(!('initialized' in pageData))
              {
                var prefElements = aWindow.document.getElementsByAttribute( "prefstring", "*" );
                
                for( var i = 0; i < prefElements.length; i++ )
                  {
                    var prefstring    = prefElements[i].getAttribute( "prefstring" );
                    var prefid        = prefElements[i].getAttribute( "id" );
                    var preftype      = prefElements[i].getAttribute( "preftype" );
                    var elt = prefElements[i].localName;
                    if (!preftype) {
                      if (elt == "textbox")
                        preftype = "string";
                      else if (elt == "checkbox" || elt == "button")
                        preftype = "bool";
                      else if (elt == "radiogroup" || elt == "menulist")
                        preftype = "int";
                    }
                    var prefdefval    = prefElements[i].getAttribute( "prefdefval" );
                    var prefattribute = prefElements[i].getAttribute( "prefattribute" );
                    if (!prefattribute) {
                      if (elt == "checkbox")
                        prefattribute = "checked";
                      else if (elt == "button")
                        prefattribute = "disabled";
                      else
                        prefattribute = "value";
                    }
                    var prefvalue = this.getPref( preftype, prefstring );
                    if( prefvalue == "!/!ERROR_UNDEFINED_PREF!/!" )
                      {
                        prefvalue = prefdefval;
                      }
                    var root = this.wsm.dataManager.getItemData( aPageTag, prefid ); 
                    root[prefattribute] = prefvalue;              
                    var isPrefLocked = this.getPrefIsLocked(prefstring);
                    if (isPrefLocked)
                      root.disabled = "true";
                    root.localname = prefElements[i].localName;
                  }
              }      
            this.wsm.setPageData( aPageTag, aWindow );  // do not set extra elements, accept hard coded defaults
            if( 'Startup' in aWindow)
              {
                aWindow.Startup();
              }
            this.wsm.dataManager.pageData[aPageTag].initialized = true;
          }
  };

