/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


/*
 * MotifBrowserControlCanvasStub.cpp
 */

// PENDING(mark): I suppose this is where I need to go into my explaination of why
// this file is needed...

#include <jni.h>
#include "MotifBrowserControlCanvas.h"

#include "NativeLoaderStub.h"

#include <dlfcn.h>

extern void locateBrowserControlStubFunctions(void *);

jint (* createTopLevelWindow) (JNIEnv *, jobject);
jint (* createContainerWindow) (JNIEnv *, jobject, jint, jint, jint);
jint (* getGTKWinID) (JNIEnv *, jobject, jint);
void (* reparentWindow) (JNIEnv *, jobject, jint, jint);
void (* processEvents) (JNIEnv *, jobject);
void (* setGTKWindowSize) (JNIEnv *, jobject, jint, jint, jint);
jint (* getHandleToPeer) (JNIEnv *, jobject);

void locateMotifBrowserControlStubFunctions(void * dll) {
  createTopLevelWindow = (jint (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_createTopLevelWindow");
 if (!createTopLevelWindow) {
    printf("got dlsym error %s\n", dlerror());
  }

 getHandleToPeer = (jint (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_getHandleToPeer");
 if (!getHandleToPeer) {
    printf("got dlsym error %s\n", dlerror());
  }

  createContainerWindow = (jint (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_createContainerWindow");
 if (!createContainerWindow) {
    printf("got dlsym error %s\n", dlerror());
  }

  reparentWindow = (void (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_reparentWindow");
 if (!reparentWindow) {
    printf("got dlsym error %s\n", dlerror());
  }

  processEvents = (void (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_processEvents");
 if (!processEvents) {
    printf("got dlsym error %s\n", dlerror());
  }

  setGTKWindowSize = (void (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_setGTKWindowSize");
 if (!setGTKWindowSize) {
    printf("got dlsym error %s\n", dlerror());
  }

  getGTKWinID = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_getGTKWinID");
 if (!getGTKWinID) {
    printf("got dlsym error %s\n", dlerror());
  }
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    createTopLevelWindow
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_createTopLevelWindow (JNIEnv * env, jobject obj) {
  return (* createTopLevelWindow) (env, obj);
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    getHandleToPeer
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_getHandleToPeer (JNIEnv * env, jobject obj) {
  return (* getHandleToPeer) (env, obj);
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    createContainerWindow
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_createContainerWindow (JNIEnv * env, jobject obj, jint parent, jint width, jint height) {
  return (* createContainerWindow) (env, obj, parent, width, height);
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    getGTKWinID
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_getGTKWinID
(JNIEnv * env, jobject obj, jint gtkWinPtr) {
  return (* getGTKWinID) (env, obj, gtkWinPtr);
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    reparentWindow
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_reparentWindow
(JNIEnv * env, jobject obj, jint childID, jint parentID) {
  (* reparentWindow) (env, obj, childID, parentID);
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    processEvents
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_processEvents
    (JNIEnv * env, jobject obj) {
    (* processEvents) (env, obj);
}


/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    setGTKWindowSize
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_setGTKWindowSize
    (JNIEnv * env, jobject obj, jint xwinID, jint width, jint height) {
    (* setGTKWindowSize) (env, obj, xwinID, width, height);
}



/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    loadMainDll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_motif_MotifBrowserControlCanvas_loadMainDll
  (JNIEnv *, jclass)
{
    loadMainDll();
}
