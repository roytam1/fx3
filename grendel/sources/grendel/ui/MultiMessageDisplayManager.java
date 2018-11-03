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
 * Created: Will Scullin <scullin@netscape.com>,  3 Sep 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.util.Enumeration;
import java.util.Vector;

import javax.swing.event.ChangeEvent;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Store;

import grendel.view.ViewedFolder;
import grendel.view.ViewedMessage;
import javax.swing.tree.TreePath;

import com.trfenv.parsers.Event;

/**
 *Main Grendel window
 */
public class MultiMessageDisplayManager extends MessageDisplayManager {
  MasterFrame     fMasterFrame;

  static MultiMessageDisplayManager fDisplayMaster = null;

  public static MultiMessageDisplayManager Get() {
    if (fDisplayMaster == null) {
      fDisplayMaster = new MultiMessageDisplayManager();
    }
    return fDisplayMaster;
  }

  /**
   * Displays a message given a Message object.
   */

  public void displayMessage(Message aMessage) {
    MessageFrame frame = MessageFrame.FindMessageFrame(aMessage);
    if (frame == null) {
      frame = new MessageFrame(null);
    }
    frame.setVisible(true);
    frame.toFront();
    frame.requestFocus();
    frame.setMessage(aMessage);
  }

  /**
   * Displays a folder given a folder object.
   */

  public void displayFolder(Folder aFolder) {
    displayFolder(aFolder, null);
  }

  /**
   * Displays folder given a Folder object and
   * selects a message in that folder given a Message
   * object.
   */

  public void displayFolder(Folder aFolder, Message aMessage) {
    FolderFrame frame = FolderFrame.FindFolderFrame(aFolder);
    if (frame == null) {
      frame = new FolderFrame(aFolder);
      frame.setVisible(true);
    }
    frame.toFront();
    frame.requestFocus();

    if (aMessage != null) {
    }
  }

  /**
   * Displays the master.
   */

  public void displayMaster() {
    displayMaster(null);
  }

  /**
   * Displays the master with the given folder selected.
   */

  public void displayMaster(Folder aFolder) {
    if (fMasterFrame == null) {
      fMasterFrame = new MasterFrame();
    }
    fMasterFrame.setVisible(true);
    fMasterFrame.toFront();
    fMasterFrame.requestFocus();

    if (aFolder != null) {

    }
  }
}

class MasterFrame extends GeneralFrame {
  MasterPanel fMasterPanel;

  public MasterFrame() {
    super("masterFrameLabel", "session");

    PrefsDialog.CheckPrefs(this);

    fMasterPanel = new MasterPanel();
    fMasterPanel.addMasterPanelListener(new FolderSelectionListener());
    fPanel.add(fMasterPanel);

    XMLMenuBuilder builder = new XMLMenuBuilder(actions);
    fMenu = builder.buildFrom("ui/grendel.xml", this);
    getRootPane().setJMenuBar(fMenu);

    fToolBar = fMasterPanel.getToolBar();
    fToolBarPanel.add(fToolBar);

    fStatusBar = buildStatusBar();
    fPanel.add(BorderLayout.SOUTH, fStatusBar);

    restoreBounds();
  }

  public void dispose() {
    saveBounds();

    fMasterPanel.dispose();

    super.dispose();
  }

  //
  // FolderSelectionListener class
  //

  class FolderSelectionListener implements MasterPanelListener {
    public void masterSelectionChanged(ChangeEvent aEvent) {
    }

    public void masterSelectionDoubleClicked(ChangeEvent aEvent) {
      TreePath path = null;
      Enumeration selection = ((MasterPanel) aEvent.getSource()).getSelection();

      MessageDisplayManager master = MultiMessageDisplayManager.Get();

      while (selection.hasMoreElements()) {
        path = (TreePath) selection.nextElement();

        if (path != null) {
          Object node = path.getPath()[path.getPath().length - 1];
          Folder folder = null;

          if (node instanceof ViewedFolder) {
            folder = ((ViewedFolder) node).getFolder();
          }
          if (folder != null) {
            try {
              if ((folder.getType() & Folder.HOLDS_MESSAGES) == 0) {
                folder = null;
              }
            } catch (MessagingException e) {
              folder = null;
            }
          }

          if (folder != null) {
            master.displayFolder(folder);
          }
        }
      }
    }
  }

  // Action array

  Event actions[] = { ActionFactory.GetExitAction(),
                       ActionFactory.GetNewMailAction(),
                       ActionFactory.GetComposeMessageAction()};
}
