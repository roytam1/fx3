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
 */


#include "nscore.h"
#include "xptcall.h"
#include "bcJavaStub.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "bcJavaMarshalToolkit.h"
#include "bcJavaGlobal.h"
#include "bcIIDJava.h"

jclass bcJavaStub::objectClass = NULL;
jclass bcJavaStub::utilitiesClass = NULL;
jmethodID bcJavaStub::callMethodByIndexMID = NULL;

bcJavaStub::bcJavaStub(jobject obj) : orb(NULL) {
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    PR_LOG(log,PR_LOG_DEBUG,("--bcJavaStub::bcJavaStub \n"));
    if (!obj) {
        PR_LOG(log,PR_LOG_DEBUG,("--bcJavaStub::bcJavaStub obj== 0\n"));
        return;
    }
    int detachRequired;
    JNIEnv * env = bcJavaGlobal::GetJNIEnv(&detachRequired);
    object = env->NewGlobalRef(obj);
    refCounter = 0;
    if (detachRequired) {
        bcJavaGlobal::ReleaseJNIEnv();
    }
}


bcJavaStub::~bcJavaStub() {
    int detachRequired;
    JNIEnv *env = bcJavaGlobal::GetJNIEnv(&detachRequired);
    env->DeleteGlobalRef(object);
    if (orb != NULL) {
        orb->UnregisterStub(oid);
    }
    if (detachRequired) {
        bcJavaGlobal::ReleaseJNIEnv();
    }
}


void bcJavaStub::SetORB(bcIORB *_orb) {
    orb = _orb;
}

void bcJavaStub::SetOID(bcOID _oid) {
    oid = _oid;
}

void bcJavaStub::Dispatch(bcICall *call) {
    //sigsend(P_PID, getpid(),SIGINT);
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    int detachRequired;
    JNIEnv * env = bcJavaGlobal::GetJNIEnv(&detachRequired);
    bcIID iid; bcOID oid; bcMID mid;
    jobjectArray args;
    call->GetParams(&iid, &oid, &mid);

    if (mid == 1) { //AddRef
        refCounter++;
        if (detachRequired) {
            bcJavaGlobal::ReleaseJNIEnv();
        }
        return;
    } else if (mid == 2) { //Release
        refCounter--;
        printf("-java mid==2\n");
        if (refCounter <= 0) { 
            printf("-java delete\n");
            delete this;
            if (detachRequired) {
                bcJavaGlobal::ReleaseJNIEnv();
            }
            return;
        }
    }

    nsIInterfaceInfo *interfaceInfo;
    nsIInterfaceInfoManager* iimgr;
    if((iimgr = XPTI_GetInterfaceInfoManager()) != NULL) {
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            if (detachRequired) {
                bcJavaGlobal::ReleaseJNIEnv();
            }
            return;  //nb exception handling
        }
        NS_RELEASE(iimgr);
    } else {
        if (detachRequired) {
            bcJavaGlobal::ReleaseJNIEnv();
        }
        return;
    }
    if (!objectClass) {
        Init();
        if (!objectClass) {
            if (detachRequired) {
                bcJavaGlobal::ReleaseJNIEnv();
            }
            return;
        }
    }
    nsXPTMethodInfo* info;
    interfaceInfo->GetMethodInfo(mid,(const nsXPTMethodInfo **)&info);
    PRUint32 paramCount = info->GetParamCount();
    PR_LOG(log, PR_LOG_DEBUG,("\n**[c++]hasRetval: %d\n", HasRetval(paramCount, info)));
    if (HasRetval(paramCount, info))
        // do not pass retval param
        paramCount--;
    args = env->NewObjectArray(paramCount, objectClass,NULL);
    bcJavaMarshalToolkit * mt = new bcJavaMarshalToolkit(mid, interfaceInfo, args, env,1, call->GetORB());
    bcIUnMarshaler * um = call->GetUnMarshaler();
    mt->UnMarshal(um);
    jobject jiid = bcIIDJava::GetObject(&iid);
    jobject retval = env->CallStaticObjectMethod(utilitiesClass, callMethodByIndexMID, object, jiid, (jint)mid, args);
    nsresult result = NS_OK;
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        result = NS_ERROR_FAILURE;
    }
    bcIMarshaler * m = call->GetMarshaler(); 
    m->WriteSimple(&result, bc_T_U32); 
    if (NS_SUCCEEDED(result)) {
        mt->Marshal(m, retval);
    }
    delete m; delete um; delete mt;
    if (detachRequired) {
        bcJavaGlobal::ReleaseJNIEnv();
    }
    return;
}


void bcJavaStub::Init() {
    int detachRequired;
    JNIEnv * env = bcJavaGlobal::GetJNIEnv(&detachRequired);
    objectClass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        if (detachRequired) {
            bcJavaGlobal::ReleaseJNIEnv();
        }
        return;
    }

    utilitiesClass = (jclass)env->NewGlobalRef(env->FindClass("org/mozilla/xpcom/Utilities"));
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        if (detachRequired) {
            bcJavaGlobal::ReleaseJNIEnv();
        }
        return;
    }
    callMethodByIndexMID = env->GetStaticMethodID(utilitiesClass,"callMethodByIndex","(Ljava/lang/Object;Lorg/mozilla/xpcom/IID;I[Ljava/lang/Object;)Ljava/lang/Object;");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        if (detachRequired) {
            bcJavaGlobal::ReleaseJNIEnv();
        }
        return;
    }
}

PRBool bcJavaStub::HasRetval(PRUint32 paramCount, nsXPTMethodInfo *info)
{
    for (unsigned int i = 0; i < paramCount; i++) {
        nsXPTParamInfo param = info->GetParam(i);
        if (param.IsRetval())
            return PR_TRUE;
    }
    return PR_FALSE;
}
