/* 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIPluginManager.h"
#include "org_mozilla_pluglet_mozilla_PlugletManagerImpl.h"
#include "PlugletLog.h"
  
static jfieldID peerFID = NULL;
PRLogModuleInfo* PlugletLog::log = NULL;


/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    reloadPluglets
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_reloadPluglets
    (JNIEnv *env, jobject jthis, jboolean jparam) {
     nsIPluginManager * manager = (nsIPluginManager*)env->GetLongField(jthis, peerFID);
     if (!manager) {
 	 PR_LOG(PlugletLog::log, PR_LOG_ERROR,
		 ("PlugletManagerImpl.reloadPluglets: ERROR, manager = NULL"));
     }
     PRBool param = (jparam == JNI_TRUE) ? PR_TRUE : PR_FALSE;
     PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletManagerImpl.reloadPluglets: param = %i\n", param));
     manager->ReloadPlugins(param);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    userAgent
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_userAgent
    (JNIEnv *env, jobject jthis) {
    nsIPluginManager * manager = (nsIPluginManager*)env->GetLongField(jthis, peerFID);
    if (!manager) {
	PR_LOG(PlugletLog::log, PR_LOG_ERROR,
		("PlugletManagerImpl.userAgent: ERROR, manager = NULL"));
    }
    const char * res = NULL;
    if (NS_FAILED(manager->UserAgent(&res)) 
	|| !res) {
        return NULL;
    } else {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletManagerImpl.userAgent: result = %s\n", res));
        return  env->NewStringUTF(res);
    }
}    

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    getURL
 * Signature: (Lorg/mozilla/pluglet/Pluglet;Ljava/net/URL;Ljava/lang/String;Lorg/mozilla/pluglet/PlugletStreamListener;Ljava/lang/String;Ljava/net/URL;Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_getURL
    (JNIEnv *env, jobject jthis, jobject, jobject, jstring, jobject, jstring, jobject, jboolean) {
    nsIPluginManager * manager = (nsIPluginManager*)env->GetLongField(jthis, peerFID);
    if (manager) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletManagerImpl.getURL: manager = %s\n", manager));
    }
    //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    postURL
 * Signature: (Lorg/mozilla/pluglet/Pluglet;Ljava/net/URL;I[BZLjava/lang/String;Lorg/mozilla/pluglet/PlugletStreamListener;Ljava/lang/String;Ljava/net/URL;ZI[B)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_postURL
    (JNIEnv *env, jobject jthis, jobject, jobject, jint, jbyteArray, jboolean, jstring, jobject, jstring, jobject, jboolean, jint, jbyteArray) {
    nsIPluginManager * manager = (nsIPluginManager*)env->GetLongField(jthis, peerFID);
    if (manager) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletManagerImpl.postURL: manager = %s\n", manager));
    }
    //nb
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_nativeFinalize
    (JNIEnv *env, jobject jthis) {
    nsIPluginManager * manager = (nsIPluginManager*)env->GetLongField(jthis, peerFID);
    if(manager) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletManagerImpl.nativeFinalize: manager = %s\n", manager));
	NS_RELEASE(manager);
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletManagerImpl
 * Method:    nativeInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletManagerImpl_nativeInitialize
    (JNIEnv *env, jobject jthis) {
    PlugletLog::log = PR_NewLogModule("pluglets");
    if(!peerFID) {
	peerFID = env->GetFieldID(env->GetObjectClass(jthis),"peer","J");
	if (!peerFID) {
	    return;
	}
    }
    nsIPluginManager * manager = (nsIPluginManager*)env->GetLongField(jthis, peerFID);
    if (manager) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletManagerImpl.nativeInitialize: manager = %p\n", manager));
	manager->AddRef();
    }
}






