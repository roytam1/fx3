/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Igor Kushnirskiy <idk@eng.sun.com>
 * Denis Sharypov <sdv@sparc.spb.su>
 */
#include "nsISupports.h"
#include "org_mozilla_xpcom_Utilities.h"
#include "bcIORB.h"
#include "bcICall.h"
#include "bcDefs.h"
#include "xptcall.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "bcJavaMarshalToolkit.h"
#include "ctype.h"
#include "bcJavaGlobal.h"


static jclass XPCOMExceptionClass = NULL;
static jmethodID XPCOMExceptionInitMID = NULL;
/*
 * Class:     org_mozilla_xpcom_Utilities
 * Method:    callMethodByIndex
 * Signature: (JILjava/lang/String;J[Ljava/lang/Object;)Ljava/lang/Object;
 */

JNIEXPORT jobject JNICALL Java_org_mozilla_xpcom_Utilities_callMethodByIndex
    (JNIEnv *env, jclass clazz, jlong _oid, jint mid, jstring jiid, jlong _orb, jobjectArray args) {
    bcIORB * orb = (bcIORB*) _orb;
    bcOID oid = (bcOID)_oid;
    nsIID iid;
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--[c++] jni Java_org_mozilla_xpcom_Utilities_callMethodByIndex %d\n",(int)mid));
    const char * str = NULL;
    str = env->GetStringUTFChars(jiid,NULL);
    iid.Parse(str);
    env->ReleaseStringUTFChars(jiid,str);
    if (mid == 2) {
        INVOKE_RELEASE(&oid,&iid,orb);
        return NULL;
    } else if (mid == 1) {
        INVOKE_ADDREF(&oid,&iid,orb);
        return NULL;
    }
    bcICall *call = orb->CreateCall(&iid, &oid, mid);
    
    /*****/
    nsIInterfaceInfo *interfaceInfo;
    nsIInterfaceInfoManager* iimgr;
    if( (iimgr = XPTI_GetInterfaceInfoManager()) != NULL) {
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            return NULL;  //nb exception handling
        }
        NS_RELEASE(iimgr);
    } else {
        return NULL;
    }
    /*****/
    bcIMarshaler * m = call->GetMarshaler();
    bcJavaMarshalToolkit * mt = new bcJavaMarshalToolkit((unsigned)mid, interfaceInfo, args, env, 0, orb);
    mt->Marshal(m);
    orb->SendReceive(call);
    bcIUnMarshaler * um = call->GetUnMarshaler();
    nsresult result;
    jobject retval;
    um->ReadSimple(&result, bc_T_U32);
    if (NS_SUCCEEDED(result)) {
        mt->UnMarshal(um, &retval);
    } else {
        if (XPCOMExceptionClass == NULL) {
            XPCOMExceptionClass = (jclass) env->FindClass("org/mozilla/xpcom/XPCOMException");
            if (!env->ExceptionOccurred()) { // if there is an exception it will be catched in java
                XPCOMExceptionClass = (jclass)env->NewGlobalRef(XPCOMExceptionClass);
                if (!env->ExceptionOccurred()) {
                    XPCOMExceptionInitMID = env->GetMethodID(XPCOMExceptionClass,"<init>","(I)V");
                    if (env->ExceptionOccurred()) {
                        XPCOMExceptionClass = NULL;
                    }
                }
            }
        }
        if (!env->ExceptionOccurred()) {
            jthrowable exception = (jthrowable) env->NewObject(XPCOMExceptionClass, XPCOMExceptionInitMID,(jint)result);
            env->Throw(exception);
        }
    }
    delete call; delete m; delete um; delete mt;
    
    return retval;
}

/*
 * Class:     org_mozilla_xpcom_Utilities
 * Method:    getInterfaceMethodNames
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_org_mozilla_xpcom_Utilities_getInterfaceMethodNames
    (JNIEnv *env, jclass clazz, jstring jiid) {
    if (!jiid) {
        return NULL;
    }
    nsIID iid;
    const char * str = NULL;
    str = env->GetStringUTFChars(jiid, NULL);
    iid.Parse(str);
    env->ReleaseStringUTFChars(jiid,str);

    nsIInterfaceInfo *interfaceInfo;
    nsIInterfaceInfoManager* iimgr;
    if((iimgr = XPTI_GetInterfaceInfoManager()) != NULL) {
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            return NULL;  //nb exception handling
        }
        NS_RELEASE(iimgr);
    } else {
        return NULL;
    }
    PRUint16 num;
    interfaceInfo->GetMethodCount(&num);
    jobjectArray names;
    jclass stringClass = env->GetObjectClass(jiid);
    names = env->NewObjectArray(num, stringClass, NULL);    
    nsXPTMethodInfo* info;
    char buf[256];
    for (int i = 0; i < num; i++) {
        interfaceInfo->GetMethodInfo(i, (const nsXPTMethodInfo **)&info);
        const char* name = info->GetName();

        if (info->IsGetter()) {
            sprintf(buf, "get%c%s", toupper(*name), name+1);
        } else if (info->IsSetter()) {
            sprintf(buf, "set%c%s", toupper(*name), name+1);
        } else {
            // first letter of the method name is in lowercase in java
            sprintf(buf, "%c%s", tolower(*name), name + 1);
        }
        env->SetObjectArrayElement(names, i, env->NewStringUTF(buf));
    }
    return names;
}

