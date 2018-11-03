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
#include "nsIPluginStreamInfo.h"
#include "org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl.h"
#include "ByteRanges.h"
#include "PlugletLog.h"

static jfieldID peerFID = NULL;
/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    getContentType
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_getContentType
    (JNIEnv *env, jobject jthis) {
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);
    if (env->ExceptionOccurred()) {
	return NULL;
    }
    nsMIMEType mimeType;
    if(NS_FAILED(streamInfo->GetContentType(&mimeType))  /* nb do we need to free this memory?  */
		 || !mimeType) {
	return NULL;
    } else {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletStreamInfoImpl.getContentType: mime type is %s\n", (char *)mimeType));
	return env->NewStringUTF((char *)mimeType);
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    isSeekable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_isSeekable
    (JNIEnv *env, jobject jthis) {
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);;
    if (env->ExceptionOccurred()) {
	return JNI_FALSE; // it does not matter
    }
    PRBool res;
    streamInfo->IsSeekable(&res);
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamInfoImpl.isSeakable: res = %i\n", res));
    if (res == PR_TRUE) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }

}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    getLength
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_getLength
    (JNIEnv *env, jobject jthis) {
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);
    if (env->ExceptionOccurred()) {
	return 0; // it does not matter
    }
    PRUint32 length;
    nsresult rv = streamInfo->GetLength(&length);
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamInfoImpl.getLength: length = %i\n", length));
    if (NS_FAILED(rv)) {
        return -1;
    }
    return length;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    getLastModified
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_getLastModified
    (JNIEnv *env, jobject jthis) {
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);;
    if (env->ExceptionOccurred()) {
	return 0; // it does not matter
    }
    PRUint32 res = 0;
    streamInfo->GetLastModified(&res);
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamInfoImpl.getLastModified: res = %i\n", res));
    return res;
    
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    getURL
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_getURL
    (JNIEnv *env, jobject jthis) {
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);
    if (env->ExceptionOccurred()) {
	return 0; // it does not matter
    }
    const char *res = NULL;
    streamInfo->GetURL(&res); /* do we need to free this memory */
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamInfoImpl.getURL: url = %s\n", res));
    return (res)? env->NewStringUTF(res) : NULL;
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    requestRead
 * Signature: (Lorg/mozilla/pluglet/mozilla/ByteRanges;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_requestRead
    (JNIEnv *env, jobject jthis, jobject object) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	   ("PlugletStreamInfoImpl.requestRead\n"));
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);
    if (!streamInfo 
	|| !object ) { //nb we need to add some Exception throwing
	return;
    }
    nsByteRange* range = ByteRanges::GetByteRanges(env,object);

    streamInfo->RequestRead(range);
    ByteRanges::FreeByteRanges(range);
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_nativeFinalize
    (JNIEnv *env, jobject jthis) {
    /* nb do as in java-dom  stuff */
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);
    if (streamInfo) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletStreamInfoImpl.nativeFinalize: streamInfo = %p\n", streamInfo));
	NS_RELEASE(streamInfo);
    }
}

/*
 * Class:     org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl
 * Method:    nativeInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_pluglet_mozilla_PlugletStreamInfoImpl_nativeInitialize
    (JNIEnv *env, jobject jthis) {
    if(!peerFID) {
	peerFID = env->GetFieldID(env->GetObjectClass(jthis),"peer","J");
	if (!peerFID) {
	    return;
	}
    }
    nsIPluginStreamInfo * streamInfo = (nsIPluginStreamInfo*)env->GetLongField(jthis, peerFID);;
    if (streamInfo) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		("PlugletStreamInfoImpl.nativeInitialize: streamInfo = %p\n", streamInfo));
	streamInfo->AddRef();
    }
}

