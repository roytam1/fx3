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

#ifndef _urpCONNECTCOMPONENT_h
#define _urpCONNECTCOMPONENT_h
#include "urpIConnectComponent.h"
#include "urpConnectComponentCID.h"

#include "../urpManager.h"
#include "../urpStub.h"

class urpConnectComponent : public urpIConnectComponent {
    NS_DECL_ISUPPORTS
    NS_IMETHOD GetCompMan(char* cntStr, nsISupports** ret);
    NS_IMETHODIMP GetTransport(char* cntStr, urpTransport** trans);
    urpConnectComponent();
    virtual ~urpConnectComponent();
 private:
    urpConnection* connection;
    urpTransport* transport;
    urpManager* man;
    urpStub* stub;
    nsISupports* compM;
    bcIORB* orb;
};

#endif
 
