/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
var completed = false;
var testcases;

var TT = "<tt>";
var TT_ = "<tt>";
var BR = "<br>";
var NBSP = "&nbsp;";
var CR = "\n";
var FONT = "";
var FONT_ = "</font>";
var FONT_RED = "<font color=\"red\">";
var FONT_GREEN = "<font color=\"green\">";
var B = "<b>";
var B_ = "</b>"
var H2 = "<h2>";
var H2_ = "</h2>";
var HR = "<hr>";

function htmlesc(str) { 
  if (str == '<') 
    return '&lt;'; 
  if (str == '>') 
    return '&gt;'; 
  if (str == '&') 
    return '&amp;'; 
  return str; 
}

function writeLineToLog( string ) {
  string = String(string);
  string = string.replace(/[<>&]/g, htmlesc);
  document.write( string + "<br>\n" );
}
function writeHeaderToLog( string ) {
  string = String(string);
  string = string.replace(/[<>&]/g, htmlesc);
  document.write( "<h2>" + string + "</h2>\n" );
}
function writeFormattedResult( expect, actual, string, passed ) {
  string = String(string);
  string = string.replace(/[<>&]/g, htmlesc);
  var s = "<tt>"+ string ;
  s += "<b>" ;
  s += ( passed ) ? "<font color=#009900> &nbsp;" + PASSED
    : "<font color=#aa0000>&nbsp;" +  FAILED + expect + "</tt>";
  document.write( s + "</font></b></tt><br>" );
  return passed;
}
function ToInteger( t ) {
  t = Number( t );

  if ( isNaN( t ) ){
    return ( Number.NaN );
  }
  if ( t == 0 || t == -0 ||
       t == Number.POSITIVE_INFINITY || t == Number.NEGATIVE_INFINITY ) {
    return 0;
  }

  var sign = ( t < 0 ) ? -1 : 1;

  return ( sign * Math.floor( Math.abs( t ) ) );
}
function Enumerate ( o ) {
  var p;
  for ( p in o ) {
    writeLineToLog( p +": " + o[p] );
  }
}

/*
 * The earlier versions of the test code used exceptions
 * to terminate the test script in "negative" test cases
 * before the failure reporting code could run. In order
 * to be able to capture errors for the "negative" case 
 * where the exception is a sign the test actually passed,
 * the err online handler will assume that any error is a 
 * failure unless gExceptionExpected is true.
 */
window.onerror = err;
var gExceptionExpected = false;

function err( msg, page, line ) {
  var testcase;

  if (typeof(EXPECTED) == "undefined" || EXPECTED != "error") {
/*
 * an unexpected exception occured
 */
    writeLineToLog( "Test failed with the message: " + msg );
    testcase = new TestCase(SECTION, "unknown", "unknown", "unknown");
    testcase.passed = false;
    testcase.reason = "Error: " + msg + 
	    " Source File: " + page + " Line: " + line + ".";
    if (document.location.href.indexOf('-n.js') != -1)
    {
      // negative test
      testcase.passed = true;
    }
    return;
  }

  if (typeof SECTION == 'undefined')
  {
    SECTION = 'Unknown';
  }
  if (typeof DESCRIPTION == 'undefined')
  {
    DESCRIPTION = 'Unknown';
  }
  if (typeof EXPECTED == 'undefined')
  {
    EXPECTED = 'Unknown';
  }

  testcase = new TestCase(SECTION, DESCRIPTION, EXPECTED, "error");
  testcase.reason += "Error: " + msg + 
    " Source File: " + page + " Line: " + line + ".";
  stopTest();
}

var gVersion = 0;

function version(v)
{
  if (v) { 
    gVersion = v; 
  } 
  return gVersion; 
}

function gc()
{
  try
  {
    // Thanks to dveditz
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    var jsdIDebuggerService = Components.interfaces.jsdIDebuggerService;
    var service = Components.classes['@mozilla.org/js/jsd/debugger-service;1'].
      getService(jsdIDebuggerService);
    service.GC();
  }
  catch(ex)
  {
    // Thanks to igor.bukanov@gmail.com
    var tmp = Math.PI * 1e500, tmp2;
    for (var i = 0; i != 1 << 15; ++i) 
    {
      tmp2 = tmp * 1.5;
    }
  }
}
