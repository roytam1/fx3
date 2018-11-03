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

#ifndef URP_MANAGER
#define URP_MANAGER

#include "nsIInterfaceInfo.h"
#include "xptinfo.h"
#include "bcDefs.h"
#include "urpTransport.h"
#include "bcIORB.h"

#include "nsHashtable.h"

#define BIG_HEADER 0x80
#define REQUEST 0x40
#define NEWTYPE 0x20
#define NEWOID 0x10
#define NEWTID 0x08
#define LONGMETHODID 0x04
#define IGNORECACHE 0x02
#define MOREFLAGS 0x01

#define MUSTREPLY 0x80
#define SYNCHRONOUSE 0x40

#define DIR_MID 0x40
#define EXCEPTION 0x20

#define INTERFACE 22
#define INTERFACE_STRING "com.sun.star.uno.XInterface"

typedef struct {
    urpPacket* mess;
    char header;
    bcIID iid;
    bcOID oid;
    bcTID tid;
    bcMID methodId;
    int request;
} forReply;

class urpManager {

friend void send_thread_start (void * arg);

public:
	urpManager(PRBool IsClient, bcIORB *orb, urpConnection* connection);
	~urpManager();
	void SendUrpRequest(bcOID oid, bcIID iid,
                                PRUint16 methodIndex,
                          nsIInterfaceInfo* interfaceInfo,
                          bcICall *call,
                          PRUint32 paramCount, const nsXPTMethodInfo* info,
			  urpConnection* conn);
	nsresult ReadMessage(urpConnection* conn, PRBool ic);
	nsresult SetCall(bcICall* call, PRMonitor *m, bcTID tid);
	nsresult RemoveCall(forReply* fR, bcTID tid);
	nsresult ReadReply(urpPacket* message, char header,
                        bcICall* call, PRUint32 paramCount, 
			const nsXPTMethodInfo *info, 
			nsIInterfaceInfo *interfaceInfo, 
			PRUint16 methodIndex, urpConnection* conn);
	nsresult ReadLongRequest(char header, urpPacket* message,
				bcIID iid, bcOID oid, bcTID tid,
				PRUint16 methodId, urpConnection* conn);
	bcTID GetThread();
private:
	nsHashtable* monitTable;
	bcIORB *broker;
	nsHashtable* threadTable;
/* for ReadReply */
    void TransformMethodIDAndIID();
    nsresult ReadShortRequest(char header, urpPacket* message);
    nsresult SendReply(bcTID tid, bcICall* call, PRUint32 paramCount,
                   const nsXPTMethodInfo* info,
                   nsIInterfaceInfo *interfaceInfo, 
		   PRUint16 methodIndex, urpConnection* conn);
};


#endif
