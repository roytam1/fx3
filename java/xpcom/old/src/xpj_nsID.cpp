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
 * The Initial Developer of the Original Code is Frank
 * Mitchell. Portions created by Frank Mitchell are
 * Copyright (C) 1999 Frank Mitchell. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
#include <iostream.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <jni.h>
#include "nscore.h" 
#include "nsID.h" 
#include "nsIAllocator.h"

#include "xpjava.h"

#ifdef INCLUDE_JNI_HEADER
#include "org_mozilla_xpcom_nsID.h"
#endif

#define ID_CLASS_NAME "org/mozilla/xpcom/nsID"
#define ID_FIELD_NAME "nsidptr"
#define ID_FIELD_TYPE "J"

/* Because not all platforms convert jlong to "long long" 
 *
 * NOTE: this code was cut&pasted from xpj_XPCMethod.cpp, with tweaks.
 *   Normally I wouldn't do this, but my reasons are:
 *
 *   1. My alternatives were to put it in xpjava.h or xpjava.cpp
 *      I'd like to take stuff *out* of xpjava.h, and putting it 
 *      in xpjava.cpp would preclude inlining.
 *
 *   2. How we represent nsIDs in Java is an implementation 
 *      detail, which may change in the future (e.g. placing the 
 *      whole value in the Java obj, not just a pointer to heap 
 *      memory).  Thus ToPtr/ToJLong is only of interest to those 
 *      objects that stuff pointers into jlongs.
 *
 *   -- frankm, 99.09.09
 */
static inline jlong ToJLong(const void *p) {
    jlong result;
    jlong_I2L(result, (int)p);
    return result;
}

static inline void* ToPtr(jlong l) {
    int result;
    jlong_L2I(result, l);
    return (void *)result;
}

#undef DEBUG_GETSET_ID

#ifdef DEBUG_GETSET_ID
static void GetClassName(char *namebuf, JNIEnv *env, jobject self, int len) {
    jclass clazz = env->GetObjectClass(self);
    jclass clazz_clazz = env->GetObjectClass(clazz);
    jmethodID nameID = env->GetMethodID(clazz_clazz, "getName", "()Ljava/lang/String;");
    jstring string = (jstring)env->CallObjectMethod(clazz, nameID);

    jsize jstrlen = env->GetStringUTFLength(string);
    const char *utf = env->GetStringUTFChars(string, NULL);

    if (jstrlen >= len) {
	jstrlen = len;
    }
    strncpy(namebuf, utf, jstrlen);
    namebuf[jstrlen] = '\0';

    env->ReleaseStringUTFChars(string, utf);
}
#endif

/********************** ID **************************/

jobject ID_NewJavaID(JNIEnv *env, const nsIID* iid) {
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jmethodID initID = env->GetMethodID(clazz, "<init>", "()V");

    jobject result = env->NewObject(clazz, initID);
    nsID *idptr = (nsID *)nsAllocator::Alloc(sizeof(nsID));

    memcpy(idptr, iid, sizeof(nsID));
    ID_SetNative(env, result, idptr);

    return result; 
}

nsID *ID_GetNative(JNIEnv *env, jobject self) {
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jfieldID nsidptrID = env->GetFieldID(clazz, ID_FIELD_NAME, ID_FIELD_TYPE);

#ifdef DEBUG_GETSET_ID
    char classname[128];

    GetClassName(classname, env, self, sizeof(classname));
    fprintf(stderr, "ID_GetNative: self instanceof %s\n", classname);
    fflush(stderr);
#endif

    assert(env->IsInstanceOf(self, clazz));

    jlong nsidptr = env->GetLongField(self, nsidptrID);

    return (nsID *)ToPtr(nsidptr);
}

void ID_SetNative(JNIEnv *env, jobject self, nsID *id) {
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jfieldID nsidptrID = env->GetFieldID(clazz, ID_FIELD_NAME, ID_FIELD_TYPE);

#ifdef DEBUG_GETSET_ID
    char classname[128];

    GetClassName(classname, env, self, sizeof(classname));
    fprintf(stderr, "ID_SetNative: self instanceof %s\n", classname);	    
    fflush(stderr);
#endif

    assert(env->IsInstanceOf(self, clazz));

    jlong nsidptr = ToJLong(id);

    env->SetLongField(self, nsidptrID, nsidptr);
}

jboolean ID_IsEqual(JNIEnv *env, jobject self, jobject other) {
    jboolean result = JNI_FALSE;
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jfieldID nsidptrID = env->GetFieldID(clazz, ID_FIELD_NAME, ID_FIELD_TYPE);

#ifdef DEBUG_GETSET_ID
    char classname[128];

    GetClassName(classname, env, self, sizeof(classname));
    fprintf(stderr, "ID_IsEqual: self instanceof %s\n", classname);	    

    GetClassName(classname, env, other, sizeof(classname));
    fprintf(stderr, "ID_IsEqual: other instanceof %s\n", classname);	    
    fflush(stderr);
#endif

    assert(env->IsInstanceOf(self, clazz));

    if (other != NULL && env->IsInstanceOf(other, clazz)) {
	nsID *selfid = (nsID *)ToPtr(env->GetLongField(self, nsidptrID));
	nsID *otherid = (nsID *)ToPtr(env->GetLongField(other, nsidptrID));

	if (selfid != NULL && otherid != NULL) {
	    result = selfid->Equals(*otherid);
	}
    }
    return result;
}

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     ID
 * Method:    NewIDPtr
 * Signature: (ISSBBBBBBBB)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_nsID_NewIDPtr__ISSBBBBBBBB
  (JNIEnv *env, jobject self, jint m0, jshort m1, jshort m2, 
   jbyte m30, jbyte m31, jbyte m32, jbyte m33, 
   jbyte m34, jbyte m35, jbyte m36, jbyte m37) {

    nsID *idptr = (nsID *)nsAllocator::Alloc(sizeof(nsID));
    idptr->m0 = m0;
    idptr->m1 = m1;
    idptr->m2 = m2;
    idptr->m3[0] = m30;
    idptr->m3[1] = m31;
    idptr->m3[2] = m32;
    idptr->m3[3] = m33;
    idptr->m3[4] = m34;
    idptr->m3[5] = m35;
    idptr->m3[6] = m36;
    idptr->m3[7] = m37;

    ID_SetNative(env, self, idptr);
}

/*
 * Class:     ID
 * Method:    NewIDPtr
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_nsID_NewIDPtr__Ljava_lang_String_2
    (JNIEnv *env, jobject self, jstring string) {

    nsID *idptr = (nsID *)nsAllocator::Alloc(sizeof(nsID));

    jboolean isCopy;
    const char *utf = env->GetStringUTFChars(string, &isCopy);
    char *aIDStr;

    if (isCopy) {
	aIDStr = (char *)utf;
    }
    else {
	jsize len = env->GetStringUTFLength(string);
	aIDStr = (char *)nsAllocator::Alloc(len * sizeof(char));
	strncpy(aIDStr, utf, len);
	aIDStr[len - 1] = 0;
    }

    if (!(idptr->Parse(aIDStr))) {
	nsAllocator::Free(idptr);
	idptr = NULL;
    }

    ID_SetNative(env, self, idptr);

    if (!isCopy) {
	nsAllocator::Free(aIDStr);
    }

    env->ReleaseStringUTFChars(string, utf);
}

/*
 * Class:     ID
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_nsID_finalize(JNIEnv *env, jobject self) {
    nsID *idptr = ID_GetNative(env, self);

    nsAllocator::Free(idptr);
}

/*
 * Class:     ID
 * Method:    equals
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_xpcom_nsID_equals(JNIEnv *env, jobject self, jobject other) {
    return ID_IsEqual(env, self, other);
}

/*
 * Class:     ID
 * Method:    toString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_xpcom_nsID_toString(JNIEnv *env, jobject self) {

    nsID *idptr = ID_GetNative(env, self);

    char *idstr = idptr->ToString();
    
    jstring result = env->NewStringUTF(idstr);

    delete [] idstr;

    return result;
}

/*
 * Class:     ID
 * Method:    hashCode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_xpcom_nsID_hashCode(JNIEnv *env, jobject self) {
    jint result;

    PRUint32 *intptr = (PRUint32 *)ID_GetNative(env, self);

    result = intptr[0] ^ intptr[1] ^ intptr[2] ^ intptr[3];

    return result;
}

#ifdef __cplusplus
}
#endif

