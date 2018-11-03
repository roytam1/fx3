/* 
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
 */
package org.mozilla.pluglet.test.basic.security; 

import org.mozilla.pluglet.test.basic.*;

import java.awt.*;
import java.awt.datatransfer.*;
import java.applet.*;
import java.io.*;
import java.net.*;
import java.util.*;
 
public class RemoteConnectTest extends BasicSecurityTest implements Test {

public RemoteConnectTest() {
}

protected void doRemoteConnect(String str) throws Exception {

	URL url = new URL(str);
	URLConnection conn = url.openConnection();

	InputStream is = conn.getInputStream();
	DataInputStream data = new DataInputStream(new BufferedInputStream(is));
	String line;

		while ((line = data.readLine()) != null) {
		}
}

public void execute( TestContext c ) {
 mustPass = false;

 if ((c.getProperty("RemoteConnectTest.mustPass")).equals(new String("true"))) {
	mustPass = true;
 };

 String url = c.getProperty("RemoteConnectTest.URL");
 
 try {
 	doRemoteConnect(url);
        if( mustPass )	
		c.registerPASSED(new String("OK")); else
		c.registerFAILED(new String("Illegal operation performed.\nSeems to be a security hole."));
 } catch ( SecurityException e ) {
     if( mustPass )	
		c.registerFAILED(e.toString()); else
		c.registerPASSED(e.toString());
 } catch ( Exception e ) {
	c.registerFAILED(e.toString());
 }

}

}
 
