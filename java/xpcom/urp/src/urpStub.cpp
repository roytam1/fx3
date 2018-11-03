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
 * Sergey Lunegov <lsv@sparc.spb.su>
 */

#include "prmem.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"
#include "nsCRT.h"
#include "urpStub.h"

#include "urpManager.h"
#include "urpLog.h"


urpStub::urpStub(urpManager* man, urpConnection* conn) {
  manager = man;
  connection = conn;
_mOwningThread = PR_CurrentThread();
}


urpStub::~urpStub() {
  PRLogModuleInfo *log = urpLog::GetLog();
  PR_LOG(log, PR_LOG_DEBUG, ("destructor of urpStub\n"));
  if(manager) 
     delete manager;
}

void urpStub::Dispatch(bcICall *call) {
    
  PRLogModuleInfo *log = urpLog::GetLog();
  PR_LOG(log, PR_LOG_DEBUG, ("this is method Dispatch of urpStub\n"));
  bcIID iid; bcOID oid; bcMID mid;
  call->GetParams(&iid, &oid, &mid);
  nsIInterfaceInfo *interfaceInfo;
  nsIInterfaceInfoManager* iimgr;
  if( (iimgr = XPTI_GetInterfaceInfoManager()) ) {
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            return;  //nb exception handling
        }
        NS_RELEASE(iimgr);
  } else {
        return;
  }
  char* name;
  interfaceInfo->GetName(&name);
  PR_LOG(log, PR_LOG_DEBUG, ("real interface name is %s\n",name));

  nsXPTMethodInfo* info;
  interfaceInfo->GetMethodInfo(mid, (const nsXPTMethodInfo **)&info);
  PRUint32 paramCount = info->GetParamCount();
  PRMonitor* mon = PR_NewMonitor();
  PR_EnterMonitor(mon);
  bcTID tid = manager->GetThread();
  nsresult rv = manager->SetCall(call, mon, tid);
  if(NS_FAILED(rv)) {
     printf("Error of SetCall in method Dispatch\n");
//     exit(-1);
  }
  manager->SendUrpRequest(oid, iid, mid, interfaceInfo, call, paramCount,
		 info, connection);
  forReply* fR = (forReply*)PR_Malloc(sizeof(forReply));
  while(1) {
    fR->request = 0;
    if(NS_FAILED(PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT))) {
	printf("Can't wait on cond var\n");
//	exit(-1);
    }
    rv = manager->RemoveCall(fR, tid);
    if(fR->request) 
       manager->ReadLongRequest(fR->header, fR->mess, fR->iid,
				fR->oid, fR->tid, fR->methodId, connection);
    else
       break;
  }
  manager->ReadReply(fR->mess, fR->header, call, paramCount,
			info, interfaceInfo, mid, connection);
  NS_RELEASE(interfaceInfo);
  PR_ExitMonitor(mon);
  PR_DestroyMonitor(mon);
  delete fR->mess;
  PR_Free(fR);
}


void urpStub::SetORB(bcIORB *orb){
   //nb to be implemented
}
void urpStub::SetOID(bcOID oid) {
   //nb to be implemented
} 
