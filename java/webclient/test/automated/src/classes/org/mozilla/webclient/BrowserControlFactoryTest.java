/*
 * $Id$
 */

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
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
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */

package org.mozilla.webclient;

import junit.framework.TestSuite;
import junit.framework.Test;

// BrowserControlFactoryTest.java

public class BrowserControlFactoryTest extends WebclientTestCase {

    public BrowserControlFactoryTest(String name) {
	super(name);
    }

    public static Test suite() {
	return (new TestSuite(BrowserControlFactoryTest.class));
    }

    public String getExpectedOutputFilename() { return "BrowserControlFactoryTest_correct"; }

    public boolean sendOutputToFile() { return true; }

    //
    // Testcases
    // 

    public void testStartupCreateAndTerminate() throws Exception {
	BrowserControl firstBrowserControl = null;
	BrowserControlFactory.setAppData(getBrowserBinDir());
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
	BrowserControlFactory.appTerminate();
	    
	// PENDING(edburns): 20060304 fix when profileManager is fixed
        // assertTrue(verifyExpectedOutput());
    }

}
