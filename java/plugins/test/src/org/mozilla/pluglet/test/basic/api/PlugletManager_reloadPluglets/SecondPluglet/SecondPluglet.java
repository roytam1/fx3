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
 */
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.io.*;
import java.awt.print.*;
import java.awt.Frame;
import org.mozilla.pluglet.test.basic.TestContext;
import org.mozilla.pluglet.test.basic.api.PlugletManager_reloadPluglets_2;
public class SecondPluglet implements PlugletFactory {
    public SecondPluglet() {
    }
    /**
     * Creates a new pluglet instance, based on a MIME type. This
     * allows different impelementations to be created depending on
     * the specified MIME type.
     */
    public Pluglet createPluglet(String mimeType) {
 	return new TestInstance();
    }
    /**
     * Initializes the pluglet and will be called before any new instances are
     * created.
     */
    public void initialize(PlugletManager manager) {	
    }
    /**
     * Called when the browser is done with the pluglet factory, or when
     * the pluglet is disabled by the user.
     */
    public void shutdown() {
    }
}

class TestInstance implements Pluglet {
    PlugletPeer peer;
    public TestInstance() {
      TestContext.registerPASSED("Second pluglet loaded");
    }
    /**
     * Initializes a newly created pluglet instance, passing to it the pluglet
     * instance peer which it should use for all communication back to the browser.
     * 
     * @param peer the corresponding pluglet instance peer
     */

    public void initialize(PlugletPeer peer) {
	this.peer = peer;
    }
    /**
     * Called to instruct the pluglet instance to start. This will be called after
     * the pluglet is first created and initialized, and may be called after the
     * pluglet is stopped (via the Stop method) if the pluglet instance is returned
     * to in the browser window's history.
     */
    public void start() {
    }
    /**
     * Called to instruct the pluglet instance to stop, thereby suspending its state.
     * This method will be called whenever the browser window goes on to display
     * another page and the page containing the pluglet goes into the window's history
     * list.
     */
    public void stop() {
    }
    /**
     * Called to instruct the pluglet instance to destroy itself. This is called when
     * it become no longer possible to return to the pluglet instance, either because 
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     */
    public void destroy() {
    }
    /**
     * Called to tell the pluglet that the initial src/data stream is
     * ready. 
     * 
     * @result PlugletStreamListener
     */
    public PlugletStreamListener newStream() {
	return new TestStreamListener();
    }
    /**
     * Called when the window containing the pluglet instance changes.
     *
     * @param frame the pluglet panel
     */
    public void setWindow(Frame frame) {
    }
     /**
     * Called to instruct the pluglet instance to print itself to a printer.
     *
     * @param print printer information.
     */ 
    public void print(PrinterJob printerJob) {
    }
}

class TestStreamListener implements PlugletStreamListener {
    public TestStreamListener() {
    }
    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     */
    public void onStartBinding(PlugletStreamInfo streamInfo) {
    }
    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param input  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the stream.
     */
    public void onDataAvailable(PlugletStreamInfo plugletInfo, InputStream input,int  length) {
    }
    public void onFileAvailable(PlugletStreamInfo plugletInfo, String fileName) {
    }
    /**
     * Notify the observer that the URL has finished loading. 
     */
    public void onStopBinding(PlugletStreamInfo plugletInfo,int status) {
    }
    public int  getStreamType() {
	return 1; //:)
    }
}






