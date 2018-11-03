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

#include "java_lang_String.h"
#include "JavaVM.h"

extern "C" {
/*
 * Class : java/lang/String
 * Method : intern
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_String_intern(Java_java_lang_String *jThis)
{
	JavaString& jString = asJavaString(*jThis);
	char* CString = jString.convertUtf();

	JavaString& internString =  VM::intern(CString);
	JavaString::freeUtf(CString);

	return (Java_java_lang_String *)(&internString);
}

}
