/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id$
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

// XXX: this class depends on sun.awt.*
package sun.jvmp.generic.motif;

import sun.jvmp.*;
import java.awt.*;

public class Plugin extends PluggableJVM 
{
    static Plugin  real_instance = null;

    protected Plugin() 
    {
	super();
	instance = this;
    };
  
    
    protected static PluggableJVM startJVM(String codebase, int debug) 
    {
	debugLevel = debug;
	
	// for now, only one instance of JVM is allowed - can be fixed easily
	if (real_instance == null)
	    {	
		
		real_instance  = new Plugin();
		// don't do nothing until init is complete
		real_instance.init(codebase, true);
		real_instance.start();
	    };
	return real_instance;
    };
      
  protected NativeFrame SysRegisterWindow(int handle, 
					  int width, int height) 
  {
      NativeFrame nf;
      if (!initedNative)
	  {
	      loadLibrary();
	  }
      long widget = getWidget(handle, width, height, 0, 0);
      Frame f = createEmbeddedFrame(widget);
      if (f == null) 
	  {
	      trace("Creation of frame failed", LOG_ERROR);
	      return null;
	  }
      f.setBounds(0, 0, width, height);
      f.setResizable(false);
      nf = new NativeFrame(f);
      return nf;
  }
  
  protected NativeFrame SysUnregisterWindow(int id) 
  {
    NativeFrame nf = (NativeFrame)nativeFrames.get(id);
    if (nf == null) return null;
    Frame f = nf.getFrame();
    f.removeAll();
    f.removeNotify();
    return nf;
  };
   
  protected SynchroObject SysRegisterMonitorObject(long handle)
  {
      SynchroObject so = null;
      try {
	  so = new PthreadSynchroObject(handle);
      } catch (Exception e) {
	  trace("SysRegisterMonitorObject failed: "+e, LOG_ERROR);
	  return null;
      }
      return so;
  }

  protected SynchroObject SysUnregisterMonitorObject(int id)
  {
      SynchroObject so = getSynchroObjectWithID(id);
      if (so == null) return null;
      so._destroy();
      return so;
  }
    
    /* 
       this is a separate method as MEmbeddedFrame constructor arguments
       differs in 1.1 and 1.2. 
       and maybe we should use reflection (like original plugin)
       to call proper constructor 
    */
  private Frame createEmbeddedFrame(long widget) 
    {
	return new sun.awt.motif.MEmbeddedFrame(widget);
    };
    
  static private native long getWidget(int winid, int width, 
				       int height, int x, int y);
};
