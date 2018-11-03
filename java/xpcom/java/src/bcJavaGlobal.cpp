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
#include "bcJavaGlobal.h"
#include "prenv.h"


JavaVM *bcJavaGlobal::jvm = NULL;
PRLogModuleInfo* bcJavaGlobal::log = NULL;
#ifdef XP_PC
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

#ifdef JRI_MD_H //we are using jni.h from netscape
#define JNIENV
#else 
#define JNIENV  (void**)
#endif

static int counter = 0;

JNIEnv * bcJavaGlobal::GetJNIEnv(int *detachRequired) {
    PRLogModuleInfo * l = GetLog();
    PR_LOG(l,PR_LOG_DEBUG,("--bcJavaGlobal::GetJNIEnv\n"));
    JNIEnv * env;
    int res;
    *detachRequired = 1;
    if (!jvm) {
        StartJVM();
    }
    if (jvm) {
        res = jvm->GetEnv(JNIENV &env, JNI_VERSION_1_2);
        if (res == JNI_OK) {
            *detachRequired = 0;
        } else {
            res = jvm->AttachCurrentThread(JNIENV &env,NULL);
#ifdef DEBUG_idk
            PR_LOG(l,PR_LOG_DEBUG,("--bcJavaGlobal::GetJNIEnv ++counter %d\n",++counter));
#endif
        }
    }
    return env;
}

void bcJavaGlobal::ReleaseJNIEnv() {
    PRLogModuleInfo * l = GetLog();
    PR_LOG(l,PR_LOG_DEBUG,("--bcJavaGlobal::ReleaseJNIEnv\n"));
    int res;
    if (jvm) {
        res = jvm->DetachCurrentThread();
#ifdef DEBUG_idk
        PR_LOG(l,PR_LOG_DEBUG,("--bcJavaGlobal::ReleaseJNIEnv --counter %d\n",--counter));
#endif
    }
}

void bcJavaGlobal::StartJVM() {
    PRLogModuleInfo * l = GetLog();
    PR_LOG(l,PR_LOG_DEBUG,("--bcJavaGlobal::StartJVM begin\n"));
    JNIEnv *env = NULL;
    jint res;
    jsize jvmCount;
    JNI_GetCreatedJavaVMs(&jvm, 1, &jvmCount);
    PR_LOG(l,PR_LOG_DEBUG,("--bcJavaGlobal::StartJVM after GetCreatedJavaVMs\n"));
    if (jvmCount) {
        return;
    }
#if  0
    JDK1_1InitArgs vm_args;
    char classpath[1024];
    JNI_GetDefaultJavaVMInitArgs(&vm_args);
    PR_LOG(l,PR_LOG_DEBUG,("--[c++] version %d",(int)vm_args.version));
    vm_args.version = 0x00010001;
    /* Append USER_CLASSPATH to the default system class path */
    sprintf(classpath, "%s%c%s",
            vm_args.classpath, PATH_SEPARATOR, PR_GetEnv("CLASSPATH"));
    PR_LOG(l,PR_LOG_DEBUG,("--[c++] classpath %s\n",classpath));
    char **props = new char*[2];
    props[0]="java.compiler=NONE";
    props[1]=0;
    vm_args.properties = props;
    vm_args.classpath = classpath;
    /* Create the Java VM */
    res = JNI_CreateJavaVM(&jvm, JNIENV &env, &vm_args);
#else
    char classpath[1024];
    JavaVMInitArgs vm_args;
    JavaVMOption options[2];
    char * classpathEnv = PR_GetEnv("CLASSPATH");
    if (classpathEnv != NULL) {
        sprintf(classpath, "-Djava.class.path=%s",classpathEnv);
        PR_LOG(l,PR_LOG_DEBUG,("--[c++] classpath %s\n",classpath));
        options[0].optionString = classpath;
        options[1].optionString=""; //-Djava.compiler=NONE";
        vm_args.options = options;
        vm_args.nOptions = 2;
    } else {
        vm_args.nOptions = 0;
    }

    vm_args.version = 0x00010002;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    /* Create the Java VM */
    res = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
#endif
    
    PR_LOG(l,PR_LOG_DEBUG,("--bcJavaGlobal::StartJVM jvm started res %d\n",res));
}


PRLogModuleInfo* bcJavaGlobal::GetLog() {
    if (log == NULL) {
        log = PR_NewLogModule(LOG_MODULE);
    }
    return log;
}
















