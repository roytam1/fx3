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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Norris Boyd
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

var SECTION = "JavaObject Field or method access";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
              SECTION;
startTest();

var dt = new DT();

var a = [
  "boolean",
  "byte",
  "integer", 
  "double",
  "float",
  "short",
  "char"
  ];

var v = [
  true,
  1,
  2,
  3.0,
  4.0,
  5,
  'a'.charCodeAt(0)
  ];

for (var i=0; i < a.length; i++) {
  var name = a[i];
  var getterName = "get" + a[i].charAt(0).toUpperCase() + 
    a[i].substring(1);
  var setterName = "set" + a[i].charAt(0).toUpperCase() + 
    a[i].substring(1);
  new TestCase(
    "dt['" + name + "'] == dt." + getterName + "()",
    dt[name],
    dt[getterName]() );

  dt[name] = v[i];
  new TestCase(
    "dt['" + name + "'] = "+ v[i] +"; dt." + getterName + "() == " + v[i],
    dt[getterName](),
    v[i]);
}

test();

