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

import java.util.*;
import java.awt.*;
import java.applet.*;
import java.text.*;
import netscape.javascript.*;

public class LC1 extends Applet implements Runnable {
    Thread   timer;                // The thread that displays smth
    int      max = 0, counter, prev_counter;
    JSObject jso;
    String   msg;
    Font     font1;
    Color    textColor;
    
    public void init() {
	prev_counter = -1;
	msg = getParameter("message");
	if (msg == null) msg = "Default message";
	try {
	    max = Integer.parseInt(getParameter("count")); 
	}
	catch (Exception e) {
	    e.printStackTrace();
	    max = 30;
	}
	counter = max;	

	font1 = new Font("Serif", Font.BOLD, 45);
	setBackground(new Color(0x00ffff));
	textColor = Color.red;	    
	try
	    {
		jso = JSObject.getWindow(this);
                System.out.println("window.location="+
				   jso.getMember("location"));

	    }
	catch(JSException e)
	    {
		System.err.println("JSException: "+e);
                return;
	    }
    }    

    public void paint(Graphics g) {
	g.setFont(font1);
	g.setColor(getBackground());
	if (prev_counter >= 0) 
	    g.drawString(new Integer(prev_counter).toString(), 60, 70);
	g.setColor(textColor);
	g.drawString(new Integer(counter).toString(), 60, 70);
    }

    public void start() {
        timer = new Thread(this);
        timer.start();
    }

    public void stop() {
        timer = null;
    }

    public void run() {
        Thread me = Thread.currentThread();
        while (timer == me) {
            try {
                Thread.currentThread().sleep(1000);		
            } catch (InterruptedException e) {
            }
	    prev_counter = counter;
	    counter--;
	    if (counter == -1)
	    {
		counter = max;
             	int x = (int)(Math.random()*400);
		int y = (int)(Math.random()*400);
		try {
		    jso.call("moveTo", 
			     new Object[] { new Integer(x), new Integer(y) });
		} catch (Exception e) {
		    System.err.println("got JS exception: "+e);
		}
	    }
            repaint();
        }
    }

    public void update(Graphics g) {
        paint(g);
    }

    public String getAppletInfo() {
        return "Title: LiveConnect Sample.";
    }
    
    public void setLimit(String limit)
    {
	int l = Integer.parseInt(limit);
	setLimit(l);
    }

    public void setLimit(int limit)
    {
	prev_counter = counter;
	max = counter = limit;
	repaint();
    }
}
