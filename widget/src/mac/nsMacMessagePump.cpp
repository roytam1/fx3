/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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

// 
// nsMacMessagePump
//
// This file contains the default implementation for the mac event loop. Events that
// pertain to the layout engine are routed there via a MessageSink that is passed in
// at creation time. Events not destined for layout are handled here (such as window
// moved).
//
// Clients may either use this implementation or write their own. Embedding applications
// will almost certainly write their own because they will want control of the event
// loop to do other processing. There is nothing in the architecture which forces the
// embedding app to use anything called a "message pump" so the event loop can actually
// live anywhere the app wants.
//

#include "nsMacMessagePump.h"
#include "nsWidgetsCID.h"
#include "nsToolkit.h"
#include "nscore.h"

#include "nsRepeater.h"

#include "nsIServiceManager.h"

#include "prthread.h"
#include "nsMacTSMMessagePump.h"
#include "nsIRollupListener.h"
#include "nsIWidget.h"
#include "nsGfxUtils.h"
#include "nsMacWindow.h"

#include <MacWindows.h>
#include <ToolUtils.h>
#ifndef XP_MACOSX
#include <DiskInit.h>
#endif
#include <Devices.h>
#include <LowMem.h>
#include <Sound.h>
#include <Quickdraw.h>
#include "nsCarbonHelpers.h"

#include "nsIEventSink.h"
#include "nsPIWidgetMac.h"
#include "nsPIEventSinkStandalone.h"

//#include "nsISocketTransportService.h"
//include "nsIFileTransportService.h"

#if !XP_MACOSX
#include "MenuSharing.h"
#endif

#ifndef topLeft
#define topLeft(r)  (((Point *) &(r))[0])
#endif

#ifndef botRight
#define botRight(r) (((Point *) &(r))[1])
#endif

#if DEBUG && !defined(XP_MACOSX)
#include <SIOUX.h>
#include "macstdlibextras.h"
#endif

#define DRAW_ON_RESIZE  0   // if 1, enable live-resize except when the command key is down

const short kMinWindowWidth = 125;
const short kMinWindowHeight = 150;

extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;



//======================================================================================
//                  PROFILE
//======================================================================================
#ifdef DEBUG

// Important Notes:
// ----------------
//
//  - To turn the profiler on, define "#pragma profile on" in IDE_Options.h
//    then set $PROFILE to 1 in BuildNGLayoutDebug.pl and recompile everything.
//
//  - You may need to turn the profiler off ("#pragma profile off")
//    in NSPR.Debug.Prefix because of incompatiblity with NSPR threads.
//    It usually isn't a problem but it may be one when profiling things like
//    imap or network i/o.
//
//  - The profiler utilities (ProfilerUtils.c) and the profiler
//    shared library (ProfilerLib) sit in NSRuntime.mcp.
//

    // Define this if you want to start profiling when the Caps Lock
    // key is pressed. Press Caps Lock, start the command you want to
    // profile, release Caps Lock when the command is done. It works
    // for all the major commands: display a page, open a window, etc...
    //
    // If you want to profile the project, you must make sure that the
    // global prefix file (IDE_Options.h) contains "#pragma profile on".

//#define PROFILE


    // Define this if you want to let the profiler run while you're
    // spending time in other apps. Usually you don't.

//#define PROFILE_WAITNEXTEVENT


#ifdef PROFILE
#include "ProfilerUtils.h"
#endif //PROFILE
#endif //DEBUG



#pragma mark -
#if !XP_MACOSX
#pragma mark MenuSharingToolkitSupport
//=================================================================
static pascal void ErrorDialog (Str255 s)
{
  //ParamText (s, "\p", "\p", "\p"); 
  //Alert (kMenuSharingAlertID, nil);
}
	
//=================================================================
static pascal void EventFilter (EventRecord *ev)
{
  // Hrm, prolly should do _something_ here
}
#pragma mark -
#endif


//=================================================================
/*  Constructor
 *  @update  dc 08/31/98
 *  @param   aToolkit -- The toolkit created by the application
 *  @return  NONE
 */
nsMacMessagePump::nsMacMessagePump(nsToolkit *aToolkit)
  : mToolkit(aToolkit), mTSMMessagePump(NULL)
{
  mRunning = PR_FALSE;
  mMouseRgn = ::NewRgn();

  NS_ASSERTION(mToolkit, "No toolkit");
  
  //
  // create the TSM Message Pump
  //
  mTSMMessagePump = nsMacTSMMessagePump::GetSingleton();
  NS_ASSERTION(mTSMMessagePump!=NULL,"nsMacMessagePump::nsMacMessagePump: Unable to create TSM Message Pump.");

#if !XP_MACOSX
  // added to support Menu Sharing API.  Initializes the Menu Sharing API.
  InitSharedMenus (ErrorDialog, EventFilter);
#endif

  // To handle middle-button clicks we must use Carbon Events
  const EventTypeSpec eventTypes[] = {
    { kEventClassMouse, kEventMouseDown },
    { kEventClassMouse, kEventMouseUp }
  };

  EventHandlerUPP handlerUPP =
                     ::NewEventHandlerUPP(nsMacMessagePump::CarbonMouseHandler);
  ::InstallApplicationEventHandler(handlerUPP, GetEventTypeCount(eventTypes),
                                   eventTypes, (void*)this, NULL);
}

//=================================================================
/*  Destructor
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  NONE
 */
nsMacMessagePump::~nsMacMessagePump()
{
  if (mMouseRgn)
    ::DisposeRgn(mMouseRgn);

  //�TODO? release the toolkits and sinks? not if we use COM_auto_ptr.
  
  //
  // release the TSM Message Pump
  //
}


//=================================================================
/*  Return the frontmost window that is not the SIOUX console
 */
WindowPtr nsMacMessagePump::GetFrontApplicationWindow()
{
  WindowPtr firstAppWindow = ::FrontWindow();

#if DEBUG && !defined(XP_MACOSX)
  if (IsSIOUXWindow(firstAppWindow))
    firstAppWindow = ::GetNextWindow(firstAppWindow);
#endif

  return firstAppWindow;
}


//=================================================================
/*  Runs the message pump for the macintosh
 *  @update  dc 08/31/98
 *  @param   NONE
 */
void nsMacMessagePump::DoMessagePump()
{
  PRBool        haveEvent;
  EventRecord     theEvent;

  nsToolkit::AppInForeground();
  
  while (mRunning)
  {     
#ifdef PROFILE 
    if (KeyDown(0x39))  // press [caps lock] to start the profile
      ProfileStart();
    else
      ProfileStop(); 
#endif // PROFILE 

#ifdef PROFILE 
#ifndef PROFILE_WAITNEXTEVENT 
    ProfileSuspend(); 
#endif // PROFILE_WAITNEXTEVENT 
#endif // PROFILE 

    haveEvent = GetEvent(theEvent, PR_TRUE);

#ifdef PROFILE 
#ifndef PROFILE_WAITNEXTEVENT 
    ProfileResume(); 
#endif // PROFILE_WAITNEXTEVENT 
#endif // PROFILE 

    DispatchEvent(haveEvent, &theEvent);
  }
}

//static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
//static NS_DEFINE_CID(kFileTransportServiceCID,   NS_FILETRANSPORTSERVICE_CID);

//#define BUSINESS_INDICATOR
#define kIdleHysterysisTicks    60

//=================================================================
/* Return TRUE if the program is busy, like doing network stuff.
 *
*/
PRBool nsMacMessagePump::BrowserIsBusy()
{ 
  // we avoid frenetic oscillations between busy and idle states by not going
  // idle for a few ticks after we've detected idleness. This was added after
  // observing rapid busy/idle oscillations during chrome loading.
  static PRUint32 sNextIdleTicks = 0;
  PRBool  foundActivity = PR_TRUE;

  do    // convenience for breaking. We'll start by assuming that we're busy.
  {
  /*
   * Don't count connections for now; mailnews holds server connections open,
   * and that causes us to behave as busy even when we're not, which eats CPU
   * time and makes machines run hot.
    
    nsCOMPtr<nsISocketTransportService> socketTransport = do_GetService(kSocketTransportServiceCID);
    if (socketTransport)
    {
      PRUint32 inUseTransports;
      socketTransport->GetInUseTransportCount(&inUseTransports);
      if (inUseTransports > 0)
        break;
    }

    nsCOMPtr<nsIFileTransportService> fileTransport = do_GetService(kFileTransportServiceCID);
    if (fileTransport)
    {
      PRUint32 inUseTransports;
      fileTransport->GetInUseTransportCount(&inUseTransports);
      if (inUseTransports > 0)
        break;
    }
  */
    if (mToolkit->ToolkitBusy())
      break;

    foundActivity = PR_FALSE;

  } while(0);

  PRBool  isBusy = foundActivity;

  if (foundActivity)
    sNextIdleTicks = ::TickCount() + kIdleHysterysisTicks;
  else if (::TickCount() < sNextIdleTicks)
    isBusy = PR_TRUE;

#ifdef BUSINESS_INDICATOR
  static PRBool wasBusy;
  if (isBusy != wasBusy)
  {
    printf("*** Message pump became %s at %ld (next idle %ld)\n", isBusy ? "busy" : "idle", ::TickCount(), sNextIdleTicks);      
    wasBusy = isBusy;
  }
#endif

  return isBusy;
}

//=================================================================
/*  Fetch a single event
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  A boolean which states whether we have a real event
 */
PRBool nsMacMessagePump::GetEvent(EventRecord &theEvent, PRBool mayWait)
{
  PRBool inForeground = nsToolkit::IsAppInForeground();
  PRBool buttonDown   = ::Button();
  
  // when in the background, we don't want to chew up time processing mouse move
  // events, so set the mouse region to null. If we're in the fg, however,
  // we want every mouseMove event we can get our grubby little hands on, so set
  // it to a 1x1 rect.
  RgnHandle mouseRgn = inForeground ? mMouseRgn : nsnull;

  // if the mouse is down, and we're in the foreground, then eat
  // CPU so we respond quickly when scrolling etc.
  SInt32 sleepTime = (!mayWait || (buttonDown && inForeground)) ? 0 : 5;
  
  ::SetEventMask(everyEvent); // we need keyUp events
  PRBool haveEvent = ::WaitNextEvent(everyEvent, &theEvent, sleepTime, mouseRgn);
  
  return haveEvent;
}

//=================================================================
/*  Dispatch a single event
 *  @param   theEvent - the event to dispatch
 *  @return  A boolean which states whether we handled the event
 */
PRBool nsMacMessagePump::DispatchEvent(PRBool aRealEvent, EventRecord *anEvent)
{
  PRBool handled = PR_FALSE;
  
  if (aRealEvent == PR_TRUE)
  {

#if DEBUG && !defined(XP_MACOSX)
    if ((anEvent->what != kHighLevelEvent) && SIOUXHandleOneEvent(anEvent))
      return PR_TRUE;
#endif

    switch(anEvent->what)
    {
      case keyUp:
      case keyDown:
      case autoKey:
        handled = DoKey(*anEvent);
        break;

      case mouseDown:
        handled = DoMouseDown(*anEvent);
        break;

      case mouseUp:
        handled = DoMouseUp(*anEvent);
        break;

      case updateEvt:
        handled = DoUpdate(*anEvent);
        break;

      case activateEvt:
        handled = DoActivate(*anEvent);
        break;

      case diskEvt:
        handled = DoDisk(*anEvent);
        break;

      case osEvt:
        {
        unsigned char eventType = ((anEvent->message >> 24) & 0x00ff);
        switch (eventType)
        {
          case suspendResumeMessage:
            if ( anEvent->message & resumeFlag )
              nsToolkit::AppInForeground();   // resume message
            else
              nsToolkit::AppInBackground();   // suspend message

            handled = DoMouseMove(*anEvent);
            break;

          case mouseMovedMessage:
            handled = DoMouseMove(*anEvent);
            break;
        }
        }
        break;
      
      case kHighLevelEvent:
        ::AEProcessAppleEvent(anEvent);
        handled = PR_TRUE;
        break;

    }
  }
  else
  {
    DoIdle(*anEvent);

    if (mRunning)
      Repeater::DoIdlers(*anEvent);

    // yield to other threads
    ::PR_Sleep(PR_INTERVAL_NO_WAIT);
    handled = PR_FALSE;
  }

  if (mRunning)
    Repeater::DoRepeaters(*anEvent);

  NS_ASSERTION(ValidateDrawingState(), "Bad drawing state");

  return handled;
}

#pragma mark -
//-------------------------------------------------------------------------
//
// DoUpdate
//
//-------------------------------------------------------------------------
//#include "ProfilerUtils.h"
PRBool nsMacMessagePump::DoUpdate(EventRecord &anEvent)
{
  WindowPtr whichWindow = reinterpret_cast<WindowPtr>(anEvent.message);
  
  StPortSetter portSetter(whichWindow);
  
  ::BeginUpdate(whichWindow);
  // The app can do its own updates here
  DispatchOSEventToRaptor(anEvent, whichWindow);
  ::EndUpdate(whichWindow);
  return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// DoMouseDown
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoMouseDown(EventRecord &anEvent)
{
  WindowPtr     whichWindow;
  WindowPartCode        partCode;
  PRBool  handled = PR_FALSE;
  
  partCode = ::FindWindow(anEvent.where, &whichWindow);
  
  switch (partCode)
  {
      case inNoWindow:
        break;

      case inCollapseBox:   // we never seem to get this.
      case inSysWindow:
        if ( gRollupListener && gRollupWidget )
          gRollupListener->Rollup();
        break;

      case inMenuBar:
      {
        // If a xul popup is displayed, roll it up and don't allow the click
        // through to the menu code. This is how MacOS context menus work, so
        // I think this is a valid solution.
        if ( gRollupListener && gRollupWidget )
        {
          gRollupListener->Rollup();
        }
        else
        {
          long menuResult = ::MenuSelect(anEvent.where);
          handled = PR_TRUE;
#if USE_MENUSELECT
          if (HiWord(menuResult) != 0)
          {
            menuResult = ConvertOSMenuResultToPPMenuResult(menuResult);
            handled = DoMenu(anEvent, menuResult);
          }
#endif
        }
        
        break;
      }

      case inContent:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
        if ( IsWindowHilited(whichWindow) || (gRollupListener && gRollupWidget) )
          handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        else {
          nsCOMPtr<nsIWidget> topWidget;
          nsToolkit::GetTopWidget ( whichWindow, getter_AddRefs(topWidget) );
          nsCOMPtr<nsPIWidgetMac> macWindow ( do_QueryInterface(topWidget) );
          if ( macWindow ) {
            // a click occurred in a background window. Use WaitMouseMove() to determine if
            // it was a click or a drag. If it was a drag, send a drag gesture to the
            // background window. We don't need to rely on the ESM to track the gesture,
            // the OS has just told us.  If it was a click, bring it to the front like normal.
            Boolean initiateDragFromBGWindow = ::WaitMouseMoved(anEvent.where);
            if ( initiateDragFromBGWindow ) {
              nsCOMPtr<nsIEventSink> sink ( do_QueryInterface(topWidget) );
              if ( sink ) {
                // dispach a mousedown, an update event to paint any changes,
                // then the drag gesture event
                PRBool handled = PR_FALSE;
                sink->DispatchEvent ( &anEvent, &handled );
                
                EventRecord updateEvent = anEvent;
                updateEvent.what = updateEvt;
                updateEvent.message = NS_REINTERPRET_CAST(UInt32, whichWindow);
                sink->DispatchEvent ( &updateEvent, &handled );
                
                sink->DragEvent ( NS_DRAGDROP_GESTURE, anEvent.where.h, anEvent.where.v, 0L, &handled );                
              }
            }
            else {
              PRBool enabled;
              if (NS_SUCCEEDED(topWidget->IsEnabled(&enabled)) && !enabled)
                ::SysBeep(1);
              else
                macWindow->ComeToFront();
            }
            handled = PR_TRUE;
          }
        }
        break;
      }

      case inDrag:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);

        Point   oldTopLeft = {0, 0};
        ::LocalToGlobal(&oldTopLeft);
        
        // roll up popups BEFORE we start the drag
        if ( gRollupListener && gRollupWidget )
          gRollupListener->Rollup();

        Rect screenRect;
        ::GetRegionBounds(::GetGrayRgn(), &screenRect);
        ::DragWindow(whichWindow, anEvent.where, &screenRect);

        Point   newTopLeft = {0, 0};
        ::LocalToGlobal(&newTopLeft);

        // only activate if the command key is not down
        if (!(anEvent.modifiers & cmdKey))
        {
          nsCOMPtr<nsIWidget> topWidget;
          nsToolkit::GetTopWidget(whichWindow, getter_AddRefs(topWidget));
          
          nsCOMPtr<nsPIWidgetMac> macWindow ( do_QueryInterface(topWidget) );
          if ( macWindow )
            macWindow->ComeToFront();
        }
        
        // Dispatch the event because some windows may want to know that they have been moved.
        anEvent.where.h += newTopLeft.h - oldTopLeft.h;
        anEvent.where.v += newTopLeft.v - oldTopLeft.v;
        
        handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        break;
      }

      case inGrow:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);

        // use the cmd-key to do the opposite of the DRAW_ON_RESIZE setting.
        Boolean cmdKeyDown = (anEvent.modifiers & cmdKey) != 0;
        Boolean drawOnResize = DRAW_ON_RESIZE ? !cmdKeyDown : cmdKeyDown;
        if (drawOnResize)
        {
          Point oldPt = anEvent.where;
          while (::WaitMouseUp())
          {
            Repeater::DoRepeaters(anEvent);

            Point origin = {0,0};
            ::LocalToGlobal(&origin);
            Point newPt;
            ::GetMouse(&newPt);
            ::LocalToGlobal(&newPt);
            if (::DeltaPoint(oldPt, newPt))
            {
              Rect portRect;
              ::GetWindowPortBounds(whichWindow, &portRect);

              short width = newPt.h - origin.h;
              short height = newPt.v - origin.v;
              if (width < kMinWindowWidth)
                width = kMinWindowWidth;
              if (height < kMinWindowHeight)
                height = kMinWindowHeight;

              oldPt = newPt;
              ::SizeWindow(whichWindow, width, height, true);

                            // simulate a click in the grow icon
              anEvent.where.h = width - 8;    // on Aqua, clicking at (width, height) misses the grow icon. inset a bit.
              anEvent.where.v = height - 8;
              ::LocalToGlobal(&anEvent.where);
              DispatchOSEventToRaptor(anEvent, whichWindow);

              Boolean         haveEvent;
              EventRecord     updateEvent;
              haveEvent = ::WaitNextEvent(updateMask, &updateEvent, 0, nil);
              if (haveEvent)
                DoUpdate(updateEvent);
            }
          }
          handled = PR_TRUE;
        }
        else
        {
          Rect sizeRect;
          ::GetRegionBounds(::GetGrayRgn(), &sizeRect);

          /* rjc sez (paraphrased) the maximum window size could also reasonably be
             taken from the size of ::GetRegionBounds(::GetGrayRgn(),...) (not simply
             the region bounds, but its width and height) or perhaps the size of the
             screen to which the window mostly belongs. But why be so restrictive? */
          sizeRect.top = kMinWindowHeight;
          sizeRect.left = kMinWindowWidth;
          sizeRect.bottom = 0x7FFF;
          sizeRect.right = 0x7FFF;
          long newSize = ::GrowWindow(whichWindow, anEvent.where, &sizeRect);
          if (newSize != 0)
            ::SizeWindow(whichWindow, newSize & 0x0FFFF, (newSize >> 16) & 0x0FFFF, true);
          Rect portRect;
          Point newPt = botRight(*::GetWindowPortBounds(whichWindow, &portRect));
          ::LocalToGlobal(&newPt);
          newPt.h -= 8, newPt.v -= 8;
          anEvent.where = newPt;  // important!
          handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        }
        break;
      }

      case inGoAway:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
        if (::TrackGoAway(whichWindow, anEvent.where)) {
          handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        }
        break;
      }

      case inZoomIn:
      case inZoomOut:
        if (::TrackBox(whichWindow, anEvent.where, partCode))
        {
          if (partCode == inZoomOut)
          {
            nsCOMPtr<nsIWidget> topWidget;
            nsToolkit::GetTopWidget ( whichWindow, getter_AddRefs(topWidget) );
            nsCOMPtr<nsPIWidgetMac> macWindow ( do_QueryInterface(topWidget) );
            if ( macWindow )
              macWindow->CalculateAndSetZoomedSize();
          }
          // !!!  Do not call ZoomWindow before calling DispatchOSEventToRaptor
          //    otherwise nsMacEventHandler::HandleMouseDownEvent won't get
          //    the right partcode for the click location
          
          handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        }
        break;

      case inToolbarButton:           // Mac OS X only
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
        handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        break;

  }

  return handled;
}


//-------------------------------------------------------------------------
//
// DoMouseUp
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoMouseUp(EventRecord &anEvent)
{
    WindowPtr     whichWindow;
    PRInt16       partCode;

  partCode = ::FindWindow(anEvent.where, &whichWindow);
  if (whichWindow == nil)
  {
    // We need to report the event even when it happens over no window:
    // when the user clicks a widget, keeps the mouse button pressed and
    // releases it outside the window, the event needs to be reported to
    // the widget so that it can deactivate itself.
    whichWindow = GetFrontApplicationWindow();
  }
  
  PRBool handled = DispatchOSEventToRaptor(anEvent, whichWindow);
  // consume mouse ups in the title bar, since nsMacWindow doesn't do that for us
  if (partCode == inDrag)
    handled = PR_TRUE;
  return handled;
}

//-------------------------------------------------------------------------
//
// DoMouseMove
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoMouseMove(EventRecord &anEvent)
{
  // same thing as DoMouseUp
  WindowPtr     whichWindow;
  PRInt16       partCode;
  PRBool        handled = PR_FALSE;
  
  if (mMouseRgn)
  {
    Point globalMouse = anEvent.where;    
    ::SetRectRgn(mMouseRgn, globalMouse.h, globalMouse.v, globalMouse.h + 1, globalMouse.v + 1);
  }

  partCode = ::FindWindow(anEvent.where, &whichWindow);
  if (whichWindow == nil)
    whichWindow = GetFrontApplicationWindow();

  /* Disable mouse moved events for windowshaded windows -- this prevents tooltips
     from popping up in empty space.
  */
  if (whichWindow == nil || !::IsWindowCollapsed(whichWindow))
    handled = DispatchOSEventToRaptor(anEvent, whichWindow);
  return handled;
}


//-------------------------------------------------------------------------
//
// DoKey
// 
// This is called for keydown, keyup, and key repeating events. So we need
// to be careful not to do things twice.
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoKey(EventRecord &anEvent)
{
  PRBool handled = PR_FALSE;
  {
    handled = DispatchOSEventToRaptor(anEvent, GetFrontApplicationWindow());
#if USE_MENUSELECT
    /* we want to call this if cmdKey is pressed and no other modifier keys are pressed */
    if((!handled) && (anEvent.what == keyDown) && (anEvent.modifiers == cmdKey) )
    {
      // do a menu key command
      long menuResult = ::MenuKey(theChar);
      if (HiWord(menuResult) != 0)
      {
          menuResult = ConvertOSMenuResultToPPMenuResult(menuResult);
        DoMenu(anEvent, menuResult);
        handled = PR_TRUE;
      }
    }
#endif
  }
  return handled;
}


//-------------------------------------------------------------------------
//
// DoDisk
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoDisk(const EventRecord& anEvent)
{
  return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// DoMenu
//
//-------------------------------------------------------------------------
extern Boolean SIOUXIsAppWindow(WindowPtr window);

#if USE_MENUSELECT
PRBool nsMacMessagePump::DoMenu(EventRecord &anEvent, long menuResult)
{
  // The app can handle its menu commands here or
  // in the nsNativeBrowserWindow and nsNativeViewerApp
  
  extern const PRInt16 kAppleMenuID;  // Danger Will Robinson!!! - this currently requires
                  // APPLE_MENU_HACK to be defined in nsMenu.h
                  // One of these days it'll become a non-hack
                  // and things will be less convoluted

  // See if it was the Apple Menu
  if (HiWord(menuResult) == kAppleMenuID)
  {
    short theItem = LoWord(menuResult);
    if (theItem > 2)
    {
      Str255  daName;
      GrafPtr savePort;
      
      ::GetMenuItemText(::GetMenuHandle(kAppleMenuID), theItem, daName);
      ::GetPort(&savePort);
      ::OpenDeskAcc(daName);
      ::SetPort(savePort);
      HiliteMenu(0);
      return PR_TRUE;
    }
  }

  // Note that we still give Raptor a shot at the event as it will eventually
  // handle the About... selection
  PRBool handled = DispatchMenuCommandToRaptor(anEvent, menuResult);
  HiliteMenu(0);
  return handled;
}
#endif

//-------------------------------------------------------------------------
//
// DoActivate
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoActivate(EventRecord &anEvent)
{
  WindowPtr whichWindow = (WindowPtr)anEvent.message;
  nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
  if (anEvent.modifiers & activeFlag)
  {
    ::HiliteWindow(whichWindow,TRUE);
  }
  else
  {
    PRBool ignoreDeactivate = PR_FALSE;
    nsCOMPtr<nsIWidget> windowWidget;
    nsToolkit::GetTopWidget ( whichWindow, getter_AddRefs(windowWidget));
    if (windowWidget)
    {
      nsCOMPtr<nsPIWidgetMac> window ( do_QueryInterface(windowWidget) );
      if (window)
      {
        window->GetIgnoreDeactivate(&ignoreDeactivate);
        window->SetIgnoreDeactivate(PR_FALSE);
      }
    }
    if (!ignoreDeactivate)
      ::HiliteWindow(whichWindow,FALSE);
  }

  return DispatchOSEventToRaptor(anEvent, whichWindow);
}


//-------------------------------------------------------------------------
//
// DoIdle
//
//-------------------------------------------------------------------------
void  nsMacMessagePump::DoIdle(EventRecord &anEvent)
{
  // send mouseMove event
  static Point  lastWhere = {0, 0};

#if !XP_MACOSX
  if ( nsToolkit::IsAppInForeground() )
  {
    // Shared Menu support - note we hardcode first menu ID available as 31000
    CheckSharedMenus(31000);
  }
#endif    

  if (*(long*)&lastWhere == *(long*)&anEvent.where)
    return;

  EventRecord localEvent = anEvent;
  localEvent.what = nullEvent;
  lastWhere = localEvent.where;
  if ( nsToolkit::IsAppInForeground() )
    DoMouseMove(localEvent);
}


#pragma mark -



//-------------------------------------------------------------------------
//
// DispatchOSEventToRaptor
//
//-------------------------------------------------------------------------
PRBool  nsMacMessagePump::DispatchOSEventToRaptor(
                          EventRecord   &anEvent,
                          WindowPtr     aWindow)
{
  PRBool handled = PR_FALSE;
  nsCOMPtr<nsIEventSink> sink;
  nsToolkit::GetWindowEventSink ( aWindow, getter_AddRefs(sink) );
  if ( sink )
    sink->DispatchEvent ( &anEvent, &handled );
  return handled;
}


#if USE_MENUSELECT

//-------------------------------------------------------------------------
//
// DispatchMenuCommandToRaptor
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DispatchMenuCommandToRaptor(
                          EventRecord   &anEvent,
                          long          menuResult)
{
  PRBool    handled = PR_FALSE;
  WindowPtr theFrontWindow = GetFrontApplicationWindow();

  nsCOMPtr<nsIEventSink> sink;
  nsToolkit::GetWindowEventSink ( theFrontWindow, getter_AddRefs(sink) );
  nsCOMPtr<nsPIEventSinkStandalone> menuSink ( do_QueryInterface(sink) );
  if ( menuSink )
    menuSink->DispatchMenuEvent ( &anEvent, menuResult, &handled );
  return handled;
}

#endif // USE_MENUSELECT

//-------------------------------------------------------------------------
//
// CarbonMouseHandler
//
//-------------------------------------------------------------------------
pascal OSStatus nsMacMessagePump::CarbonMouseHandler(
                          EventHandlerCallRef nextHandler,
                          EventRef theEvent, void *userData)
{
  EventMouseButton button;
  OSErr err = ::GetEventParameter(theEvent, kEventParamMouseButton,
                                  typeMouseButton, NULL,
                                  sizeof(EventMouseButton), NULL, &button);
  if (err != noErr)
    return eventNotHandledErr;

  // Only handle middle click events here.  Let the rest fall through to
  // WaitNextEvent().
  if (button != kEventMouseButtonTertiary)
    return eventNotHandledErr;

  EventRecord theRecord;
  if (!::ConvertEventRefToEventRecord(theEvent, &theRecord)) {
    // This will return FALSE on a middle click event; that's to let us know
    // it's giving us a nullEvent, which is expected since Classic events
    // don't support the middle button normally.
    //
    // We know better, so let's restore the actual event kind.
    UInt32 kind = ::GetEventKind(theEvent);
    theRecord.what = (kind == kEventMouseDown) ? mouseDown : mouseUp;
  }

  // Classic mouse events don't record the button specifier. The message
  // parameter is unused in mouse click events, so let's stuff it there.
  // We'll pick it up in nsMacEventHandler::HandleMouseDownEvent().
  theRecord.message = (UInt32)button;

  // Process the modified event internally
  nsMacMessagePump *pump = (nsMacMessagePump*)userData;
  PRBool handled = pump->DispatchEvent(PR_TRUE, &theRecord);
  return handled ? noErr : eventNotHandledErr;
}

