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

/**
   File Name:      method-005.js
   Description:

   Assigning a Java method to a JavaScript object should not change the
   context associated with the Java method -- its this object should
   be the Java object, not the JavaScript object.

   @author     christine@netscape.com
   @version    1.00
*/
var SECTION = "LiveConnect Objects";
var VERSION = "1_3";
var TITLE   = "Assigning a Static Java Method to a JavaScript object";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var java_string = new java.lang.String("LiveConnect");
var js_string   = "JavaScript";

js_string.startsWith = java_string.startsWith;
/*
  new TestCase(
  SECTION,
  "var java_string = new java.lang.String(\"LiveConnect\");" +
  "var js_string = \"JavaScript\"" +
  "js_string.startsWith = java_string.startsWith"+
  "js_string.startsWith(\"J\")",
  false,
  js_string.startsWith("J") );
*/
var mo = new MyObject();

var c = mo.classForName( "java.lang.String" );

new TestCase(
    SECTION,
    "var mo = new MyObject(); "+
    "var c = mo.classForName(\"java.lang.String\");" +
    "c.equals(java.lang.Class.forName(\"java.lang.String\))",
    true,
    c.equals(java.lang.Class.forName("java.lang.String")) );



test();

function MyObject() {
    this.println = java.lang.System.out.println;
    this.classForName = java.lang.Class.forName;
    return this;
}

