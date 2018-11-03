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

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.*;
import java.util.*;
import java.io.*;
import java.net.URL;

/**
 * Dim or Debugger Implementation for Rhino.
*/
class Dim {

    static final int STEP_OVER = 0;
    static final int STEP_INTO = 1;
    static final int STEP_OUT = 2;
    static final int GO = 3;
    static final int BREAK = 4;
    static final int EXIT = 5;

    GuiCallback callback;

    boolean breakFlag = false;

    ScopeProvider scopeProvider;

    int frameIndex = -1;

    private volatile ContextData interruptedContextData = null;

    ContextFactory contextFactory;
    private Object monitor = new Object();
    private Object eventThreadMonitor = new Object();
    private volatile int returnValue = -1;
    private boolean insideInterruptLoop;
    private String evalRequest;
    private StackFrame evalFrame;
    private String evalResult;

    boolean breakOnExceptions;
    boolean breakOnEnter;
    boolean breakOnReturn;

    private final Hashtable urlToSourceInfo = new Hashtable();
    private final Hashtable functionNames = new Hashtable();
    private final Hashtable functionToSource = new Hashtable();

    static class ContextData
    {
        static ContextData get(Context cx) {
            return (ContextData)cx.getDebuggerContextData();
        }

        int frameCount() {
            return frameStack.size();
        }

        StackFrame getFrame(int frameNumber) {
            return (StackFrame) frameStack.get(frameStack.size() - frameNumber - 1);
        }

        void pushFrame(StackFrame frame) {
            frameStack.push(frame);
        }

        void popFrame() {
            frameStack.pop();
        }

        ObjArray frameStack = new ObjArray();
        boolean breakNextLine;
        int stopAtFrameDepth = -1;
        boolean eventThreadFlag;
        Throwable lastProcessedException;
    }

    static class StackFrame implements DebugFrame {

        StackFrame(Context cx, Dim dim, FunctionSource fsource)
        {
            this.dim = dim;
            this.contextData = ContextData.get(cx);
            this.fsource = fsource;
            this.breakpoints = fsource.sourceInfo().breakpoints;
            this.lineNumber = fsource.firstLine();
        }

        public void onEnter(Context cx, Scriptable scope,
                            Scriptable thisObj, Object[] args)
        {
            contextData.pushFrame(this);
            this.scope = scope;
            this.thisObj = thisObj;
            if (dim.breakOnEnter) {
                dim.handleBreakpointHit(this, cx);
            }
        }

        public void onLineChange(Context cx, int lineno)
        {
            this.lineNumber = lineno;

            if (!breakpoints[lineno] && !dim.breakFlag) {
                boolean lineBreak = contextData.breakNextLine;
                if (lineBreak && contextData.stopAtFrameDepth >= 0) {
                    lineBreak = (contextData.frameCount()
                                 <= contextData.stopAtFrameDepth);
                }
                if (!lineBreak) {
                    return;
                }
                contextData.stopAtFrameDepth = -1;
                contextData.breakNextLine = false;
            }

            dim.handleBreakpointHit(this, cx);
        }

        public void onExceptionThrown(Context cx, Throwable exception)
        {
            dim.handleExceptionThrown(cx, exception, this);
        }

        public void onExit(Context cx, boolean byThrow, Object resultOrException)
        {
            if (dim.breakOnReturn && !byThrow) {
                dim.handleBreakpointHit(this, cx);
            }
            contextData.popFrame();
        }

        SourceInfo sourceInfo() {
            return fsource.sourceInfo();
        }

        ContextData contextData()
        {
            return contextData;
        }

        Object scope()
        {
            return scope;
        }

        Object thisObj()
        {
            return thisObj;
        }

        String getUrl()
        {
            return fsource.sourceInfo().url();
        }

        int getLineNumber() {
            return lineNumber;
        }

        private Dim dim;
        private ContextData contextData;
        private Scriptable scope;
        private Scriptable thisObj;
        private FunctionSource fsource;
        private boolean[] breakpoints;
        private int lineNumber;
    }

    static class FunctionSource
    {
        private SourceInfo sourceInfo;
        private int firstLine;
        private String name;

        FunctionSource(SourceInfo sourceInfo, int firstLine, String name)
        {
            if (name == null) throw new IllegalArgumentException();
            this.sourceInfo = sourceInfo;
            this.firstLine = firstLine;
            this.name = name;
        }

        SourceInfo sourceInfo()
        {
            return sourceInfo;
        }

        int firstLine()
        {
            return firstLine;
        }

        String name()
        {
            return name;
        }
    }

    static class SourceInfo
    {
        private String source;
        private String url;

        private int minLine;
        private boolean[] breakableLines;
        boolean[] breakpoints;

        private static final boolean[] EMPTY_BOOLEAN_ARRAY = new boolean[0];

        private FunctionSource[] functionSources;

        SourceInfo(String source, DebuggableScript[] functions,
                   String normilizedUrl)
        {
            this.source = source;
            this.url = normilizedUrl;

            int N = functions.length;
            int[][] lineArrays = new int[N][];
            for (int i = 0; i != N; ++i) {
                lineArrays[i] = functions[i].getLineNumbers();
            }

            int minAll = 0, maxAll = -1;
            int[] firstLines = new int[N];
            for (int i = 0; i != N; ++i) {
                int[] lines = lineArrays[i];
                if (lines == null || lines.length == 0) {
                    firstLines[i] = -1;
                } else {
                    int min, max;
                    min = max = lines[0];
                    for (int j = 1; j != lines.length; ++j) {
                        int line = lines[j];
                        if (line < min) {
                            min = line;
                        } else if (line > max) {
                            max = line;
                        }
                    }
                    firstLines[i] = min;
                    if (minAll > maxAll) {
                        minAll = min;
                        maxAll = max;
                    } else {
                        if (min < minAll) {
                            minAll = min;
                        }
                        if (max > maxAll) {
                            maxAll = max;
                        }
                    }
                }
            }

            if (minAll > maxAll) {
                // No line information
                this.minLine = -1;
                this.breakableLines = EMPTY_BOOLEAN_ARRAY;
                this.breakpoints = EMPTY_BOOLEAN_ARRAY;
            } else {
                if (minAll < 0) {
                    // Line numbers can not be negative
                    throw new IllegalStateException(String.valueOf(minAll));
                }
                this.minLine = minAll;
                int linesTop = maxAll + 1;
                this.breakableLines = new boolean[linesTop];
                this.breakpoints = new boolean[linesTop];
                for (int i = 0; i != N; ++i) {
                    int[] lines = lineArrays[i];
                    if (lines != null && lines.length != 0) {
                        for (int j = 0; j != lines.length; ++j) {
                            int line = lines[j];
                            this.breakableLines[line] = true;
                        }
                    }
                }
            }
            this.functionSources = new FunctionSource[N];
            for (int i = 0; i != N; ++i) {
                String name = functions[i].getFunctionName();
                if (name == null) {
                    name = "";
                }
                this.functionSources[i]
                    = new FunctionSource(this, firstLines[i], name);
            }
        }

        String source()
        {
            return this.source;
        }

        String url()
        {
            return this.url;
        }

        int functionSourcesTop()
        {
            return functionSources.length;
        }

        FunctionSource functionSource(int i)
        {
            return functionSources[i];
        }

        void copyBreakpointsFrom(SourceInfo old)
        {
            int end = old.breakpoints.length;
            if (end > this.breakpoints.length) {
                end = this.breakpoints.length;
            }
            for (int line = 0; line != end; ++line) {
                if (old.breakpoints[line]) {
                    this.breakpoints[line] = true;
                }
            }
        }

        boolean breakableLine(int line)
        {
            return (line < this.breakableLines.length)
                   && this.breakableLines[line];
        }

        boolean breakpoint(int line)
        {
            if (!breakableLine(line)) {
                throw new IllegalArgumentException(String.valueOf(line));
            }
            return line < this.breakpoints.length && this.breakpoints[line];
        }

        boolean breakpoint(int line, boolean value)
        {
            if (!breakableLine(line)) {
                throw new IllegalArgumentException(String.valueOf(line));
            }
            boolean changed;
            synchronized (breakpoints) {
                if (breakpoints[line] != value) {
                    breakpoints[line] = value;
                    changed = true;
                } else {
                    changed = false;
                }
            }
            return changed;
        }

        void removeAllBreakpoints()
        {
            synchronized (breakpoints) {
                for (int line = 0; line != breakpoints.length; ++line) {
                    breakpoints[line] = false;
                }
            }
        }
    }

    private static final int IPROXY_DEBUG = 0;
    private static final int IPROXY_LISTEN = 1;
    private static final int IPROXY_COMPILE_SCRIPT = 2;
    private static final int IPROXY_EVAL_SCRIPT = 3;
    private static final int IPROXY_STRING_IS_COMPILABLE = 4;
    private static final int IPROXY_OBJECT_TO_STRING = 5;
    private static final int IPROXY_OBJECT_PROPERTY = 6;
    private static final int IPROXY_OBJECT_IDS = 7;

    /**
     * Proxy class to implement debug interfaces without bloat of class
     * files.
     */
    private static class DimIProxy
        implements ContextAction, ContextFactory.Listener, Debugger
    {
        private Dim dim;
        private int type;

        String url;
        String text;
        Object object;
        Object id;

        boolean booleanResult;
        String stringResult;
        Object objectResult;
        Object[] objectArrayResult;

        DimIProxy(Dim dim, int type)
        {
            this.dim = dim;
            this.type = type;
        }

        // ContextAction interface

        public Object run(Context cx)
        {
            switch (type) {
              case IPROXY_COMPILE_SCRIPT:
                cx.compileString(text, url, 1, null);
                break;

              case IPROXY_EVAL_SCRIPT:
                {
                    Scriptable scope = null;
                    if (dim.scopeProvider != null) {
                        scope = dim.scopeProvider.getScope();
                    }
                    if (scope == null) {
                        scope = new ImporterTopLevel(cx);
                    }
                    cx.evaluateString(scope, text, url, 1, null);
                }
                break;

              case IPROXY_STRING_IS_COMPILABLE:
                booleanResult = cx.stringIsCompilableUnit(text);
                break;

              case IPROXY_OBJECT_TO_STRING:
                if (object == Undefined.instance) {
                    stringResult = "undefined";
                } else if (object == null) {
                    stringResult = "null";
                } else if (object instanceof NativeCall) {
                    stringResult = "[object Call]";
                } else {
                    stringResult = Context.toString(object);
                }
                break;

              case IPROXY_OBJECT_PROPERTY:
                objectResult = dim.getObjectPropertyImpl(cx, object, id);
                break;

              case IPROXY_OBJECT_IDS:
                objectArrayResult = dim.getObjectIdsImpl(cx, object);
                break;

              default:
                throw Kit.codeBug();
            }
            return null;
        }

        void withContext()
        {
            dim.contextFactory.call(this);
        }

        // ContextFactory.Listener interface

        public void contextCreated(Context cx)
        {
            if (type != IPROXY_LISTEN) Kit.codeBug();
            ContextData contextData = new ContextData();
            Debugger debugger = new DimIProxy(dim, IPROXY_DEBUG);
            cx.setDebugger(debugger, contextData);
            cx.setGeneratingDebug(true);
            cx.setOptimizationLevel(-1);
        }

        public void contextReleased(Context cx)
        {
            if (type != IPROXY_LISTEN) Kit.codeBug();
        }

        // Debugger interface

        public DebugFrame getFrame(Context cx, DebuggableScript fnOrScript)
        {
            if (type != IPROXY_DEBUG) Kit.codeBug();

            FunctionSource item = dim.getFunctionSource(fnOrScript);
            if (item == null) {
                // Can not debug if source is not available
                return null;
            }
            return new StackFrame(cx, dim, item);
        }

        public void handleCompilationDone(Context cx,
                                          DebuggableScript fnOrScript,
                                          String source)
        {
            if (type != IPROXY_DEBUG) Kit.codeBug();

            if (!fnOrScript.isTopLevel()) {
                return;
            }
            dim.registerTopScript(fnOrScript, source);
        }
    }

    void attachTo(ContextFactory factory)
    {
        this.contextFactory = factory;
        factory.addListener(new DimIProxy(this, IPROXY_LISTEN));
    }

    FunctionSource getFunctionSource(DebuggableScript fnOrScript)
    {
        FunctionSource fsource = functionSource(fnOrScript);
        if (fsource == null) {
            String url = getNormilizedUrl(fnOrScript);
            SourceInfo si = sourceInfo(url);
            if (si == null) {
                if (!fnOrScript.isGeneratedScript()) {
                    // Not eval or Function, try to load it from URL
                    String source = loadSource(url);
                    if (source != null) {
                        DebuggableScript top = fnOrScript;
                        for (;;) {
                            DebuggableScript parent = top.getParent();
                            if (parent == null) {
                                break;
                            }
                            top = parent;
                        }
                        registerTopScript(top, source);
                        fsource = functionSource(fnOrScript);
                    }
                }
            }
        }
        return fsource;
    }

    private String loadSource(String sourceUrl)
    {
        String source = null;
        int hash = sourceUrl.indexOf('#');
        if (hash >= 0) {
            sourceUrl = sourceUrl.substring(0, hash);
        }
        try {
            InputStream is;
          openStream:
            {
                if (sourceUrl.indexOf(':') < 0) {
                    // Can be a file name
                    try {
                        if (sourceUrl.startsWith("~/")) {
                            String home = System.getProperty("user.home");
                            if (home != null) {
                                String pathFromHome = sourceUrl.substring(2);
                                File f = new File(new File(home), pathFromHome);
                                if (f.exists()) {
                                    is = new FileInputStream(f);
                                    break openStream;
                                }
                            }
                        }
                        File f = new File(sourceUrl);
                        if (f.exists()) {
                            is = new FileInputStream(f);
                            break openStream;
                        }
                    } catch (SecurityException ex) { }
                    // No existing file, assume missed http://
                    if (sourceUrl.startsWith("//")) {
                        sourceUrl = "http:" + sourceUrl;
                    } else if (sourceUrl.startsWith("/")) {
                        sourceUrl = "http://127.0.0.1" + sourceUrl;
                    } else {
                        sourceUrl = "http://" + sourceUrl;
                    }
                }

                is = (new URL(sourceUrl)).openStream();
            }

            try {
                source = Kit.readReader(new InputStreamReader(is));
            } finally {
                is.close();
            }
        } catch (IOException ex) {
            System.err.println
                ("Failed to load source from "+sourceUrl+": "+ ex);
        }
        return source;
    }

    void registerTopScript(DebuggableScript topScript, String source)
    {
        if (!topScript.isTopLevel()) {
            throw new IllegalArgumentException();
        }
        String url = getNormilizedUrl(topScript);
        DebuggableScript[] functions = getAllFunctions(topScript);
        final SourceInfo sourceInfo = new SourceInfo(source, functions, url);

        synchronized (urlToSourceInfo) {
            SourceInfo old = (SourceInfo)urlToSourceInfo.get(url);
            if (old != null) {
                sourceInfo.copyBreakpointsFrom(old);
            }
            urlToSourceInfo.put(url, sourceInfo);
            for (int i = 0; i != sourceInfo.functionSourcesTop(); ++i) {
                FunctionSource fsource = sourceInfo.functionSource(i);
                String name = fsource.name();
                if (name.length() != 0) {
                    functionNames.put(name, fsource);
                }
            }
        }

        synchronized (functionToSource) {
            for (int i = 0; i != functions.length; ++i) {
                FunctionSource fsource = sourceInfo.functionSource(i);
                functionToSource.put(functions[i], fsource);
            }
        }

        callback.updateSourceText(sourceInfo);
    }

    FunctionSource functionSource(DebuggableScript fnOrScript)
    {
        return (FunctionSource)functionToSource.get(fnOrScript);
    }

    String[] functionNames()
    {
        String[] a;
        synchronized (urlToSourceInfo) {
            Enumeration e = functionNames.keys();
            a = new String[functionNames.size()];
            int i = 0;
            while (e.hasMoreElements()) {
                a[i++] = (String)e.nextElement();
            }
        }
        return a;
    }

    FunctionSource functionSourceByName(String functionName)
    {
        return (FunctionSource)functionNames.get(functionName);
    }

    SourceInfo sourceInfo(String url)
    {
        return (SourceInfo)urlToSourceInfo.get(url);
    }

    String getNormilizedUrl(DebuggableScript fnOrScript)
    {
        String url = fnOrScript.getSourceName();
        if (url == null) { url = "<stdin>"; }
        else {
            // Not to produce window for eval from different lines,
            // strip line numbers, i.e. replace all #[0-9]+\(eval\) by
            // (eval)
            // Option: similar teatment for Function?
            char evalSeparator = '#';
            StringBuffer sb = null;
            int urlLength = url.length();
            int cursor = 0;
            for (;;) {
                int searchStart = url.indexOf(evalSeparator, cursor);
                if (searchStart < 0) {
                    break;
                }
                String replace = null;
                int i = searchStart + 1;
                boolean hasDigits = false;
                while (i != urlLength) {
                    int c = url.charAt(i);
                    if (!('0' <= c && c <= '9')) {
                        break;
                    }
                    ++i;
                }
                if (i != searchStart + 1) {
                    // i points after #[0-9]+
                    if ("(eval)".regionMatches(0, url, i, 6)) {
                        cursor = i + 6;
                        replace = "(eval)";
                    }
                }
                if (replace == null) {
                    break;
                }
                if (sb == null) {
                    sb = new StringBuffer();
                    sb.append(url.substring(0, searchStart));
                }
                sb.append(replace);
            }
            if (sb != null) {
                if (cursor != urlLength) {
                    sb.append(url.substring(cursor));
                }
                url = sb.toString();
            }
        }
        return url;
    }

    private static DebuggableScript[] getAllFunctions(DebuggableScript function)
    {
        ObjArray functions = new ObjArray();
        collectFunctions_r(function, functions);
        DebuggableScript[] result = new DebuggableScript[functions.size()];
        functions.toArray(result);
        return result;
    }

    private static void collectFunctions_r(DebuggableScript function,
                                           ObjArray array)
    {
        array.add(function);
        for (int i = 0; i != function.getFunctionCount(); ++i) {
            collectFunctions_r(function.getFunction(i), array);
        }
    }

    void clearAllBreakpoints()
    {
        Enumeration e = urlToSourceInfo.elements();
        while (e.hasMoreElements()) {
            SourceInfo si = (SourceInfo)e.nextElement();
            si.removeAllBreakpoints();
        }
    }

    void handleBreakpointHit(StackFrame frame, Context cx) {
        breakFlag = false;
        interrupted(cx, frame, null);
    }

    void handleExceptionThrown(Context cx, Throwable ex, StackFrame frame) {
        if (breakOnExceptions) {
            ContextData cd = frame.contextData();
            if (cd.lastProcessedException != ex) {
                interrupted(cx, frame, ex);
                cd.lastProcessedException = ex;
            }
        }
    }

    /* end Debugger interface */

    void contextSwitch (int frameIndex) {
        this.frameIndex = frameIndex;
    }

    ContextData currentContextData() {
        return interruptedContextData;
    }

    void setReturnValue(int returnValue)
    {
        synchronized (monitor) {
            this.returnValue = returnValue;
            monitor.notify();
        }
    }

    void go()
    {
        synchronized (monitor) {
            this.returnValue = GO;
            monitor.notifyAll();
        }
    }

    String eval(String expr)
    {
        String result = "undefined";
        if (expr == null) {
            return result;
        }
        ContextData contextData = currentContextData();
        if (contextData == null || frameIndex >= contextData.frameCount()) {
            return result;
        }
        StackFrame frame = contextData.getFrame(frameIndex);
        if (contextData.eventThreadFlag) {
            Context cx = Context.getCurrentContext();
            result = do_eval(cx, frame, expr);
        } else {
            synchronized (monitor) {
                if (insideInterruptLoop) {
                    evalRequest = expr;
                    evalFrame = frame;
                    monitor.notify();
                    do {
                        try {
                            monitor.wait();
                        } catch (InterruptedException exc) {
                            Thread.currentThread().interrupt();
                            break;
                        }
                    } while (evalRequest != null);
                    result = evalResult;
                }
            }
        }
        return result;
    }

    void compileScript(String url, String text)
    {
        DimIProxy action = new DimIProxy(this, IPROXY_COMPILE_SCRIPT);
        action.url = url;
        action.text = text;
        action.withContext();
    }

    void evalScript(final String url, final String text)
    {
        DimIProxy action = new DimIProxy(this, IPROXY_EVAL_SCRIPT);
        action.url = url;
        action.text = text;
        action.withContext();
    }

    String objectToString(Object object)
    {
        DimIProxy action = new DimIProxy(this, IPROXY_OBJECT_TO_STRING);
        action.object = object;
        action.withContext();
        return action.stringResult;
    }

    boolean stringIsCompilableUnit(String str)
    {
        DimIProxy action = new DimIProxy(this, IPROXY_STRING_IS_COMPILABLE);
        action.text = str;
        action.withContext();
        return action.booleanResult;
    }

    Object getObjectProperty(Object object, Object id)
    {
        DimIProxy action = new DimIProxy(this, IPROXY_OBJECT_PROPERTY);
        action.object = object;
        action.id = id;
        action.withContext();
        return action.objectResult;
    }

    Object[] getObjectIds(Object object)
    {
        DimIProxy action = new DimIProxy(this, IPROXY_OBJECT_IDS);
        action.object = object;
        action.withContext();
        return action.objectArrayResult;
    }

    Object getObjectPropertyImpl(Context cx, Object object, Object id)
    {
        Scriptable scriptable = (Scriptable)object;
        Object result;
        if (id instanceof String) {
            String name = (String)id;
            if (name.equals("this")) {
                result = scriptable;
            } else if (name.equals("__proto__")) {
                result = scriptable.getPrototype();
            } else if (name.equals("__parent__")) {
                result = scriptable.getParentScope();
            } else {
                result = ScriptableObject.getProperty(scriptable, name);
                if (result == ScriptableObject.NOT_FOUND) {
                    result = Undefined.instance;
                }
            }
        } else {
            int index = ((Integer)id).intValue();
            result = ScriptableObject.getProperty(scriptable, index);
            if (result == ScriptableObject.NOT_FOUND) {
                result = Undefined.instance;
            }
        }
        return result;
    }

    Object[] getObjectIdsImpl(Context cx, Object object)
    {
        if (!(object instanceof Scriptable) || object == Undefined.instance) {
            return Context.emptyArgs;
        }

        Object[] ids;
        Scriptable scriptable = (Scriptable)object;
        if (scriptable instanceof DebuggableObject) {
            ids = ((DebuggableObject)scriptable).getAllIds();
        } else {
            ids = scriptable.getIds();
        }

        Scriptable proto = scriptable.getPrototype();
        Scriptable parent = scriptable.getParentScope();
        int extra = 0;
        if (proto != null) {
            ++extra;
        }
        if (parent != null) {
            ++extra;
        }
        if (extra != 0) {
            Object[] tmp = new Object[extra + ids.length];
            System.arraycopy(ids, 0, tmp, extra, ids.length);
            ids = tmp;
            extra = 0;
            if (proto != null) {
                ids[extra++] = "__proto__";
            }
            if (parent != null) {
                ids[extra++] = "__parent__";
            }
        }

        return ids;
    }

    private void interrupted(Context cx, final StackFrame frame,
                             Throwable scriptException)
    {
        ContextData contextData = frame.contextData();
        int line = frame.getLineNumber();
        String url = frame.getUrl();
        boolean eventThreadFlag = callback.isGuiEventThread();
        contextData.eventThreadFlag = eventThreadFlag;

        boolean recursiveEventThreadCall = false;

      interruptedCheck:
        synchronized (eventThreadMonitor) {
            if (eventThreadFlag) {
                if (interruptedContextData != null) {
                    recursiveEventThreadCall = true;
                    break interruptedCheck;
                }
            } else {
                while (interruptedContextData != null) {
                    try {
                        eventThreadMonitor.wait();
                    } catch (InterruptedException exc) {
                        return;
                    }
                }
            }
            interruptedContextData = contextData;
        }

        if (recursiveEventThreadCall) {
            // XXX: For now the foolowing is commented out as on Linux
            // too deep recursion of dispatchNextGuiEvent causes GUI lockout.
            // Note: it can make GUI unresponsive if long-running script
            // will be called on GUI thread while processing another interrupt
            if (false) {
               // Run event dispatch until gui sets a flag to exit the initial
               // call to interrupted.
                while (this.returnValue == -1) {
                    try {
                        callback.dispatchNextGuiEvent();
                    } catch (InterruptedException exc) {
                    }
                }
            }
            return;
        }

        if (interruptedContextData == null) Kit.codeBug();

        try {
            do {
                int frameCount = contextData.frameCount();
                this.frameIndex = frameCount -1;

                final String threadTitle = Thread.currentThread().toString();
                final String alertMessage;
                if (scriptException == null) {
                    alertMessage = null;
                } else {
                    alertMessage = scriptException.toString();
                }

                int returnValue = -1;
                if (!eventThreadFlag) {
                    synchronized (monitor) {
                        if (insideInterruptLoop) Kit.codeBug();
                        this.insideInterruptLoop = true;
                        this.evalRequest = null;
                        this.returnValue = -1;
                        callback.enterInterrupt(frame, threadTitle,
                                                alertMessage);
                        try {
                            for (;;) {
                                try {
                                    monitor.wait();
                                } catch (InterruptedException exc) {
                                    Thread.currentThread().interrupt();
                                    break;
                                }
                                if (evalRequest != null) {
                                    this.evalResult = null;
                                    try {
                                        evalResult = do_eval(cx, evalFrame,
                                                             evalRequest);
                                    } finally {
                                        evalRequest = null;
                                        evalFrame = null;
                                        monitor.notify();
                                    }
                                    continue;
                                }
                                if (this.returnValue != -1) {
                                    returnValue = this.returnValue;
                                    break;
                                }
                            }
                        } finally {
                            insideInterruptLoop = false;
                        }
                    }
                } else {
                    this.returnValue = -1;
                    callback.enterInterrupt(frame, threadTitle, alertMessage);
                    while (this.returnValue == -1) {
                        try {
                            callback.dispatchNextGuiEvent();
                        } catch (InterruptedException exc) {
                        }
                    }
                    returnValue = this.returnValue;
                }
                switch (returnValue) {
                case STEP_OVER:
                    contextData.breakNextLine = true;
                    contextData.stopAtFrameDepth = contextData.frameCount();
                    break;
                case STEP_INTO:
                    contextData.breakNextLine = true;
                    contextData.stopAtFrameDepth = -1;
                    break;
                case STEP_OUT:
                    if (contextData.frameCount() > 1) {
                        contextData.breakNextLine = true;
                        contextData.stopAtFrameDepth
                            = contextData.frameCount() -1;
                    }
                    break;
                }
            } while (false);
        } finally {
            synchronized (eventThreadMonitor) {
                interruptedContextData = null;
                eventThreadMonitor.notifyAll();
            }
        }

    }

    private static String do_eval(Context cx, StackFrame frame, String expr)
    {
        String resultString;
        Debugger saved_debugger = cx.getDebugger();
        Object saved_data = cx.getDebuggerContextData();
        int saved_level = cx.getOptimizationLevel();

        cx.setDebugger(null, null);
        cx.setOptimizationLevel(-1);
        cx.setGeneratingDebug(false);
        try {
            Callable script = (Callable)cx.compileString(expr, "", 0, null);
            Object result = script.call(cx, frame.scope, frame.thisObj,
                                        ScriptRuntime.emptyArgs);
            if (result == Undefined.instance) {
                resultString = "";
            } else {
                resultString = ScriptRuntime.toString(result);
            }
        } catch (Exception exc) {
            resultString = exc.getMessage();
        } finally {
            cx.setGeneratingDebug(true);
            cx.setOptimizationLevel(saved_level);
            cx.setDebugger(saved_debugger, saved_data);
        }
        if (resultString == null) {
            resultString = "null";
        }
        return resultString;
    }
}

