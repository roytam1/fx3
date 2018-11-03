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
 * The Original Code is tab UI for Camino.
 *
 * The Initial Developer of the Original Code is
 * Geoff Beier.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Geoff Beier <me@mollyandgeoff.com>
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

#import <Cocoa/Cocoa.h>
#import "BrowserTabView.h"
#import "TabButtonCell.h"

@interface BrowserTabBarView : NSView 
{
@private
  // this tab view should be tabless and borderless
  IBOutlet BrowserTabView*  mTabView;
  
  TabButtonCell*    mActiveTabButton;     // active tab button, mainly useful for handling drags (STRONG)
  NSButton*         mOverflowButton;      // button for overflow menu if we've got more tabs than space (STRONG)
  NSMenu*           mOverflowMenu;        // menu for tab overflow (STRONG);
  
  // drag tracking
  NSPoint           mLastClickPoint;
  BOOL              mDragOverBar;         // either over a button, or the bar background
  TabButtonCell*    mDragDestButton;
  
  BOOL              mVisible;             // whether tabs are visible or not; used to disable creation of tracking rects when they're not
  BOOL              mOverflowTabs;        // track whether there are more tabs than we can fit onscreen
  NSMutableArray*   mTrackingCells;       // cells which currently have tracking rects in this view
  
  NSImage*          mBackgroundImage;
  NSImage*          mButtonDividerImage;
}

// destroy the tab bar and recreate it from the tabview
-(void)rebuildTabBar;
// return the height the tab bar should be
-(float)tabBarHeight;
-(BrowserTabViewItem*)tabViewItemAtPoint:(NSPoint)location;
-(void)windowClosed;
-(IBAction)overflowMenu:(id)sender;
-(BOOL)isVisible;
// show or hide tabs- should be called if this view will be hidden, to give it a chance to register or
// unregister tracking rects as appropriate
-(void)setVisible:(BOOL)show;

@end
