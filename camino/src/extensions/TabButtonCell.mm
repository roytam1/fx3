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

#import "ImageAdditions.h"
#import "NSBezierPath+Utils.h"

#import "TabButtonCell.h"
#import "TruncatingTextAndImageCell.h"

static const int kTabLeftMargin = 4; //distance between left edge and close button
static const int kTabCloseButtonPad = 2; //distance between close button and label
static const int kTabRightMargin = 4; //distance between label cell and right edge 
static const int kTabBottomPad = 4; // number of pixels to offset from the bottom
static const int kTabSelectOffset =  1; //number of pixels to drop everything down when selected

static NSImage* gTabLeft = nil;
static NSImage* gTabRight = nil;
static NSImage* gActiveTabBg = nil;
static NSImage* gTabMouseOverBg = nil;
static NSImage* gTabButtonDividerImage = nil;

@interface TabButtonCell (PRIVATE)

+(void)loadImages;

@end

// these are the "tabs" for the tabless tab view
// to keep them lighter weight, they are not responsible for creating/maintaining much of anything
// the BrowserTabViewItem maintains the spinner, favicon, label and close button... this just positions
// them and brings them in and out of view, as well as painting itself and (TODO) handles dnd.

@implementation TabButtonCell

-(id)initFromTabViewItem:(BrowserTabViewItem *)tabViewItem
{
  if ((self = [super init]))
  {
    mTabViewItem = tabViewItem;
    mNeedsDivider = YES;
  }
  return self;
}

-(void)dealloc
{
  [mCloseButton removeFromSuperview];
  [mCloseButton release];
  [super dealloc];
}

-(BOOL)isSelected
{
  return ([mTabViewItem tabState] == NSSelectedTab);
}

-(BrowserTabViewItem *)tabViewItem
{
  return mTabViewItem;
}

// TODO
-(BOOL)willTrackMouse:(NSEvent*)theEvent inRect:(NSRect)cellFrame ofView:(NSView*)controlView
{
  return NO;
}

-(void)drawWithFrame:(NSRect)rect inView:(NSView*)controlView
{
  if (!gTabLeft || !gTabRight || !gActiveTabBg || !gTabMouseOverBg || !gTabButtonDividerImage)
    [TabButtonCell loadImages];
  
  // XXX is it worth it to see if the load failed? I don't think so, as if it did, the bundle is trashed and
  // we have bigger problems
  
  NSSize textSize = [mTabViewItem sizeOfLabel:NO];
  
  // retain a reference to the close button; otherwise we can't be sure it's removed from the view hierarchy
  // during our d'tor, when mTabViewItem will be invalid.
  if (!mCloseButton) mCloseButton = [[mTabViewItem closeButton] retain];
  NSSize buttonSize = [mCloseButton frame].size;
  
  // center based on the larger of the two heights if there's a difference
  float maxHeight = textSize.height > buttonSize.height ? textSize.height : buttonSize.height;
  NSRect buttonRect = NSMakeRect(rect.origin.x + kTabLeftMargin,
                                  rect.origin.y + (int)((rect.size.height - kTabBottomPad - maxHeight)/2.0 + kTabBottomPad),
                                  buttonSize.width, buttonSize.height);
  NSRect labelRect = NSMakeRect(NSMaxX(buttonRect) + kTabCloseButtonPad, 
                                rect.origin.y + (int)((rect.size.height - kTabBottomPad - maxHeight)/2.0 + kTabBottomPad),
                                rect.size.width - (NSWidth(buttonRect) +kTabCloseButtonPad+kTabLeftMargin+kTabRightMargin),
                                textSize.height);
  
  TruncatingTextAndImageCell *labelCell = [mTabViewItem labelCell];
  
  // make sure the close button and the favicon line up properly
  [labelCell setImagePadding:0.0];
  [labelCell setMaxImageHeight:buttonSize.height];

  if (mNeedsDivider) {
    rect.size.width -= [gTabButtonDividerImage size].width;
    [gTabButtonDividerImage compositeToPoint:NSMakePoint(NSMaxX(rect), rect.origin.y) operation:NSCompositeSourceOver];
  }
  
  NSPoint patternOrigin = [controlView convertPoint:NSMakePoint(0.0f, 0.0f) toView:nil];

  if ([mTabViewItem tabState] == NSSelectedTab) {
    // move things down a little, to give the impression of being pulled forward
    labelRect.origin.y -= kTabSelectOffset;
    buttonRect.origin.y -= kTabSelectOffset;
    // XXX would it be better to maintain a CGContext and do a real gradient here?
    // it sounds heavier, but I haven't tested to be sure. This looks just as nice so long as
    // the tabbar height is fixed.
    NSRect bgRect = NSMakeRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    bgRect.origin.x += [gTabLeft size].width;
    bgRect.size.width -= ([gTabRight size].width + [gTabLeft size].width);
    [gActiveTabBg drawTiledInRect:rect origin:patternOrigin operation: NSCompositeSourceOver];
    [gTabLeft compositeToPoint:NSMakePoint(rect.origin.x, rect.origin.y) operation:NSCompositeSourceOver];
    [gTabRight compositeToPoint:NSMakePoint(NSMaxX(bgRect), bgRect.origin.y) operation:NSCompositeSourceOver];
  } else if ([self mouseWithin] && ![self dragTarget]) {
    [gTabMouseOverBg drawTiledInRect:rect origin:patternOrigin operation:NSCompositeSourceOver];
  }
  // TODO: Make this look nicer
  if ([self dragTarget]) {
    rect.origin.y += kTabBottomPad;
    NSBezierPath* dropTargetOutline = [NSBezierPath bezierPathWithRoundCorneredRect:rect cornerRadius:2.0f];
    [[[NSColor colorForControlTint:NSDefaultControlTint] colorWithAlphaComponent:0.5] set];
    [dropTargetOutline fill];
  }
  if (controlView != [mCloseButton superview]) {
    [controlView addSubview:mCloseButton];
  }
  [mCloseButton setFrame:buttonRect];
  [labelCell drawInteriorWithFrame:labelRect inView:controlView];
}

-(void)addTrackingRectInView:(NSView *)aView withFrame:(NSRect)trackingRect cursorLocation:(NSPoint)currentLocation
{
  [super addTrackingRectInView:aView withFrame:trackingRect cursorLocation:currentLocation];
}

-(void)removeTrackingRectFromView:(NSView *)aView
{
  [super removeTrackingRectFromView:aView];
}

-(void)hideCloseButton
{
  if ([mCloseButton superview] != nil)
    [mCloseButton removeFromSuperview];
}

-(void)setDrawDivider:(BOOL)willDraw
{
  mNeedsDivider = willDraw;
}


+(void)loadImages
{
  if (!gTabLeft)                gTabLeft                = [[NSImage imageNamed:@"tab_left_side"] retain];
  if (!gTabRight)               gTabRight               = [[NSImage imageNamed:@"tab_right_side"] retain];
  if (!gActiveTabBg)            gActiveTabBg            = [[NSImage imageNamed:@"tab_active_bg"] retain];
  if (!gTabMouseOverBg)         gTabMouseOverBg         = [[NSImage imageNamed:@"tab_hover"] retain];
  if (!gTabButtonDividerImage)  gTabButtonDividerImage  = [[NSImage imageNamed:@"tab_button_divider"] retain];
}


@end
