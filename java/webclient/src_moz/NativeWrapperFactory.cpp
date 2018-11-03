/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ann Sunhachawee
 */

#include "nsIEventQueueService.h" // for PLEventQueue
#include "nsIServiceManager.h" // for do_GetService
#include "nsEmbedAPI.h" // for NS_HandleEmbeddingEvent

#include "NativeWrapperFactory.h"

#include "ns_util.h"

#if defined(XP_UNIX) && !defined(XP_MACOSX)
#include <unistd.h>
#include "gdksuperwin.h"
#include "gtkmozarea.h"

extern "C" {
    static int	    wc_x_error	 (Display     *display, 
				  XErrorEvent *error);
}
#endif

PLEventQueue	*NativeWrapperFactory::sActionQueue        = nsnull;
PRThread	*NativeWrapperFactory::sEmbeddedThread     = nsnull;
PRBool           NativeWrapperFactory::sInitComplete       = PR_FALSE;

NativeWrapperFactory::NativeWrapperFactory(void)
{
    mEnv = nsnull;
    mNativeEventThread = nsnull;
}

NativeWrapperFactory::~NativeWrapperFactory()
{
    sProfile = nsnull;
    sProfileInternal = nsnull;
    sPrefs = nsnull;
    sAppShell = nsnull;
}

nsresult
NativeWrapperFactory::Init(JNIEnv * env, jobject newNativeEventThread)
{   
    mFailureCode = NS_ERROR_FAILURE;
    if (nsnull == sEmbeddedThread) {
	//
	// Do java communication initialization
	// 
	mEnv = env;
	// store the java NativeEventThread class
	mNativeEventThread = ::util_NewGlobalRef(env, 
						 newNativeEventThread);
	//
	// create the singleton event queue
	// 
	
	// create the static sActionQueue
        nsCOMPtr<nsIEventQueueService> 
            aEventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
        if (!aEventQService) {
            mFailureCode = NS_ERROR_FAILURE;
            return mFailureCode;
        }
        
        // Create the event queue.
        mFailureCode = aEventQService->CreateThreadEventQueue();
        sEmbeddedThread = PR_GetCurrentThread();
        
        if (!sEmbeddedThread) {
            mFailureCode = NS_ERROR_FAILURE;
            return mFailureCode;
        }
        
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("NativeBrowserControl_Init: Create UI Thread EventQueue: %d\n",
                mFailureCode));
        
        // We need to do something different for Unix
        nsIEventQueue * EQueue = nsnull;
        
        mFailureCode = aEventQService->GetThreadEventQueue(sEmbeddedThread, &EQueue);
        if (NS_FAILED(mFailureCode)) {
            return mFailureCode;
        }
        
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("NativeBrowserControl_Init: Get UI Thread EventQueue: %d\n",
                mFailureCode));
        
#ifdef XP_UNIX
        /************
        gdk_input_add(EQueue->GetEventQueueSelectFD(),
                      GDK_INPUT_READ,
                      event_processor_callback,
                      EQueue);
        ************/
#endif
        
        PLEventQueue * plEventQueue = nsnull;
        
        EQueue->GetPLEventQueue(&plEventQueue);
        sActionQueue = plEventQueue;
        if (!sActionQueue) {
            mFailureCode = NS_ERROR_FAILURE;
            return mFailureCode;
        }
        
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("NativeBrowserControl_Init: get ActionQueue: %d\n",
                mFailureCode));
        
#if defined(XP_UNIX) && !defined(XP_MACOSX)
        
        // The gdk_x_error function exits in some cases, we don't 
        // want that.
        XSetErrorHandler(wc_x_error);
#endif
        sInitComplete = PR_TRUE;
    }
    mFailureCode = NS_OK;
}


void
NativeWrapperFactory::Destroy(void)
{
    if (nsnull != mNativeEventThread) {
        ::util_DeleteGlobalRef(mEnv, mNativeEventThread);
    }
    
    // PENDING(edburns): take over the stuff from
    // WindowControlActionEvents
    // wsDeallocateInitContextEvent::handleEvent()
    
    // This flag might have been set from
    // EmbedWindow::DestroyBrowserWindow() as well if someone used a
    // window.close() or something or some other script action to close
    // the window.  No harm setting it again.
}

PRUint32 
NativeWrapperFactory::ProcessEventLoop(void)
{
    if (PR_GetCurrentThread() != sEmbeddedThread) {
        return 0;
    }
    
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    while(gtk_events_pending()) {
        gtk_main_iteration();
    }
#elif !defined(XP_MACOSX)
    MSG msg;
    PRBool wasHandled;
    
    if (::PeekMessage(&msg, nsnull, 0, 0, PM_NOREMOVE)) {
        if (::GetMessage(&msg, nsnull, 0, 0)) {
            wasHandled = PR_FALSE;
            ::NS_HandleEmbeddingEvent(msg, wasHandled);
            if (!wasHandled) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
    }
#endif
    ::PR_Sleep(PR_INTERVAL_NO_WAIT);
    
    if (sInitComplete && sActionQueue) {
        PLEvent * event = nsnull;
        
        PL_ENTER_EVENT_QUEUE_MONITOR(sActionQueue);
        if (::PL_EventAvailable(sActionQueue)) {
            event = ::PL_GetEvent(sActionQueue);
        }
        PL_EXIT_EVENT_QUEUE_MONITOR(sActionQueue);
        if (event != nsnull) {
            ::PL_HandleEvent(event);
        }
    }

    // PENDING(edburns): revisit this.  Not sure why this is necessary, but
    // this fixes bug 44327
    //    printf("%c", 8); // 8 is ASCII for backspace

    return 1;
}

nsresult
NativeWrapperFactory::GetFailureCode(void)
{
    return mFailureCode;
}

/* static */
PRBool
NativeWrapperFactory::IsInitialized(void)
{
    return sInitComplete;
}

#if defined(XP_UNIX) && !defined(XP_MACOSX)
static int
wc_x_error (Display	 *display,
            XErrorEvent *error)
{
    if (error->error_code)
        {
            char buf[64];
            
            XGetErrorText (display, error->error_code, buf, 63);
            
            fprintf (stderr, "Webclient-Gdk-ERROR **: %s\n  serial %ld error_code %d request_code %d minor_code %d\n",
                     buf, 
                     error->serial, 
                     error->error_code, 
                     error->request_code,
                     error->minor_code);
        }
    
    return 0;
}
#endif
