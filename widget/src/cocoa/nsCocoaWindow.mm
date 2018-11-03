/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
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

#include "nsCocoaWindow.h"

#include "nsIServiceManager.h"    // for drag and drop
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIDragSessionMac.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsGUIEvent.h"
#include "nsMacResources.h"
#include "nsIRollupListener.h"
#import  "nsChildView.h"

#include "nsIEventQueueService.h"

// Define Class IDs -- i hate having to do this
static NS_DEFINE_CID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

// externs defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

NS_IMPL_ISUPPORTS_INHERITED0(nsCocoaWindow, Inherited)


/*
 * Gecko rects (nsRect) contain an origin (x,y) in a coordinate
 * system with (0,0) in the top-left of the screen. Cocoa rects
 * (NSRect) contain an origin (x,y) in a coordinate system with
 * (0,0) in the bottom-left of the screen. Both nsRect and NSRect
 * contain width/height info, with no difference in their use.
 */
static NSRect geckoRectToCocoaRect(const nsRect &geckoRect)
{
  // first we get the highest point on all screens
  float highestScreenPoint = 0.0;
  NSArray* allScreens = [NSScreen screens];
  for (unsigned int i = 0; i < [allScreens count]; i++) {
    NSRect currScreenFrame = [[allScreens objectAtIndex:i] frame];
    float currScreenHighestPoint = currScreenFrame.origin.y + currScreenFrame.size.height;
    if (currScreenHighestPoint > highestScreenPoint)
      highestScreenPoint = currScreenHighestPoint;
  }

  // We only need to change the Y coordinate by starting with the screen
  // height, subtracting the gecko Y coordinate, and subtracting the
  // height.
  return NSMakeRect(geckoRect.x,
                    highestScreenPoint - geckoRect.y - geckoRect.height,
                    geckoRect.width,
                    geckoRect.height);
}


//
// nsCocoaWindow constructor
//

nsCocoaWindow::nsCocoaWindow()
: mParent(nsnull)
, mWindow(nil)
, mIsSheet(PR_FALSE)
, mIsResizing(PR_FALSE)
, mWindowMadeHere(PR_FALSE)
, mVisible(PR_FALSE)
{

}


//-------------------------------------------------------------------------
//
// nsCocoaWindow destructor
//
//-------------------------------------------------------------------------
nsCocoaWindow::~nsCocoaWindow()
{
  if (mWindow && mWindowMadeHere) {
    [mWindow autorelease];
    [mDelegate autorelease];
  }
}


//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult nsCocoaWindow::StandardCreate(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeWindow)
{
  Inherited::BaseCreate(aParent, aRect, aHandleEventFunction, aContext, aAppShell,
                        aToolkit, aInitData);
  
  mParent = aParent;
  
  // create a window if we aren't given one, always create if this should be a popup
  if (!aNativeWindow || (aInitData && aInitData->mWindowType == eWindowType_popup)) {
    // decide on a window type
    PRBool allOrDefault = PR_FALSE;
    if (aInitData) {
      allOrDefault = aInitData->mBorderStyle == eBorderStyle_all ||
                     aInitData->mBorderStyle == eBorderStyle_default;
      mWindowType = aInitData->mWindowType;
      // if a toplevel window was requested without a titlebar, use a dialog
      if (mWindowType == eWindowType_toplevel &&
          (aInitData->mBorderStyle == eBorderStyle_none ||
           !allOrDefault &&
           !(aInitData->mBorderStyle & eBorderStyle_title)))
        mWindowType = eWindowType_dialog;
    }
    else {
      allOrDefault = PR_TRUE;
      mWindowType = eWindowType_toplevel;
    }
    
#ifdef MOZ_MACBROWSER
    if (mWindowType == eWindowType_popup)
      return NS_OK;
#endif
    
    // we default to NSBorderlessWindowMask, add features if needed
    unsigned int features = NSBorderlessWindowMask;
    
    // Configure the window we will create based on the window type
    switch (mWindowType)
    {
      case eWindowType_child:
        // In Carbon, we made this a window of type kPlainWindowClass.
        // I think that is pretty much equiv to NSBorderlessWindowMask.
        break;
      case eWindowType_dialog:
        if (aInitData) {
          // we never give dialogs a close button
          switch (aInitData->mBorderStyle)
          {
            case eBorderStyle_none:
              break;
            case eBorderStyle_default:
              features |= NSTitledWindowMask;
              break;
            case eBorderStyle_all:
              features |= NSTitledWindowMask;
              features |= NSResizableWindowMask;
              features |= NSMiniaturizableWindowMask;
              break;
            default:
              if (aInitData->mBorderStyle & eBorderStyle_title) {
                features |= NSTitledWindowMask;
                features |= NSMiniaturizableWindowMask;
              }
              if (aInitData->mBorderStyle & eBorderStyle_resizeh) {
                features |= NSResizableWindowMask;
              }
              break;
          }
        }
        else {
          features |= NSTitledWindowMask;
          features |= NSMiniaturizableWindowMask;
        }
        break;
      case eWindowType_sheet:
        if (aInitData) {
          nsWindowType parentType;
          aParent->GetWindowType(parentType);
          if (parentType != eWindowType_invisible) {
            // at this point we make the window a sheet
            mIsSheet = PR_TRUE;
            if (aInitData->mBorderStyle & eBorderStyle_resizeh) {
              features = NSResizableWindowMask;
            }
          }
          else {
            features = NSMiniaturizableWindowMask;
          }
        }
        else {
          features = NSMiniaturizableWindowMask;
        }
        break;
      case eWindowType_popup:
        features |= NSBorderlessWindowMask;
        break;
      case eWindowType_toplevel:
        features |= NSTitledWindowMask;
        features |= NSMiniaturizableWindowMask;
        if (allOrDefault || aInitData->mBorderStyle & eBorderStyle_close)
          features |= NSClosableWindowMask;
        if (allOrDefault || aInitData->mBorderStyle & eBorderStyle_resizeh)
          features |= NSResizableWindowMask;
        break;
      case eWindowType_invisible:
        break;
      default:
        NS_ERROR("Unhandled window type!");
        return NS_ERROR_FAILURE;
    }
    
    /* 
     * We pass a content area rect to initialize the native Cocoa window. The
     * content rect we give is the same size as the size we're given by gecko.
     * The origin we're given for non-popup windows is moved down by the height
     * of the menu bar so that an origin of (0,100) from gecko puts the window
     * 100 pixels below the top of the available desktop area. We also move the
     * origin down by the height of a title bar if it exists. This is so the
     * origin that gecko gives us for the top-left of  the window turns out to
     * be the top-left of the window we create. This is how it was done in
     * Carbon. If it ought to be different we'll probably need to look at all
     * the callers.
     *
     * Note: This means that if you put a secondary screen on top of your main
     * screen and open a window in the top screen, it'll be incorrectly shifted
     * down by the height of the menu bar. Same thing would happen in Carbon.
     *
     * Note: If you pass a rect with 0,0 for an origin, the window ends up in a
     * weird place for some reason. This stops that without breaking popups.
     */
    NSRect rect = geckoRectToCocoaRect(aRect);
    
    // compensate for difference between frame and content area height (e.g. title bar)
    NSRect newWindowFrame = [NSWindow frameRectForContentRect:rect styleMask:features];

    rect.origin.y -= (newWindowFrame.size.height - rect.size.height);
    
    if (mWindowType != eWindowType_popup)
      rect.origin.y -= ::GetMBarHeight();
    
#ifdef DEBUG
    NSLog(@"Top-level window being created at Cocoa rect: %f, %f, %f, %f\n",
          rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
#endif
    
    // create the window
    mWindow = [[NSWindow alloc] initWithContentRect:rect styleMask:features 
                                backing:NSBackingStoreBuffered defer:NO];
    
    if (mWindowType == eWindowType_popup) {
        [mWindow setLevel:NSPopUpMenuWindowLevel];
        [mWindow setHasShadow:YES];
    }

    [mWindow setReleasedWhenClosed:NO];

    // register for mouse-moved events. The default is to ignore them for perf reasons.
    [mWindow setAcceptsMouseMovedEvents:YES];
    
    // setup our notification delegate. Note that setDelegate: does NOT retain.
    mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
    [mWindow setDelegate:mDelegate];
    
    mWindowMadeHere = PR_TRUE;
  }
  else {
    mWindow = (NSWindow*)aNativeWindow;
    mVisible = PR_TRUE;
  }
  
  return NS_OK;
}


//
// Create a nsCocoaWindow using a native window provided by the application
//
NS_IMETHODIMP nsCocoaWindow::Create(nsNativeWidget aNativeWindow,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(nsnull, aRect, aHandleEventFunction,
                          aContext, aAppShell, aToolkit, aInitData,
                            aNativeWindow));
}


NS_IMETHODIMP nsCocoaWindow::Create(nsIWidget* aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(aParent, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData, nsnull));
}


void*
nsCocoaWindow::GetNativeData(PRUint32 aDataType)
{
  void* retVal = nsnull;
  
  switch (aDataType) {
    // to emulate how windows works, we always have to return a NSView
    // for NS_NATIVE_WIDGET
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_DISPLAY:
      retVal = [mWindow contentView];
      break;
      
    case NS_NATIVE_WINDOW:  
      retVal = mWindow;
      break;
      
    case NS_NATIVE_GRAPHIC: // quickdraw port of top view (for now)
      retVal = [[mWindow contentView] qdPort];
      break;
  }

  return retVal;
}

NS_IMETHODIMP
nsCocoaWindow::IsVisible(PRBool & aState)
{
  aState = mVisible;
  return NS_OK;
}
   
//-------------------------------------------------------------------------
//
// Hide or show this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::Show(PRBool bState)
{
    if (bState)
        [mWindow orderFront:NULL];
    else
        [mWindow orderOut:NULL];
    
    mVisible = bState;

    return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::Enable(PRBool aState)
{
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::IsEnabled(PRBool *aState)
{
  if (aState)
    *aState = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::ConstrainPosition(PRBool aAllowSlop,
                                               PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::Move(PRInt32 aX, PRInt32 aY)
{  
  if (mWindow) {  
    // if we're a popup, we have to convert from our parent widget's coord
    // system to the global coord system first because the (x,y) we're given
    // is in its coordinate system.
    if (mWindowType == eWindowType_popup) {
      nsRect localRect, globalRect; 
      localRect.x = aX;
      localRect.y = aY;  
      if (mParent) {
        mParent->WidgetToScreen(localRect,globalRect);
        aX=globalRect.x;
        aY=globalRect.y;
     }
    }
    
    NSPoint coord = {aX, aY};
 
    // the point we have assumes that the screen origin is the top-left. Well,
    // it's not. Use the screen object to convert.
    //FIXME -- doesn't work on monitors other than primary
    NSRect screenRect = [[NSScreen mainScreen] frame];
    coord.y = (screenRect.origin.y + screenRect.size.height) - coord.y;
    //printf("final coords %f %f\n", coord.x, coord.y);
    //printf("- window coords before %f %f\n", [mWindow frame].origin.x, [mWindow frame].origin.y);
    [mWindow setFrameTopLeftPoint:coord];
    //printf("- window coords after %f %f\n", [mWindow frame].origin.x, [mWindow frame].origin.y);
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsCocoaWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                     nsIWidget *aWidget, PRBool aActivate)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// zoom/restore
//
//-------------------------------------------------------------------------
NS_METHOD nsCocoaWindow::SetSizeMode(PRInt32 aMode)
{
  return NS_OK;
}


void nsCocoaWindow::CalculateAndSetZoomedSize()
{
  
} // CalculateAndSetZoomedSize


//-------------------------------------------------------------------------
//
// Resize this window to a point given in global (screen) coordinates. This
// differs from simple Move(): that method makes JavaScript place windows
// like other browsers: it puts the top-left corner of the outer edge of the
// window border at the given coordinates, offset from the menubar.
// MoveToGlobalPoint expects the top-left corner of the portrect, which
// is inside the border, and is not offset by the menubar height.
//
//-------------------------------------------------------------------------
void nsCocoaWindow::MoveToGlobalPoint(PRInt32 aX, PRInt32 aY)
{

}


NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX, aY);
  Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}


//
// Resize this window
//
NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  if (mWindow) {
    NSRect newBounds = [mWindow frame];
    newBounds.size.width = aWidth;
    if (mWindowType == eWindowType_popup)
      newBounds.size.height = aHeight;
    else
      newBounds.size.height = aHeight + kTitleBarHeight;     // add height of title bar
    StartResizing();
    [mWindow setFrame:newBounds display:NO];
    StopResizing();
  }

  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  
  // tell gecko to update all the child widgets
  ReportSizeEvent();
  
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::GetScreenBounds(nsRect &aRect)
{
  return NS_OK;
}


PRBool nsCocoaWindow::OnPaint(nsPaintEvent &event)
{
  return PR_TRUE; // don't dispatch the update event
}

//
// Set this window's title
//
NS_IMETHODIMP nsCocoaWindow::SetTitle(const nsAString& aTitle)
{
  const nsString& strTitle = PromiseFlatString(aTitle);
  NSString* title = [NSString stringWithCharacters:strTitle.get() length:strTitle.Length()];
  [mWindow setTitle:title];

  return NS_OK;
}


//-------------------------------------------------------------------------
// Pass notification of some drag event to Gecko
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
//-------------------------------------------------------------------------
PRBool nsCocoaWindow::DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers )
{
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Like ::BringToFront, but constrains the window to its z-level
//
//-------------------------------------------------------------------------
void nsCocoaWindow::ComeToFront()
{

}


NS_IMETHODIMP nsCocoaWindow::ResetInputState()
{
// return mMacEventHandler->ResetInputState();
  return NS_OK;
}


void nsCocoaWindow::SetIsActive(PRBool aActive)
{
// mIsActive = aActive;
}


void nsCocoaWindow::IsActive(PRBool* aActive)
{
// *aActive = mIsActive;
}


//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------
NS_IMETHODIMP 
nsCocoaWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsIWidget* aWidget = event->widget;
  NS_IF_ADDREF(aWidget);
  
  if (nsnull != mMenuListener){
    if(NS_MENU_EVENT == event->eventStructType)
      aStatus = mMenuListener->MenuSelected(static_cast<nsMenuEvent&>(*event));
  }
  if (mEventCallback)
    aStatus = (*mEventCallback)(event);

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eConsumeNoDefault) && (mEventListener != nsnull))
    aStatus = mEventListener->ProcessEvent(*event);

  NS_IF_RELEASE(aWidget);

  return NS_OK;
}


void
nsCocoaWindow::ReportSizeEvent()
{
  // nsEvent
  nsSizeEvent sizeEvent(PR_TRUE, NS_SIZE, this);
  sizeEvent.time = PR_IntervalNow();

  // nsSizeEvent
  sizeEvent.windowSize = &mBounds;
  sizeEvent.mWinWidth  = mBounds.width;
  sizeEvent.mWinHeight = mBounds.height;
  
  // dispatch event
  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&sizeEvent, status);
}


NS_IMETHODIMP nsCocoaWindow::SetMenuBar(nsIMenuBar *aMenuBar)
{
  if (mMenuBar)
    mMenuBar->SetParent(nsnull);
  mMenuBar = aMenuBar;
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::ShowMenuBar(PRBool aShow)
{
  return NS_ERROR_FAILURE;
}


nsIMenuBar* nsCocoaWindow::GetMenuBar()
{
  return mMenuBar;
}


NS_IMETHODIMP nsCocoaWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                                 PRBool aDoCapture, 
                                                 PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
  } else {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
  }
  
  return NS_OK;
}


@implementation WindowDelegate


- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind
{
  [super init];
  mGeckoWindow = geckoWind;
  return self;
}


- (void)windowDidResize:(NSNotification *)aNotification
{
  if (mGeckoWindow->IsResizing())
    return;
  
  // must remember to give Gecko top-left, not straight cocoa origin
  // and that Gecko already compensates for the title bar, so we have to
  // strip it out here.
  NSRect frameRect = [[aNotification object] frame];
  mGeckoWindow->Resize (NS_STATIC_CAST(PRInt32,frameRect.size.width),
                        NS_STATIC_CAST(PRInt32,frameRect.size.height - nsCocoaWindow::kTitleBarHeight), PR_TRUE);
}


- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
  nsIMenuBar* myMenuBar = mGeckoWindow->GetMenuBar();
  if (myMenuBar)
    myMenuBar->Paint();
  
  nsGUIEvent guiEvent(PR_TRUE, NS_GOTFOCUS, mGeckoWindow);
  guiEvent.time = PR_IntervalNow();
  nsEventStatus status = nsEventStatus_eIgnore;
  mGeckoWindow->DispatchEvent(&guiEvent, status);
}


- (void)windowDidResignMain:(NSNotification *)aNotification
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
  
  // tell Gecko that we lost focus
  nsGUIEvent guiEvent(PR_TRUE, NS_LOSTFOCUS, mGeckoWindow);
  guiEvent.time = PR_IntervalNow();
  nsEventStatus status = nsEventStatus_eIgnore;
  mGeckoWindow->DispatchEvent(&guiEvent, status);
}


- (void)windowWillMove:(NSNotification *)aNotification
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
}


- (BOOL)windowShouldClose:(id)sender
{
  // We only want to send NS_XUL_CLOSE and let gecko close the window
  nsGUIEvent guiEvent(PR_TRUE, NS_XUL_CLOSE, mGeckoWindow);
  guiEvent.time = PR_IntervalNow();
  nsEventStatus status = nsEventStatus_eIgnore;
  mGeckoWindow->DispatchEvent(&guiEvent, status);
  return NO; // gecko will do it
}


-(void)windowWillClose:(NSNotification *)aNotification
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
}


- (void)windowWillMiniaturize:(NSNotification *)aNotification
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
}


@end
