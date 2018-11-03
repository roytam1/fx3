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
 * Contributor(s): Kyle Yuan <kyle.yuan@sun.com>
 */

#include "nsIWebBrowserChrome.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowser.h"
#include "WindowCreator.h"

#include "NativeBrowserControl.h"
#include "EmbedEventListener.h"
#include "EmbedWindow.h"

NativeBrowserControl* gNewWindowNativeBCPtr;

NS_IMPL_ISUPPORTS2(WindowCreator, nsIWindowCreator, nsIWindowCreator2)

WindowCreator::WindowCreator(NativeBrowserControl *yourNativeBCPtr)
{
    mNativeBCPtr = yourNativeBCPtr;
    mTarget = 0;
}

WindowCreator::~WindowCreator()
{
}

NS_IMETHODIMP WindowCreator::AddNewWindowListener(jobject target)
{
    if (! mTarget)
        mTarget = target;

    return NS_OK;
}

NS_IMETHODIMP
WindowCreator::CreateChromeWindow(nsIWebBrowserChrome *parent,
                                  PRUint32 chromeFlags,
                                  nsIWebBrowserChrome **_retval)
{
    if (!mTarget)
        return NS_OK;

    gNewWindowNativeBCPtr = nsnull;

    /********
        
    util_SendEventToJava(mNativeBCPtr->env,
                         mNativeBCPtr->nativeEventThread,
                         mTarget,
                         NEW_WINDOW_LISTENER_CLASSNAME,
                         chromeFlags,
                         0);

    // check gNewWindowNativeBCPtr to see if the initialization had completed
    while (!gNewWindowNativeBCPtr) {
        processEventLoop(mNativeBCPtr);
        ::PR_Sleep(PR_INTERVAL_NO_WAIT);
    }

    nsCOMPtr<nsIWebBrowserChrome> webChrome(do_QueryInterface(gNewWindowNativeBCPtr->browserContainer));
    *_retval = webChrome;
    NS_IF_ADDREF(*_retval);
    printf ("RET=%x\n", *_retval);
    *************/
    return NS_OK;
}

NS_IMETHODIMP
WindowCreator::CreateChromeWindow2(nsIWebBrowserChrome *parent, 
                                   PRUint32 chromeFlags, 
                                   PRUint32 contextFlags, 
                                   nsIURI *uri, PRBool *cancel, 
                                   nsIWebBrowserChrome **_retval)
{
    nsresult rv = NS_OK;
    PRBool hasNewWindowListener = PR_FALSE;
    NativeBrowserControl *newNativeBrowserControl = nsnull;
    jint newNativeBCPtr = -1;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    rv = mNativeBCPtr->GetNewWindowListenerAttached(&hasNewWindowListener);
    if (NS_FAILED(rv) || !hasNewWindowListener) {
        return rv;
    }

    nsCOMPtr<nsIWebBrowser> webBrowser;
    
    parent->GetWebBrowser(getter_AddRefs(webBrowser));
    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(webBrowser));

    if (nsnull != baseWindow) {
        jobject eventRegistration = nsnull;
        rv = mNativeBCPtr->mEventListener->GetEventRegistration(&eventRegistration);
        if (NS_FAILED(rv) || !eventRegistration) {
            return rv;
        }
        
        // send this event to allow the user to create the new BrowserControl
        newNativeBCPtr = util_SendEventToJava(nsnull,
                                              eventRegistration,
                                              NEW_WINDOW_LISTENER_CLASSNAME,
                                              chromeFlags, nsnull);
        newNativeBrowserControl = (NativeBrowserControl *) newNativeBCPtr;
        PR_ASSERT(nsnull != newNativeBrowserControl);

        nsCOMPtr<nsIWebBrowserChrome> webChrome(newNativeBrowserControl->mWindow);
        *_retval = webChrome;
        NS_IF_ADDREF(*_retval);
    }
    
    return NS_OK;
}
