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

#include "org_mozilla_webclient_impl_wrapper_0005fnative_ProfileManagerImpl.h"

#include "ns_util.h"

#include <nsCRT.h> // for nsCRT::strlen

// edburns_20060216
// #include <nsICmdLineService.h> // for the cmdline service to give to the
                               // profile manager.
#include <nsIProfile.h> // for the profile manager
#include <nsIProfileInternal.h> // for the profile manager
#include <nsString.h> // for nsCAutoString

// edburns_20060216
// static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);

//
// global data
//

//
// Local Functions
//

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeStartup
  (JNIEnv *env, jobject obj, jint nativeContext, 
   jstring profileDir , jstring profileNameJstr)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeStartup: entering\n"));
#if 0 // edburns_20060219 comment this out
    nsresult rv;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    
    // handle the profile manager nonsense
    nsCOMPtr<nsICmdLineService> cmdLine =do_GetService(kCmdLineServiceCID);
    nsCOMPtr<nsIProfile> profile = do_GetService(NS_PROFILE_CONTRACTID);
    if (!cmdLine || !profile) {
        ::util_ThrowExceptionToJava(env, "Can't get the profile manager.");
        return;
    }
    PRInt32 numProfiles=0;
    rv = profile->GetProfileCount(&numProfiles);
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeStartup: GetProfileCount rv: %d\n", 
            rv));

    PRUnichar *webclientProfile = nsnull;
    char *webclientProfileCstr = nsnull; 

    if (nsnull == profileNameJstr) {
        NS_NAMED_LITERAL_STRING(webclientStr, "webclient");
        webclientProfile = (PRUnichar *) webclientStr.get();
        webclientProfileCstr = "webclient";
    }
    else {
        webclientProfile = (PRUnichar *)
            ::util_GetStringChars(env, profileNameJstr);
        webclientProfileCstr =  (char *)
            ::util_GetStringUTFChars(env, profileNameJstr);
    }

    char *argv[3];
    int i,j;
    PRBool hasWebclientProfile = PR_FALSE;
    argv[0] = PL_strdup(nsnull);
    argv[1] = PL_strdup("-p");
    
    if (0 < numProfiles) {
        PRUnichar **Names;
        PRUint32 NamesLen = 0;
        rv = profile->GetProfileList(&NamesLen, &Names);
        
        if (NS_SUCCEEDED(rv)) {
            PR_ASSERT(NamesLen >= 1);
            
            char * temp = new char[100]; // de-allocated following for loop
            for (i = 0; i < NamesLen; i++) {
                for (j = 0; Names[i][j] != '\0'; j++) {
                    temp[j] = (char) Names[i][j];
                }
                temp[j] = '\0';
                if (0 == strcmp(webclientProfileCstr, temp)) {
                    hasWebclientProfile = PR_TRUE;
                    break;
                }
            }
            nsMemory::Free(Names);
        }
        argv[2] = PL_strdup(webclientProfileCstr);
    }
    
    if (!hasWebclientProfile) {
        rv = profile->CreateNewProfile(webclientProfile, nsnull, nsnull, 
                                       PR_FALSE);
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Can't statrup nsIProfile service.");
            return;
        }
    }
    
    rv = cmdLine->Initialize(3, argv);
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeStartup: commandLineService initialize rv: %d\n", 
            rv));
    
    for (i = 0; i < 3; i++) {
        nsCRT::free(argv[i]);
    }
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't initialize nsICmdLineService.");
        return;
    }
    nsCOMPtr<nsIProfileInternal> profileInt = do_QueryInterface(profile);
    if (profileInt) {
        rv = profileInt->StartupWithArgs(cmdLine, PR_FALSE);
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("ProfileManagerImpl_nativeStartup: profileInternal startupWithArgs rv: %d\n", 
                rv));
        
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Can't statrup nsIProfile service.");
            return;
        }
    }
    else {
        ::util_ThrowExceptionToJava(env, "Can't statrup nsIProfile service.");
        return;
    }

    // only release if we allocated
    if (nsnull != profileNameJstr) {
        ::util_ReleaseStringChars(env, profileNameJstr, webclientProfile);
        ::util_ReleaseStringUTFChars(env, profileNameJstr, 
                                     webclientProfileCstr);
    }
        
    wcContext->sProfile = profile.get();
    NS_ADDREF(wcContext->sProfile);
    wcContext->sProfileInternal = profileInt.get();
    NS_ADDREF(wcContext->sProfileInternal);

#endif // edburns_20060216
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeStartup: exiting\n"));
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeShutdown
  (JNIEnv *env, jobject, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeShutdown: entering\n"));
    // edburns_20060216
#if 0

    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;

    PR_ASSERT(wcContext);

    nsresult rv = NS_OK;
    
    rv = 
        wcContext->sProfile->ShutDownCurrentProfile(nsIProfile::SHUTDOWN_PERSIST);
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeShutdown: ShutDownCurrentProfile: rv: %d\n", rv));

    NS_RELEASE(wcContext->sProfile);
    wcContext->sProfile = nsnull;
    NS_RELEASE(wcContext->sProfileInternal);
    wcContext->sProfileInternal = nsnull;
#endif // edburns_20060216
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeShutdown: exiting\n"));
}

JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeGetProfileCount
(JNIEnv *env, jobject obj, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeGetProfileCount: entering\n"));

    nsresult rv;
    jint result = -1;
    PRInt32 count = 0;
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    nsCOMPtr<nsIProfile> profile = nsnull;
    PR_ASSERT(wcContext);

    profile = wcContext->sProfile;
    PR_ASSERT(profile);
    
    rv = profile->GetProfileCount(&count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't get profile count.");
        return result;
    }
    result = (jint) count;
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeGetProfileCount: exiting\n"));
    return result;
}

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeProfileExists
(JNIEnv *env, jobject obj, jint nativeContext, jstring profileNameJstr)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeProfileExists: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIProfile> profile = wcContext->sProfile;
    PR_ASSERT(profile);
    nsresult rv;
    jboolean result;
    PRBool exists;
    PRUnichar *profileName = 
        (PRUnichar *) ::util_GetStringChars(env, profileNameJstr);
    
    rv = profile->ProfileExists(profileName, &exists);
    ::util_ReleaseStringChars(env, profileNameJstr, profileName);
    
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't see if profile exists.");
        return result;
    }
    
    result = exists == PR_TRUE ? JNI_TRUE : JNI_FALSE;
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeProfileExists: exiting\n"));
    return result;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeSetCurrentProfile
(JNIEnv *env, jobject obj, jint nativeContext, jstring profileNameJstr)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeSetCurrentProfile: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIProfile> profile = wcContext->sProfile;
    PR_ASSERT(profile);
    nsresult rv;
    PRUnichar *profileName = 
        (PRUnichar *) ::util_GetStringChars(env, profileNameJstr);
    
    rv = profile->SetCurrentProfile(profileName);
    ::util_ReleaseStringChars(env, profileNameJstr, profileName);
    
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't set current profile.");
        return;
    }
    
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeSetCurrentProfile: exiting\n"));
}

JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeGetCurrentProfile
(JNIEnv *env, jobject obj, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeGetCurrentProfile: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIProfile> profile = wcContext->sProfile;
    PR_ASSERT(profile);
    nsresult rv;
    jstring result = nsnull;
    nsXPIDLString profileName;
    
    rv = profile->GetCurrentProfile(getter_Copies(profileName));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't get current profile.");
        return result;
    }
    result = (jstring) ::util_NewString(env, profileName.get(),
                                        profileName.Length());
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeGetCurrentProfile: exiting\n"));
    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeGetProfileList
(JNIEnv *env, jobject obj, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeGetProfileList: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIProfile> profile = wcContext->sProfile;
    PR_ASSERT(profile);
    nsresult rv;
    jobjectArray result = nsnull;
    PRUint32 length = -1;
    PRUnichar **profileNames = nsnull;
    jint *profileNameLengths = nsnull;
    int i = 0;
    
    rv = profile->GetProfileList(&length, &profileNames);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't get profile list.");
        return result;
    }
    
    profileNameLengths = new jint[length];
    for (i = 0; i < length; i++) {
        profileNameLengths[i] = 
            nsCRT::strlen((const PRUnichar*)profileNames[i]);
    }
    result = ::util_GetJstringArrayFromJcharArray(env, length, profileNames,
                                                  profileNameLengths);
    if (nsnull == result) {
        ::util_ThrowExceptionToJava(env, "Can't get profile list.");
        return result;
    }
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeGetProfileList: exiting\n"));
    return result;
    
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeCreateNewProfile
(JNIEnv *env, jobject obj, jint nativeContext, 
 jstring profileNameJstr, 
 jstring nativeProfileDirJstr, 
 jstring langCodeJstr, 
 jboolean useExistingDir) 
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeCreateNewProfile: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIProfile> profile = wcContext->sProfile;
    PR_ASSERT(profile);
    PR_ASSERT(profileNameJstr);
    nsresult rv;
    const PRUnichar 
        *profileName = 
        (PRUnichar *) ::util_GetStringChars(env, profileNameJstr),
        *nativeProfileDir = (nsnull == nativeProfileDirJstr) ? nsnull :
        (PRUnichar *) ::util_GetStringChars(env, nativeProfileDirJstr),
        *langCode = (nsnull == langCodeJstr) ? nsnull : 
        (PRUnichar *) ::util_GetStringChars(env, langCodeJstr);
    
    rv = profile->CreateNewProfile(profileName, nativeProfileDir, langCode,
                                   useExistingDir);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't create new profile.");
    }
    
    ::util_ReleaseStringChars(env, profileNameJstr, profileName);
    if (nsnull != nativeProfileDir) {
        ::util_ReleaseStringChars(env, nativeProfileDirJstr, nativeProfileDir);
    }
    if (nsnull != langCode) {
        ::util_ReleaseStringChars(env, langCodeJstr, langCode);
    }
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeCreateNewProfile: exiting\n"));
    return;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeRenameProfile
(JNIEnv *env, jobject obj, jint nativeContext, 
 jstring oldNameJstr, 
 jstring newNameJstr)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeRenameProfile: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIProfile> profile = wcContext->sProfile;
    PR_ASSERT(profile);
    PR_ASSERT(oldNameJstr);
    PR_ASSERT(newNameJstr);
    nsresult rv;
    const PRUnichar 
        *oldName = 
        (PRUnichar *) ::util_GetStringChars(env, oldNameJstr),
        *newName = (nsnull == newNameJstr) ? nsnull :
        (PRUnichar *) ::util_GetStringChars(env, newNameJstr);

    rv = profile->RenameProfile(oldName, newName);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't rename new profile.");
    }
    
    ::util_ReleaseStringChars(env, oldNameJstr, oldName);
    ::util_ReleaseStringChars(env, newNameJstr, newName);
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeRenameProfile: exiting\n"));
    return;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_ProfileManagerImpl_nativeDeleteProfile
(JNIEnv *env, jobject obj, jint nativeContext, 
 jstring profileNameJstr, jboolean canDeleteFiles)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeDeleteProfile: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIProfile> profile = wcContext->sProfile;
    PR_ASSERT(profile);
    PR_ASSERT(profileNameJstr);
    nsresult rv;
    const PRUnichar 
        *profileName = 
        (PRUnichar *) ::util_GetStringChars(env, profileNameJstr);
    
    rv = profile->DeleteProfile(profileName, canDeleteFiles);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't delete profile.");
    }
    
    ::util_ReleaseStringChars(env, profileNameJstr, profileName);
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("ProfileManagerImpl_nativeDeleteProfile: exiting\n"));
    return;
}
