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
#ifndef _NETSCAPE_MANGLERS_H_
#define _NETSCAPE_MANGLERS_H_

/* Name mangling for "short" versions of Netscape-style mangled names 
 * (without a mangled signature) and "long" versions (with a mangled 
 * signature). Netscape-style mangling is essentially JNI-style mangling,
 * with a Netscape_ prefixed to the function name. 
 */

#include "JNIManglers.h"

class NS_EXTERN NetscapeShortMangler : public JNIShortMangler {
public:
  /* Like JNI, we support both stdcall and regular calls */
  NetscapeShortMangler() : JNIShortMangler() { }

  virtual char *mangle(const char *className,
		       const char *methodName,
		       const char *signature,
		       char *buffer,
		       Int32 len);

  virtual Int32 getExtraArgsSize(bool);
  virtual void *getCode(const Method &, void *actualCode);

};

class NS_EXTERN NetscapeLongMangler : public JNILongMangler {
public:
  NetscapeLongMangler() : JNILongMangler() { } 

  virtual char *mangle(const char *className,
		       const char *methodName,
		       const char *signature,
		       char *buffer,
		       Int32 len);

  virtual Int32 getExtraArgsSize(bool);
  virtual void *getCode(const Method &, void *actualCode);

};

#endif /* _JNI_SHORT_MANGLER_H_ */
