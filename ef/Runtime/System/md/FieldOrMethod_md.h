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
#ifndef _FIELD_OR_METHOD_MD_H_
#define _FIELD_OR_METHOD_MD_H_

#ifdef GENERATE_FOR_X86
#define PC_ONLY(x) x
#else
#define PC_ONLY(x) 
#endif

#ifdef GENERATE_FOR_X86
    #ifdef __GNUC__
        #define prepareArg(i, arg) __asm__ ("pushl %0" : /* no outputs */ : "g" (arg))     // Prepare the ith argument
        #define callFunc(func) __asm__ ("call *%0" : /* no outputs */ : "r" (func))        // Call function func
        #define getReturnValue(ret) __asm__ ("movl %%eax,%0" : "=g" (ret) : /* no inputs */)  // Put return value into ret
    #else   // !__GNUC__
        #define prepareArg(i, arg) _asm { push arg }     // Prepare the ith argument
        #define callFunc(func) _asm { call func }        // Call function func
        #define getReturnValue(ret) _asm {mov ret, eax}  // Put return value into ret
    #endif
#elif defined(GENERATE_FOR_PPC)
    #ifdef XP_MAC
        #define prepareArg(i, arg) 
        #define callFunc(func) 
        #define getReturnValue(ret)  ret = 0
    #else
        #define prepareArg(i, arg) 
        #define callFunc(func) 
        #define getReturnValue(ret)  ret = 0
    #endif
#endif


#endif /* _FIELD_OR_METHOD_MD_H_ */
