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

#if !defined(_PPC601_CPU_H_) || defined(INCLUDE_EMITTER)
#define _PPC601_CPU_H_

#define FIRST_CALLER_SAVED_GR       0
#define LAST_CALLER_SAVED_GR        9
#define FIRST_CALLEE_SAVED_GR       10
#define LAST_CALLEE_SAVED_GR        28
#define FIRST_CALLER_SAVED_FPR      0
#define LAST_CALLER_SAVED_FPR       9
#define FIRST_CALLEE_SAVED_FPR      10
#define LAST_CALLEE_SAVED_FPR       28

#define FIRST_GREGISTER             0
#define LAST_GREGISTER              28
#define FIRST_FPREGISTER            0
#define LAST_FPREGISTER             28
#define NUMBER_OF_SPECIAL_REGISTERS 0

class PPCEmitter;
class PPCFormatter;
typedef PPCFormatter MdFormatter;
typedef PPCEmitter MdEmitter;

#ifdef INCLUDE_EMITTER
#include "PPC601AppleMacOSEmitter.h"
#endif

#define CPU_IS_SUPPORTED

#endif /* _PPC601_CPU_H_ */
