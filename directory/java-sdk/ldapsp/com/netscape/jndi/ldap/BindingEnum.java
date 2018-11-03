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
package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;
import com.netscape.jndi.ldap.common.ExceptionMapper;

import netscape.ldap.*;

import java.util.*;

/**
 * Wrapper for the LDAPSearchResult. Convert each LDAPEntry entry into
 * a JNDI Binding. Bindings are accessed through the NamingEnumeration
 * interface
 */
class BindingEnum extends BaseSearchEnum {

    public BindingEnum(LDAPSearchResults res, LdapContextImpl ctx)  throws NamingException {
        super(res, ctx);
    }

    public Object next() throws NamingException{
        LDAPEntry entry = nextLDAPEntry();
        String name = LdapNameParser.getRelativeName(m_ctxName, entry.getDN());
        Object obj = ObjectMapper.entryToObject(entry, m_ctx);
        String className = obj.getClass().getName();
        return new Binding(name, className, obj, /*isRelative=*/true);
    }

}