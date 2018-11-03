/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "java_security_AccessController.h"
#include "prprf.h"
#include "JavaVM.h"

extern "C" {

/*
 * Class : java/security/AccessController
 * Method : beginPrivileged
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_security_AccessController_beginPrivileged()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_beginPrivilege not implemented\n");
}


/*
 * Class : java/security/AccessController
 * Method : endPrivileged
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_security_AccessController_endPrivileged()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_endPrivileged not implemented\n");
}



/*
 * Class : java/security/AccessController
 * Method : getInheritedAccessControlContext
 * Signature : ()Ljava/security/AccessControlContext;
 */
NS_EXPORT NS_NATIVECALL(Java_java_security_AccessControlContext *)
Netscape_Java_java_security_AccessController_getInheritedAccessControlContext()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_getInheritedAccessControlContext not implemented\n");
  return 0;
}



/*
 * Class : java/security/AccessController
 * Method : getStackAccessControlContext
 * Signature : ()Ljava/security/AccessControlContext;
 */
NS_EXPORT NS_NATIVECALL(Java_java_security_AccessControlContext *)
Netscape_Java_java_security_AccessController_getStackAccessControlContext()
{
  PR_fprintf(PR_STDERR, "Netscape_Java_java_security_AccessController_getStackAccessControlContext not implemented\n");
  return 0;
}

/*
 * Class : java/security/AccessController
 * Method : doPrivileged
 * Signature : (Ljava/security/PrivilegedAction;)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_security_AccessController_doPrivileged__Ljava_security_PrivilegedAction_2(Java_java_security_PrivilegedAction *inAction)
{
    JavaObject *action = (JavaObject *) inAction;
    Class &clazz = *const_cast<Class*>(&action->getClass());
    Method &run_method = clazz.getMethod("run", NULL, 0);
    
    // Invoke run() method on interface, but don't bother with security
    return (Java_java_lang_Object *)run_method.invoke(action, NULL, 0);
}

}





