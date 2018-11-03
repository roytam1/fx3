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
 * The Initial Developer of the Original Code is Frank
 * Mitchell. Portions created by Frank Mitchell are
 * Copyright (C) 1999 Frank Mitchell. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Frank Mitchell (frank.mitchell@sun.com)
 */

import java.util.Vector;
import java.lang.reflect.Method;
import org.mozilla.xpcom.*;

public class XPCTest {
    static ComObject CreateSampleInstance() {
	nsID kSampleCID = 
	    new nsID("d3944dd0-ae1a-11d1-b66c-00805f8a2676");

	nsID kISampleIID = 
	    new nsID("57ecad90-ae1a-11d1-b66c-00805f8a2676");


	// XXX: convert to nsIComponentManager calls
	return (ComObject)XPComUtilities.CreateInstance(kSampleCID,
							null,
							kISampleIID);
    }

    public static void main(String[] argv) {
	String command = "PrintStats";
	int    cmdOffset = -1;
	boolean useXPCMethod = true;

	try {
	    Object[] params = null;

	    // Process arguments
	    if (argv.length == 0) {
		params = new Object[0];
	    }
	    else {
		int argi = 0;
		while (argv[argi].charAt(0) == '-') {
		    switch(argv[argi].charAt(1)) {
		    case 'r':
			useXPCMethod = false;
			break;
		    case 'x':
			useXPCMethod = true;
			break;
		    }
		    argi++;
		}

		command = argv[argi++];
		if (Character.isDigit(command.charAt(0))) {
		    cmdOffset = Integer.parseInt(command);
		}

		params = paramArray(argv, argi);
	    }

	    ComObject sample = CreateSampleInstance();

	    // DEBUG
	    System.out.print("Command: ");  // DEBUG
	    System.out.print(command);  // DEBUG
	    System.out.print(", arguments: ");  // DEBUG
	    System.out.println(paramString(params));  // DEBUG

	    nsID kISampleIID = 
		new nsID("57ecad90-ae1a-11d1-b66c-00805f8a2676");

	    if (cmdOffset >= 0) {
		XPComUtilities.InvokeMethodByIndex(kISampleIID, 
						   sample,
						   cmdOffset, 
						   params);
	    }
	    else if (useXPCMethod) {
		XPCMethod method = new XPCMethod(kISampleIID, command);

		method.invoke(sample, params);
	    }
	    else {
		Method[] methods = sample.getClass().getMethods();

		for (int i = 0; i < methods.length; i++) {
		    if (methods[i].getName().equals(command)) {
			Object retval =
			    methods[i].invoke(sample, params);
			System.out.print("Retval: ");  // DEBUG
			System.out.println(retval);  // DEBUG
			break;
		    }
		}
	    }

	    // Get "out" parameters and return value
	    System.out.print("Results: ");  // DEBUG
	    System.out.println(paramString(params));  // DEBUG

	}
	catch (Exception e) {
	    e.printStackTrace();
	}
    }

    static String paramString(Object[] params) {
	StringBuffer buf = new StringBuffer();
	buf.append('[');
	for (int i = 0; i < params.length; i++) {
	    if (i != 0) {
		buf.append(", ");
	    }
	    buf.append(params[i]);
	}
	buf.append(']');
	return buf.toString();
    }

    static Object[] paramArray(String[] argv) throws NumberFormatException {
	return paramArray(argv, 0);
    }

    static Object[] paramArray(String[] argv, int argi)
	throws NumberFormatException {
	Vector vector = new Vector(argv.length - argi);

	while (argi < argv.length) {
	    String opt = argv[argi];
	    if (opt.charAt(0) != '-') {
		vector.addElement(opt);
	    }
	    else {
		switch (opt.charAt(1)) {
		case 'c':
		    argi++;
		    vector.addElement(new Character(argv[argi].charAt(0)));
		    break;
		case 'b':
		    argi++;
		    vector.addElement(Byte.valueOf(argv[argi]));
		    break;
		case 's':
		    argi++;
		    vector.addElement(Short.valueOf(argv[argi]));
		    break;
		case 'i':
		    argi++;
		    vector.addElement(Integer.valueOf(argv[argi]));
		    break;
		case 'j':
		case 'l':
		    argi++;
		    vector.addElement(Long.valueOf(argv[argi]));
		    break;
		case 'f':
		    argi++;
		    vector.addElement(Float.valueOf(argv[argi]));
		    break;
		case 'd':
		    argi++;
		    vector.addElement(Double.valueOf(argv[argi]));
		    break;
		case 'r':
		case '0':
		    vector.addElement(null);
		    break;
		case '@':
		    argi++;
		    vector.addElement(new nsID(argv[argi]));
		    break;
		case 'z':
		    argi++;
		    vector.addElement(Boolean.valueOf(argv[argi]));
		    break;
		}
	    }
	    argi++;
	}

	Object[] result = new Object[vector.size()];
	for (int i = 0; i < result.length; i++) {
	    result[i] = vector.elementAt(i);
	}
	return result;
    }
}
