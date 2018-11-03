/* -*- Mode: Java; tab-width: 4; insert-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

// the rdf service
var rdf_uri = 'component://netscape/rdf/rdf-service'
var RDF = Components.classes[rdf_uri].getService()
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService)

// the magic number to find panels.rdf
var PANELS_RDF_FILE = 66626;

// the default sidebar:
var sidebar = new Object;

function debug(msg) {
  // uncomment for noise
  // dump(msg+"\n");
}

var panel_observer = new Object;
panel_observer = {
  OnAssert   : function(src,prop,target)
    {
      debug("Setting timeout to open default");

      //xxxslamm This timeout does not work. JS bug #???? (bugzilla is down).
      // Brendan said that norris is working on this.
      //setTimeout("sidebarOpenDefaultPanel(100, 0)",100);
    },
  OnUnassert : function(src,prop,target)
               { debug("panel_observer: onUnassert"); },
  OnChange   : function(src,prop,old_target,new_target)
               { debug("panel_observer: onChange"); },
  OnMove     : function(old_src,new_src,prop,target)
               { debug("panel_observer: onMove"); }
}


function getSidebarDatasourceURI(panels_file_id) {
  try {
    var fileLocatorInterface = Components.interfaces.nsIFileLocator;
    var fileLocatorProgID = 'component://netscape/filelocator';
    var fileLocatorService  = Components.classes[fileLocatorProgID].getService();
    // use the fileLocator to look in the profile directory
    // to find 'panels.rdf', which is the
    // database of the user's currently selected panels.
    fileLocatorService = fileLocatorService.QueryInterface(fileLocatorInterface);

    // if <profile>/panels.rdf doesn't exist, GetFileLocation() will copy
    // bin/defaults/profile/panels.rdf to <profile>/panels.rdf
    var sidebar_file = fileLocatorService.GetFileLocation(panels_file_id);

    if (!sidebar_file.exists()) {
      // this should not happen, as GetFileLocation() should copy
      // defaults/panels.rdf to the users profile directory
      return null;
    }

    debug("sidebar uri is " + sidebar_file.URLString);
    return sidebar_file.URLString;
  }
  catch (ex) {
    // this should not happen
    return null;
  }
}

function sidebarOverlayInit() {
  sidebar.datasource_uri = getSidebarDatasourceURI(PANELS_RDF_FILE);
  sidebar.resource = 'urn:sidebar:current-panel-list';

  // Initialize the display
  var sidebar_element = document.getElementById('sidebar-box')
  var sidebar_menuitem = document.getElementById('menu_sidebar')
  if (sidebar_element.getAttribute('hidden') == 'true') {
    sidebar_element.setAttribute('style', 'display:none')

    if (sidebar_menuitem)
      sidebar_menuitem.setAttribute('checked', 'false')
  } else {
    if (sidebar_menuitem)
      sidebar_menuitem.setAttribute('checked', 'true');

    debug("sidebar = " + sidebar);
    debug("sidebar.resource = " + sidebar.resource);
    debug("sidebar.datasource_uri = " + sidebar.datasource_uri);

    // Add the user's current panel choices to the template builder,
    // which will aggregate it with the other datasources that describe
    // the individual panel's title, customize URL, and content URL.
    var panels = document.getElementById('sidebar-panels');
    panels.database.AddDataSource(RDF.GetDataSource(sidebar.datasource_uri));

    debug("Adding observer to database.");
    panels.database.AddObserver(panel_observer);

    // XXX This is a hack to force re-display
    panels.setAttribute('ref', 'urn:sidebar:current-panel-list');

    sidebarOpenDefaultPanel(100, 0);
  }
}

function sidebarOpenDefaultPanel(wait, tries) {
  var parent = document.getElementById('sidebar-panels');
  var target = parent.getAttribute('open-panel-src');
  var children = parent.childNodes;

  debug("sidebarOpenDefaultPanel("+wait+","+tries+")");
  debug("  target="+target);

  if (children.length < 3) {
    if (tries < 5) {
      // No children yet, try again later
      setTimeout('sidebarOpenDefaultPanel('+(wait*2)+','+(tries+1)+')',wait);
    }
    return;
  }
  if (target && target != '') {
    for (var ii=0; ii < children.length; ii++) {
      if (children.item(ii).getAttribute('src') == target) {
        children.item(ii).removeAttribute('collapsed');
        return;
      }
    }
  }
  // Pick the first one
  var first_iframe = children.item(2);
  if (first_iframe) {
    first_iframe.removeAttribute('collapsed');
    parent.setAttribute('open-panel-src',first_iframe.getAttribute('src'));
  }
}

function sidebarOpenClosePanel(titledbutton) {
  var target = titledbutton.getAttribute('iframe-src');
  var last_src = titledbutton.parentNode.getAttribute('open-panel-src');
  var children = titledbutton.parentNode.childNodes;

  if (target == last_src) {
    return;
  }

  for (var ii=0; ii < children.length; ii++) {
    var src = children.item(ii).getAttribute('src')

    if (src == target) {
      children.item(ii).removeAttribute('collapsed');
      titledbutton.parentNode.setAttribute('open-panel-src',target);
    }
    if (src == last_src) {
      children.item(ii).setAttribute('collapsed','true');
    }
  }
}

function sidebarReload() {
  sidebarOverlayInit(sidebar);
}

// Set up a lame hack to avoid opening two customize
// windows on a double click.
var gDisableCustomize = false;
function enableCustomize() {
  gDisableCustomize = false;
}

function sidebarCustomize() {
  // Use a single sidebar customize dialog

  var cwindowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);

  var customizeWindow = windowManager.GetMostRecentWindow('sidebar:customize');

  if (customizeWindow) {
    debug("Reuse existing customize dialog");
    customizeWindow.focus();
  } else {
    debug("Open a new customize dialog");

    if (true == gDisableCustomize) {
      debug("Recently opened one. Wait a little bit.");
      return;
    }
    gDisableCustomize = true;

    customizeWindow = window.openDialog(
                        'chrome://sidebar/content/customize.xul',
                        '_blank','chrome',
                        sidebar.datasource_uri, sidebar.resource);
    setTimeout(enableCustomize, 2000);
  }
}

function sidebarShowHide() {
  var sidebar = document.getElementById('sidebar-box')
  var sidebar_splitter = document.getElementById('sidebar-splitter')
  var is_hidden = sidebar.getAttribute('hidden')

  if (is_hidden && is_hidden == "true") {
    debug("Showing the sidebar")
    sidebar.setAttribute('hidden','')
    sidebar_splitter.setAttribute('hidden','')
    //sidebarOverlayInit()
  } else {
    debug("Hiding the sidebar")
    sidebar.setAttribute('hidden','true')
    sidebar_splitter.setAttribute('hidden','true')
  }
  // Immediately save persistent values
  document.persist('sidebar-box', 'hidden');
  document.persist('sidebar-box', 'width');
}

function dumpAttributes(node) {
  var attributes = node.attributes

  if (!attributes || attributes.length == 0) {
    debug("no attributes")
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii)
    debug("attr "+ii+": "+ attr.name +"="+attr.value)
  }
}

function dumpStats() {
  var box = document.getElementById('sidebar-box');
  var splitter = document.getElementById('sidebar-splitter');
  var style = box.getAttribute('style')

  var visibility = style.match('visibility:([^;]*)')
  if (visibility) {
    visibility = visibility[1]
  }
  debug("sidebar-box.style="+style)
  debug("sidebar-box.visibility="+visibility)
  debug('sidebar-box.width='+box.getAttribute('width'))
  debug('sidebar-box attrs\n---------------------')
  dumpAttributes(box)
  debug('sidebar-splitter attrs\n--------------------------')
  dumpAttributes(splitter)
}

function dumpTree(node, depth) {
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + "
  var kids = node.childNodes
  debug(indent.substr(indent.length - depth*2))

  // Print your favorite attributes here
  debug(node.nodeName)
  debug(" "+node.getAttribute('id'))
  debug("")

  for (var ii=0; ii < kids.length; ii++) {
    dumpTree(kids[ii], depth + 1)
  }
}

// Install our load handler
addEventListener("load", sidebarOverlayInit, false);
