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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#import <AddressBook/AddressBook.h>
#import "ABAddressBook+Utils.h"

#import "NSString+Utils.h"
#import "NSSplitView+Utils.h"

#import "BrowserWindowController.h"
#import "BrowserWindow.h"

#import "BookmarkToolbar.h"
#import "BookmarkViewController.h"
#import "BookmarkManager.h"
#import "AddBookmarkDialogController.h"
#import "ProgressDlgController.h"
#import "PageInfoWindowController.h"

#import "BrowserContentViews.h"
#import "BrowserWrapper.h"
#import "PreferenceManager.h"
#import "BrowserTabView.h"
#import "BrowserTabViewItem.h"
#import "UserDefaults.h"
#import "PageProxyIcon.h"
#import "AutoCompleteTextField.h"
#import "SearchTextField.h"
#import "SearchTextFieldCell.h"
#import "STFPopUpButtonCell.h"
#import "DraggableImageAndTextCell.h"
#import "MVPreferencesController.h"
#import "ViewCertificateDialogController.h"
#import "ExtendedSplitView.h"
#import "wallet.h"

#include "nsString.h"
#include "nsCRT.h"
#include "nsServiceManagerUtils.h"

#include "CHBrowserService.h"
#include "GeckoUtils.h"

#include "nsIWebNavigation.h"
#include "nsISHistory.h"
#include "nsIHistoryEntry.h"
#include "nsIHistoryItems.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMLocation.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIContextMenuListener.h"
#include "nsIDOMWindow.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserChrome.h"
#include "nsNetUtil.h"
#include "nsIPref.h"
#include "nsISupportsArray.h"

#include "nsIClipboardCommands.h"
#include "nsICommandManager.h"
#include "nsICommandParams.h"
#include "nsIWebBrowser.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsIBrowserHistory.h"
#include "nsIPermissionManager.h"
#include "nsIWebPageDescriptor.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIFocusController.h"
#include "nsIX509Cert.h"

#include "nsAppDirectoryServiceDefs.h"

static NSString* const BrowserToolbarIdentifier	        = @"Browser Window Toolbar Combined";
static NSString* const BackToolbarItemIdentifier	      = @"Back Toolbar Item";
static NSString* const ForwardToolbarItemIdentifier	    = @"Forward Toolbar Item";
static NSString* const ReloadToolbarItemIdentifier	    = @"Reload Toolbar Item";
static NSString* const StopToolbarItemIdentifier	      = @"Stop Toolbar Item";
static NSString* const HomeToolbarItemIdentifier	      = @"Home Toolbar Item";
static NSString* const CombinedLocationToolbarItemIdentifier  = @"Combined Location Toolbar Item";
static NSString* const BookmarksToolbarItemIdentifier	  = @"Sidebar Toolbar Item";    // note legacy name
static NSString* const PrintToolbarItemIdentifier	      = @"Print Toolbar Item";
static NSString* const ThrobberToolbarItemIdentifier    = @"Throbber Toolbar Item";
static NSString* const SearchToolbarItemIdentifier      = @"Search Toolbar Item";
static NSString* const ViewSourceToolbarItemIdentifier  = @"View Source Toolbar Item";
static NSString* const BookmarkToolbarItemIdentifier    = @"Bookmark Toolbar Item";
static NSString* const TextBiggerToolbarItemIdentifier  = @"Text Bigger Toolbar Item";
static NSString* const TextSmallerToolbarItemIdentifier = @"Text Smaller Toolbar Item";
static NSString* const NewTabToolbarItemIdentifier      = @"New Tab Toolbar Item";
static NSString* const CloseTabToolbarItemIdentifier    = @"Close Tab Toolbar Item";
static NSString* const SendURLToolbarItemIdentifier     = @"Send URL Toolbar Item";
static NSString* const DLManagerToolbarItemIdentifier   = @"Download Manager Toolbar Item";
static NSString* const FormFillToolbarItemIdentifier    = @"Form Fill Toolbar Item";
static NSString* const HistoryToolbarItemIdentifier     = @"History Toolbar Item";

int TabBarVisiblePrefChangedCallback(const char* pref, void* data);
static const char* const gTabBarVisiblePref = "camino.tab_bar_always_visible";

const float kMininumURLAndSearchBarWidth = 128.0;

static NSString* const NavigatorWindowFrameSaveName = @"NavigatorWindow";
static NSString* const NavigatorWindowSearchBarWidth = @"SearchBarWidth";
static NSString* const NavigatorWindowSearchBarHidden = @"SearchBarHidden";

static NSString* const kViewSourceProtocolString = @"view-source:";
const unsigned long kNoToolbarsChromeMask = (nsIWebBrowserChrome::CHROME_ALL & ~(nsIWebBrowserChrome::CHROME_TOOLBAR |
                                                                                 nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR |
                                                                                 nsIWebBrowserChrome::CHROME_LOCATIONBAR)); 

// Cached toolbar defaults read in from a plist. If null, we'll use
// hardcoded defaults.
static NSArray* sToolbarDefaults = nil;

#pragma mark -

// small class that owns C++ objects on behalf of BrowserWindowController.
// this just allows us to use nsCOMPtr rather than doing manual refcounting.
class BWCDataOwner
{
public:

  BWCDataOwner()
  : mContextMenuFlags(0)
  , mGotOnContextMenu(false)
  {
    mGlobalHistory = do_GetService("@mozilla.org/browser/global-history;2");
    mURIFixer      = do_GetService("@mozilla.org/docshell/urifixup;1");
  }
  
  nsCOMPtr<nsIURIFixup>         mURIFixer;
  nsCOMPtr<nsIBrowserHistory>   mGlobalHistory;

  int                           mContextMenuFlags;
  nsCOMPtr<nsIDOMEvent>         mContextMenuEvent;
  nsCOMPtr<nsIDOMNode>          mContextMenuNode;
  bool                          mGotOnContextMenu;
};


#pragma mark -

// This little class exists so that we can clear up the context menu-related
// pointers in the mDataOwner at autorelease time. See the comments in -onShowContextMenu:
@interface ContextMenuDataClearer : NSObject
{
  id              mTarget;    // retained
  SEL             mSelector;
}

- (id)initWithTarget:(id)inTarget selector:(SEL)inSelector;

@end

@implementation ContextMenuDataClearer

- (id)initWithTarget:(id)inTarget selector:(SEL)inSelector
{
  if ((self = [super init]))
  {
    mTarget   = [inTarget retain];
    mSelector = inSelector;
  }
  return self;
}

-(void)dealloc
{
  [mTarget performSelector:mSelector];    // do our work
  [mTarget release];
  [super dealloc];
}

@end // ContextMenuDataClearer

#pragma mark -

//////////////////////////////////////
@interface AutoCompleteTextFieldEditor : NSTextView
{
  NSFont* mDefaultFont;	// will be needed if editing empty field
  NSUndoManager *mUndoManager; //we handle our own undo to avoid stomping on bookmark undo
}
- (id)initWithFrame:(NSRect)bounds defaultFont:(NSFont*)defaultFont;
@end

@implementation AutoCompleteTextFieldEditor

// sets the defaultFont so that we don't paste in the wrong one
- (id)initWithFrame:(NSRect)bounds defaultFont:(NSFont*)defaultFont
{
  if ((self = [super initWithFrame:bounds])) {
    mDefaultFont = defaultFont;
    mUndoManager = [[NSUndoManager alloc] init];
    [self setDelegate:self];
  }
  return self;
}

-(void) dealloc
{
  [mUndoManager release];
  [super dealloc];
}

-(void)paste:(id)sender
{
  NSPasteboard *pboard = [NSPasteboard generalPasteboard];
  NSEnumerator *dataTypes = [[pboard types] objectEnumerator];
  NSString *aType;
  while ((aType = [dataTypes nextObject])) {
    if ([aType isEqualToString:NSStringPboardType]) {
      NSString *oldText = [pboard stringForType:NSStringPboardType];
      NSString *newText = [oldText stringByRemovingCharactersInSet:[NSCharacterSet controlCharacterSet]];
      NSRange aRange = [self selectedRange];
      if ([self shouldChangeTextInRange:aRange replacementString:newText]) {
        [[self textStorage] replaceCharactersInRange:aRange withString:newText];
        if (NSMaxRange(aRange) == 0 && mDefaultFont) // will only be true if the field is empty
          [self setFont:mDefaultFont];	// wrong font will be used otherwise
        [self didChangeText];
      }
      // after a paste, the insertion point should be after the pasted text
      unsigned int newInsertionPoint = aRange.location + [newText length];
      [self setSelectedRange:NSMakeRange(newInsertionPoint,0)];
      return;
    }
  }
}

- (NSUndoManager *)undoManagerForTextView:(NSTextView *)aTextView
{
  if (aTextView == self)
    return mUndoManager;
  return nil;
}

// Opt-return and opt-enter should download the URL in the location bar
- (void)insertNewlineIgnoringFieldEditor:(id)sender
{
  BrowserWindowController* bwc = (BrowserWindowController *)[[[self delegate] window] delegate];
  [bwc saveURL:nil url:[self string] suggestedFilename:nil];
}

@end
//////////////////////////////////////

#pragma mark -

//
// IconPopUpCell
//
// A popup cell that displays only an icon with no border, yet retains the
// behaviors of a popup menu. It's amazing you can't get this w/out having
// to subclass, but *shrug*.
//
@interface IconPopUpCell : NSPopUpButtonCell
{
@private
  NSImage* fImage;
  NSRect fSrcRect;      // rect cached for drawing, same size as image
}
- (id)initWithImage:(NSImage *)inImage;
@end

@implementation IconPopUpCell

- (id)initWithImage:(NSImage *)inImage
{
  if ( (self = [super initTextCell:@"" pullsDown:YES]) )
  {
    fImage = [inImage retain];
    fSrcRect = NSMakeRect(0,0,0,0);
    fSrcRect.size = [fImage size];
  }
  return self;
}

- (void)dealloc
{
  [fImage release];
  [super dealloc];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
  [fImage setFlipped:[controlView isFlipped]];
  cellFrame.size = fSrcRect.size;                  // don't scale
  [fImage drawInRect:cellFrame fromRect:fSrcRect operation:NSCompositeSourceOver fraction:1.0];
}

@end

#pragma mark -

//
// interface ToolbarViewItem
//
// NSToolbarItem, by default, doesn't do validation for view items. Override
// that behavior to call |-validateToolbarItem:| on the item's target.
//
@interface ToolbarViewItem : NSToolbarItem
{
}
@end

@implementation ToolbarViewItem

//
// -validate
//
// Override default behavior (which does nothing at all for a view item) to
// ask the target to handle it. The target must perform all the appropriate
// enabling/disabling within |-validateToolbarItem:| because we can't know
// all the details. The return value is ignored.
//
- (void)validate
{
  id target = [self target];
  if ([target respondsToSelector:@selector(validateToolbarItem:)])
    [target validateToolbarItem:self];
}

@end

#pragma mark -

//
// interface ToolbarButton
//
// A subclass of NSButton that responds to |-setControlSize:| which
// comes from the toolbar when it changes sizes. Adjust the size
// of our associated NSToolbarItem when the call comes.
//
// Note that |-setControlSize:| is not part of NSView's api, but the
// toolbar code calls it anyway, without any documentation to that
// effect.
//
@interface ToolbarButton : NSButton
{
  NSToolbarItem* mToolbarItem;
}
-(id)initWithFrame:(NSRect)inFrame item:(NSToolbarItem*)inItem;
@end

@implementation ToolbarButton

-(id)initWithFrame:(NSRect)inFrame item:(NSToolbarItem*)inItem
{
  if ((self = [super initWithFrame:inFrame])) {
    mToolbarItem = inItem;
  }
  return self;
}

//
// -setControlSize:
//
// Called by the toolbar when the toolbar changes icon size. Adjust our
// toolbar item so that it can adjust larger or smaller.
//
- (void)setControlSize:(NSControlSize)size
{
  NSSize s;
  if (size == NSRegularControlSize) {
    s = NSMakeSize(32., 32.);
    [mToolbarItem setMinSize:s];
    [mToolbarItem setMaxSize:s];
  }
  else {
    s = NSMakeSize(24., 24.);
    [mToolbarItem setMinSize:s];
    [mToolbarItem setMaxSize:s];
  }
  [[self image] setSize:s];
}

//
// -controlSize
//
// The toolbar assumes this implemented whenever |-setControlSize:| is implemented,
// though I'm not sure why. 
//
- (NSControlSize)controlSize
{
  return [[self cell] controlSize];
}

@end

#pragma mark -

enum BWCOpenDest {
  kDestinationNewWindow = 0,
  kDestinationNewTab,
  kDestinationCurrentView
};

@interface BrowserWindowController(Private)
  // open a new window or tab, but doesn't load anything into them. Must be matched
  // with a call to do that.
- (BrowserWindowController*)openNewWindow:(BOOL)aLoadInBG;
- (BrowserTabViewItem*)openNewTab:(BOOL)aLoadInBG;

- (void)setupToolbar;
- (void)setGeckoActive:(BOOL)inActive;
- (BOOL)isResponderGeckoView:(NSResponder*) responder;
- (NSString*)getContextMenuNodeDocumentURL;
- (void)loadSourceOfURL:(NSString*)urlStr inBackground:(BOOL)loadInBackground;
- (void)transformFormatString:(NSMutableString*)inFormat domain:(NSString*)inDomain search:(NSString*)inSearch;
- (void)openNewWindowWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG;
- (void)openNewTabWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG;
- (BOOL)isPageTextFieldFocused;
- (void)performSearch:(SearchTextField *)inSearchField inView:(BWCOpenDest)inDest inBackground:(BOOL)inLoadInBG;
- (void)goToLocationFromToolbarURLField:(AutoCompleteTextField *)inURLField inView:(BWCOpenDest)inDest inBackground:(BOOL)inLoadInBG;

- (BrowserTabViewItem*)tabForBrowser:(BrowserWrapper*)inWrapper;
- (BookmarkViewController*)bookmarkViewControllerForCurrentTab;
- (void)bookmarkableTitle:(NSString **)outTitle URL:(NSString**)outURLString forWrapper:(BrowserWrapper*)inWrapper;

- (void)clearContextMenuTarget;
- (void)updateLock:(unsigned int)securityState;

// create back/forward session history menus on toolbar button
- (IBAction)backMenu:(id)inSender;
- (IBAction)forwardMenu:(id)inSender;

@end

#pragma mark -

@implementation BrowserWindowController

- (id)initWithWindowNibName:(NSString *)windowNibName
{
  if ( (self = [super initWithWindowNibName:(NSString *)windowNibName]) )
  {
    // we cannot rely on the OS to correctly cascade new windows (RADAR bug 2972893)
    // so we turn off the cascading. We do it at the end of |windowDidLoad|
    [self setShouldCascadeWindows:NO];
    
    mInitialized = NO;
    mMoveReentrant = NO;
    mShouldAutosave = YES;
    mShouldLoadHomePage = YES;
    mChromeMask = 0;
    mThrobberImages = nil;
    mThrobberHandler = nil;
    mURLFieldEditor = nil;
    mProgressSuperview = nil;
    mBookmarkToolbarItem = nil;
    mSidebarToolbarItem = nil;
  
    // register for services
    NSArray* sendTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
    NSArray* returnTypes = [NSArray arrayWithObjects:NSStringPboardType, nil];
    [NSApp registerServicesMenuSendTypes:sendTypes returnTypes:returnTypes];
    
    mDataOwner = new BWCDataOwner();
  }
  return self;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
  BOOL windowWithMultipleTabs = ([mTabBrowser numberOfTabViewItems] > 1);
  // When this window gets focus, fix the Close Window modifiers depending
  // on whether we have multiple tabs
  [[NSApp delegate] adjustCloseTabMenuItemKeyEquivalent:windowWithMultipleTabs];
  [[NSApp delegate] adjustCloseWindowMenuItemKeyEquivalent:windowWithMultipleTabs];

  // the widget code (via -viewsWindowDidBecomeKey) takes care
  // of sending focus and activate events to gecko,
  // but we still need to call the embedding activate API
  [self setGeckoActive:YES];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
  // when we are no longer the key window, set the Close shortcut back
  // to Command-W, for other windows.
  [[NSApp delegate] adjustCloseTabMenuItemKeyEquivalent:NO];
  [[NSApp delegate] adjustCloseWindowMenuItemKeyEquivalent:NO];

  // the widget code (via -viewsWindowDidResignKey) takes care
  // of sending focus and activate events to gecko,
  // but we still need to call the embedding activate API
  [self setGeckoActive:NO];
}

- (void)setGeckoActive:(BOOL)inActive
{
  if ([self isResponderGeckoView:[[self window] firstResponder]])
  {
    BrowserWindow* browserWin = (BrowserWindow*)[self window];
    [browserWin setSuppressMakeKeyFront:YES];	// prevent gecko focus bringing the window to the front
    [mBrowserView setBrowserActive:inActive];
    [browserWin setSuppressMakeKeyFront:NO];
  }
}

- (BOOL)isResponderGeckoView:(NSResponder*) responder
{
  return ([responder isKindOfClass:[NSView class]] &&
          [(NSView*)responder isDescendantOf:[mBrowserView getBrowserView]]);
}

- (void)windowDidChangeMain
{
  // On 10.4, the unified title bar and toolbar is used, and the bookmark
  // toolbar's appearance is tweaked to better match the unified look.
  // Its active/inactive state needs to change along with the toolbar's.
  BrowserWindow* browserWin = (BrowserWindow*)[self window];
  if ([browserWin hasUnifiedToolbarAppearance]) {
    BookmarkToolbar* bookmarkToolbar = [self bookmarkToolbar];
    if (bookmarkToolbar)
      [bookmarkToolbar setNeedsDisplay:YES];
  }
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
  // MainController listens for window layering notifications and updates
  // bookmarks.
  [self windowDidChangeMain];
}

- (void)windowDidResignMain:(NSNotification *)notification
{
  // MainController listens for window layering notifications and updates
  // bookmarks.
  [self windowDidChangeMain];
}

-(void)mouseMoved:(NSEvent*)aEvent
{
  if (mMoveReentrant)
      return;
      
  mMoveReentrant = YES;
  NSView* view = [[[self window] contentView] hitTest: [aEvent locationInWindow]];
  [view mouseMoved: aEvent];
  [super mouseMoved: aEvent];
  mMoveReentrant = NO;
}

-(void)autosaveWindowFrame
{
  if (mShouldAutosave) {
    [[self window] saveFrameUsingName: NavigatorWindowFrameSaveName];
    
    // save the width and visibility of the search bar so it's consistent regardless of the
    // size of the next window we create
    const float searchBarWidth = [mSearchBar frame].size.width;
    [[NSUserDefaults standardUserDefaults] setFloat:searchBarWidth forKey:NavigatorWindowSearchBarWidth];
    BOOL isCollapsed = [mLocationToolbarView isSubviewCollapsed:mSearchBar];
    [[NSUserDefaults standardUserDefaults] setBool:isCollapsed forKey:NavigatorWindowSearchBarHidden];
  }
}

-(void)disableAutosave
{
  mShouldAutosave = NO;
}

-(void)disableLoadPage
{
  mShouldLoadHomePage = NO;
}

- (BOOL)windowShouldClose:(id)sender 
{
  if (!mWindowClosesQuietly &&
      [[PreferenceManager sharedInstance] getBooleanPref:"camino.warn_when_closing" withSuccess:NULL])
  {
    unsigned int numberOfTabs = [mTabBrowser numberOfTabViewItems];
    if (numberOfTabs > 1)
    {
      NSString* closeMultipleTabWarning = NSLocalizedString(@"CloseWindowWithMultipleTabsExplFormat", @"");

      nsAlertController* controller = CHBrowserService::GetAlertController();
      BOOL dontShowAgain = NO;
      int result = NSAlertErrorReturn;

      NS_DURING
        // note that this is a pseudo-sheet (and causes Cocoa to complain about runModalForWindow:relativeToWindow).
        // Ideally, we'd be able to get a panel from nsAlertController and run it as a sheet ourselves.
        result = [controller confirmCheckEx:[self window]
                                      title:NSLocalizedString(@"CloseWindowWithMultipleTabsMsg", @"")
                                       text:[NSString stringWithFormat:closeMultipleTabWarning, numberOfTabs]
                                    button1:NSLocalizedString(@"CloseWindowWithMultipleTabsButton", @"")
                                    button2:NSLocalizedString(@"CancelButtonText", @"")
                                    button3:nil
                                   checkMsg:NSLocalizedString(@"CloseWindowWithMultipleTabsCheckboxLabel", @"")
                                 checkValue:&dontShowAgain];
      NS_HANDLER
      NS_ENDHANDLER
      
      if (dontShowAgain)
        [[PreferenceManager sharedInstance] setPref:"camino.warn_when_closing" toBoolean:NO];
      
      return (result == NSAlertDefaultReturn);
    }
  }
  return YES;
}

- (void)windowWillClose:(NSNotification *)notification
{
  mClosingWindow = YES;
    
  [self autosaveWindowFrame];
  
  // ensure that the URL auto-complete popup is closed before the mork
  // database is shut down, or we crash
  [mURLBar clearResults];

  if (mDataOwner)
  {
    nsCOMPtr<nsIHistoryItems> history(do_QueryInterface(mDataOwner->mGlobalHistory));
    if (history)
      history->Flush();
  }

  delete mDataOwner;
  mDataOwner = NULL;

  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
  if (pref)
    pref->UnregisterCallback(gTabBarVisiblePref, TabBarVisiblePrefChangedCallback, self);
  
  // Tell the BrowserTabView the window is closed
  [mTabBrowser windowClosed];
  
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // autorelease just in case we're here because of a window closing
  // initiated from gecko, in which case this BWC would still be on the 
  // stack and may need to stay alive until it unwinds. We've already
  // shut down gecko above, so we can safely go away at a later time.
  [self autorelease];
}

//
// - stopAllPendingLoads
//
// For each tab, stop it from loading
//
- (void)stopAllPendingLoads
{
  int numTabs = [mTabBrowser numberOfTabViewItems];
  for (int i = 0; i < numTabs; i++) {
    NSTabViewItem* item = [mTabBrowser tabViewItemAtIndex: i];
    [[[item view] getBrowserView] stop:NSStopLoadAll];
  }
}

- (void)dealloc
{
#if DEBUG
  NSLog(@"Browser controller died.");
#endif

  // clear the window-level undo manager used by the edit field. Not sure
  // why this isn't automatically done, but we'll leave objects hanging around in
  // the undo/redo if we do not. We also cannot do this in the url bar's dealloc, 
  // it only works if it's here.
  [[[self window] undoManager] removeAllActions];

  // active Gecko connections have already been shut down in |windowWillClose|
  // so we don't need to worry about that here. We only have to be careful
  // not to access anything related to the document, as it's been destroyed. The
  // superclass dealloc takes care of our child NSView's, which include the 
  // BrowserWrappers and their child CHBrowserViews.
  
  [mProgress release];
  [self stopThrobber];
  [mThrobberImages release];
  [mURLFieldEditor release];
  [mLocationToolbarView release];

  delete mDataOwner;    // paranoia; should have been deleted in -windowWillClose

  [super dealloc];
}

//
// windowDidLoad
//
// setup all the things we can't do in the nib. Note that we defer the setup of
// the bookmarks view until the user actually displays it the first time.
//
- (void)windowDidLoad
{
    [super windowDidLoad];

    // we shouldn't have to do this, yet for some reason removing it from
    // the toolbar destroys the view. However, this also helps us by ensuring
    // that we always have a search bar alive to do things with, like redirect
    // context menu searches to.
    [mLocationToolbarView retain];
    // explicitly don't save the splitter position, we want to save it oursevles
    // since we want a different behavior.
    [mLocationToolbarView setAutosaveSplitterPosition:NO];
    
    BOOL mustResizeChrome = NO;
    
    // hide the resize control if specified by the chrome mask
    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) )
      [[self window] setShowsResizeIndicator:NO];
    
    if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR) ) {
      // remove the status bar at the bottom
      // XXX we should just hide it and allow the user to show it again
      [mStatusBar removeFromSuperview];
      mustResizeChrome = YES;
      
      // clear out everything in the status bar we were holding on to. This will cause us to
      // pass nil for these status items into the CHBrowserwWrapper which is what we want. We'll
      // crash if we give them things that have gone away.
      mProgress = nil;
      mStatus = nil;
    }
    else {
      // Retain with a single extra refcount. This allows us to remove
      // the progress meter from its superview without having to worry
      // about retaining and releasing it. Cache the superview of the
      // progress. Dynamically fetch the superview so as not to burden
      // someone rearranging the nib with this detail. Note that this
      // needs to be in a subview from the status bar because if the
      // window resizes while it is hidden, its position wouldn't get updated.
      // Having it in a separate view that stays visible (and is thus
      // involved in the layout process) solves this.
      [mProgress retain];
      mProgressSuperview = [mProgress superview];
      
      // due to a cocoa issue with it updating the bounding box of two rects
      // that both needing updating instead of just the two individual rects
      // (radar 2194819), we need to make the text area opaque.
      [mStatus setBackgroundColor:[NSColor windowBackgroundColor]];
      [mStatus setDrawsBackground:YES];            
    }

    // Set up the toolbar's search text field
    NSMutableArray *searchTitles =
      [NSMutableArray arrayWithArray:[[[BrowserWindowController searchURLDictionary] allKeys] sortedArrayUsingSelector:@selector(compare:)]];

    [searchTitles removeObject:@"PreferredSearchEngine"];

    [mSearchBar addPopUpMenuItemsWithTitles:searchTitles];
    [[[mSearchBar cell] popUpButtonCell] selectItemWithTitle:
      [[BrowserWindowController searchURLDictionary] objectForKey:@"PreferredSearchEngine"]];

    // Set the sheet's search text field
    [mSearchSheetTextField addPopUpMenuItemsWithTitles:searchTitles];
    [[[mSearchSheetTextField cell] popUpButtonCell] selectItemWithTitle:
      [[BrowserWindowController searchURLDictionary] objectForKey:@"PreferredSearchEngine"]];    
    
    // Get our saved dimensions.
    NSRect oldFrame = [[self window] frame];
    BOOL haveSavedFrame = [[self window] setFrameUsingName: NavigatorWindowFrameSaveName];
    if (!haveSavedFrame)
    {
      NSRect mainScreenBounds = [[NSScreen mainScreen] visibleFrame];
      NSRect windowBounds = NSInsetRect(mainScreenBounds, 4.0f, 4.0f);
      const float kDefaultWindowWidth = 800.0f;
      if (NSWidth(windowBounds) > kDefaultWindowWidth)
        windowBounds.size.width = kDefaultWindowWidth;
      [[self window] setFrame:windowBounds display:YES];
    }

    if (NSEqualSizes(oldFrame.size, [[self window] frame].size))
      mustResizeChrome = YES;
    
    mInitialized = YES;

    [[self window] setAcceptsMouseMovedEvents:YES];

    [self setupToolbar];

    // set the size of the search bar to the width it was last time and hide it
    // programmatically if it wasn't visible
    BOOL searchBarHidden = [[NSUserDefaults standardUserDefaults] boolForKey:NavigatorWindowSearchBarHidden];
    if (searchBarHidden)
      [mLocationToolbarView collapseSubviewAtIndex:1];
    else {
      float searchBarWidth = [[NSUserDefaults standardUserDefaults] floatForKey:NavigatorWindowSearchBarWidth];
      if (searchBarWidth <= 0)
        searchBarWidth = kMininumURLAndSearchBarWidth;
      const float currentWidth = [mLocationToolbarView frame].size.width;
      float newDividerPosition = currentWidth - searchBarWidth - [mLocationToolbarView dividerThickness];
      if (newDividerPosition < kMininumURLAndSearchBarWidth)
        newDividerPosition = kMininumURLAndSearchBarWidth;
      [mLocationToolbarView setLeftWidth:newDividerPosition];
      [mLocationToolbarView adjustSubviews];
    }
    
    // set up autohide behavior on tab browser and register for changes on that pref. The
    // default is for it to hide when only 1 tab is visible, so if no pref is found, it will
    // be NO, and that works. However, if any of the JS chrome flags are set, we don't want
    // to let the tab bar show so leave it off and don't register for the pref updates.
    BOOL allowTabBar = YES;
    if (mChromeMask && (!(mChromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR) ||
                        !(mChromeMask & nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR)))
      allowTabBar = NO;
    if (allowTabBar) {
      BOOL tabBarAlwaysVisible = [[PreferenceManager sharedInstance] getBooleanPref:gTabBarVisiblePref withSuccess:nil];
      [mTabBrowser setBarAlwaysVisible:tabBarAlwaysVisible];
      nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
      if (pref)
        pref->RegisterCallback(gTabBarVisiblePref, TabBarVisiblePrefChangedCallback, self);
    }

    // remove the dummy tab view
    [mTabBrowser removeTabViewItem:[mTabBrowser tabViewItemAtIndex:0]];
    
    // create ourselves a new tab and fill it with the appropriate content. If we
    // have a URL pending to be opened here, don't load anything in it, otherwise,
    // load the homepage if that's what the user wants (or about:blank).
    [self createNewTab:(mPendingURL ? eNewTabEmpty : (mShouldLoadHomePage ? eNewTabHomepage : eNewTabAboutBlank))];
    
    // we have a url "pending" from the "open new window with link" command. Deal
    // with it now that everything is loaded.
    if (mPendingURL) {
      if (mShouldLoadHomePage)
        [self loadURL:mPendingURL referrer:mPendingReferrer activate:mPendingActivate allowPopups:mPendingAllowPopups];
      [mPendingURL release];
      [mPendingReferrer release];
      mPendingURL = mPendingReferrer = nil;
    }
    
    [mPersonalToolbar rebuildButtonList];

    BOOL chromeHidesToolbar = (mChromeMask != 0) && !(mChromeMask & nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
    if (chromeHidesToolbar || ![self shouldShowBookmarkToolbar])
      [mPersonalToolbar showBookmarksToolbar:NO];
    
    if (mustResizeChrome)
      [mContentView resizeSubviewsWithOldSize:[mContentView frame].size];
      
    // stagger window from last browser, if there is one. we can't just use autoposition
    // because it doesn't work on multiple monitors (radar bug 2972893). |getFrontmostBrowserWindow|
    // only gets fully chromed windows, so this will do the right thing for popups (yay!).
    const int kWindowStaggerOffset = 22;
    
    NSWindow* lastBrowser = [[NSApp delegate] getFrontmostBrowserWindow];
    if ( lastBrowser && lastBrowser != [self window] ) {
      NSRect screenRect = [[lastBrowser screen] visibleFrame];
      NSRect testBrowserFrame = [lastBrowser frame];
      NSPoint previousOrigin = testBrowserFrame.origin;
      testBrowserFrame.origin.x += kWindowStaggerOffset;
      testBrowserFrame.origin.y -= kWindowStaggerOffset;
      
      // check if this new window position would overlap the dock or go off the screen. We test
      // this by ensuring that it is contained by the  visible screen rect (excluding dock). If
      // not, the window juts out somewhere and needs to be repositioned.
      if ( !NSContainsRect(screenRect, testBrowserFrame) ) {
        // if a normal cascade fails, try shifting horizontally and reseting vertically
        testBrowserFrame.origin.y = NSMaxY(screenRect) - testBrowserFrame.size.height;
        if ( !NSContainsRect(screenRect, testBrowserFrame) ) {
          // if shifting right also fails, try shifting vertically and reseting horizontally instead
          testBrowserFrame.origin.x = NSMinX(screenRect);
          testBrowserFrame.origin.y = previousOrigin.y - kWindowStaggerOffset;
          if ( !NSContainsRect(screenRect, testBrowserFrame) ) {
            // if all else fails, give up and reset to the upper left corner
            testBrowserFrame.origin.x = NSMinX(screenRect);
            testBrowserFrame.origin.y = NSMaxY(screenRect) - testBrowserFrame.size.height;
          }
        }
      }
      // actually move the window
      [[self window] setFrameOrigin: testBrowserFrame.origin];
    }
    
    // if the search field is not on the toolbar, nil out the nextKeyView of the
    // url bar so that we know to break off the toolbar when tabbing. If it is,
    // and we're running on pre-panther, set the search bar as the tab view. We
    // don't want to do this on panther because it will do it for us.
    if (![mSearchBar window])
      [mURLBar setNextKeyView:nil];
    else {
      const float kPantherAppKit = 743.0;
      if (NSAppKitVersionNumber < kPantherAppKit)
        [mURLBar setNextKeyView:mSearchBar];
    }
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(newTab:)
                                        name:kTabBarBackgroundDoubleClickedNotification object:mTabBrowser];

}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
	//if ( mChromeMask && !(mChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) )
  //  return [[self window] frame].size;
	return proposedFrameSize;
}

#pragma mark -

// -createToolbarPopupButton:
//
// Create a new instance of one of our special click-hold popup buttons that knows
// how to display a menu on click-hold. Associate it with the toolbar item |inItem|.
- (NSButton*)createToolbarPopupButton:(NSToolbarItem*)inItem
{
  NSRect frame = NSMakeRect(0.,0.,32.,32.);
  NSButton* button = [[[ToolbarButton alloc] initWithFrame:frame item:inItem] autorelease];
  if (button) {
    DraggableImageAndTextCell* newCell = [[[DraggableImageAndTextCell alloc] init] autorelease];
    [newCell setDraggable:YES];
    [newCell setClickHoldTimeout:0.45];
    [button setCell:newCell];

    [button setBezelStyle: NSRegularSquareBezelStyle];
    [button setButtonType: NSMomentaryChangeButton];
    [button setBordered: NO];
    [button setImagePosition: NSImageOnly];
  }
  return button;
}

- (void)setupToolbar
{
  NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:BrowserToolbarIdentifier] autorelease];
  
  [toolbar setDisplayMode:NSToolbarDisplayModeDefault];
  [toolbar setAllowsUserCustomization:YES];
  [toolbar setAutosavesConfiguration:YES];
  [toolbar setDelegate:self];
  [[self window] setToolbar:toolbar];
  
  // for a chromed window without the toolbar or locationbar flag, hide the toolbar (but allow the user to show it)
  if (mChromeMask && (!(mChromeMask & nsIWebBrowserChrome::CHROME_TOOLBAR) &&
                      !(mChromeMask & nsIWebBrowserChrome::CHROME_LOCATIONBAR)))
  {
    [toolbar setAutosavesConfiguration:NO]; // make sure this hiding doesn't get saved
    [toolbar setVisible:NO];
  }
}

// toolbarWillAddItem: (toolbar delegate method)
//
// Called when a button is about to be added to a toolbar. This is where we should
// cache items we may need later. For instance, we want to hold onto the sidebar
// toolbar item so we can change it when the drawer opens and closes.
- (void)toolbarWillAddItem:(NSNotification *)notification
{
  NSToolbarItem* item = [[notification userInfo] objectForKey:@"item"];
  if ( [[item itemIdentifier] isEqual:BookmarksToolbarItemIdentifier] )
    mSidebarToolbarItem = item;
  else if ( [[item itemIdentifier] isEqual:BookmarkToolbarItemIdentifier] )
    mBookmarkToolbarItem = item;
  else if ( [[item itemIdentifier] isEqual:SearchToolbarItemIdentifier] ) {
    // restore the next key view of the url bar to the search bar, but only
    // if we're on jaguar. On panther, we really don't know that it should
    // be the search toolbar (it could be another toolbar button if full keyboard
    // access is enabled) but it will fix itself automatically.
    const float kPantherAppKit = 743.0;
    if (NSAppKitVersionNumber < kPantherAppKit)
      [mURLBar setNextKeyView:mSearchBar];
  }
}

//
// toolbarDidRemoveItem: (toolbar delegate method)
//
// Called when a button is about to be removed from a toolbar. This is where we should
// uncache items so we don't access them after they're gone. For instance, we want to
// clear our ref to the sidebar toolbar item.
//
- (void)toolbarDidRemoveItem:(NSNotification *)notification
{
  NSToolbarItem* item = [[notification userInfo] objectForKey:@"item"];
  if ( [[item itemIdentifier] isEqual:BookmarksToolbarItemIdentifier] )
    mSidebarToolbarItem = nil;
  else if ( [[item itemIdentifier] isEqual:ThrobberToolbarItemIdentifier] )
    [self stopThrobber];
  else if ( [[item itemIdentifier] isEqual:BookmarkToolbarItemIdentifier] )
    mBookmarkToolbarItem = nil;
  else if ( [[item itemIdentifier] isEqual:SearchToolbarItemIdentifier] ) {
    // search bar removed, set next key view of url bar to nil which tells
    // it to break out of the toolbar tab ring on a tab.
    [mURLBar setNextKeyView:nil];
  }

}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:   BackToolbarItemIdentifier,
                                        ForwardToolbarItemIdentifier,
                                        ReloadToolbarItemIdentifier,
                                        StopToolbarItemIdentifier,
                                        HomeToolbarItemIdentifier,
                                        CombinedLocationToolbarItemIdentifier,
                                        BookmarksToolbarItemIdentifier,
                                        ThrobberToolbarItemIdentifier,
                                        PrintToolbarItemIdentifier,
                                        ViewSourceToolbarItemIdentifier,
                                        BookmarkToolbarItemIdentifier,
                                        NewTabToolbarItemIdentifier,
                                        CloseTabToolbarItemIdentifier,
                                        TextBiggerToolbarItemIdentifier,
                                        TextSmallerToolbarItemIdentifier,
                                        SendURLToolbarItemIdentifier,
                                        NSToolbarCustomizeToolbarItemIdentifier,
                                        NSToolbarFlexibleSpaceItemIdentifier,
                                        NSToolbarSpaceItemIdentifier,
                                        NSToolbarSeparatorItemIdentifier,
                                        DLManagerToolbarItemIdentifier,
                                        FormFillToolbarItemIdentifier,
                                        HistoryToolbarItemIdentifier,
                                        nil];
}

// + toolbarDefaults
//
// Parse a plist called "ToolbarDefaults.plist" in our Resources subfolder. This
// allows anyone to easily customize the default set w/out having to recompile. We
// hold onto the list for the duration of the app to avoid reparsing it every
// time.
+ (NSArray*) toolbarDefaults
{
  if ( !sToolbarDefaults ) {
    sToolbarDefaults = [NSArray arrayWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ToolbarDefaults" ofType:@"plist"]];
    [sToolbarDefaults retain];
  }
  return sToolbarDefaults;
}


- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
  // try to get the defaults from the plist, but if not, hardcode something so
  // the user always has a toolbar.
  NSArray* defaults = [BrowserWindowController toolbarDefaults];
  NS_ASSERTION(defaults, "Couldn't load toolbar defaults from plist");
  return ( defaults ? defaults : [NSArray arrayWithObjects:   BackToolbarItemIdentifier,
                                        ForwardToolbarItemIdentifier,
                                        ReloadToolbarItemIdentifier,
                                        StopToolbarItemIdentifier,
                                        CombinedLocationToolbarItemIdentifier,
                                        BookmarksToolbarItemIdentifier,
                                        nil] );
}

// XXX use a dictionary to speed up the following?
// Better to just read it from a plist.
- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar
      itemForItemIdentifier:(NSString *)itemIdent
  willBeInsertedIntoToolbar:(BOOL)willBeInserted
{
  NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdent] autorelease];
  if ( [itemIdent isEqual:BackToolbarItemIdentifier] && willBeInserted ) {
    // create a new toolbar item that knows how to do validation
    toolbarItem = [[[ToolbarViewItem alloc] initWithItemIdentifier:itemIdent] autorelease];
    
    NSButton* button = [self createToolbarPopupButton:toolbarItem];
    [toolbarItem setLabel:NSLocalizedString(@"Back", @"Back")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Back", @"Go Back")];
    [toolbarItem setToolTip:NSLocalizedString(@"BackToolTip", @"Go back one page")];

    NSSize size = NSMakeSize(32., 32.);
    NSImage* icon = [NSImage imageNamed:@"back"];
    [icon setScalesWhenResized:YES];
    [button setImage:icon];
    
    [toolbarItem setView:button];
    [toolbarItem setMinSize:size];
    [toolbarItem setMaxSize:size];
    
    [button setTarget:self];
    [button setAction:@selector(back:)];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(back:)];      // so validateToolbarItem: works correctly
    [[button cell] setClickHoldAction:@selector(backMenu:)];

    NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];
    [menuFormRep setTarget:self];
    [menuFormRep setAction:@selector(back:)];
    [menuFormRep setTitle:[toolbarItem label]];

    [toolbarItem setMenuFormRepresentation:menuFormRep];
  }
  else if ([itemIdent isEqual:BackToolbarItemIdentifier]) {
    // not going onto the toolbar, don't need to go through the gynmastics above
    // and create a separate view
    [toolbarItem setLabel:NSLocalizedString(@"Back", @"Back")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Back", @"Go Back")];
    [toolbarItem setImage:[NSImage imageNamed:@"back"]];
  }
  else if ( [itemIdent isEqual:ForwardToolbarItemIdentifier] && willBeInserted ) {
    // create a new toolbar item that knows how to do validation
    toolbarItem = [[[ToolbarViewItem alloc] initWithItemIdentifier:itemIdent] autorelease];
    
    NSButton* button = [self createToolbarPopupButton:toolbarItem];
    [toolbarItem setLabel:NSLocalizedString(@"Forward", @"Forward")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Forward", @"Go Forward")];
    [toolbarItem setToolTip:NSLocalizedString(@"ForwardToolTip", @"Go forward one page")];

    NSSize size = NSMakeSize(32., 32.);
    NSImage* icon = [NSImage imageNamed:@"forward"];
    [icon setScalesWhenResized:YES];
    [button setImage:icon];

    [toolbarItem setView:button];
    [toolbarItem setMinSize:size];
    [toolbarItem setMaxSize:size];
    
    [button setTarget:self];
    [button setAction:@selector(forward:)];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(forward:)];      // so validateToolbarItem: works correctly
    [[button cell] setClickHoldAction:@selector(forwardMenu:)];

    NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];
    [menuFormRep setTarget:self];
    [menuFormRep setAction:@selector(forward:)];
    [menuFormRep setTitle:[toolbarItem label]];

    [toolbarItem setMenuFormRepresentation:menuFormRep];
  }
  else if ([itemIdent isEqual:ForwardToolbarItemIdentifier]) {
    // not going onto the toolbar, don't need to go through the gynmastics above
    // and create a separate view
    [toolbarItem setLabel:NSLocalizedString(@"Forward", @"Forward")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Forward", @"Go Forward")];
    [toolbarItem setImage:[NSImage imageNamed:@"forward"]];
  }
  else if ([itemIdent isEqual:ReloadToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"Reload", @"Reload")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Reload Page", @"Reload Page")];
    [toolbarItem setToolTip:NSLocalizedString(@"ReloadToolTip", @"Reload current page")];
    [toolbarItem setImage:[NSImage imageNamed:@"reload"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(reload:)];
  }
  else if ([itemIdent isEqual:StopToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"Stop", @"Stop")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Stop Loading", @"Stop Loading")];
    [toolbarItem setToolTip:NSLocalizedString(@"StopToolTip", @"Stop loading this page")];
    [toolbarItem setImage:[NSImage imageNamed:@"stop"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(stop:)];
  }
  else if ([itemIdent isEqual:HomeToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"Home", @"Home")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Go Home", @"Go Home")];
    [toolbarItem setToolTip:NSLocalizedString(@"HomeToolTip", @"Go to home page")];
    [toolbarItem setImage:[NSImage imageNamed:@"home"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(home:)];
  }
  else if ([itemIdent isEqual:BookmarksToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"ToggleBookmarks", @"Manage Bookmarks label")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Manage Bookmarks", @"Manage Bookmarks palette")];
    [toolbarItem setToolTip:NSLocalizedString(@"BookmarkMgrToolTip", @"Show or hide all bookmarks")];
    [toolbarItem setImage:[NSImage imageNamed:@"manager"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(manageBookmarks:)];
  }
  else if ( [itemIdent isEqual:SearchToolbarItemIdentifier] ) {
    NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];

    [toolbarItem setLabel:NSLocalizedString(@"Search", @"Search")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Search", @"Search")];
    [toolbarItem setToolTip:NSLocalizedString(@"SearchToolTip", @"Search the Internet")];
    [toolbarItem setView:mSearchBar];
    [toolbarItem setMinSize:NSMakeSize(128, NSHeight([mSearchBar frame]))];
    [toolbarItem setMaxSize:NSMakeSize(150, NSHeight([mSearchBar frame]))];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(performSearch:)];

    [menuFormRep setTarget:self];
    [menuFormRep setAction:@selector(beginSearchSheet)];
    [menuFormRep setTitle:[toolbarItem label]];

    [toolbarItem setMenuFormRepresentation:menuFormRep];
  }
  else if ([itemIdent isEqual:ThrobberToolbarItemIdentifier]) {
    [toolbarItem setLabel:@""];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Progress", @"Progress")];
    [toolbarItem setToolTip:NSLocalizedStringFromTable(@"ThrobberPageDefault", @"WebsiteDefaults", nil)];
    [toolbarItem setImage:[NSImage imageNamed:@"throbber-01"]];
    [toolbarItem setTarget:self];
    [toolbarItem setTag:'Thrb'];
    [toolbarItem setAction:@selector(clickThrobber:)];
  }
  else if ([itemIdent isEqual:CombinedLocationToolbarItemIdentifier]) {
    NSMenuItem *menuFormRep = [[[NSMenuItem alloc] init] autorelease];

    [toolbarItem setLabel:NSLocalizedString(@"Location", @"Location")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Location", @"Location")];
    [toolbarItem setView:mLocationToolbarView];
    [toolbarItem setMinSize:NSMakeSize(250, NSHeight([mLocationToolbarView frame]))];
    [toolbarItem setMaxSize:NSMakeSize(FLT_MAX, NSHeight([mLocationToolbarView frame]))];

    [mSearchBar setTarget:self];
    [mSearchBar setAction:@selector(performSearch:)];

    [menuFormRep setTarget:self];
    [menuFormRep setAction:@selector(performAppropriateLocationAction)];
    [menuFormRep setTitle:[toolbarItem label]];

    [toolbarItem setMenuFormRepresentation:menuFormRep];
  }
  else if ([itemIdent isEqual:PrintToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"Print", @"Print")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Print", @"Print")];
    [toolbarItem setToolTip:NSLocalizedString(@"PrintToolTip", @"Print this page")];
    [toolbarItem setImage:[NSImage imageNamed:@"print"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(printDocument:)];
  }
  else if ([itemIdent isEqual:ViewSourceToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"View Source", @"View Source")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"View Page Source", @"View Page Source")];
    [toolbarItem setToolTip:NSLocalizedString(@"ViewSourceToolTip", @"Display the HTML source of this page")];
    [toolbarItem setImage:[NSImage imageNamed:@"showsource"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(viewSource:)];
  }
  else if ([itemIdent isEqual:BookmarkToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"Bookmark", @"Bookmark")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Bookmark Page", @"Bookmark Page")];
    [toolbarItem setToolTip:NSLocalizedString(@"BookmarkToolTip", @"Add this page to your bookmarks")];
    [toolbarItem setImage:[NSImage imageNamed:@"add_to_bookmark.tif"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(addBookmark:)];
  }
  else if ([itemIdent isEqual:TextBiggerToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"BigText", @"Enlarge Text")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"BigText", @"Enlarge Text")];
    [toolbarItem setToolTip:NSLocalizedString(@"BigTextToolTip", @"Enlarge the text on this page")];
    [toolbarItem setImage:[NSImage imageNamed:@"textBigger.tif"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(biggerTextSize:)];
  }
  else if ([itemIdent isEqual:TextSmallerToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"SmallText", @"Shrink Text")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"SmallText", @"Shrink Text")];
    [toolbarItem setToolTip:NSLocalizedString(@"SmallTextToolTip", @"Shrink the text on this page")];
    [toolbarItem setImage:[NSImage imageNamed:@"textSmaller.tif"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(smallerTextSize:)];
  }
  else if ([itemIdent isEqual:NewTabToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"NewTab", @"New Tab")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"NewTab", @"New Tab")];
    [toolbarItem setToolTip:NSLocalizedString(@"NewTabToolTip", @"Create a new tab")];
    [toolbarItem setImage:[NSImage imageNamed:@"newTab.tif"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(newTab:)];
  }
  else if ([itemIdent isEqual:CloseTabToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"CloseTab", @"Close Tab")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"CloseTab", @"Close Tab")];
    [toolbarItem setToolTip:NSLocalizedString(@"CloseTabToolTip", @"Close the current tab")];
    [toolbarItem setImage:[NSImage imageNamed:@"closeTab.tif"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(closeCurrentTab:)];
  }
  else if ([itemIdent isEqual:SendURLToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"SendLink", @"Send Link")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"SendLinkPaletteLabel", @"Email Page Location")];
    [toolbarItem setToolTip:NSLocalizedString(@"SendLinkToolTip", @"Send current URL")];
    [toolbarItem setImage:[NSImage imageNamed:@"sendLink.tif"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(sendURL:)];
  }
  else if ([itemIdent isEqual:DLManagerToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"Downloads", @"Downloads")];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Downloads", @"Downloads")];
    [toolbarItem setToolTip:NSLocalizedString(@"DownloadsToolTip", @"Show the download manager")];
    [toolbarItem setImage:[NSImage imageNamed:@"dl_manager.tif"]];
    [toolbarItem setTarget:[ProgressDlgController sharedDownloadController]];
    [toolbarItem setAction:@selector(showWindow:)];
  }
  else if ([itemIdent isEqual:FormFillToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"Fill Form", nil)];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"Fill Form", nil)];
    [toolbarItem setToolTip:NSLocalizedString(@"FillFormToolTip", nil)];
    [toolbarItem setImage:[NSImage imageNamed:@"autofill"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(fillForm:)];
  }
  else if ([itemIdent isEqual:HistoryToolbarItemIdentifier]) {
    [toolbarItem setLabel:NSLocalizedString(@"ShowHistory", nil)];
    [toolbarItem setPaletteLabel:NSLocalizedString(@"ShowHistory", nil)];
    [toolbarItem setToolTip:NSLocalizedString(@"ShowHistoryToolTip", nil)];
    [toolbarItem setImage:[NSImage imageNamed:@"history"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector(manageHistory:)];
  }
  else {
    toolbarItem = nil;
  }

  return toolbarItem;
}

// This method handles the enabling/disabling of the toolbar buttons.
- (BOOL)validateToolbarItem:(NSToolbarItem *)theItem
{
  // Check the action and see if it matches.
  SEL action = [theItem action];
  // NSLog(@"Validating toolbar item %@ with selector %s", [theItem label], action);
  if (action == @selector(back:)) {
    // if the bookmark manager is showing, we enable the back button so that
    // they can click back to return to the webpage they were viewing.
    BOOL enable = [[mBrowserView getBrowserView] canGoBack];

    // we have to handle all the enabling/disabling ourselves because this
    // toolbar button is a view item. Note the return value is ignored.
    [theItem setEnabled:enable];
    return enable;
  }
  else if (action == @selector(manageBookmarks:)) {
    BOOL enable = [[mBrowserView getBrowserView] canGoBack];
    if (!enable && ![self bookmarkManagerIsVisible])
      enable = true;
    return enable;
  }
  else if (action == @selector(forward:)) {
    // we have to handle all the enabling/disabling ourselves because this
    // toolbar button is a view item. Note the return value is ignored.
    BOOL enable = [[mBrowserView getBrowserView] canGoForward];
    [theItem setEnabled:enable];
    return enable;
  }
  else if (action == @selector(reload:))
    return (![mBrowserView isBusy] && ![self bookmarkManagerIsVisible]);
  else if (action == @selector(stop:))
    return [mBrowserView isBusy];
  else if (action == @selector(addBookmark:))
    return ![mBrowserView isEmpty];
  else if (action == @selector(biggerTextSize:))
    return ![mBrowserView isEmpty] && [[mBrowserView getBrowserView] canMakeTextBigger];
  else if ( action == @selector(smallerTextSize:))
    return ![mBrowserView isEmpty] && [[mBrowserView getBrowserView] canMakeTextSmaller];
  else if (action == @selector(newTab:))
    return YES;
  else if (action == @selector(closeCurrentTab:))
    return ([mTabBrowser numberOfTabViewItems] > 1);
  else if (action == @selector(sendURL:))
  {
    NSString* curURL = [[self getBrowserWrapper] getCurrentURI];
    return ![MainController isBlankURL:curURL];
  }
  else if (action == @selector(viewSource:))
    return ![self bookmarkManagerIsVisible];
  else
    return YES;
}

//
// -splitView:canCollapseSubview:
// NSSplitView delegate
// 
// Allow the user (read: smokey) to collapse the search bar but not the url bar.
//
- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview
{
  if (sender == mLocationToolbarView)
    return (subview == mSearchBar);
  return YES;
}

//
// -splitView:constrainMinCoordiante:ofSubviewAt:
// NSSplitView delegate
//
// Called when the combined url/search splitter is being resized to provide a mininum
// value for the splitter, which in our case is we want to be the min width of the url bar.
//
- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedMin ofSubviewAt:(int)offset
{
  if (sender == mLocationToolbarView)
    return kMininumURLAndSearchBarWidth;
  return proposedMin;
}

//
// -splitView:constrainMaxCoordinate:ofSubviewAt:
//
// Called when the combined url/search splitter is being resized to provide a max
// value for the splitter. |proposedMax| is the rightmost extent of the
// view to the right of the splitter, which in our case is the search bar. We
// want the splitter to stop at that extent less the minimum search bar width.
//
- (float)splitView:(NSSplitView *)sender constrainMaxCoordinate:(float)proposedMax ofSubviewAt:(int)offset
{
  if (sender == mLocationToolbarView)
    return proposedMax - kMininumURLAndSearchBarWidth;
  return proposedMax;
}

//
// -splitView:resizeSubviewsWithOldSize:
// NSSplitView delegate
//
// Called when the split view is being resized. We are now in full control over
// how our subviews are repositioned. We want to fix the width of the search bar so
// that no matter how narrow/wide the window gets, the url bar is the one that changes
// size.
//
- (void)splitView:(NSSplitView *)sender resizeSubviewsWithOldSize:(NSSize)oldSize 
{
  NSSize newSize = [sender frame].size;
  NSRect searchFrame = [mSearchBar frame];
  NSView* urlSuperview = [mURLBar superview];
  NSRect urlBarFrame = [urlSuperview frame];
  
  // keep the search field constant size, expanding the url bar to take up the new slack
  float deltaX = newSize.width - oldSize.width;     // positive when window grows
  urlBarFrame.size.width += deltaX;
  searchFrame.origin.x += deltaX;
  [urlSuperview setFrame:urlBarFrame];
  [mSearchBar setFrame:searchFrame];
  [sender setNeedsDisplay:YES];
}

#pragma mark -


-(BOOL)validateMenuItem: (NSMenuItem*)aMenuItem
{
  SEL action = [aMenuItem action];
  
  if (action == @selector(moveTabToNewWindow:) ||
      action == @selector(closeCurrentTab:)    ||
      action == @selector(closeSendersTab:)    ||
      action == @selector(closeOtherTabs:))
    return ([mTabBrowser numberOfTabViewItems] > 1);
  
  if (action == @selector(fillForm:))
    return ![self bookmarkManagerIsVisible];

  return YES;
}

#pragma mark -

// BrowserUIDelegate methods (called from the frontmost tab's BrowserWrapper)


- (void)loadingStarted
{
}

- (void)loadingDone:(BOOL)activateContent
{
  if (activateContent)
  {
    // if we're the front/key window, focus the content area. If we're not,
    // set gecko as the first responder so that it will be activated when
    // the window is focused. If the user is typing in the urlBar, however,
    // don't mess with the focus at all.
    if ([[self window] isKeyWindow])
    {
      if (![self userChangedLocationField])
        [mBrowserView setBrowserActive:YES];
    }
    else
      [[self window] makeFirstResponder:[mBrowserView getBrowserView]];
  }
  
  if ([[self window] isMainWindow])
    [[PageInfoWindowController visiblePageInfoWindowController] updateFromBrowserView:[self activeBrowserView]];
}

- (void)setLoadingActive:(BOOL)active
{
  if (active)
  {
    [self startThrobber];
    [mProgress setIndeterminate:YES];
    [self showProgressIndicator];
    [mProgress startAnimation:self];
  }
  else
  {
    [self stopThrobber];
    [mProgress stopAnimation:self];
    [self hideProgressIndicator];
    [mProgress setIndeterminate:YES];
  }
}

- (void)setLoadingProgress:(float)progress
{
  if (progress > 0.0f)
  {
    [mProgress setIndeterminate:NO];
    [mProgress setDoubleValue:progress];
  }
  else
  {
    [mProgress setIndeterminate:YES];
    [mProgress startAnimation:self];
  }
}

- (void)updateWindowTitle:(NSString*)title
{
  [[self window] setTitle:title];
}

- (void)updateStatus:(NSString*)status
{
  if (![[mStatus stringValue] isEqualToString:status])
    [mStatus setStringValue:status];
}

- (void)updateLocationFields:(NSString*)url ignoreTyping:(BOOL)ignoreTyping
{
  if (!ignoreTyping && [self userChangedLocationField])
    return;

  if ([url isEqual:@"about:blank"])
    url = @""; // return;

  [mURLBar setURI:url];
  [mLocationSheetURLField setStringValue:url];

  if ([[self window] isMainWindow])
    [[PageInfoWindowController visiblePageInfoWindowController] updateFromBrowserView:[self activeBrowserView]];
}

- (void)updateSiteIcons:(NSImage*)icon ignoreTyping:(BOOL)ignoreTyping
{
  if (!ignoreTyping && [self userChangedLocationField])
    return;

  if (icon == nil)
    icon = [NSImage imageNamed:@"globe_ico"];
  [mProxyIcon setImage:icon];
}

- (void)showPopupBlocked:(BOOL)inBlocked
{
  // do nothing, everything is now handled by the BrowserWindow.
}

//
// -configurePopupBlocking
//
// Called to display our popup blocking configuration ui, which is in prefs. 
// Show the prefs window focused on the "web features" panel.
//
- (void)configurePopupBlocking
{
  [[MVPreferencesController sharedInstance] showPreferences:nil];
  [[MVPreferencesController sharedInstance] selectPreferencePaneByIdentifier:@"org.mozilla.camino.preference.webfeatures"];
}

//
// -unblockAllPopupSites:
//
// Called in response to the menu item from the unblock popup. Loop over all
// the items in the blocked sites array in the browser wrapper and add them
// to the whitelist.
//
- (void)unblockAllPopupSites:(nsISupportsArray*)inSites
{
  nsCOMPtr<nsIPermissionManager> pm (do_GetService(NS_PERMISSIONMANAGER_CONTRACTID));
  if (!pm)
    return;

  PRUint32 count = 0;
  inSites->Count(&count);
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsISupports> genUri = dont_AddRef(inSites->ElementAt(i));
    nsCOMPtr<nsIURI> uri = do_QueryInterface(genUri);
    pm->Add(uri, "popup", nsIPermissionManager::ALLOW_ACTION);   
  }
}

- (void)showSecurityState:(unsigned long)state
{
  [self updateLock:state];
}

- (BOOL)userChangedLocationField
{
  return [mURLBar userHasTyped];
}

- (void)contentViewChangedTo:(NSView*)inView forURL:(NSString*)inURL
{
  // update bookmarks menu
  [[NSApp delegate] delayedAdjustBookmarksMenuItemsEnabling];

  // should we change page info for bookmarks?
}

- (void)updateFromFrontmostTab
{
  [[self window] setTitle:  [mBrowserView windowTitle]];
  [self setLoadingActive:   [mBrowserView isBusy]];
  [self setLoadingProgress: [mBrowserView loadingProgress]];
  [self showSecurityState:  [mBrowserView securityState]];
  [self updateSiteIcons:    [mBrowserView siteIcon] ignoreTyping:NO];
  [self updateStatus:       [mBrowserView statusString]];
  [self updateLocationFields:[mBrowserView location] ignoreTyping:NO];
}

#pragma mark -

- (BrowserTabViewItem*)tabForBrowser:(BrowserWrapper*)inWrapper
{
  NSEnumerator* tabsEnum = [[mTabBrowser tabViewItems] objectEnumerator];
  id curTabItem;
  while ((curTabItem = [tabsEnum nextObject]))
  {
    if ([curTabItem isKindOfClass:[BrowserTabViewItem class]] && ([(BrowserTabViewItem*)curTabItem view] == inWrapper))
      return curTabItem;
  }
  return nil;
}

- (BookmarkViewController*)bookmarkViewControllerForCurrentTab
{
  id viewProvider = [mBrowserView contentViewProviderForURL:@"about:bookmarks"];
  if ([viewProvider isKindOfClass:[BookmarkViewController class]])
    return (BookmarkViewController*)viewProvider;
  return nil;
}

// this gets the previous entry in session history if bookmarks are showing
- (void)bookmarkableTitle:(NSString **)outTitle URL:(NSString**)outURLString forWrapper:(BrowserWrapper*)inWrapper
{
  *outTitle = nil;
  *outURLString = nil;

  NSString* curTitle = nil;
  NSString* curURL = nil;
  [inWrapper getTitle:&curTitle andHref:&curURL];

  // if we're currently showing history or bookmarks, hand back the last URL.
  if ([[curURL lowercaseString] isEqualToString:@"about:bookmarks"] ||
      [[curURL lowercaseString] isEqualToString:@"about:history"])
  {
    nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([[inWrapper getBrowserView] getWebBrowser]);
    if (webBrowser)
    {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(webBrowser));

      nsCOMPtr<nsISHistory> sessionHistory;
      webNav->GetSessionHistory(getter_AddRefs(sessionHistory));
      if (sessionHistory)
      {
        PRInt32 curEntryIndex;
        sessionHistory->GetIndex(&curEntryIndex);
        if (curEntryIndex > 0)
        {
          nsCOMPtr<nsIHistoryEntry> entry;
          sessionHistory->GetEntryAtIndex(curEntryIndex - 1, PR_FALSE, getter_AddRefs(entry));
          
          nsCAutoString uriSpec;
          nsCOMPtr<nsIURI> entryURI;
          entry->GetURI(getter_AddRefs(entryURI));
          if (entryURI)
            entryURI->GetSpec(uriSpec);
          
          nsXPIDLString textStr;
          entry->GetTitle(getter_Copies(textStr));
          
          curTitle = [NSString stringWith_nsAString:textStr];
          curURL = [NSString stringWithUTF8String:uriSpec.get()];
        }
      }
    }
  }

  *outTitle = curTitle;
  *outURLString = curURL;  
}

- (void)performAppropriateLocationAction
{
  NSToolbar *toolbar = [[self window] toolbar];
  if ( [toolbar isVisible] )
  {
    if ( ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconAndLabel) ||
          ([[[self window] toolbar] displayMode] == NSToolbarDisplayModeIconOnly) )
    {
      NSArray *itemsWeCanSee = [toolbar visibleItems];
      
      for (unsigned int i = 0; i < [itemsWeCanSee count]; i++)
      {
        if ([[[itemsWeCanSee objectAtIndex:i] itemIdentifier] isEqual:CombinedLocationToolbarItemIdentifier])
        {
          [self focusURLBar];
          return;
        }
      }
    }
  }
  
  [self beginLocationSheet];
}

- (void)focusURLBar
{
  [mBrowserView setBrowserActive:NO];
	[mURLBar selectText:self];
}

- (void)beginLocationSheet
{
  [mLocationSheetURLField setStringValue:[mURLBar stringValue]];
  [mLocationSheetURLField selectText:nil];

  [NSApp beginSheet:  mLocationSheetWindow
     modalForWindow:  [self window]
      modalDelegate:  nil
     didEndSelector:  nil
        contextInfo:  nil];
}

- (IBAction)endLocationSheet:(id)sender
{
  [mLocationSheetWindow close];   // assumes it's not released on close
  [NSApp endSheet:mLocationSheetWindow returnCode:1];
  [self goToLocationFromToolbarURLField:mLocationSheetURLField];
}

- (IBAction)cancelLocationSheet:(id)sender
{
  [mLocationSheetWindow close];   // assumes it's not released on close
  [NSApp endSheet:mLocationSheetWindow returnCode:0];
}

//
// -performAppropriateSearchAction
//
// Called when the user executes the "search the web" action. If the combined
// url/search bar is visible, focus the text field. If it's not (text only or
// removed from toolbar), show the search sheet.
//
// Note that with the combined url/search bar, the only way to get this sheet
// is to use the menu item/key combo, as clicking the text-only toolbar item
// will show the location sheet. I'm not really happy about this, but I couldn't
// come up with a good unified sheet that made sense. 
//
- (void)performAppropriateSearchAction
{
  if ([mSearchBar window] && ![mLocationToolbarView isSubviewCollapsed:mSearchBar])
    [self focusSearchBar];
  else 
    [self beginSearchSheet];
}

- (void)focusSearchBar
{
  [mBrowserView setBrowserActive:NO];
  [mSearchBar selectText:self];
}

- (void)beginSearchSheet
{
  [NSApp beginSheet:  mSearchSheetWindow
     modalForWindow:  [self window]
      modalDelegate:  nil
     didEndSelector:  nil
        contextInfo:  nil];
}

- (IBAction)endSearchSheet:(id)sender
{
  [mSearchSheetWindow orderOut:self];
  [NSApp endSheet:mSearchSheetWindow returnCode:1];
  [self performSearch:mSearchSheetTextField];
}

- (IBAction)cancelSearchSheet:(id)sender
{
  [mSearchSheetWindow orderOut:self];
  [NSApp endSheet:mSearchSheetWindow returnCode:0];
}

//
// - manageBookmarks:
//
// Load the bookmarks in the frontmost tab or window.
//
-(IBAction)manageBookmarks:(id)aSender
{
  if ([self bookmarkManagerIsVisible])
    [self back:aSender];
  else
    [self loadURL:@"about:bookmarks" referrer:nil activate:YES allowPopups:NO];

  [[NSApp delegate] delayedAdjustBookmarksMenuItemsEnabling];
}

//
// -manageHistory:
//
// History is a slightly different beast from bookmarks. Unlike 
// bookmarks, which acts as a toggle, history ensures the manager
// is visible and selects the history collection. If the manager
// is already visible, selects the history collection.
//
// An alternate solution would be to have it select history when
// it wasn't the selected container, and hide when history was
// the selected collection (toggling in that one case). This makes
// me feel dirty as the command does two different things depending
// on the (possibly undiscoverable to the user) context in which it is
// invoked. For that reason, I've chosen to not have history be a 
// toggle and see the fallout.
//
-(IBAction)manageHistory: (id)aSender
{
  [self loadURL:@"about:history" referrer:nil activate:YES allowPopups:YES];

  // aSender could be a history menu item with a represented object of
  // an item that we wish to reveal. However, it belongs to a different
  // data source than the one we just created. need a way to find the one
  // to reveal...
}

- (IBAction)goToLocationFromToolbarURLField:(id)sender
{
  if ([sender isKindOfClass:[AutoCompleteTextField class]])
    [self goToLocationFromToolbarURLField:(AutoCompleteTextField *)sender
                                    inView:kDestinationCurrentView inBackground:NO];
}

- (void)goToLocationFromToolbarURLField:(AutoCompleteTextField *)inURLField 
                                 inView:(BWCOpenDest)inDest inBackground:(BOOL)inLoadInBG
{
  // trim off any whitespace around url
  NSString *theURL = [[inURLField stringValue] stringByTrimmingWhitespace];
  
  if ([theURL length] == 0)
  {
    // re-focus the url bar if it's visible (might be in sheet?)
    if ([inURLField window] == [self window])
      [[self window] makeFirstResponder:inURLField];
    
    return;
  }

  // look for bookmarks keywords match
  NSArray *resolvedURLs = [[BookmarkManager sharedBookmarkManager] resolveBookmarksKeyword:theURL];
  
  NSString* resolvedURL = nil;
  if ([resolvedURLs count] == 1) {
    resolvedURL = [resolvedURLs lastObject];
    if (inDest == kDestinationNewTab)
      [self openNewTabWithURL:resolvedURL referrer:nil loadInBackground:inLoadInBG allowPopups:NO];
    else if (inDest == kDestinationNewWindow)
      [self openNewWindowWithURL:resolvedURL referrer:nil loadInBackground:inLoadInBG allowPopups:NO];
    else // if it's not a new window or a new tab, load into the current view
      [self loadURL:resolvedURL referrer:nil activate:YES allowPopups:NO];
  } else {
    if (inDest == kDestinationNewTab || inDest == kDestinationNewWindow)
      [self openURLArray:resolvedURLs tabOpenPolicy:eAppendTabs allowPopups:NO];
    else
      [self openURLArray:resolvedURLs tabOpenPolicy:eReplaceTabs allowPopups:NO];
  }
  
  // global history needs to know the user typed this url so it can present it
  // in autocomplete. We use the URI fixup service to strip whitespace and remove
  // invalid protocols, etc. Don't save keyword-expanded urls.
  if (resolvedURL && [theURL isEqualToString:resolvedURL] &&
      mDataOwner &&
      mDataOwner->mGlobalHistory &&
      mDataOwner->mURIFixer && [theURL length] > 0)
  {
    nsAutoString url;
    [theURL assignTo_nsAString:url];
    NS_ConvertUTF16toUTF8 utf8URL(url);
    
    nsCOMPtr<nsIURI> fixedURI;
    mDataOwner->mURIFixer->CreateFixupURI(utf8URL, 0, getter_AddRefs(fixedURI));
    if (fixedURI)
      mDataOwner->mGlobalHistory->MarkPageAsTyped(fixedURI);
  }
}
- (void)saveDocument:(BOOL)focusedFrame filterView:(NSView*)aFilterView
{
  [[mBrowserView getBrowserView] saveDocument:focusedFrame filterView:aFilterView];
}

- (void)saveURL:(NSView*)aFilterView url:(NSString*)aURLSpec suggestedFilename:(NSString*)aFilename
{
  [[mBrowserView getBrowserView] saveURL:aFilterView url:aURLSpec suggestedFilename:aFilename];
}

- (void)loadSourceOfURL:(NSString*)urlStr inBackground:(BOOL)loadInBackground
{
  BOOL shouldUseTab = [[PreferenceManager sharedInstance] getBooleanPref:"camino.viewsource_in_tab" withSuccess:NULL];
  NSString* viewSource = [kViewSourceProtocolString stringByAppendingString:urlStr];
  
  // first attempt to get the source that's already loaded
  BOOL canUseCache = NO;
  nsCOMPtr<nsISupports> desc = [[mBrowserView getBrowserView] getPageDescriptor];
  if (desc) {
    // make sure we're not trying to load a subframe by checking |urlStr| against the url in
    // the desc (which is a history entry). We can only use the desc if it's the toplevel page.
    nsCOMPtr<nsIHistoryEntry> entry(do_QueryInterface(desc));
    if (entry) {
      nsCOMPtr<nsIURI> uri;
      entry->GetURI(getter_AddRefs(uri));
      nsCAutoString spec;
      uri->GetSpec(spec);
      if ([urlStr isEqualToString:[NSString stringWithUTF8String:spec.get()]])
        canUseCache = YES;
    }
  }

  if (shouldUseTab) {
    if (canUseCache)
      [self openNewTabWithDescriptor:desc displayType:nsIWebPageDescriptor::DISPLAY_AS_SOURCE loadInBackground:loadInBackground];
    else
      [self openNewTabWithURL:viewSource referrer:nil loadInBackground:loadInBackground allowPopups:NO];
  }      
  else {
    // open a new window and hide the toolbars for prettyness
    BrowserWindowController* controller = [[BrowserWindowController alloc] initWithWindowNibName:@"BrowserWindow"];
    [controller setChromeMask:kNoToolbarsChromeMask];
    if (loadInBackground)
      [[controller window] orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
    else
      [controller showWindow:nil];

    if (canUseCache)
      [[[controller getBrowserWrapper] getBrowserView] setPageDescriptor:desc displayType:nsIWebPageDescriptor::DISPLAY_AS_SOURCE];
    else
      [controller loadURL:viewSource referrer:nil activate:!loadInBackground allowPopups:NO];
  }
}

- (IBAction)viewSource:(id)aSender
{
  BOOL loadInBackground = (([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask) != 0);
  NSString* urlStr = [[mBrowserView getBrowserView] getFocusedURLString];
  [self loadSourceOfURL:urlStr inBackground:loadInBackground];
}

- (IBAction)viewPageSource:(id)aSender
{
  BOOL loadInBackground = (([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask) != 0);
  NSString* urlStr = [[mBrowserView getBrowserView] getCurrentURI];
  [self loadSourceOfURL:urlStr inBackground:loadInBackground];
}

- (IBAction)printDocument:(id)aSender
{
  [[mBrowserView getBrowserView] printDocument];
}

- (IBAction)pageSetup:(id)aSender
{
  [[mBrowserView getBrowserView] pageSetup];
}

- (IBAction)performSearch:(id)aSender
{
  // If we have a valid SearchTextField, perform a search using its contents
  if ([aSender isKindOfClass:[SearchTextField class]]) 
    [self performSearch:(SearchTextField *)aSender inView:kDestinationCurrentView inBackground:NO];
}

//
// -searchForSelection:
//
// Get the selection, stick it into the search bar, and do a search with the
// currently selected search engine in the search bar. If there is no search
// bar in the toolbar, that's still ok because we've guaranteed that we always
// have a search bar even if it's not on a toolbar.
//
- (IBAction)searchForSelection:(id)aSender
{
  NSString* selection = [[mBrowserView getBrowserView] getSelection];
  [mSearchBar becomeFirstResponder];
  [mSearchBar setStringValue:selection];
  [self performSearch:mSearchBar];
}

//
// - performSearch:inView:inBackground
//
// performs a search using searchField and opens either in the current view, a new tab, or a new
// window. If it's a new tab or window, loadInBG determines whether the window/tab is opened in the background
//
-(void)performSearch:(SearchTextField *)inSearchField inView:(BWCOpenDest)inDest inBackground:(BOOL)inLoadInBG
{
  // Get the search URL from our dictionary of sites and search urls
  NSMutableString *searchURL = [NSMutableString stringWithString:
    [[BrowserWindowController searchURLDictionary] objectForKey:
      [inSearchField titleOfSelectedPopUpItem]]];
  NSString *currentURL = [[self getBrowserWrapper] getCurrentURI];
  NSString *searchString = [inSearchField stringValue];
  
  const char *aURLSpec = [currentURL lossyCString];
  NSString *aDomain = @"";
  nsIURI *aURI = nil;
  
  // If we have an about: type URL, remove " site:%d" from the search string
  // This is a fix to deal with Google's Search this Site feature
  // If other sites use %d to search the site, we'll have to have specific rules
  // for those sites.
  
  if ([currentURL hasPrefix:@"about:"]) {
    NSRange domainStringRange = [searchURL rangeOfString:@" site:%d"
                                                 options:NSBackwardsSearch];
    
    NSRange notFoundRange = NSMakeRange(NSNotFound, 0);
    if (NSEqualRanges(domainStringRange, notFoundRange) == NO)
      [searchURL deleteCharactersInRange:domainStringRange];
  }
  
  // If they didn't type anything in the search field, visit the domain of
  // the search site, i.e. www.google.com for the Google site
  if ([[inSearchField stringValue] isEqualToString:@""]) {
    aURLSpec = [searchURL lossyCString];
    
    if (NS_NewURI(&aURI, aURLSpec, nsnull, nsnull) == NS_OK) {
      nsCAutoString spec;
      aURI->GetHost(spec);
      
      aDomain = [NSString stringWithUTF8String:spec.get()];
      
      if (inDest == kDestinationNewTab)
        [self openNewTabWithURL:aDomain referrer:nil loadInBackground:inLoadInBG allowPopups:NO];
      else if (inDest == kDestinationNewWindow)
        [self openNewWindowWithURL:aDomain referrer:nil loadInBackground:inLoadInBG allowPopups:NO];
      else // if it's not a new window or a new tab, load into the current view
        [self loadURL:aDomain referrer:nil activate:NO allowPopups:NO];

    } 
  } else {
    aURLSpec = [[[self getBrowserWrapper] getCurrentURI] UTF8String];
    
    // Get the domain so that we can replace %d in our searchURL
    if (NS_NewURI(&aURI, aURLSpec, nsnull, nsnull) == NS_OK) {
      nsCAutoString spec;
      aURI->GetHost(spec);
      
      aDomain = [NSString stringWithUTF8String:spec.get()];
    }
    
    // Escape the search string so the user can search for strings with
    // special characters ("&", "+", etc.) List from RFC2396.
    NSString *escapedSearchString = (NSString *) CFURLCreateStringByAddingPercentEscapes(NULL, (CFStringRef)searchString, NULL, CFSTR(";/?:@&=+$,"), kCFStringEncodingUTF8);
    
    // replace the conversion specifiers (%d, %s) in the search string
    [self transformFormatString:searchURL domain:aDomain search:escapedSearchString];
    [escapedSearchString release];
    
    if (inDest == kDestinationNewTab)
      [self openNewTabWithURL:searchURL referrer:nil loadInBackground:inLoadInBG allowPopups:NO];
    else if (inDest == kDestinationNewWindow)
      [self openNewWindowWithURL:searchURL referrer:nil loadInBackground:inLoadInBG allowPopups:NO];
    else // if it's not a new window or a new tab, load into the current view
      [self loadURL:searchURL referrer:nil activate:NO allowPopups:NO];
  }
}

// - transformFormatString:domain:search
//
// Replaces all occurrences of %d in |inFormat| with |inDomain| and all occurrences of
// %s with |inSearch|.
- (void) transformFormatString:(NSMutableString*)inFormat domain:(NSString*)inDomain search:(NSString*)inSearch
{
  // Replace any occurence of %d with the current domain
  [inFormat replaceOccurrencesOfString:@"%d" withString:inDomain options:NSBackwardsSearch
                                 range:NSMakeRange(0, [inFormat length])];
  
  // Replace any occurence of %s with the contents of the search text field
  [inFormat replaceOccurrencesOfString:@"%s" withString:inSearch options:NSBackwardsSearch
                                 range:NSMakeRange(0, [inFormat length])];
}

- (IBAction)sendURL:(id)aSender
{
  NSString* titleString = nil;
  NSString* urlString = nil;

  [[self getBrowserWrapper] getTitle:&titleString andHref:&urlString];
  
  if (!urlString)
    return;

  if (!titleString)
    titleString = @"";

  // put < > around the URL to minimise problems when e-mailing
  urlString = [NSString stringWithFormat:@"<%@>", urlString];

  // we need to encode entities in the title and url strings first. For some reason
  // CFURLCreateStringByAddingPercentEscapes is only happy with UTF-8 strings.
  CFStringRef urlUTF8String   = CFStringCreateWithCString(kCFAllocatorDefault, [urlString   UTF8String], kCFStringEncodingUTF8);
  CFStringRef titleUTF8String = CFStringCreateWithCString(kCFAllocatorDefault, [titleString UTF8String], kCFStringEncodingUTF8);
  
  CFStringRef escapedURL   = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, urlUTF8String,   NULL, CFSTR("&?="), kCFStringEncodingUTF8);
  CFStringRef escapedTitle = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, titleUTF8String, NULL, CFSTR("&?="), kCFStringEncodingUTF8);
    
  NSString* mailtoURLString = [NSString stringWithFormat:@"mailto:?subject=%@&body=%@", (NSString*)escapedTitle, (NSString*)escapedURL];

  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:mailtoURLString]];
  
  CFRelease(urlUTF8String);
  CFRelease(titleUTF8String);
  
  CFRelease(escapedURL);
  CFRelease(escapedTitle);
}

- (IBAction)sendURLFromLink:(id)aSender
{
  if (!mDataOwner->mContextMenuNode)
    return;

  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  GeckoUtils::GetEnclosingLinkElementAndHref(mDataOwner->mContextMenuNode, getter_AddRefs(linkContent), href);
  
  // XXXdwh Handle simple XLINKs if we want to be compatible with Mozilla, but who
  // really uses these anyway? :)
  if (!linkContent || href.IsEmpty())
    return;
  
  // put < > around the URL to minimise problems when e-mailing
  NSString* urlString = [NSString stringWithFormat:@"<%@>", [NSString stringWith_nsAString:href]];
  
  // we need to encode entities in the title and url strings first. For some reason
  // CFURLCreateStringByAddingPercentEscapes is only happy with UTF-8 strings.
  CFStringRef urlUTF8String = CFStringCreateWithCString(kCFAllocatorDefault, [urlString UTF8String], kCFStringEncodingUTF8);
  CFStringRef escapedURL    = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, urlUTF8String, NULL, CFSTR("&?="), kCFStringEncodingUTF8);
  
  NSString* mailtoURLString = [NSString stringWithFormat:@"mailto:?body=%@", (NSString*)escapedURL];
  
  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:mailtoURLString]];
  
  CFRelease(urlUTF8String);
  CFRelease(escapedURL);
}

- (NSToolbarItem*)throbberItem
{
    // find our throbber toolbar item.
    NSToolbar* toolbar = [[self window] toolbar];
    NSArray* items = [toolbar visibleItems];
    unsigned count = [items count];
    for (unsigned i = 0; i < count; ++i) {
        NSToolbarItem* item = [items objectAtIndex: i];
        if ([item tag] == 'Thrb') {
            return item;
        }
    }
    return nil;
}

- (NSArray*)throbberImages
{
  // Simply load an array of NSImage objects from the files "throbber-NN.tif". I used "Quicktime Player" to
  // save all of the frames of the animated gif as individual .tif files for simplicity of implementation.
  if (mThrobberImages == nil)
  {
    NSImage* images[64];
    int i;
    for (i = 0;; ++i) {
      NSString* imageName = [NSString stringWithFormat: @"throbber-%02d", i + 1];
      images[i] = [NSImage imageNamed: imageName];
      if (images[i] == nil)
        break;
    }
    mThrobberImages = [[NSArray alloc] initWithObjects: images count: i];
  }
  return mThrobberImages;
}


- (void)clickThrobber:(id)aSender
{
  NSString *pageToLoad = NSLocalizedStringFromTable(@"ThrobberPageDefault", @"WebsiteDefaults", nil);
  if (![pageToLoad isEqualToString:@"ThrobberPageDefault"])
    [self loadURL:pageToLoad referrer:nil activate:YES allowPopups:NO];
}

- (void)startThrobber
{
  // optimization:  only throb if the throbber toolbar item is visible.
  NSToolbarItem* throbberItem = [self throbberItem];
  if (throbberItem) {
    [self stopThrobber];
    mThrobberHandler = [[ThrobberHandler alloc] initWithToolbarItem:throbberItem 
                          images:[self throbberImages]];
  }
}

- (void)stopThrobber
{
  [mThrobberHandler stopThrobber];
  [mThrobberHandler release];
  mThrobberHandler = nil;
  NSToolbarItem* throbberItem = [self throbberItem];
  if (throbberItem)
    [throbberItem setImage: [[self throbberImages] objectAtIndex: 0]];
}


- (BOOL)findInPageWithPattern:(NSString*)text caseSensitive:(BOOL)inCaseSensitive
    wrap:(BOOL)inWrap backwards:(BOOL)inBackwards
{
  return [[mBrowserView getBrowserView] findInPageWithPattern:text caseSensitive:inCaseSensitive
    wrap:inWrap backwards:inBackwards];
}

- (BOOL)findInPage:(BOOL)inBackwards
{
  return [[mBrowserView getBrowserView] findInPage:inBackwards];
}

- (NSString*)lastFindText
{
  return [[mBrowserView getBrowserView] lastFindText];
}

- (BOOL)bookmarkManagerIsVisible
{
  NSString* currentURL = [[[self getBrowserWrapper] getCurrentURI] lowercaseString];
  return [currentURL isEqualToString:@"about:bookmarks"] || [currentURL isEqualToString:@"about:history"];
}

- (BOOL)canHideBookmarks
{
  return [self bookmarkManagerIsVisible] && [[mBrowserView getBrowserView] canGoBack];
}

- (BOOL)singleBookmarkIsSelected
{
  if (![self bookmarkManagerIsVisible])
    return NO;

  BookmarkViewController* bookmarksController = [self bookmarkViewControllerForCurrentTab];
  return ([bookmarksController numberOfSelectedRows] == 1);
}

- (IBAction)addBookmark:(id)aSender
{
  int numTabs = [mTabBrowser numberOfTabViewItems];
  NSMutableArray* itemsArray = [NSMutableArray arrayWithCapacity:numTabs];
  for (int i = 0; i < numTabs; i++)
  {
    BrowserWrapper* browserWrapper = (BrowserWrapper*)[[mTabBrowser tabViewItemAtIndex:i] view];
    NSString* curTitleString = nil;
    NSString* hrefString = nil;
    [self bookmarkableTitle:&curTitleString URL:&hrefString forWrapper:browserWrapper];

    NSMutableDictionary* itemInfo = [NSMutableDictionary dictionaryWithObject:hrefString forKey:kAddBookmarkItemURLKey];

    // title can be nil (e.g. for text files)
    if (curTitleString)
      [itemInfo setObject:curTitleString forKey:kAddBookmarkItemTitleKey];
    
    if (browserWrapper == mBrowserView)
      [itemInfo setObject:[NSNumber numberWithBool:YES] forKey:kAddBookmarkItemPrimaryTabKey];

    [itemsArray addObject:itemInfo];
  }
  
  [[AddBookmarkDialogController sharedAddBookmarkDialogController] showDialogWithLocationsAndTitles:itemsArray isFolder:NO onWindow:[self window]];
}

- (IBAction)addBookmarkForLink:(id)aSender
{
  if (!mDataOwner->mContextMenuNode)
    return;

  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  GeckoUtils::GetEnclosingLinkElementAndHref(mDataOwner->mContextMenuNode, getter_AddRefs(linkContent), href);
  nsAutoString linkText;
  GeckoUtils::GatherTextUnder(linkContent, linkText);
  NSString* urlStr = [NSString stringWith_nsAString:href];
  NSString* titleStr = [NSString stringWith_nsAString:linkText];

  NSDictionary* itemInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                                            titleStr, kAddBookmarkItemTitleKey,
                                              urlStr, kAddBookmarkItemURLKey,
                                                      nil];
  NSArray* items = [NSArray arrayWithObject:itemInfo];
  [[AddBookmarkDialogController sharedAddBookmarkDialogController] showDialogWithLocationsAndTitles:items isFolder:NO onWindow:[self window]];
}

- (IBAction)addBookmarkFolder:(id)aSender
{
  if ([self bookmarkManagerIsVisible])
  {
    // if the bookmarks view controller is visible, delegate to it (so that it can use the
    // selection to set the parent folder)
    BookmarkViewController* bookmarksController = [self bookmarkViewControllerForCurrentTab];
    [bookmarksController addBookmarkFolder:aSender];
  }
  
  [[AddBookmarkDialogController sharedAddBookmarkDialogController] showDialogWithLocationsAndTitles:nil isFolder:YES onWindow:[self window]];
}

- (IBAction)addBookmarkSeparator:(id)aSender
{
  BookmarkViewController* bookmarksController = [self bookmarkViewControllerForCurrentTab];
  [bookmarksController addBookmarkSeparator:aSender];
}

//
// -currentWebNavigation
//
// return a weak reference to the current web navigation object. Callers should
// not hold onto this for longer than the current call unless they addref it.
//
- (nsIWebNavigation*) currentWebNavigation
{
  BrowserWrapper* wrapper = [self getBrowserWrapper];
  if (!wrapper) return nsnull;
  CHBrowserView* view = [wrapper getBrowserView];
  if (!view) return nsnull;
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([view getWebBrowser]);
  if (!webBrowser) return nsnull;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(webBrowser));
  return webNav.get();
}

//
// -sessionHistory
//
// Return a weak reference to the current session history object. Callers
// should not hold onto this for longer than the current call unless they addref.
//
- (nsISHistory*) sessionHistory
{
  nsIWebNavigation* webNav = [self currentWebNavigation];
  if (!webNav) return nil;
  nsCOMPtr<nsISHistory> sessionHistory;
  webNav->GetSessionHistory(getter_AddRefs(sessionHistory));
  return sessionHistory.get();
}

- (void)historyItemAction:(id)inSender
{
  // get web navigation for current browser
  nsIWebNavigation* webNav = [self currentWebNavigation];
  if (!webNav) return;
  
  // browse to the history entry for the menuitem that was selected
  PRInt32 historyIndex = [inSender tag];
  webNav->GotoIndex(historyIndex);
}

//
// -populateSessionHistoryMenu:shist:from:to:
//
// Adds to the given popup menu the items of the session history from index |inFrom| to
// |inTo|. Sets the tag on the item to the index in the session history. When an item
// is selected, calls |-historyItemAction:|. Correctly handles iterating in the 
// correct direction based on relative indices of from/to.
//
- (void)populateSessionHistoryMenu:(NSMenu*)inPopup shist:(nsISHistory*)inHistory from:(unsigned long)inFrom to:(unsigned long)inTo
{
  // max number of characters in a menu title before cropping it
  const unsigned int kMaxTitleLength = 80;

  // go forwards if |inFrom| < |inTo| and backwards if |inFrom| > |inTo|
  int direction = -1;
  if (inFrom <= inTo)
    direction = 1;

  // create a menu item for each item in the session history. Instead of simply going
  // from |inFrom| to |inTo|, we use |count| to take the directionality out of the loop
  // control so we can go fwd or backwards with the same loop control.
  const int numIterations = abs(inFrom - inTo) + 1;    
  for (PRInt32 i = inFrom, count = 0; count < numIterations; i += direction, ++count) {
    nsCOMPtr<nsIHistoryEntry> entry;
    inHistory->GetEntryAtIndex(i, PR_FALSE, getter_AddRefs(entry));
    if (entry) {
      nsXPIDLString textStr;
      entry->GetTitle(getter_Copies(textStr));
      NSString* title = [[NSString stringWith_nsAString: textStr] stringByTruncatingTo:kMaxTitleLength at:kTruncateAtMiddle];    
      NSMenuItem *newItem = [inPopup addItemWithTitle:title action:@selector(historyItemAction:) keyEquivalent:@""];
      [newItem setTarget:self];
      [newItem setTag:i];
    }
  }
}

//
// -forwardMenu:
//
// Create a menu off the fwd button (the sender) that contains the session history
// from the current position forward to the most recent in the session history.
//
- (IBAction)forwardMenu:(id)inSender
{
  NSMenu* popupMenu = [[[NSMenu alloc] init] autorelease];
  [popupMenu addItemWithTitle:@"" action:NULL keyEquivalent:@""];  // dummy first item

  // figure out what indices of the history to build in the menu. Item 0
  // in the shared history is the least recent (beginning of history) page.
  nsISHistory* sessionHistory = [self sessionHistory];
  PRInt32 currentIndex;
  sessionHistory->GetIndex(&currentIndex);
  PRInt32 count;
  sessionHistory->GetCount(&count);

  // builds forwards from the item after the current to the end (|count|)
  [self populateSessionHistoryMenu:popupMenu shist:sessionHistory from:currentIndex+1 to:count-1];

  // use a temporary NSPopUpButtonCell to display the menu.
  NSPopUpButtonCell *popupCell = [[[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES] autorelease];
  [popupCell setMenu: popupMenu];
  [popupCell trackMouse:[NSApp currentEvent] inRect:[inSender bounds] ofView:inSender untilMouseUp:YES];
}

//
// -backMenu:
//
// Create a menu off the back button (the sender) that contains the session history
// from the current position backward to the first item in the session history.
//
- (IBAction)backMenu:(id)inSender
{
  NSMenu* popupMenu = [[[NSMenu alloc] init] autorelease];
  [popupMenu addItemWithTitle:@"" action:NULL keyEquivalent:@""];  // dummy first item

  // figure out what indices of the history to build in the menu. Item 0
  // in the shared history is the least recent (beginning of history) page.
  nsISHistory* sessionHistory = [self sessionHistory];
  PRInt32 currentIndex;
  sessionHistory->GetIndex(&currentIndex);

  // builds backwards from the item before the current item to 0, the first item in the list
  [self populateSessionHistoryMenu:popupMenu shist:sessionHistory from:currentIndex-1 to:0];

  // use a temporary NSPopUpButtonCell to display the menu.
  NSPopUpButtonCell *popupCell = [[[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES] autorelease];
  [popupCell setMenu: popupMenu];
  [popupCell trackMouse:[NSApp currentEvent] inRect:[inSender bounds] ofView:inSender untilMouseUp:YES];
}

- (IBAction)back:(id)aSender
{
  [[mBrowserView getBrowserView] goBack];
}

- (IBAction)forward:(id)aSender
{
  [[mBrowserView getBrowserView] goForward];
}

- (IBAction)reload:(id)aSender
{
  unsigned int reloadFlags = NSLoadFlagsNone;
  
  if ([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask)
    reloadFlags = NSLoadFlagsBypassCacheAndProxy;
  
  [[mBrowserView getBrowserView] reload: reloadFlags];
}

- (IBAction)stop:(id)aSender
{
  [[mBrowserView getBrowserView] stop:NSStopLoadAll];
}

- (IBAction)home:(id)aSender
{
  [mBrowserView loadURI:[[PreferenceManager sharedInstance] homePageUsingStartPage:NO]
               referrer:nil
                  flags:NSLoadFlagsNone
               activate:NO
            allowPopups:NO];
}

- (NSString*)getContextMenuNodeDocumentURL
{
  if (!mDataOwner->mContextMenuNode) return @"";
  
  nsCOMPtr<nsIDOMDocument> ownerDoc;
  mDataOwner->mContextMenuNode->GetOwnerDocument(getter_AddRefs(ownerDoc));

  nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(ownerDoc);
  if (!nsDoc) return @"";

  nsCOMPtr<nsIDOMLocation> location;
  nsDoc->GetLocation(getter_AddRefs(location));
  if (!location) return @"";
	
  nsAutoString urlStr;
  location->GetHref(urlStr);
  return [NSString stringWith_nsAString:urlStr];
}

- (IBAction)frameToNewWindow:(id)sender
{
  // assumes mContextMenuNode has been set
  NSString* frameURL = [self getContextMenuNodeDocumentURL];
  if ([frameURL length] > 0)
    [self openNewWindowWithURL:frameURL referrer:nil loadInBackground:NO allowPopups:NO];     // follow the pref?
}

- (IBAction)frameToNewTab:(id)sender
{
  // assumes mContextMenuNode has been set
  NSString* frameURL = [self getContextMenuNodeDocumentURL];
  if ([frameURL length] > 0)
    [self openNewTabWithURL:frameURL referrer:nil loadInBackground:NO allowPopups:NO];        // follow the pref?
}

- (IBAction)frameToThisWindow:(id)sender
{
  // assumes mContextMenuNode has been set
  NSString* frameURL = [self getContextMenuNodeDocumentURL];
  if ([frameURL length] > 0)
    [self loadURL:frameURL referrer:nil activate:YES allowPopups:NO];
}

// map command-left arrow to 'back'
- (void)moveToBeginningOfLine:(id)sender
{
  [self back:sender];
}

// map command-right arrow to 'forward'
- (void)moveToEndOfLine:(id)sender
{
  [self forward:sender];
}

//
// -focusedElement
//
// Returns the currently focused DOM element in the currently visible tab
//
- (void)focusedElement:(nsIDOMElement**)outElement
{
  #define ENSURE_TRUE(x) if (!x) return;
  if (!outElement)
    return;
  *outElement = nsnull;

  nsCOMPtr<nsIWebBrowser> webBrizzle = dont_AddRef([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
  ENSURE_TRUE(webBrizzle);
  nsCOMPtr<nsIDOMWindow> domWindow;
  webBrizzle->GetContentDOMWindow(getter_AddRefs(domWindow));
  nsCOMPtr<nsPIDOMWindow> privateWindow = do_QueryInterface(domWindow);
  ENSURE_TRUE(privateWindow);
  nsIFocusController *controller = privateWindow->GetRootFocusController();
  ENSURE_TRUE(controller);
  nsCOMPtr<nsIDOMElement> focusedItem;
  controller->GetFocusedElement(getter_AddRefs(focusedItem));
  *outElement = focusedItem.get();
  NS_IF_ADDREF(*outElement);

  #undef ENSURE_TRUE
}

//
// -isPageTextFieldFocused
//
// Determine if a text field in the content area has focus. Returns YES if the
// focus is in a <input type="text"> or <textarea>
//
- (BOOL)isPageTextFieldFocused
{
  BOOL isFocused = NO;
  
  nsCOMPtr<nsIDOMElement> focusedItem;
  [self focusedElement:getter_AddRefs(focusedItem)];
  
  // we got it, now check if it's what we care about
  nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(focusedItem);
  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea = do_QueryInterface(focusedItem);
  if (input) {
    nsAutoString type;
    input->GetType(type);
    if (type == NS_LITERAL_STRING("text"))
      isFocused = YES;
  }
  else if (textArea)
    isFocused = YES;
  
  return isFocused;
}

//
// -isPagePluginFocused
//
// Determine if a plugin/applet in the content area has focus. Returns YES if the
// focus is in a <embed>, <object>, or <applet>
//
- (BOOL)isPagePluginFocused
{
  BOOL isFocused = NO;
  
  nsCOMPtr<nsIDOMElement> focusedItem;
  [self focusedElement:getter_AddRefs(focusedItem)];
  
  // we got it, now check if it's what we care about
  nsCOMPtr<nsIDOMHTMLEmbedElement> embed = do_QueryInterface(focusedItem);
  nsCOMPtr<nsIDOMHTMLObjectElement> object = do_QueryInterface(focusedItem);
  nsCOMPtr<nsIDOMHTMLAppletElement> applet = do_QueryInterface(focusedItem);
  if (embed || object || applet)
    isFocused = YES;
  
  return isFocused;
}

// map delete key to Back according to browser.backspace_action pref
- (void)deleteBackward:(id)sender
{
  // there are times when backspaces can seep through from IME gone wrong. As a 
  // workaround until we can get them all fixed, ignore backspace when the
  // focused widget is a text field (<input type="text"> or <textarea>)
  if ([self isPageTextFieldFocused] || [self isPagePluginFocused])
    return;

  int deleteKeyAction = [[PreferenceManager sharedInstance] getIntPref:"browser.backspace_action" withSuccess:NULL];  

  if (deleteKeyAction == 0) { // map to back/forward
    if ([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask)
      [self forward:sender];
    else
      [self back:sender];
  }
  // all other values for browser.backspace_action should give no mapping at all,
  // including 1 (PgUp/PgDn mapping has no precedent on Mac OS, and we're not supporting it)
}

-(void)loadURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer activate:(BOOL)activate allowPopups:(BOOL)inAllowPopups
{
    if (mInitialized) {
      [mBrowserView loadURI:aURLSpec referrer:aReferrer flags:NSLoadFlagsNone activate:activate allowPopups:inAllowPopups];
    }
    else {
        // we haven't yet initialized all the browser machinery, stash the url and referrer
        // until we're ready in windowDidLoad:
        mPendingURL = aURLSpec;
        [mPendingURL retain];
        mPendingReferrer = aReferrer;
        [mPendingReferrer retain];
        mPendingActivate = activate;
        mPendingAllowPopups = inAllowPopups;
    }
}

//
// closeBrowserWindow:
//
// Some gecko view in this window wants to close the window. If we have
// a bunch of tabs, only close the relevant tab, otherwise close the
// window as a whole.
//
- (void)closeBrowserWindow:(BrowserWrapper*)inBrowser
{
  if ( [mTabBrowser numberOfTabViewItems] > 1 ) {
    // close the appropriate browser (it may not be frontmost) and
    // remove it from the tab UI
    [inBrowser windowClosed];
    [mTabBrowser removeTabViewItem:[inBrowser tab]];
  }
  else
  {
    // if a window unload handler calls window.close(), we
    // can get here while the window is already being closed,
    // so we don't want to close it again (and recurse).
    if (!mClosingWindow)
      [[self window] close];
  }
}

- (void)willShowPromptForBrowser:(BrowserWrapper*)inBrowser
{
  // bring the tab to the front (for security reasons)
  BrowserTabViewItem* tabItem = [self tabForBrowser:inBrowser];
  [mTabBrowser selectTabViewItem:tabItem];
  // force a display, so that the tab view redraws before the sheet is shown
  [mTabBrowser display];
}

- (void)didDismissPromptForBrowser:(BrowserWrapper*)inBrowser
{
}

- (void)createNewTab:(ENewTabContents)contents
{
    BrowserTabViewItem* newTab  = [self createNewTabItem];
    BrowserWrapper*     newView = [newTab view];
    
    BOOL loadHomepage = NO;
    if (contents == eNewTabHomepage)
    {
      // 0 = about:blank, 1 = home page, 2 = last page visited
      int newTabPage = [[PreferenceManager sharedInstance] getIntPref:"browser.tabs.startPage" withSuccess:NULL];
      loadHomepage = (newTabPage == 1);
    }

    [newTab setLabel: (loadHomepage ? NSLocalizedString(@"TabLoading", @"") : NSLocalizedString(@"UntitledPageTitle", @""))];
    [mTabBrowser addTabViewItem: newTab];
    
    BOOL focusURLBar = NO;
    if (contents != eNewTabEmpty)
    {
      // Focus the URL bar if we're opening a blank tab and the URL bar is visible.
      NSToolbar* toolbar = [[self window] toolbar];
      BOOL locationBarVisible = [toolbar isVisible] &&
                                ([toolbar displayMode] == NSToolbarDisplayModeIconAndLabel ||
                                 [toolbar displayMode] == NSToolbarDisplayModeIconOnly);
                                
      NSString* urlToLoad = @"about:blank";
      if (loadHomepage)
        urlToLoad = [[PreferenceManager sharedInstance] homePageUsingStartPage:NO];

      focusURLBar = locationBarVisible && [MainController isBlankURL:urlToLoad];      

      [newView loadURI:urlToLoad referrer:nil flags:NSLoadFlagsNone activate:!focusURLBar allowPopups:NO];
    }
    
    [mTabBrowser selectLastTabViewItem: self];
    
    if (focusURLBar)
      [self focusURLBar];
}

- (IBAction)newTab:(id)sender
{
  [self createNewTab:eNewTabHomepage];  // we'll look at the pref to decide whether to load the home page
}

-(IBAction)closeCurrentTab:(id)sender
{
  if ( [mTabBrowser numberOfTabViewItems] > 1 )
  {
    [[[mTabBrowser selectedTabViewItem] view] windowClosed];
    [mTabBrowser removeTabViewItem:[mTabBrowser selectedTabViewItem]];
  }
}

- (IBAction)previousTab:(id)sender
{
  if ([mTabBrowser indexOfTabViewItem:[mTabBrowser selectedTabViewItem]] == 0)
    [mTabBrowser selectLastTabViewItem:sender];
  else
    [mTabBrowser selectPreviousTabViewItem:sender];
}

- (IBAction)nextTab:(id)sender
{
  if ([mTabBrowser indexOfTabViewItem:[mTabBrowser selectedTabViewItem]] == [mTabBrowser numberOfTabViewItems] - 1)
    [mTabBrowser selectFirstTabViewItem:sender];
  else
    [mTabBrowser selectNextTabViewItem:sender];

  [[NSApp delegate] delayedAdjustBookmarksMenuItemsEnabling];
}

- (IBAction)closeSendersTab:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      [[tabViewItem view] windowClosed];
      [mTabBrowser removeTabViewItem:tabViewItem];
    }
  }
}

- (IBAction)closeOtherTabs:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      while ([mTabBrowser numberOfTabViewItems] > 1)
      {
        NSTabViewItem* doomedItem = nil;
        if ([mTabBrowser indexOfTabViewItem:tabViewItem] == 0)
          doomedItem = [mTabBrowser tabViewItemAtIndex:1];
        else
          doomedItem = [mTabBrowser tabViewItemAtIndex:0];
        
        [[doomedItem view] windowClosed];
        [mTabBrowser removeTabViewItem:doomedItem];
      }
    }
  }
}

- (IBAction)reloadSendersTab:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      [[[tabViewItem view] getBrowserView] reload: NSLoadFlagsNone];
    }
  }
}

- (IBAction)reloadAllTabs:(id)sender
{
  unsigned int reloadFlags = NSLoadFlagsNone;
  if ([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask)
    reloadFlags = NSLoadFlagsBypassCacheAndProxy;

  NSEnumerator* tabsEnum = [[mTabBrowser tabViewItems] objectEnumerator];
  BrowserTabViewItem* curTabItem;
  while ((curTabItem = [tabsEnum nextObject]))
  {
    if ([curTabItem isKindOfClass:[BrowserTabViewItem class]])
      [[[curTabItem view] getBrowserView] reload:reloadFlags];
  }
}

- (IBAction)moveTabToNewWindow:(id)sender
{
  if ([sender isMemberOfClass:[NSMenuItem class]])
  {
    BrowserTabViewItem* tabViewItem = [mTabBrowser itemWithTag:[sender tag]];
    if (tabViewItem)
    {
      NSString* url = [[tabViewItem view] getCurrentURI];
      BOOL backgroundLoad = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];

      [self openNewWindowWithURL:url referrer:nil loadInBackground:backgroundLoad allowPopups:NO];

      [[tabViewItem view] windowClosed];
      [mTabBrowser removeTabViewItem:tabViewItem];
    }
  }
}

- (void)tabView:(NSTabView *)aTabView didSelectTabViewItem:(NSTabViewItem *)aTabViewItem
{
  // we'll get called for the sidebar tabs as well. ignore any calls coming from
  // there, we're only interested in the browser tabs.
  if (aTabView != mTabBrowser)
    return;

  // Disconnect the old view, if one has been designated.
  // If the window has just been opened, none has been.
  if (mBrowserView)
  {
    [mBrowserView willResignActiveBrowser];
    [mBrowserView setDelegate:nil];
  }

  // Connect up the new view
  mBrowserView = [aTabViewItem view];
  [mTabBrowser refreshTabBar:YES];
      
  // Make the new view the primary content area.
  [mBrowserView setDelegate:self];
  [mBrowserView didBecomeActiveBrowser];
  [self updateFromFrontmostTab];

  if (![self userChangedLocationField] && [[self window] isKeyWindow])
    [mBrowserView setBrowserActive:YES];

  [[NSApp delegate] delayedAdjustBookmarksMenuItemsEnabling];
}

- (void)tabView:(NSTabView *)aTabView willSelectTabViewItem:(NSTabViewItem *)aTabViewItem
{
  // we'll get called for the sidebar tabs as well. ignore any calls coming from
  // there, we're only interested in the browser tabs.
  if (aTabView != mTabBrowser)
    return;
  if ([aTabView isKindOfClass:[BrowserTabView class]] && [aTabViewItem isKindOfClass:[BrowserTabViewItem class]]) {
    [(BrowserTabViewItem *)[aTabView selectedTabViewItem] willDeselect];
    [(BrowserTabViewItem *)aTabViewItem willSelect];
  }
}

- (void)tabViewDidChangeNumberOfTabViewItems:(NSTabView *)aTabView
{
  [[NSApp delegate] delayedFixCloseMenuItemKeyEquivalents];
  [mTabBrowser refreshTabBar:YES];
  // paranoia, to avoid stale mBrowserView pointer (since we don't own it)
  if ([aTabView numberOfTabViewItems] == 0)
    mBrowserView = nil;
}

-(BrowserTabView*)getTabBrowser
{
  return mTabBrowser;
}

-(BrowserWrapper*)getBrowserWrapper
{
  return mBrowserView;
}

// this should really be a class method
-(BrowserWindowController*)openNewWindowWithURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground:(BOOL)aLoadInBG allowPopups:(BOOL)inAllowPopups
{
  BrowserWindowController* browser = [self openNewWindow:aLoadInBG];
  [browser loadURL:aURLSpec referrer:aReferrer activate:!aLoadInBG allowPopups:inAllowPopups];
  return browser;
}

//
// -openNewWindow:
//
// open a new window, but doesn't load anything into it. Must be matched
// with a call to do that.
// 
// this should really be a class method
//
- (BrowserWindowController*)openNewWindow:(BOOL)aLoadInBG
{
  // Autosave our dimensions before we open a new window.  That ensures the size ends up matching.
  [self autosaveWindowFrame];

  BrowserWindowController* browser = [[BrowserWindowController alloc] initWithWindowNibName: @"BrowserWindow"];
  if (aLoadInBG)
  {
    BrowserWindow* browserWin = (BrowserWindow*)[browser window];
    [browserWin setSuppressMakeKeyFront:YES];	// prevent gecko focus bringing the window to the front
    [browserWin orderWindow: NSWindowBelow relativeTo: [[self window] windowNumber]];
    [browserWin setSuppressMakeKeyFront:NO];
  }
  else
    [browser showWindow:nil];

  return browser;
}

-(void)openNewTabWithURL:(NSString*)aURLSpec referrer:(NSString*)aReferrer loadInBackground:(BOOL)aLoadInBG allowPopups:(BOOL)inAllowPopups
{
  BrowserTabViewItem* newTab = [self openNewTab:aLoadInBG];
  [[newTab view] loadURI:aURLSpec referrer:aReferrer flags:NSLoadFlagsNone activate:!aLoadInBG allowPopups:inAllowPopups];
}

//
// -createNewTabBrowser:
// BrowserUICreationDelegate
//
// create a new tab in the window associated with this wrapper and get its
// browser view without loading anything into it.
//
- (CHBrowserView*)createNewTabBrowser:(BOOL)inLoadInBG
{
  BrowserTabViewItem* newTab = [self openNewTab:inLoadInBG];
  return [[newTab view] getBrowserView];
}

//
// -openNewTab:
//
// open a new tab, but doesn't load anything into it. Must be matched
// with a call to do that.
//
- (BrowserTabViewItem*)openNewTab:(BOOL)aLoadInBG
{
  BrowserTabViewItem* newTab  = [self createNewTabItem];

  
  // hyatt originally made new tabs open on the far right and tabs opened from a content
  // link open to the right of the current tab. The idea was to keep the new tab
  // close to the tab that spawned it, since they are related. Users, however, got confused
  // as to why tabs appeared in different places, so now all tabs go on the far right.
#ifdef OPEN_TAB_TO_RIGHT_OF_SELECTED    
  NSTabViewItem* selectedTab = [mTabBrowser selectedTabViewItem];
  int index = [mTabBrowser indexOfTabViewItem: selectedTab];
  [mTabBrowser insertTabViewItem: newTab atIndex: index+1];
#else
  [mTabBrowser addTabViewItem: newTab];
#endif

  [newTab setLabel: NSLocalizedString(@"TabLoading", @"")];

  // unless we're told to load this tab in the bg, select the tab
  // before we load so that it's the primary and will push the url into
  // the url bar immediately rather than waiting for the server.
  if (!aLoadInBG)
    [mTabBrowser selectTabViewItem: newTab];

  return newTab;
}

-(void)openNewWindowWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG
{
  BrowserWindowController* browser = [self openNewWindow:aLoadInBG];
  [[[browser getBrowserWrapper] getBrowserView] setPageDescriptor:aDesc displayType:aDisplayType];
}

-(void)openNewTabWithDescriptor:(nsISupports*)aDesc displayType:(PRUint32)aDisplayType loadInBackground:(BOOL)aLoadInBG
{
  BrowserTabViewItem* newTab = [self openNewTab:aLoadInBG];
  [[[newTab view] getBrowserView] setPageDescriptor:aDesc displayType:aDisplayType];
}

- (void)openURLArray:(NSArray*)urlArray tabOpenPolicy:(ETabOpenPolicy)tabPolicy allowPopups:(BOOL)inAllowPopups
{
  int curNumTabs	 = [mTabBrowser numberOfTabViewItems];
  int numItems 		 = (int)[urlArray count];
  int selectedTabIndex = [[mTabBrowser tabViewItems] indexOfObject:[mTabBrowser selectedTabViewItem]];
  BrowserTabViewItem* tabViewToSelect = nil;
  
  for (int i = 0; i < numItems; i++)
  {
    NSString* thisURL = [urlArray objectAtIndex:i];
    BrowserTabViewItem* tabViewItem = nil;
    
    if (tabPolicy == eReplaceTabs && i < curNumTabs)
      tabViewItem = (BrowserTabViewItem*)[mTabBrowser tabViewItemAtIndex:i];
    else if (tabPolicy == eReplaceFromCurrentTab && selectedTabIndex < curNumTabs)
      tabViewItem = (BrowserTabViewItem*)[mTabBrowser tabViewItemAtIndex:selectedTabIndex++];
    else
    {
      tabViewItem = [self createNewTabItem];
      [tabViewItem setLabel:NSLocalizedString(@"UntitledPageTitle", @"")];
      [mTabBrowser addTabViewItem:tabViewItem];
    }
    
    if (!tabViewToSelect)
      tabViewToSelect = tabViewItem;

    [[tabViewItem view] loadURI:thisURL referrer:nil
                          flags:NSLoadFlagsNone activate:(i == 0) allowPopups:inAllowPopups];
  }
  
  // if we replace all tabs (because we opened a tab group), or we open additional tabs
  // with the "focus new tab"-pref on, focus the first new tab.
  BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
  
  if (!((tabPolicy == eAppendTabs) && loadInBackground))
    [mTabBrowser selectTabViewItem:tabViewToSelect];
    
}

-(void) openURLArrayReplacingTabs:(NSArray*)urlArray closeExtraTabs:(BOOL)closeExtra allowPopups:(BOOL)inAllowPopups
{
  // if there are no urls to load (say, an empty tab group), just bail outright. otherwise we'd be
  // left with no tabs at all and hell to pay when it came time to do menu/toolbar item validation.
  if (![urlArray count])
    return;

  [self openURLArray:urlArray tabOpenPolicy:eReplaceTabs allowPopups:inAllowPopups];
  if (closeExtra) {
    int closeIndex = [urlArray count];
    int closeCount = [mTabBrowser numberOfTabViewItems] - closeIndex;
    for (int i = 0; i < closeCount; i++) {
      [(BrowserTabViewItem*)[mTabBrowser tabViewItemAtIndex:closeIndex] closeTab:nil];
    }
  }
}

-(BrowserTabViewItem*)createNewTabItem
{
  BrowserTabViewItem* newTab = [BrowserTabView makeNewTabItem];
  BrowserWrapper* newView = [[BrowserWrapper alloc] initWithTab:newTab inWindow:[self window]];
  [newView setUICreationDelegate:self];

  // register the bookmarks as a custom view
  BookmarkViewController* bmController = [[[BookmarkViewController alloc] initWithBrowserWindowController:self] autorelease];
  [newView registerContentViewProvider:bmController forURL:@"about:bookmarks"];
  [newView registerContentViewProvider:bmController forURL:@"about:history"];
  
  // size the new browser view properly up-front, so that if the
  // page is scrolled to a relative anchor, we don't mess with the
  // scroll position by resizing it later
  [newView setFrame:[mTabBrowser contentRect] resizingBrowserViewIfHidden:YES];

  [newTab setView: newView];  // takes ownership
  [newView release];
  
  // we have to copy the context menu for each tag, because
  // the menu gets the tab view item's tag.
  NSMenu* contextMenu = [mTabMenu copy];
  [[newTab tabItemContentsView] setMenu:contextMenu];
  [contextMenu release];

  return newTab;
}

-(void)setChromeMask:(unsigned int)aMask
{
  mChromeMask = aMask;
}

-(unsigned int)chromeMask
{
  return mChromeMask;
}

-(BOOL)hasFullBrowserChrome
{
  return (mChromeMask == 0 || 
            (mChromeMask & nsIWebBrowserChrome::CHROME_TOOLBAR &&
             mChromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR &&
             mChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE));
}

- (IBAction)biggerTextSize:(id)aSender
{
  [[mBrowserView getBrowserView] biggerTextSize];
}

- (IBAction)smallerTextSize:(id)aSender
{
  [[mBrowserView getBrowserView] smallerTextSize];
}

- (IBAction)getInfo:(id)sender
{
  if ([self bookmarkManagerIsVisible])
    [self showBookmarksInfo:sender];
  else
    [self showPageInfo:sender];
}

- (BOOL)shouldShowBookmarkToolbar
{
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults integerForKey:USER_DEFAULTS_HIDE_PERS_TOOLBAR_KEY] == 1)
     return NO;

  return YES;
}

// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode
{
  if (mDataOwner)
  {
    mDataOwner->mContextMenuFlags = flags;
    mDataOwner->mContextMenuNode  = aNode;
    mDataOwner->mContextMenuEvent = aEvent;
    mDataOwner->mGotOnContextMenu = true;
  }
  
  // There is no simple way of getting a callback when the context menu handling
  // has finished, so we don't have a good place to clear mContextMenuNode etc.
  // Cocoa doesn't provide any methods that can be overridden on the menu, view
  // or window.
  // The menu closes before the actions are dispatched, so that's too early.
  // And we can't just use -performSelector:afterDelay:0 because that will fire
  // while the menu is still up.
  // So the best solution I've been able to come up with is relying on the
  // autorelease of an object, which will happen when we get back to the
  // main loop.
  [[[ContextMenuDataClearer alloc] initWithTarget:self selector:@selector(clearContextMenuTarget)] autorelease];
}

- (void)clearContextMenuTarget
{
  if (mDataOwner)
  {
    mDataOwner->mContextMenuFlags = 0;
    mDataOwner->mContextMenuNode  = nsnull;
    mDataOwner->mContextMenuEvent = nsnull;
    mDataOwner->mGotOnContextMenu = false;
  }
}

// Returns the text of the href attribute for the link the context menu is
// currently on. Returns an empty string if the context menu is not on a
// link or we couldn't work out the href for some other reason.
- (NSString*)getContextMenuNodeHrefText
{
  if (!mDataOwner->mContextMenuNode)
    return @"";

  nsCOMPtr<nsIDOMElement> linkContent;
  nsAutoString href;
  GeckoUtils::GetEnclosingLinkElementAndHref(mDataOwner->mContextMenuNode, getter_AddRefs(linkContent), href);

  // XXXdwh Handle simple XLINKs if we want to be compatible with Mozilla, but who
  // really uses these anyway? :)
  if (linkContent && !href.IsEmpty())
    return [NSString stringWith_nsAString:href];

  return @"";
}

//
// Determine if the node the context menu has been invoked for is an <a> node
// indicating a mailto: link. If so return an array containing the e-mail addresses
// in the link. Otherwise return nil.
//
- (NSArray*)mailAddressesInContextMenuLinkNode
{
  NSString* hrefStr = [self getContextMenuNodeHrefText];
  
  if ([hrefStr hasPrefix:@"mailto:"]) {
    NSString* linkTargetText = [hrefStr substringFromIndex:7];
    
    // mailto: links can contain arguments (after '?')
    NSRange separatorRange = [linkTargetText rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"?"]];
    
    if (separatorRange.length != 0)
      linkTargetText = [linkTargetText substringToIndex:separatorRange.location];
      
    return [linkTargetText componentsSeparatedByString:@","];
  }

  return nil;
}

// Add the e-mail address from the mailto: link of the context menu node
// to the user's address book. If the address already exists we just
// open the address book at the record containing it.
- (IBAction)addToAddressBook:(id)aSender
{
  NSString* emailAddress = [aSender representedObject];
  if (emailAddress) {
    ABAddressBook* abook = [ABAddressBook sharedAddressBook];
    if ([abook emailAddressExistsInAddressBook:emailAddress] )
      [abook openAddressBookForRecordWithEmail:emailAddress];
    else
      [abook addNewPersonFromEmail:emailAddress];
  }
}

// Copy the e-mail address(es) from the mailto: link of the context menu node
// onto the clipboard.
- (IBAction)copyAddressToClipboard:(id)aSender
{
  NSString* emailAddress = [[self mailAddressesInContextMenuLinkNode] componentsJoinedByString:@","];
  
  if (emailAddress) {
    NSPasteboard* clipboard = [NSPasteboard generalPasteboard];
    
    NSArray* types = [NSArray arrayWithObject:NSStringPboardType];
    
    [clipboard declareTypes:types owner:nil];
    [clipboard setString:emailAddress forType:NSStringPboardType];
  }
}

//
// Create a menu item to add/open the specified e-mail address to Address Book
//
- (NSMenuItem*)prepareAddToAddressBookMenuItem:(NSString*)emailAddress
{
  NSMenuItem* addToAddressBookItem = nil;

  if ([emailAddress length] > 0) {
    addToAddressBookItem = [[NSMenuItem alloc] init];

    if ([[ABAddressBook sharedAddressBook] emailAddressExistsInAddressBook:emailAddress]) {
      NSString* realName = [[ABAddressBook sharedAddressBook] getRealNameForEmailAddress:emailAddress];
      [addToAddressBookItem setTitle:[NSString stringWithFormat:NSLocalizedString(@"Open %@ in Address Book", @""), realName != nil ? realName : @""]];
    } else {
      [addToAddressBookItem setTitle:[NSString stringWithFormat:NSLocalizedString(@"Add %@ to Address Book", @""), emailAddress]];
    }

    [addToAddressBookItem setEnabled:YES];
    [addToAddressBookItem setRepresentedObject:emailAddress];
    [addToAddressBookItem setAction:@selector(addToAddressBook:)];
    [addToAddressBookItem autorelease];
  }
  
  return addToAddressBookItem;
}

- (NSMenu*)getContextMenu
{
  if (!mDataOwner->mGotOnContextMenu)
    return nil;

  BOOL showFrameItems = NO;
  
  NSMenu* menuPrototype = nil;
  int contextMenuFlags = mDataOwner->mContextMenuFlags;
  
  NSArray* emailAddresses = nil;
  unsigned numEmailAddresses = 0;

  BOOL hasSelection = [[mBrowserView getBrowserView] canCopy];

  if ((contextMenuFlags & nsIContextMenuListener::CONTEXT_LINK) != 0)
  {
    emailAddresses = [self mailAddressesInContextMenuLinkNode];
    if (emailAddresses != nil)
      numEmailAddresses = [emailAddresses count];

    if ((contextMenuFlags & nsIContextMenuListener::CONTEXT_IMAGE) != 0) {
      if (numEmailAddresses > 0)
        menuPrototype = mImageMailToLinkMenu;
      else
        menuPrototype = mImageLinkMenu;
    } 
    else {
      if (numEmailAddresses > 0)
        menuPrototype = mMailToLinkMenu;
      else
        menuPrototype = mLinkMenu;
    }
  }
  else if ((contextMenuFlags & nsIContextMenuListener::CONTEXT_INPUT) != 0 ||
           (contextMenuFlags & nsIContextMenuListener::CONTEXT_TEXT) != 0) {
    menuPrototype = mInputMenu;
  }
  else if ((contextMenuFlags & nsIContextMenuListener::CONTEXT_IMAGE) != 0) {
    menuPrototype = mImageMenu;
  }
  else if (!contextMenuFlags || (contextMenuFlags & nsIContextMenuListener::CONTEXT_DOCUMENT) != 0) {
    // if there aren't any flags or we're in the background of a page,
    // show the document menu. This prevents us from failing to find a case
    // and not showing the context menu.
    menuPrototype = mPageMenu;
    [mBackItem    setEnabled: [[mBrowserView getBrowserView] canGoBack]];
    [mForwardItem setEnabled: [[mBrowserView getBrowserView] canGoForward]];
    [mCopyItem		setEnabled:hasSelection];
  }
    
  if (mDataOwner->mContextMenuNode) {
    nsCOMPtr<nsIDOMDocument> ownerDoc;
    mDataOwner->mContextMenuNode->GetOwnerDocument(getter_AddRefs(ownerDoc));
  
    nsCOMPtr<nsIDOMWindow> contentWindow = [[mBrowserView getBrowserView] getContentWindow];

    nsCOMPtr<nsIDOMDocument> contentDoc;
    if (contentWindow)
      contentWindow->GetDocument(getter_AddRefs(contentDoc));
    
    showFrameItems = (contentDoc != ownerDoc);
  }

  // we have to clone the menu and return that, so that we don't change
  // our only copy of the menu
  NSMenu* result = [[menuPrototype copy] autorelease];

  const int kFrameRelatedItemsTag = 100;
  const int kFrameInapplicableItemsTag = 101;
  const int kSelectionRelatedItemsTag = 102;
  
  // if there's no selection or no search bar in the toolbar, hide the search item.
  // We need a search item to know what the user's preferred search is.
  if (!hasSelection) {
    NSMenuItem* selectionItem;
    while ((selectionItem = [result itemWithTag:kSelectionRelatedItemsTag]) != nil)
      [result removeItem:selectionItem];
  }
  
  if (showFrameItems) {
    NSMenuItem* frameItem;
    while ((frameItem = [result itemWithTag:kFrameInapplicableItemsTag]) != nil)
      [result removeItem:frameItem];
  }
  else {
    NSMenuItem* frameItem;
    while ((frameItem = [result itemWithTag:kFrameRelatedItemsTag]) != nil)
      [result removeItem:frameItem];
  }
  
  // Add items to add/open each e-mail address in a mailto: link and
  // change "address" to the plural form if necessary
  if (numEmailAddresses > 0) {
    for (signed i = (signed) numEmailAddresses - 1; i >= 0 ; --i) {
      NSMenuItem* item = [self prepareAddToAddressBookMenuItem:[emailAddresses objectAtIndex:i]];
      if (item)
        [result insertItem:item atIndex:1];
    }
    if (numEmailAddresses > 1)
      [[result itemWithTarget:self andAction:@selector(copyAddressToClipboard:)] setTitle:NSLocalizedString(@"Copy Addresses", @"")];
  }
  
  return result;
}

// Context menu methods
- (IBAction)openLinkInNewWindow:(id)aSender
{
  [self openLinkInNewWindowOrTab: YES];
}

- (IBAction)openLinkInNewTab:(id)aSender
{
  [self openLinkInNewWindowOrTab: NO];
}

-(void)openLinkInNewWindowOrTab: (BOOL)aUseWindow
{
  NSString* hrefStr = [self getContextMenuNodeHrefText];

  if ([hrefStr length] == 0)
    return;

  BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];

  NSString* referrer = [[mBrowserView getBrowserView] getFocusedURLString];

  if (aUseWindow)
    [self openNewWindowWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground allowPopups:NO];
  else
    [self openNewTabWithURL: hrefStr referrer:referrer loadInBackground: loadInBackground allowPopups:NO];
}

- (IBAction)savePageAs:(id)aSender
{
  NSView* accessoryView = [[NSApp delegate] getSavePanelView];
  [self saveDocument:NO filterView:accessoryView];
}

- (IBAction)saveFrameAs:(id)aSender
{
  NSView* accessoryView = [[NSApp delegate] getSavePanelView];
  [self saveDocument:YES filterView:accessoryView];
}

- (IBAction)saveLinkAs:(id)aSender
{
  NSString* hrefStr = [self getContextMenuNodeHrefText];
  if ([hrefStr length] == 0)
    return;

  // The user wants to save this link.
  nsAutoString text;
  GeckoUtils::GatherTextUnder(mDataOwner->mContextMenuNode, text);

  [self saveURL:nil url:hrefStr suggestedFilename:[NSString stringWith_nsAString:text]];
}

- (IBAction)saveImageAs:(id)aSender
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mDataOwner->mContextMenuNode));
  if (imgElement) {
      nsAutoString text;
      imgElement->GetAttribute(NS_LITERAL_STRING("src"), text);
      nsAutoString url;
      imgElement->GetSrc(url);

      NSString* hrefStr = [NSString stringWith_nsAString: url];

      [self saveURL:nil url:hrefStr suggestedFilename: [NSString stringWith_nsAString: text]];
  }
}

- (IBAction)copyImage:(id)sender
{
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
  nsCOMPtr<nsICommandManager> commandMgr(do_GetInterface(webBrowser));
  if (!commandMgr)
    return;

  (void)commandMgr->DoCommand("cmd_copyImageContents", nsnull, nsnull);
}

- (IBAction)copyImageLocation:(id)sender
{
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(webBrowser));
  if (clipboard)
    clipboard->CopyImageLocation();
}

- (IBAction)copyLinkLocation:(id)aSender
{
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([[[self getBrowserWrapper] getBrowserView] getWebBrowser]);
  nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(webBrowser));
  if (clipboard)
    clipboard->CopyLinkLocation();
}

- (IBAction)viewOnlyThisImage:(id)aSender
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(mDataOwner->mContextMenuNode));
  if (imgElement) {
    nsAutoString url;
    imgElement->GetSrc(url);

    NSString* urlStr = [NSString stringWith_nsAString: url];
    NSString* referrer = [[mBrowserView getBrowserView] getFocusedURLString];

    unsigned int modifiers = [[NSApp currentEvent] modifierFlags];

    if (modifiers & NSCommandKeyMask) {
      BOOL loadInTab = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL];
      BOOL loadInBG  = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
      if (modifiers & NSShiftKeyMask)
        loadInBG = !loadInBG; // shift key should toggle the foreground/background pref as it does elsewhere
      if (loadInTab)
        [self openNewTabWithURL:urlStr referrer:referrer loadInBackground:loadInBG allowPopups:NO];
      else
        [self openNewWindowWithURL:urlStr referrer:referrer loadInBackground:loadInBG allowPopups:NO];
    }
    else
      [self loadURL:urlStr referrer:referrer activate:YES allowPopups:NO];
  }  
}

- (IBAction)showPageInfo:(id)sender
{
  PageInfoWindowController* pageInfoController = [PageInfoWindowController sharedPageInfoWindowController];

  [pageInfoController updateFromBrowserView:[[self getBrowserWrapper] getBrowserView]];
  [[pageInfoController window] makeKeyAndOrderFront:nil];
}

- (IBAction)showBookmarksInfo:(id)sender
{
    BookmarkViewController* bookmarksController = [self bookmarkViewControllerForCurrentTab];
    [bookmarksController ensureBookmarks];
    [bookmarksController showBookmarkInfo:sender];
}

- (IBAction)showSiteCertificate:(id)sender
{
  CertificateItem* certItem = [[self activeBrowserView] siteCertificate];
  if (certItem)
  {
    [ViewCertificateDialogController showCertificateWindowWithCertificateItem:certItem
                                                         certTypeForTrustSettings:nsIX509Cert::SERVER_CERT];
  }
}

- (BookmarkToolbar*) bookmarkToolbar
{
  return mPersonalToolbar;
}

- (NSProgressIndicator*)progressIndicator
{
  return mProgress;
}

- (void)showProgressIndicator
{
  // note we do nothing to check if the progress indicator is already there.
  [mProgressSuperview addSubview:mProgress];
}

- (void)hideProgressIndicator
{
  [mProgress removeFromSuperview];
}

- (BOOL)windowClosesQuietly
{
  return mWindowClosesQuietly;
}

- (void)setWindowClosesQuietly:(BOOL)inClosesQuietly
{
  mWindowClosesQuietly = inClosesQuietly;
}

//
// - unblockAllSites:
//
// Called in response to the menu item from the unblock popup. Loop over all
// the items in the blocked sites array in the browser wrapper and add them
// to the whitelist.
//
- (void)unblockAllSites:(nsISupportsArray*)sender
{
  nsCOMPtr<nsISupportsArray> blockedSites;
  [[self getBrowserWrapper] getBlockedSites:getter_AddRefs(blockedSites)];
  nsCOMPtr<nsIPermissionManager> pm ( do_GetService(NS_PERMISSIONMANAGER_CONTRACTID) );

  PRUint32 count = 0;
  blockedSites->Count(&count);
  for ( PRUint32 i = 0; i < count; ++i ) {
    nsCOMPtr<nsISupports> genUri = dont_AddRef(blockedSites->ElementAt(i));
    nsCOMPtr<nsIURI> uri = do_QueryInterface(genUri);
    pm->Add(uri, "popup", nsIPermissionManager::ALLOW_ACTION);   
  }
}

// updateLock:
//
// updates the url bar display of security state appropriately.
- (void)updateLock:(unsigned int)inSecurityState
{
  unsigned char securityState = inSecurityState & 0x000000FF;
  [mURLBar setSecureState:securityState];
}

+ (NSImage*) insecureIcon
{
  static NSImage* sInsecureIcon = nil;
  if (!sInsecureIcon)
    sInsecureIcon = [[NSImage imageNamed:@"globe_ico"] retain];
  return sInsecureIcon;
}

+ (NSImage*) secureIcon
{
  static NSImage* sSecureIcon = nil;
  if (!sSecureIcon)
    sSecureIcon = [[NSImage imageNamed:@"security_lock"] retain];
  return sSecureIcon;
}

+ (NSImage*) brokenIcon
{
  static NSImage* sBrokenIcon = nil;
  if (!sBrokenIcon)
    sBrokenIcon = [[NSImage imageNamed:@"security_broken"] retain];
  return sBrokenIcon;
}

+ (NSDictionary *)searchURLDictionary
{
  static NSDictionary *searchURLDictionary = nil;
  if (searchURLDictionary)
    return searchURLDictionary;

  NSString *defaultSearchEngineList = [[NSBundle mainBundle] pathForResource:@"SearchURLList" ofType:@"plist"];

  //
  // We haven't read the search engine list yet attempt to read from user's profile directory
  //
  nsCOMPtr<nsIFile> aDir;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aDir));
  if (aDir) {
    nsCAutoString aDirPath;
    nsresult rv = aDir->GetNativePath(aDirPath);
    if (NS_SUCCEEDED(rv)) {
      NSString *profileDir = [NSString stringWithUTF8String:aDirPath.get()];  

      //
      // If the file exists we read it from the profile directory
      // If it doesn't we attempt to copy it there from our app bundle first
      // (so that the user has something to edit in future)
      //
      NSString *searchEngineListPath    = [profileDir stringByAppendingPathComponent:@"SearchURLList.plist"];
      NSFileManager *fileManager = [NSFileManager defaultManager];
      if ( [fileManager isReadableFileAtPath:searchEngineListPath] ||
           [fileManager copyPath:defaultSearchEngineList toPath:searchEngineListPath handler:nil] )
        searchURLDictionary = [[NSDictionary alloc] initWithContentsOfFile:searchEngineListPath];
      else {
#if DEBUG
        NSLog(@"Unable to copy search engine list to user profile directory");
#endif
      }
    }
    else {
#if DEBUG
      NSLog(@"Unable to get profile directory");
#endif
    }
  }
  else {
#if DEBUG
    NSLog(@"Unable to determine profile directory");
#endif
  }
  
  //
  // If reading from the profile directory failed for any reason
  // then read the default search engine list from our application bundle directly
  //
  if (!searchURLDictionary) 
    searchURLDictionary = [[NSDictionary alloc] initWithContentsOfFile:defaultSearchEngineList];
  
  return searchURLDictionary;
}


- (void) focusChangedFrom:(NSResponder*) oldResponder to:(NSResponder*) newResponder
{
  BOOL oldResponderIsGecko = [self isResponderGeckoView:oldResponder];
  BOOL newResponderIsGecko = [self isResponderGeckoView:newResponder];

  if (oldResponderIsGecko != newResponderIsGecko && [[self window] isKeyWindow])
    [mBrowserView setBrowserActive:newResponderIsGecko];
}


- (PageProxyIcon *)proxyIconView
{
  return mProxyIcon;
}

// XXX this is only used to show bm after an import
- (BookmarkViewController *)bookmarkViewController
{
  return [self bookmarkViewControllerForCurrentTab];
}

- (CHBrowserView*)activeBrowserView
{
  return [mBrowserView getBrowserView];
}

- (id)windowWillReturnFieldEditor:(NSWindow *)aWindow toObject:(id)anObject
{
  if ([anObject isEqual:mURLBar]) {
    if (!mURLFieldEditor) {
      mURLFieldEditor = [[AutoCompleteTextFieldEditor alloc] initWithFrame:[anObject bounds]
                                                               defaultFont:[mURLBar font]];
      [mURLFieldEditor setFieldEditor:YES];
      [mURLFieldEditor setAllowsUndo:YES];
    }
    return mURLFieldEditor;
  }
  return nil;
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)sender
{
  if ([[self window] firstResponder] == mURLFieldEditor)
    return [mURLFieldEditor undoManagerForTextView:mURLFieldEditor];
  
  return [[BookmarkManager sharedBookmarkManager] undoManager];
}

- (IBAction)reloadWithNewCharset:(NSString*)charset
{
  [mBrowserView reloadWithNewCharset:charset];
}

- (NSString*)currentCharset
{
  return [mBrowserView currentCharset];
}

//
// -loadBookmarkBarIndex:
//
// Load the item in the bookmark bar given by |inIndex| using the given behavior.
// Uses the top-level |-loadBookmark:...| in order to get the right behavior with folders and
// tab groups.
//
- (BOOL)loadBookmarkBarIndex:(unsigned short)inIndex openBehavior:(EBookmarkOpenBehavior)inBehavior
{
  BookmarkItem* item = [[[BookmarkManager sharedBookmarkManager] toolbarFolder] objectAtIndex:inIndex];
  if (item)
    [[NSApp delegate] loadBookmark:item withWindowController:self openBehavior:inBehavior];
  return YES;
}

//
// - handleCommandReturn:
//
// handle command-return in location or search field, opening a new tab or window as appropriate
//
- (BOOL)handleCommandReturn:(BOOL)aShiftIsDown
{
  // determine whether to load in background
  BOOL loadInBG  = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
  if (aShiftIsDown)  // if shift is being held down, do the opposite of the pref
    loadInBG = !loadInBG;

  // determine whether to load in tab or window
  BOOL loadInTab = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL];

  BWCOpenDest destination = loadInTab ? kDestinationNewTab : kDestinationNewWindow;
  
  // see if command-return came in the url bar
  BOOL handled = NO;
  if ([mURLBar fieldEditor] && [[self window] firstResponder] == [mURLBar fieldEditor]) {
    handled = YES;
    [self goToLocationFromToolbarURLField:mURLBar inView:destination inBackground:loadInBG];
    // kill any autocomplete that was in progress
    [mURLBar revertText];
    // set the text in the URL bar back to the current URL
    [self updateLocationFields:[mBrowserView getCurrentURI] ignoreTyping:YES];
    
  // see if command-return came in the search field
  } else if ([mSearchBar isFirstResponder]) {
    handled = YES;
    [self performSearch:mSearchBar inView:destination inBackground:loadInBG]; 
  }
  return handled;
}

//
// -fillForm:
//
// Fills the form on the current web page using wallet
//
- (IBAction)fillForm:(id)sender
{
  CHBrowserView* browser = [[self getBrowserWrapper] getBrowserView];
  nsCOMPtr<nsIDOMWindow> domWindow = [browser getContentWindow];
  nsCOMPtr<nsIDOMWindowInternal> internalDomWindow (do_QueryInterface(domWindow));
  
  Wallet_Prefill(internalDomWindow);
}

@end

#pragma mark -

@implementation ThrobberHandler

-(id)initWithToolbarItem:(NSToolbarItem*)inButton images:(NSArray*)inImages
{
  if ( (self = [super init]) ) {
    mImages = [inImages retain];
    mTimer = [[NSTimer scheduledTimerWithTimeInterval: 0.2
                        target: self selector: @selector(pulseThrobber:)
                        userInfo: inButton repeats: YES] retain];
    mFrame = 0;
    [self startThrobber];
  }
  return self;
}

-(void)dealloc
{
  [self stopThrobber];
  [mImages release];
  [super dealloc];
}


// Called by an NSTimer.

- (void)pulseThrobber:(id)aSender
{
  // advance to next frame.
  if (++mFrame >= [mImages count])
      mFrame = 0;
  NSToolbarItem* toolbarItem = (NSToolbarItem*) [aSender userInfo];
  [toolbarItem setImage: [mImages objectAtIndex: mFrame]];
}

#define QUICKTIME_THROBBER 0

#if QUICKTIME_THROBBER

#include <QuickTime/QuickTime.h>

static Boolean movieControllerFilter(MovieController mc, short action, void *params, long refCon)
{
    if (action == mcActionMovieClick || action == mcActionMouseDown) {
        EventRecord* event = (EventRecord*) params;
        event->what = nullEvent;
        return true;
    }
    return false;
}
#endif

- (void)startThrobber
{
#if QUICKTIME_THROBBER
    // Use Quicktime to draw the frames from a single Animated GIF. This works fine for the animation, but
    // when the frames stop, the poster frame disappears.
    NSToolbarItem* throbberItem = [self throbberItem];
    if (throbberItem != nil && [throbberItem view] == nil) {
        NSSize minSize = [throbberItem minSize];
        NSLog(@"Origin minSize = %f X %f", minSize.width, minSize.height);
        NSSize maxSize = [throbberItem maxSize];
        NSLog(@"Origin maxSize = %f X %f", maxSize.width, maxSize.height);
        
        NSURL* throbberURL = [NSURL fileURLWithPath: [[NSBundle mainBundle] pathForResource:@"throbber" ofType:@"gif"]];
        NSLog(@"throbberURL = %@", throbberURL);
        NSMovie* throbberMovie = [[[NSMovie alloc] initWithURL: throbberURL byReference: YES] autorelease];
        NSLog(@"throbberMovie = %@", throbberMovie);
        
        if ([throbberMovie QTMovie] != nil) {
            NSMovieView* throbberView = [[[NSMovieView alloc] init] autorelease];
            [throbberView setMovie: throbberMovie];
            [throbberView showController: NO adjustingSize: NO];
            [throbberView setLoopMode: NSQTMovieLoopingPlayback];
            [throbberItem setView: throbberView];
            NSSize size = NSMakeSize(32, 32);
            [throbberItem setMinSize: size];
            [throbberItem setMaxSize: size];
            [throbberView gotoPosterFrame: self];
            [throbberView start: self];
    
            // experiment, veto mouse clicks in the movie controller by using an action filter.
            MCSetActionFilterWithRefCon((MovieController) [throbberView movieController],
                                        NewMCActionFilterWithRefConUPP(movieControllerFilter),
                                        0);
        }
    }
#endif
}

- (void)stopThrobber
{
#if QUICKTIME_THROBBER
    // Stop the quicktime animation.
    NSToolbarItem* throbberItem = [self throbberItem];
    if (throbberItem != nil) {
        NSMovieView* throbberView = [throbberItem view];
        if ([throbberView isPlaying]) {
            [throbberView stop: self];
            [throbberView gotoPosterFrame: self];
        } else {
            [throbberView start: self];
        }
    }
#else
  if (mTimer) {
    [mTimer invalidate];
    [mTimer release];
    mTimer = nil;

    mFrame = 0;
  }
#endif
}

@end

#pragma mark -

//
// TabBarVisiblePrefChangedCallback
//
// Pref callback to tell us when the pref values for the visibility of the tab
// view with just one tab open.
//
int TabBarVisiblePrefChangedCallback(const char* inPref, void* inBWC)
{
  if (strcmp(inPref, gTabBarVisiblePref) == 0) {
    BOOL newValue = [[PreferenceManager sharedInstance] getBooleanPref:gTabBarVisiblePref withSuccess:nil];
    BrowserWindowController* bwc = (BrowserWindowController*)inBWC;
    [[bwc getTabBrowser] setBarAlwaysVisible:newValue];
  }
  return NS_OK;
}


