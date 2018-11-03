/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Will Scullin <scullin@netscape.com>, 23 Oct 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 */

package grendel.ui;

import java.util.EventListener;

import javax.swing.event.ChangeEvent;

import grendel.widgets.StatusEvent;

public interface MessagePanelListener extends EventListener {
  public void loadingMessage(ChangeEvent aEvent);
  public void messageLoaded(ChangeEvent aEvent);
  public void messageStatus(StatusEvent aEvent);
}
