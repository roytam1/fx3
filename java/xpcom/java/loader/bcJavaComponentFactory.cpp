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
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "bcJavaComponentFactory.h"
#include "bcIJavaStubsAndProxies.h"
#include "bcJavaStubsAndProxiesCID.h"
#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"
#include "bcIORBComponent.h"
#include "bcORBComponentCID.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(bcJavaComponentFactory, nsIFactory)

bcJavaComponentFactory::bcJavaComponentFactory(const char *_location) {
    NS_INIT_ISUPPORTS();
    location = nsCRT::strdup(_location);
}

bcJavaComponentFactory::~bcJavaComponentFactory() {
    nsCRT::free((char*)location);
}

/* void CreateInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); 
*/
NS_IMETHODIMP bcJavaComponentFactory::CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result) {
    printf("--bcJavaComponentFactory::CreateInstance\n");
    nsresult r;
    nsCOMPtr<bcIJavaStubsAndProxies> javaStubsAndProxies = do_GetService(BC_JAVASTUBSANDPROXIES_ContractID, &r);
    if (NS_FAILED(r)) {
        printf("--bcJavaComponentFactory::CreateInstance javaStubsAndProxies failed \n");
        return r;
    }
    nsCOMPtr<bcIXPCOMStubsAndProxies> xpcomStubsAndProxies = do_GetService(BC_XPCOMSTUBSANDPROXIES_ContractID, &r);
    if (NS_FAILED(r)) {
        printf("--bcJavaComponentFactory::CreateInstance xpcomStubsAndProxies failed \n");
        return r;
    }
    nsCOMPtr<bcIORBComponent> _orb = do_GetService(BC_ORBCOMPONENT_ContractID, &r);
    if (NS_FAILED(r)) {
        printf("--bcJavaComponentFactory::CreateInstance bcORB failed \n");
        return r;
    }
    bcIORB *orb;
    _orb->GetORB(&orb);
    bcOID oid;
    r = javaStubsAndProxies->GetOID(location, &oid);
    if (NS_FAILED(r)) {
        return r;
    }
    printf("--bcJavaComponentFactory::CreateInstance after GetOID");
    nsISupports *proxy;
    printf("--[c++]bcJavaComponentFactory::CreateInstance iid:%s\n",iid.ToString());
    xpcomStubsAndProxies->GetProxy(oid, iid, orb, &proxy);
    if (NS_FAILED(r)) {
        return r;
    }
    *result = proxy;
    printf("--bcJavaComponentFactory::CreateInstance end");
    return NS_OK;
}

/* void LockFactory (in PRBool lock); */
NS_IMETHODIMP bcJavaComponentFactory::LockFactory(PRBool lock)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}








