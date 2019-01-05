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
 * The Original Code is a Cocoa widget run loop and event implementation.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Mentovai <mark@moxienet.com> (Original Author)
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

/*
 * Runs the main native Cocoa run loop, interrupting it as needed to process
 * Gecko events.
 */

#import <Cocoa/Cocoa.h>

#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"

// AppShellDelegate
//
// Cocoa bridge class.  An object of this class is used as an NSPort
// delegate called on the main thread when Gecko wants to interrupt
// the native run loop.
//
@interface AppShellDelegate : NSObject
- (void)handlePortMessage:(NSPortMessage*)aPortMessage;
@end

// nsAppShell implementation

nsAppShell::nsAppShell()
: mPort(nil)
, mDelegate(nil)
{
  // mMainPool sits low on the autorelease pool stack to serve as a catch-all
  // for autoreleased objects on this thread.  Because it won't be popped
  // until the appshell is destroyed, objects attached to this pool will
  // be leaked until app shutdown.  You probably don't want this!
  //
  // Objects autoreleased to this pool may result in warnings in the future.
  mMainPool = [[NSAutoreleasePool alloc] init];
}

nsAppShell::~nsAppShell()
{
  if (mPort) {
    [[NSRunLoop currentRunLoop] removePort:mPort forMode:NSDefaultRunLoopMode];
    [mPort release];
  }

  [mDelegate release];
  [mMainPool release];
}

// Init
//
// Loads the nib (see bug 316076c21) and sets up the NSPort used to
// interrupt the main Cocoa event loop.
//
// public
nsresult
nsAppShell::Init()
{
  // No event loop is running yet.  Avoid autoreleasing objects to
  // mMainPool.  The appshell retains objects it needs to be long-lived
  // and will release them as appropriate.
  NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];

  // Get the path of the nib file, which lives in the GRE location
  nsCOMPtr<nsIFile> nibFile;
  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(nibFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nibFile->AppendNative(NS_LITERAL_CSTRING("res"));
  nibFile->AppendNative(NS_LITERAL_CSTRING("MainMenu.nib"));

  nsCAutoString nibPath;
  rv = nibFile->GetNativePath(nibPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // This call initializes NSApplication.
  [NSBundle loadNibFile:
                     [NSString stringWithUTF8String:(const char*)nibPath.get()]
      externalNameTable:
           [NSDictionary dictionaryWithObject:[NSApplication sharedApplication]
                                       forKey:@"NSOwner"]
               withZone:NSDefaultMallocZone()];


  // A message will be sent through mPort to mDelegate on the main thread
  // to interrupt the run loop while it is running.
  mDelegate = [[AppShellDelegate alloc] init];
  NS_ENSURE_STATE(mDelegate);

  mPort = [[NSPort port] retain];
  NS_ENSURE_STATE(mPort);

  [mPort setDelegate:mDelegate];
  [[NSRunLoop currentRunLoop] addPort:mPort forMode:NSDefaultRunLoopMode];

  rv = nsBaseAppShell::Init();

  [localPool release];

  return rv;
}

// ProcessGeckoEvents
//
// Arrange for Gecko events to be processed.  They will either be processed
// after the main run loop returns (if we own the run loop) or on
// NativeEventCallback (if an embedder owns the loop).
//
// Called by -[AppShellDelegate handlePortMessage:] after mPort signals as a
// result of a ScheduleNativeEventCallback call.  This method is public only
// because it needs to be called by that Objective-C fragment, and C++ can't
// make |friend|s with Objective-C.
//
// public
void
nsAppShell::ProcessGeckoEvents()
{
  if (RunWasCalled()) {
    // We own the run loop.  Interrupt it.  It will be started again later
    // (unless exiting) by nsBaseAppShell.  Trust me, I'm a doctor.
    [NSApp stop:nil];

    // The run loop is sleeping.  [NSApp run] won't return until it's given
    // a reason to wake up.  Awaken it by posting a bogus event.
    // There's no need to make the event presentable (by setting a subtype,
    // for example) because we own the run loop.
    [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                        location:NSMakePoint(0,0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:-1
                                         context:NULL
                                         subtype:0
                                           data1:0
                                           data2:0]
             atStart:NO];
  }

  NativeEventCallback();
}

// ScheduleNativeEventCallback
//
// Called (possibly on a non-main thread) when Gecko has an event that
// needs to be processed.  The Gecko event needs to be processed on the
// main thread, so the native run loop must be interrupted.
//
// protected virtual
void
nsAppShell::ScheduleNativeEventCallback()
{
  NS_ADDREF(this);

  void* self = NS_STATIC_CAST(void*, this);
  NSData* data = [[NSData alloc] initWithBytes:&self length:sizeof(this)];
  NSArray* components = [[NSArray alloc] initWithObjects:&data count:1];

  // This will invoke [mDelegate handlePortMessage:message] on the main thread.

  NSPortMessage* message = [[NSPortMessage alloc] initWithSendPort:mPort
                                                       receivePort:nil
                                                        components:components];
  [message sendBeforeDate:[NSDate distantFuture]];

  [message release];
  [components release];
  [data release];
}

// ProcessNextNativeEvent
//
// If aMayWait is false, process a single native event.  If it is true, run
// the native run loop until stopped by ProcessGeckoEvents.
//
// Returns true if more events are waiting in the native event queue.
//
// protected virtual
PRBool
nsAppShell::ProcessNextNativeEvent(PRBool aMayWait)
{
  if (!aMayWait) {
    // Only process a single event.
    if (NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                            untilDate:nil
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES]) {
      // Handle the event on its own autorelease pool.
      // Ordinarily, each event gets a new pool when dispatched by
      // [NSApp run].
      NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];
      [NSApp sendEvent:event];
      [localPool release];
    }
  }
  else {
    // Run the run loop until interrupted by a stop: message.
    [NSApp run];
  }

  if ([NSApp nextEventMatchingMask:NSAnyEventMask
                         untilDate:nil
                            inMode:NSDefaultRunLoopMode
                           dequeue:NO]) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

// AppShellDelegate implementation

@implementation AppShellDelegate
// handlePortMessage:
//
// The selector called on the delegate object when nsAppShell::mPort is sent an
// NSPortMessage by ScheduleNativeEventCallback.  Call into the nsAppShell
// object for access to mRunWasCalled and NativeEventCallback.
//
- (void)handlePortMessage:(NSPortMessage*)aPortMessage
{
  NSData* data = [[aPortMessage components] objectAtIndex:0];
  nsAppShell* appShell = *NS_STATIC_CAST(nsAppShell* const*,[data bytes]);
  appShell->ProcessGeckoEvents();

  NS_RELEASE(appShell);
}
@end
