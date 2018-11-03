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
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.mozilla.webclient.impl.wrapper_native;

// GtkBrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControlCanvas;

import java.awt.Graphics;
import java.awt.Toolkit;
import java.awt.Dimension;

/**

 * GtkBrowserControlCanvas provides a concrete realization
 * of the RaptorCanvas for Gtk.

 * <B>Lifetime And Scope</B> <P>

 * There is one instance of GtkBrowserControlCanvas per top level awt Frame.

 * @version $Id$
 * 
 * @see	org.mozilla.webclient.BrowserControlCanvasFactory
 * 

 */


public class GtkBrowserControlCanvas extends BrowserControlCanvas /* implements ActionListener*/  {

    private boolean firstTime = true;
    private int gtkWinID;
    private int gtkTopWindow;
    private int canvasWinID;
    private int gtkWinPtr;
    // We don't need this, now that we use the JAWT Native Interface
    //    private MDrawingSurfaceInfo drawingSurfaceInfo;

    native int createTopLevelWindow();
    native int createContainerWindow(int parent, int width, int height);
    native int getGTKWinID(int gtkWinPtr);
    native void reparentWindow(int child, int parent);
    native void processEvents();
    native void setGTKWindowSize(int gtkWinPtr, int width, int height);
    //New method for obtaining access to the Native Peer handle
    native int getHandleToPeer();

    public GtkBrowserControlCanvas() {
        super();
        
        this.gtkWinID = 0;
        this.canvasWinID = 0;
        this.gtkWinPtr = 0;
        // We don't need this, now that we use the JAWT Native Interface
        //        this.drawingSurfaceInfo = null;

    }

    public void paint(Graphics g) {
        super.paint(g);
        
        if (firstTime) {
            synchronized(getTreeLock()) {
                //Use the AWT Native Peer interface to get the handle
                //of this Canvas's native peer
                Integer canvasWin = (Integer)
		    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
			    public Object run() {
				Integer result = 
				    new Integer(GtkBrowserControlCanvas.this.getHandleToPeer());
				return result;
			    }
			});
                canvasWinID = canvasWin.intValue();
                //Set our canvas as a parent of the top-level gtk widget
                //which contains Mozilla.
		NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
			public Object run() {
			    GtkBrowserControlCanvas.this.reparentWindow(GtkBrowserControlCanvas.this.gtkWinID, GtkBrowserControlCanvas.this.canvasWinID);
			    return null;
			}
		    });
		firstTime = false;
            }
        }
    }

    public void setBounds(int x, int y, int width, int height) {
        super.setBounds(x, y, width, height);
	final int finalWidth = width;
	final int finalHeight = height;

        synchronized(getTreeLock()) {
	    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
		    public Object run() {
			GtkBrowserControlCanvas.this.setGTKWindowSize(GtkBrowserControlCanvas.this.gtkTopWindow, 
					      finalWidth, finalHeight);
			return null;
		    }
		});
        }
    }

    /**
     * Needed for the hashtable look up of gtkwinid <-> WebShellInitContexts
     */
    public int getGTKWinPtr() {
        return this.gtkWinPtr;
    }

	/**
	 * Create the top-level gtk window to be embedded in our AWT
     * Window and return a handle to this window
	 *
	 * @returns The native window handle. 
	 */
    
    protected int getWindow() {
        synchronized(getTreeLock()) {
	    Integer topWindow = (Integer)
		NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
			public Object run() {
			    Integer result = 
				new Integer(GtkBrowserControlCanvas.this.createTopLevelWindow());
			    return result;
			}
		    });
            this.gtkTopWindow = topWindow.intValue();
	    
            final Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();
	    Integer winPtr = (Integer)
		NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
			public Object run() {
			    Integer result = 
				new Integer(GtkBrowserControlCanvas.this.createContainerWindow(GtkBrowserControlCanvas.this.gtkTopWindow, screenSize.width, screenSize.height));
			    return result;
			}
		    });
            this.gtkWinPtr = winPtr.intValue();
	    
	    Integer winId = (Integer)
		NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
			public Object run() {
			    Integer result = new Integer(GtkBrowserControlCanvas.this.getGTKWinID(GtkBrowserControlCanvas.this.gtkWinPtr));
			    return result;
			}
		    });
            
            this.gtkWinID = winId.intValue();
	}
	
	return this.gtkWinPtr;
    }

    // The test for this class is TestGtkBrowserControlCanvas
    
}

