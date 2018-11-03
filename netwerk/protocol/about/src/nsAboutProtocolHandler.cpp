/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAboutProtocolHandler.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAboutModule.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNetCID.h"
#include "nsAboutProtocolUtils.h"
#include "nsNetError.h"

static NS_DEFINE_CID(kSimpleURICID,     NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsAboutProtocolHandler::nsAboutProtocolHandler()
{
}

nsresult
nsAboutProtocolHandler::Init()
{
    return NS_OK;
}

nsAboutProtocolHandler::~nsAboutProtocolHandler()
{
}

NS_IMPL_ISUPPORTS1(nsAboutProtocolHandler, nsIProtocolHandler)

NS_METHOD
nsAboutProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsAboutProtocolHandler* ph = new nsAboutProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsAboutProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("about");
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for about: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::NewURI(const nsACString &aSpec,
                               const char *aCharset, // ignore charset info
                               nsIURI *aBaseURI,
                               nsIURI **result)
{
    nsresult rv;

    nsIURI* url;
    rv = CallCreateInstance(kSimpleURICID, &url);
    if (NS_FAILED(rv)) return rv;

    rv = url->SetSpec(aSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsAboutProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    NS_ENSURE_ARG_POINTER(uri);
    // about:what you ask?
    nsresult rv;
    nsCAutoString contractID;
    rv = NS_GetAboutModuleName(uri, contractID);
    if (NS_FAILED(rv)) return rv;

    // look up a handler to deal with "what"
    contractID.Insert(NS_LITERAL_CSTRING(NS_ABOUT_MODULE_CONTRACTID_PREFIX), 0);

    nsCOMPtr<nsIAboutModule> aboutMod(do_GetService(contractID.get(), &rv));
    if (NS_SUCCEEDED(rv)) {
        // The standard return case:
        return aboutMod->NewChannel(uri, result);
    }

    // mumble...

    if (rv == NS_ERROR_FACTORY_NOT_REGISTERED) {
        // This looks like an about: we don't know about.  Convert
        // this to an invalid URI error.
        rv = NS_ERROR_MALFORMED_URI;
    }

    return rv;
}

NS_IMETHODIMP 
nsAboutProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
