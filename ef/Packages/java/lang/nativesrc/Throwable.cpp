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
#include "java_lang_Throwable.h"
#include <stdio.h>

extern "C" {
/*
 * Class : java/lang/Throwable
 * Method : printStackTrace0
 * Signature : (Ljava/lang/Object;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Throwable_printStackTrace0(Java_java_lang_Throwable *, Java_java_lang_Object *)
{
  printf("Netscape_Java_java_lang_Throwable_printStackTrace0() not implemented");
}


/*
 * Class : java/lang/Throwable
 * Method : fillInStackTrace
 * Signature : ()Ljava/lang/Throwable;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Throwable *)
Netscape_Java_java_lang_Throwable_fillInStackTrace(Java_java_lang_Throwable *)
{
  //printf("Netscape_Java_java_lang_Throwable_fillInStackTrace() not implemented");
  return 0;
}


} /* extern "C" */

