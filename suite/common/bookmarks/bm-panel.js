/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Stephen Lamm <slamm@netscape.com>
 */

/*
  Code for the Bookmarks Sidebar Panel
 */

function clicked(event, target) {
  if (target.getAttribute('container') == 'true') {
    if (target.getAttribute('open') == 'true') {
	  target.removeAttribute('open');
    } else {
      target.setAttribute('open','true');
    }
  } else {
    if (event.clickCount == 2) {
      top.OpenBookmarkURL(target, document.getElementById('bookmarksTree').database);
    }
  }
}
