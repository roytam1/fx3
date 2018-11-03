/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino JavaScript Debugger code, released
 * November 21, 2000.
 *
 * The Initial Developer of the Original Code is SeeBeyond Corporation.

 * Portions created by SeeBeyond are
 * Copyright (C) 2000 SeeBeyond Technology Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Matt Gould
 * Christopher Oliver
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.tools.debugger;

import java.io.InputStream;
import java.io.PrintStream;

import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.shell.Global;


public class Main implements ContextListener
{
    // The class implements ContextListener only for compatibility!

    Dim dim;
    SwingGui debugGui;

    /**
     * Class to consolidate all internal implementations of interfaces
     * to avoid class generation bloat.
     */
    private static class IProxy implements Runnable, ScopeProvider
    {
        static final int EXIT_ACTION = 1;
        static final int SCOPE_PROVIDER = 2;

        private final int type;
        Scriptable scope;

        IProxy(int type)
        {
            this.type = type;
        }

        public static ScopeProvider newScopeProvider(Scriptable scope)
        {
            IProxy scopeProvider = new IProxy(SCOPE_PROVIDER);
            scopeProvider.scope = scope;
            return scopeProvider;
        }

        public void run()
        {
            if (type != EXIT_ACTION) Kit.codeBug();
            System.exit(0);
        }

        public Scriptable getScope()
        {
            if (type != SCOPE_PROVIDER) Kit.codeBug();
            if (scope == null) Kit.codeBug();
            return scope;
        }
    }

    //
    // public interface
    //

    public Main(String title)
    {
        dim = new Dim();
        debugGui = new SwingGui(dim, title);
        dim.callback = debugGui;
    }

    public void doBreak() {
        dim.breakFlag = true;
    }

   /**
    *  Toggle Break-on-Exception behavior
    */
    public void setBreakOnExceptions(boolean value) {
        dim.breakOnExceptions = value;
        debugGui.menubar.breakOnExceptions.setSelected(value);
    }

   /**
    *  Toggle Break-on-Enter behavior
    */
    public void setBreakOnEnter(boolean value) {
        dim.breakOnEnter = value;
        debugGui.menubar.breakOnEnter.setSelected(value);
    }

   /**
    *  Toggle Break-on-Return behavior
    */
    public void setBreakOnReturn(boolean value) {
        dim.breakOnReturn = value;
        debugGui.menubar.breakOnReturn.setSelected(value);
    }

    /**
     *
     * Remove all breakpoints
     */
    public void clearAllBreakpoints()
    {
        dim.clearAllBreakpoints();
    }

   /**
    *  Resume Execution
    */
    public void go()
    {
        dim.go();
    }

    public void setScope(Scriptable scope)
    {
        setScopeProvider(IProxy.newScopeProvider(scope));
    }

    public void setScopeProvider(ScopeProvider p) {
        dim.scopeProvider = p;
    }

    /**
     * Assign a Runnable object that will be invoked when the user
     * selects "Exit..." or closes the Debugger main window
     */
    public void setExitAction(Runnable r) {
        debugGui.exitAction = r;
    }

    /**
     * Get an input stream to the Debugger's internal Console window
     */

    public InputStream getIn() {
        return debugGui.console.getIn();
    }

    /**
     * Get an output stream to the Debugger's internal Console window
     */

    public PrintStream getOut() {
        return debugGui.console.getOut();
    }

    /**
     * Get an error stream to the Debugger's internal Console window
     */

    public PrintStream getErr() {
        return debugGui.console.getErr();
    }

    public void pack()
    {
        debugGui.pack();
    }

    public void setSize(int w, int h)
    {
        debugGui.setSize(w, h);
    }

    /**
     * @deprecated Use {@link #setSize(int, int)} instead.
     */
    public void setSize(java.awt.Dimension dimension)
    {
        debugGui.setSize(dimension.width, dimension.height);
    }

    public void setVisible(boolean flag)
    {
        debugGui.setVisible(flag);
    }

    public boolean isVisible()
    {
        return debugGui.isVisible();
    }

    public void dispose()
    {
        debugGui.dispose();
    }

    public void attachTo(ContextFactory factory)
    {
        dim.attachTo(factory);
    }

    /**
     * @deprecated
     * The method does nothing and is only present for compatibility.
     */
    public void setOptimizationLevel(int level)
    {
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextEntered(Context cx)
    {
        throw new IllegalStateException();
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextExited(Context cx)
    {
        throw new IllegalStateException();
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextCreated(Context cx)
    {
        throw new IllegalStateException();
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextReleased(Context cx)
    {
        throw new IllegalStateException();
    }

    public static void main(String[] args)
    {
        Main main = new Main("Rhino JavaScript Debugger");
        main.doBreak();
        main.setExitAction(new IProxy(IProxy.EXIT_ACTION));

        System.setIn(main.getIn());
        System.setOut(main.getOut());
        System.setErr(main.getErr());

        Global global = org.mozilla.javascript.tools.shell.Main.getGlobal();
        global.setIn(main.getIn());
        global.setOut(main.getOut());
        global.setErr(main.getErr());

        main.attachTo(
            org.mozilla.javascript.tools.shell.Main.shellContextFactory);

        main.setScope(global);

        main.pack();
        main.setSize(600, 460);
        main.setVisible(true);

        org.mozilla.javascript.tools.shell.Main.exec(args);
    }

    public static void mainEmbedded(String title)
    {
        ContextFactory factory = ContextFactory.getGlobal();
        Global global = new Global();
        global.init(factory);
        mainEmbedded(factory, global, title);
    }

    // same as plain main(), stdin/out/err redirection removed and
    // explicit ContextFactory and scope
    public static void mainEmbedded(ContextFactory factory,
                                    Scriptable scope,
                                    String title)
    {
        mainEmbeddedImpl(factory, scope, title);
    }

    // same as plain main(), stdin/out/err redirection removed and
    // explicit ContextFactory and ScopeProvider
    public static void mainEmbedded(ContextFactory factory,
                                    ScopeProvider scopeProvider,
                                    String title)
    {
        mainEmbeddedImpl(factory, scopeProvider, title);
    }


    private static void mainEmbeddedImpl(ContextFactory factory,
                                         Object scopeProvider,
                                         String title)
    {
        if (title == null) {
            title = "Rhino JavaScript Debugger (embedded usage)";
        }
        Main main = new Main(title);
        main.doBreak();
        main.setExitAction(new IProxy(IProxy.EXIT_ACTION));

        main.attachTo(factory);
        if (scopeProvider instanceof ScopeProvider) {
            main.setScopeProvider((ScopeProvider)scopeProvider);
        } else {
            Scriptable scope = (Scriptable)scopeProvider;
            if (scope instanceof Global) {
                Global global = (Global)scope;
                global.setIn(main.getIn());
                global.setOut(main.getOut());
                global.setErr(main.getErr());
            }
            main.setScope(scope);
        }

        main.pack();
        main.setSize(600, 460);
        main.setVisible(true);
    }
}

