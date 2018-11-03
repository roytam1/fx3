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

#ifndef __bcXPCOMStub_h
#define __bcXPCOMStub_h
#include "nsISupports.h"
#include "bcIStub.h"
#include "nsCOMPtr.h"
#include "nsIEventQueueService.h"

class bcXPCOMStub : public bcIStub {
public:
    bcXPCOMStub(nsISupports *obj);
    virtual ~bcXPCOMStub();
    virtual void Dispatch(bcICall *call) ;
    virtual void SetORB(bcIORB *orb);
    virtual void SetOID(bcOID oid);
    void DispatchAndSaveThread(bcICall *call, nsIEventQueue *q = NULL);
private:
    nsISupports *object;
    bcIORB *orb;
    bcOID oid;
    void* _mOwningThread;
    nsCOMPtr<nsIEventQueue> owningEventQ;
    nsCOMPtr<nsIEventQueueService>  eventQService;
    PRUint32 refCounter;
};

#endif


