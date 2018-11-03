/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Jeff Galyan <jeffrey.galyan@sun.com>
 */

package grendel.ui;

import java.awt.AWTException;
import javax.swing.JWindow;
import javax.swing.JLabel;
import javax.swing.ImageIcon;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Rectangle;
import java.awt.Robot;
import java.awt.Toolkit;
import java.awt.event.FocusAdapter;
import java.awt.event.FocusEvent;

/**
 *Default screen to be shown while application is initializing. Run the
 *dispose method on this class when you're ready to close the splash screen.
 */
public class Splash extends JWindow {
    private Image img;
    private ImageIcon image;
    private Robot r;
    
    public Splash() {
        super();
        capture();
        image= new ImageIcon("ui/images/GrendelSplash.png");
        setSize(image.getIconWidth(),image.getIconHeight());
        
        Dimension screensize = Toolkit.getDefaultToolkit().getScreenSize();
        setLocation(screensize.width/2 - 150, screensize.height/2 - 150);
        
        setVisible(true);
    }
    
    
    public void capture() {
        Dimension d = Toolkit.getDefaultToolkit().getScreenSize();
        if (r ==null) {
            try {
                r = new Robot();
            } catch (AWTException awte) {
                awte.printStackTrace();
            }
        }
        img = r.createScreenCapture(new Rectangle(0, 0, d.width, d.height));
    }
    
    public void paint(Graphics g) {
        Rectangle rect = g.getClipBounds();
        g.drawImage(img, 0, 0, getWidth(), getHeight(), getX(), getY(),
                getX() + getWidth(), getY() + getHeight(), null);
        //g.drawImage(screen, 0, 0, getWidth(), getHeight(),Color.BLUE, null);
        image.paintIcon(this,g,0,0);
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String args[]) {
        java.awt.EventQueue.invokeLater(new Runnable() {
            public void run() {
                new Splash();
            }
        });
    }
}
