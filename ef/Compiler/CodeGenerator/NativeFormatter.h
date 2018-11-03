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
//
// File:	NativeFormatter.h
// Author:	smsilver
//

#ifndef H_NATIVEFORMATTER
#define H_NATIVEFORMATTER

class ControlNode;
class Instruction;
class Pool;
class ExceptionTable;
struct MethodDescriptor;

#include "FormatStructures.h"
#include "LogModule.h"

#define INCLUDE_EMITTER
#include "CpuInfo.h"
#undef INCLUDE_EMITTER

class NativeFormatter
{
protected:			
  MdEmitter& mEmitter;
  MdFormatter mFormatter;	
  ControlNode** nodes;
  Uint32 nNodes;

  void				outputNativeToMemory(void* inWhere,  const FormattedCodeInfo& inInfo);						
  Uint32			accumulateSize(ControlNode& inNode, Uint32 inPrologSize, Uint32 inEpilogSize);
  Uint32			resolveBranches(Uint32 inPrologSize, Uint32 inEpilogSize);
  ExceptionTable&	buildExceptionTable(Uint8* methodMemoryStart, Pool* inPool);

public:
  NativeFormatter(MdEmitter& inEmitter, ControlNode** mNodes, uint32 mNNodes) : 
	mEmitter(inEmitter), mFormatter(mEmitter), nodes(mNodes), nNodes(mNNodes) {}

  void* format(Method& inMethod, FormattedCodeInfo*	outInfo = NULL);

#ifdef DEBUG_LOG
  void dumpMethodToHTML(FormattedCodeInfo& fci, ExceptionTable& inExceptionTable);	// see HTMLMethodDump.cpp for implementation
#endif
};

#endif // H_NATIVEFORMATTER
