/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package netscape.ldap;

/**
 * Performs explicit bind processing on a referral. A client may
 * specify an instance of this class for use on a single operation
 * (through the <CODE>LDAPConstraints</CODE> object) or all operations 
 * (through <CODE>LDAPConnection.setOption()</CODE>). It is typically used
 * to control the authentication mechanism used on implicit referral 
 * handling.
 */

public interface LDAPBind {

    /**
     * This method is called by <CODE>LDAPConnection</CODE> when 
     * authenticating. An implementation of <CODE>LDAPBind</CODE> may access 
     * the host, port, credentials, and other information in the 
     * <CODE>LDAPConnection</CODE> in order to decide on an appropriate 
     * authentication mechanism.<BR> 
     * The bind method can also interact with a user or external module. 
     * @exception netscape.ldap.LDAPException
     * @see netscape.ldap.LDAPConnection#bind
     * @param conn an established connection to an LDAP server
     */

    public void bind(LDAPConnection conn) throws LDAPException;
}
