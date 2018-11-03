/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package org.mozilla.xpcom;

public class Components {
    public static nsIComponentManager getComponentManager() {
        return componentMgr;
    }
    public static void setComponentManager(nsIComponentManager mgr) {
        componentMgr = mgr;
    }
    public static nsIXPIDLServiceManager getServiceManager() {
        return serviceMgr;
    }
    public static void setServiceManager(nsIXPIDLServiceManager mgr) {
        serviceMgr = mgr;
    }
    public static native void initXPCOM();
    private static nsIComponentManager componentMgr = null;
    private static nsIXPIDLServiceManager serviceMgr = null;
    static {
        System.loadLibrary("blackconnectjni");
    }

};

