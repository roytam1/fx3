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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov, igor@fastmail.fm
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

package org.mozilla.javascript;

import java.util.Hashtable;

public class CompilerEnvirons
{
    public CompilerEnvirons()
    {
        errorReporter = DefaultErrorReporter.instance;
        languageVersion = Context.VERSION_DEFAULT;
        generateDebugInfo = true;
        useDynamicScope = false;
        reservedKeywordAsIdentifier = false;
        allowMemberExprAsFunctionName = false;
        xmlAvailable = true;
        optimizationLevel = 0;
        generatingSource = true;
    }

    public void initFromContext(Context cx)
    {
        setErrorReporter(cx.getErrorReporter());
        this.languageVersion = cx.getLanguageVersion();
        useDynamicScope = cx.compileFunctionsWithDynamicScopeFlag;
        generateDebugInfo = (!cx.isGeneratingDebugChanged()
                             || cx.isGeneratingDebug());
        reservedKeywordAsIdentifier
            = cx.hasFeature(Context.FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER);
        allowMemberExprAsFunctionName
            = cx.hasFeature(Context.FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME);
        xmlAvailable
            = cx.hasFeature(Context.FEATURE_E4X);

        optimizationLevel = cx.getOptimizationLevel();

        generatingSource = cx.isGeneratingSource();
        activationNames = cx.activationNames;
    }

    public final ErrorReporter getErrorReporter()
    {
        return errorReporter;
    }

    public void setErrorReporter(ErrorReporter errorReporter)
    {
        if (errorReporter == null) throw new IllegalArgumentException();
        this.errorReporter = errorReporter;
    }

    public final int getLanguageVersion()
    {
        return languageVersion;
    }

    public void setLanguageVersion(int languageVersion)
    {
        Context.checkLanguageVersion(languageVersion);
        this.languageVersion = languageVersion;
    }

    public final boolean isGenerateDebugInfo()
    {
        return generateDebugInfo;
    }

    public void setGenerateDebugInfo(boolean flag)
    {
        this.generateDebugInfo = flag;
    }

    public final boolean isUseDynamicScope()
    {
        return useDynamicScope;
    }

    public final boolean isReservedKeywordAsIdentifier()
    {
        return reservedKeywordAsIdentifier;
    }

    public void setReservedKeywordAsIdentifier(boolean flag)
    {
        reservedKeywordAsIdentifier = flag;
    }

    public final boolean isAllowMemberExprAsFunctionName()
    {
        return allowMemberExprAsFunctionName;
    }

    public void setAllowMemberExprAsFunctionName(boolean flag)
    {
        allowMemberExprAsFunctionName = flag;
    }

    public final boolean isXmlAvailable()
    {
        return xmlAvailable;
    }

    public void setXmlAvailable(boolean flag)
    {
        xmlAvailable = flag;
    }

    public final int getOptimizationLevel()
    {
        return optimizationLevel;
    }

    public void setOptimizationLevel(int level)
    {
        Context.checkOptimizationLevel(level);
        this.optimizationLevel = level;
    }

    public final boolean isGeneratingSource()
    {
        return generatingSource;
    }

    /**
     * Specify whether or not source information should be generated.
     * <p>
     * Without source information, evaluating the "toString" method
     * on JavaScript functions produces only "[native code]" for
     * the body of the function.
     * Note that code generated without source is not fully ECMA
     * conformant.
     */
    public void setGeneratingSource(boolean generatingSource)
    {
        this.generatingSource = generatingSource;
    }

    private ErrorReporter errorReporter;

    private int languageVersion;
    private boolean generateDebugInfo;
    private boolean useDynamicScope;
    private boolean reservedKeywordAsIdentifier;
    private boolean allowMemberExprAsFunctionName;
    private boolean xmlAvailable;
    private int optimizationLevel;
    private boolean generatingSource;
    Hashtable activationNames;
}

