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


package org.mozilla.pluglet.test.basic.api;

import java.util.*;
import java.io.*;
import java.net.*;
import org.mozilla.pluglet.test.basic.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.*;

public class PlugletStreamInfo_getURL_0  implements Test
{
   private static String srcURL = null;
   public static boolean actionPerformed = false;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletStreamInfo_getURL_0()
   {
   }


   /**
    *
    ***********************************************************
    *  Starting point of application
    *
    *  @param   args    Array of command line arguments
    *
    ***********************************************************
    */
   public static void main(String[] args)
   {
   }

   /**
    ***********************************************************
    *
    *  Execute Method 
    *
    *  @param   contex  TestContext reference with properties and parameters    
    *  @return          void, the result set to true or false via context
    *
    ***********************************************************
    */
   public void execute(TestContext context)
   {	
	Properties paramProps = null;
	PlugletStreamInfo PlugletStreamInfo_obj = null;
	
	if( context == null ) {
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext)" +
				      " of PlugletStreamInfo_getURL_0");
	   return;
	}
	if ( context.failedIsRegistered()) {
	   return;
	}
	PlugletStreamInfo_obj = context.getPlugletStreamInfo();
	if(PlugletStreamInfo_obj == null) {
	   TestContext.registerFAILED("ERROR:null PlugletStreamInfo passed in execute(TestContext)" +
				      " of PlugletStreamInfo_getURL_0");
	   return;
	}
	URL srcURL = null;
	URL expectedURL = null;
	try {
	    srcURL = new URL(PlugletStreamInfo_obj.getURL());
            expectedURL= new URL(context.getProperty("HTML_ROOT") + "/" + context.getProperty("EXPECTED_VALUE"));
	} catch (Exception e) {
	    TestContext.registerFAILED("Exception during compiling URLs: " + e);
	    return;
	}
	if (srcURL.equals(expectedURL)) {
	    TestContext.registerPASSED("Good value " + srcURL + " returned");
	    return;
	} else {
	    TestContext.registerFAILED("Value " + srcURL + " returned instead of " + expectedURL);
	}
   }
   /**
    *
    ***********************************************************
    *  Routine where OS specific checks are made. 
    *
    *  @param   os      OS Name (SunOS/Linus/MacOS/...)
    ***********************************************************
    *
    */
   private void osRoutine(String os)
   {
     if(os == null) return;

     os = os.trim();
     if(os.compareTo("SunOS") == 0) {}
     else if(os.compareTo("Linux") == 0) {}
     else if(os.compareTo("Windows") == 0) {}
     else if(os.compareTo("MacOS") == 0) {}
     else {}
   }
}
