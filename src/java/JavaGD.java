//
//  JavaGD - Java Graphics Device for R
//  JavaGD.java - default GDInterface implementation for use in JavaGD
// 
//  Copyright (C) 2004-2009  Simon Urbanek
// 
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation;
//  version 2.1 of the License.
//  
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//  
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//

package org.rosuda.javaGD;

import java.awt.*;
import java.awt.event.*;
import java.lang.reflect.Method;

/** JavaGD is an implementation of the {@link GDInterface} protocol which displays the R graphics in an AWT window (via {@link GDCanvas}). It can be used as an example gfor implementing custom display classes which can then be used by JavaGD. Three sample back-ends are included in the JavaGD sources: {@link GDCanvas} (AWT), {@link JGDPanel} (Swing) and {@link JGDBufferedPanel} (Swing with cached update). */
public class JavaGD extends GDInterface implements WindowListener {
    /** frame containing the graphics canvas */ 
    public Frame f;
    
    /** default, public constructor - creates a new JavaGD instance. The actual window (and canvas) is not created until {@link #gdOpen} is called. */
    public JavaGD() {
        super();
    }
    
    /** creates a new graphics window containing a canvas
     *  @param w width of the canvas
     *  @param h height of the canvas */
    public void     gdOpen(double w, double h) {
        if (f!=null) gdClose();

        f=new Frame("JavaGD");
        f.addWindowListener(this);
        c=new GDCanvas(w, h);
        f.add((GDCanvas)c);
        f.pack();
        f.setVisible(true);
    }

    public void     gdActivate() {
        super.gdActivate();
        if (f!=null) {
            f.requestFocus();
            f.setTitle("JavaGD "+((devNr>0)?("("+(devNr+1)+")"):"")+" *active*");
        }
    }

    public void     gdClose() {
        super.gdClose();
        if (f!=null) {
            c=null;
            f.removeAll();
            f.dispose();
            f=null;
        }
    }

    public void     gdDeactivate() {
        super.gdDeactivate();
        if (f!=null) f.setTitle("JavaGD "+((devNr>0)?("("+(devNr+1)+")"):""));
    }

    public void     gdNewPage(int devNr) { // new API: provides the device Nr.
        super.gdNewPage(devNr);
        if (f!=null) f.setTitle("JavaGD ("+(devNr+1)+")"+(active?" *active*":""));
    }

    /*-- WindowListener interface methods */
    
    /** listener response to "Close" - effectively invokes <code>dev.off()</code> on the device */
    public void windowClosing(WindowEvent e) {
        if (c!=null) executeDevOff();
    }
    public void windowClosed(WindowEvent e) {}
    public void windowOpened(WindowEvent e) {}
    public void windowIconified(WindowEvent e) {}
    public void windowDeiconified(WindowEvent e) {}
    public void windowActivated(WindowEvent e) {}
    public void windowDeactivated(WindowEvent e) {}
    
}
