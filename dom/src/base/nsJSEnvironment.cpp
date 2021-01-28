/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsJSEnvironment.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIDOMChromeWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMChromeWindow.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsJSUtils.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsPresContext.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrompt.h"
#include "nsIObserverService.h"
#include "nsGUIEvent.h"
#include "nsScriptNameSpaceManager.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsDOMClassInfo.h"
#include "nsIAtom.h"
#include "nsContentUtils.h"
#include "jscntxt.h"
#include "nsEventDispatcher.h"
#include "nsIDOMGCParticipant.h"

// For locale aware string methods
#include "plstr.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtils.h"
#include "nsILocaleService.h"
#include "nsICollation.h"
#include "nsCollationCID.h"
#include "nsDOMClassInfo.h"

#ifdef NS_DEBUG
#include "jsgc.h"       // for WAY_TOO_MUCH_GC, if defined for GC debugging
#endif

#include "nsIStringBundle.h"

#ifdef MOZ_LOGGING
// Force PR_LOGGING so we can get JS strict warnings even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "prthread.h"

#ifdef OJI
#include "nsIJVMManager.h"
#include "nsILiveConnectManager.h"
#endif

const size_t gStackSize = 8192;

#ifdef PR_LOGGING
static PRLogModuleInfo* gJSDiagnostics;
#endif

// Thank you Microsoft!
#ifndef WINCE
#ifdef CompareString
#undef CompareString
#endif
#endif // WINCE

#define NS_GC_DELAY                2000 // ms
#define NS_FIRST_GC_DELAY          10000 // ms

// if you add statics here, add them to the list in nsJSEnvironment::Startup

static nsITimer *sGCTimer;
static PRBool sReadyForGC;

nsScriptNameSpaceManager *gNameSpaceManager;

static nsIJSRuntimeService *sRuntimeService;
JSRuntime *nsJSEnvironment::sRuntime;

static const char kJSRuntimeServiceContractID[] =
  "@mozilla.org/js/xpc/RuntimeService;1";

static const char kDOMStringBundleURL[] =
  "chrome://global/locale/dom/dom.properties";

static JSGCCallback gOldJSGCCallback;

static PRBool sIsInitialized;
static PRBool sDidShutdown;

static PRInt32 sContextCount;

static PRTime sMaxScriptRunTime;

static nsIScriptSecurityManager *sSecurityManager;

static nsICollation *gCollation;

static nsIUnicodeDecoder *gDecoder;

void JS_DLL_CALLBACK
NS_ScriptErrorReporter(JSContext *cx,
                       const char *message,
                       JSErrorReport *report)
{
  NS_ASSERTION(message || report,
               "Must have a message or a report; otherwise what are we "
               "reporting?");
  
  // XXX this means we are not going to get error reports on non DOM contexts
  nsIScriptContext *context = nsJSUtils::GetDynamicScriptContext(cx);

  nsEventStatus status = nsEventStatus_eIgnore;

  // Note: we must do this before running any more code on cx (if cx is the
  // dynamic script context).
  ::JS_ClearPendingException(cx);

  if (context) {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(context->GetGlobalObject()));

    if (win) {
      nsAutoString fileName, msg;

      if (report) {
        fileName.AssignWithConversion(report->filename);

        const PRUnichar *m = NS_REINTERPRET_CAST(const PRUnichar*,
                                                 report->ucmessage);

        if (m) {
          msg.Assign(m);
        }
      }

      if (msg.IsEmpty() && message) {
        msg.AssignWithConversion(message);
      }

      // First, notify the DOM that we have a script error.
      /* We do not try to report Out Of Memory via a dom
       * event because the dom event handler would encounter
       * an OOM exception trying to process the event, and
       * then we'd need to generate a new OOM event for that
       * new OOM instance -- this isn't pretty.
       */
      nsIDocShell *docShell = win->GetDocShell();
      if (docShell &&
          (!report ||
           (report->errorNumber != JSMSG_OUT_OF_MEMORY &&
            !JSREPORT_IS_WARNING(report->flags)))) {
        static PRInt32 errorDepth; // Recursion prevention
        ++errorDepth;

        nsCOMPtr<nsPresContext> presContext;
        docShell->GetPresContext(getter_AddRefs(presContext));

        if (presContext && errorDepth < 2) {
          nsScriptErrorEvent errorevent(PR_TRUE, NS_SCRIPT_ERROR);

          errorevent.fileName = fileName.get();
          errorevent.errorMsg = msg.get();
          errorevent.lineNr = report ? report->lineno : 0;

          // Dispatch() must be synchronous for the recursion block
          // (errorDepth) to work.
          nsEventDispatcher::Dispatch(win, presContext, &errorevent, nsnull,
                                      &status);
        }

        --errorDepth;
      }

      if (status != nsEventStatus_eConsumeNoDefault) {
        // Make an nsIScriptError and populate it with information from
        // this error.
        nsCOMPtr<nsIScriptError> errorObject =
          do_CreateInstance("@mozilla.org/scripterror;1");

        if (errorObject != nsnull) {
          nsresult rv = NS_ERROR_NOT_AVAILABLE;

          // Set category to chrome or content
          nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
            do_QueryInterface(win);
          NS_ASSERTION(scriptPrincipal, "Global objects must implement "
                       "nsIScriptObjectPrincipal");
          nsCOMPtr<nsIPrincipal> systemPrincipal;
          sSecurityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
          const char * category =
            scriptPrincipal->GetPrincipal() == systemPrincipal
            ? "chrome javascript"
            : "content javascript";

          if (report) {
            PRUint32 column = report->uctokenptr - report->uclinebuf;

            rv = errorObject->Init(msg.get(), fileName.get(),
                                   NS_REINTERPRET_CAST(const PRUnichar*,
                                                       report->uclinebuf),
                                   report->lineno, column, report->flags,
                                   category);
          } else if (message) {
            rv = errorObject->Init(msg.get(), nsnull, nsnull, 0, 0, 0,
                                   category);
          }

          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIConsoleService> consoleService =
              do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
            if (NS_SUCCEEDED(rv)) {
              consoleService->LogMessage(errorObject);
            }
          }
        }
      }
    }
  }

#ifdef DEBUG
  // Print it to stderr as well, for the benefit of those invoking
  // mozilla with -console.
  nsCAutoString error;
  error.Assign("JavaScript ");
  if (!report) {
    error.Append("[no report]: ");
    error.Append(message);
  } else {
    if (JSREPORT_IS_STRICT(report->flags))
      error.Append("strict ");
    if (JSREPORT_IS_WARNING(report->flags))
      error.Append("warning: ");
    else
      error.Append("error: ");
    error.Append(report->filename);
    error.Append(", line ");
    error.AppendInt(report->lineno, 10);
    error.Append(": ");
    if (report->ucmessage) {
      AppendUTF16toUTF8(NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage),
                        error);
    } else {
      error.Append(message);
    }
    if (status != nsEventStatus_eIgnore && !JSREPORT_IS_WARNING(report->flags))
      error.Append(" Error was suppressed by event handler\n");
  }
  fprintf(stderr, "%s\n", error.get());
  fflush(stderr);
#endif

#ifdef PR_LOGGING
  if (report) {
    if (!gJSDiagnostics)
      gJSDiagnostics = PR_NewLogModule("JSDiagnostics");

    if (gJSDiagnostics) {
      PR_LOG(gJSDiagnostics,
             JSREPORT_IS_WARNING(report->flags) ? PR_LOG_WARNING : PR_LOG_ERROR,
             ("file %s, line %u: %s\n%s%s",
              report->filename, report->lineno, message,
              report->linebuf ? report->linebuf : "",
              (report->linebuf &&
               report->linebuf[strlen(report->linebuf)-1] != '\n')
              ? "\n"
              : ""));
    }
  }
#endif
}

JS_STATIC_DLL_CALLBACK(JSBool)
LocaleToUnicode(JSContext *cx, char *src, jsval *rval)
{
  nsresult rv;

  if (!gDecoder) {
    // use app default locale
    nsCOMPtr<nsILocaleService> localeService = 
      do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsILocale> appLocale;
      rv = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
      if (NS_SUCCEEDED(rv)) {
        nsAutoString localeStr;
        rv = appLocale->
          GetCategory(NS_LITERAL_STRING(NSILOCALE_TIME), localeStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get app locale info");

        nsCOMPtr<nsIPlatformCharset> platformCharset =
          do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);

        if (NS_SUCCEEDED(rv)) {
          nsCAutoString charset;
          rv = platformCharset->GetDefaultCharsetForLocale(localeStr, charset);
          if (NS_SUCCEEDED(rv)) {
            // get/create unicode decoder for charset
            nsCOMPtr<nsICharsetConverterManager> ccm =
              do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
            if (NS_SUCCEEDED(rv))
              ccm->GetUnicodeDecoder(charset.get(), &gDecoder);
          }
        }
      }
    }
  }

  JSString *str = nsnull;
  PRInt32 srcLength = PL_strlen(src);

  if (gDecoder) {
    PRInt32 unicharLength = srcLength;
    PRUnichar *unichars = (PRUnichar *)malloc((srcLength + 1) * sizeof(PRUnichar));
    if (unichars) {
      rv = gDecoder->Convert(src, &srcLength, unichars, &unicharLength);
      if (NS_SUCCEEDED(rv)) {
        // terminate the returned string
        unichars[unicharLength] = 0;

        // nsIUnicodeDecoder::Convert may use fewer than srcLength PRUnichars
        if (unicharLength + 1 < srcLength + 1) {
          PRUnichar *shrunkUnichars =
            (PRUnichar *)realloc(unichars, (unicharLength + 1) * sizeof(PRUnichar));
          if (shrunkUnichars)
            unichars = shrunkUnichars;
        }
        str = JS_NewUCString(cx,
                             NS_REINTERPRET_CAST(jschar*, unichars),
                             unicharLength);
      }
      if (!str)
        free(unichars);
    }
  }

  if (!str) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_OUT_OF_MEMORY);
    return JS_FALSE;
  }

  *rval = STRING_TO_JSVAL(str);
  return JS_TRUE;
}


static JSBool
ChangeCase(JSContext *cx, JSString *src, jsval *rval,
           void(* changeCaseFnc)(const nsAString&, nsAString&))
{
  nsAutoString result;
  changeCaseFnc(nsDependentJSString(src), result);

  JSString *ucstr = JS_NewUCStringCopyN(cx, (jschar*)result.get(), result.Length());
  if (!ucstr) {
    return JS_FALSE;
  }

  *rval = STRING_TO_JSVAL(ucstr);

  return JS_TRUE;
}

static JSBool JS_DLL_CALLBACK
LocaleToUpperCase(JSContext *cx, JSString *src, jsval *rval)
{
  return ChangeCase(cx, src, rval, ToUpperCase);
}

static JSBool JS_DLL_CALLBACK
LocaleToLowerCase(JSContext *cx, JSString *src, jsval *rval)
{
  return ChangeCase(cx, src, rval, ToLowerCase);
}

static JSBool JS_DLL_CALLBACK
LocaleCompare(JSContext *cx, JSString *src1, JSString *src2, jsval *rval)
{
  nsresult rv;

  if (!gCollation) {
    nsCOMPtr<nsILocaleService> localeService =
      do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsILocale> locale;
      rv = localeService->GetApplicationLocale(getter_AddRefs(locale));

      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICollationFactory> colFactory =
          do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID, &rv);

        if (NS_SUCCEEDED(rv)) {
          rv = colFactory->CreateCollation(locale, &gCollation);
        }
      }
    }

    if (NS_FAILED(rv)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);

      return JS_FALSE;
    }
  }

  PRInt32 result;
  rv = gCollation->CompareString(nsICollation::kCollationStrengthDefault,
                                 nsDependentJSString(src1),
                                 nsDependentJSString(src2),
                                 &result);

  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  *rval = INT_TO_JSVAL(result);

  return JS_TRUE;
}

// The number of branch callbacks between calls to JS_MaybeGC
#define MAYBE_GC_BRANCH_COUNT_MASK 0x00000fff // 4095

// The number of branch callbacks before we even check if our start
// timestamp is initialized. This is a fairly low number as we want to
// initialize the timestamp early enough to not waste much time before
// we get there, but we don't want to bother doing this too early as
// it's not generally necessary.
#define INITIALIZE_TIME_BRANCH_COUNT_MASK 0x000000ff // 255

// This function is called after each JS branch execution
JSBool JS_DLL_CALLBACK
nsJSContext::DOMBranchCallback(JSContext *cx, JSScript *script)
{
  // Get the native context
  nsJSContext *ctx = NS_STATIC_CAST(nsJSContext *, ::JS_GetContextPrivate(cx));

  PRUint32 callbackCount = ++ctx->mBranchCallbackCount;

  if (callbackCount & INITIALIZE_TIME_BRANCH_COUNT_MASK) {
    return JS_TRUE;
  }

  if (callbackCount == INITIALIZE_TIME_BRANCH_COUNT_MASK + 1 &&
      LL_IS_ZERO(ctx->mBranchCallbackTime)) {
    // Initialize mBranchCallbackTime to start timing how long the
    // script has run
    ctx->mBranchCallbackTime = PR_Now();

    return JS_TRUE;
  }

  if (callbackCount & MAYBE_GC_BRANCH_COUNT_MASK) {
    return JS_TRUE;
  }

  // XXX Save the branch callback time so we can restore it after the GC,
  // because GCing can cause JS to run on our context, causing our
  // ScriptEvaluated to be called, and clearing our branch callback time and
  // count. See bug 302333.
  PRTime callbackTime = ctx->mBranchCallbackTime;

  // Run the GC if we get this far.
  JS_MaybeGC(cx);

  // Now restore the callback time and count, in case they got reset.
  ctx->mBranchCallbackTime = callbackTime;
  ctx->mBranchCallbackCount = callbackCount;

  PRTime now = PR_Now();

  PRTime duration;
  LL_SUB(duration, now, callbackTime);

  // Check the amount of time this script has been running, or if the
  // dialog is disabled.
  if (LL_CMP(duration, <, sMaxScriptRunTime)) {
    return JS_TRUE;
  }

  // If we get here we're most likely executing an infinite loop in JS,
  // we'll tell the user about this and we'll give the user the option
  // of stopping the execution of the script.
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(ctx->GetGlobalObject()));
  NS_ENSURE_TRUE(win, JS_TRUE);

  nsIDocShell *docShell = win->GetDocShell();
  NS_ENSURE_TRUE(docShell, JS_TRUE);

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(ireq, JS_TRUE);

  // Get the nsIPrompt interface from the docshell
  nsCOMPtr<nsIPrompt> prompt;
  ireq->GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompt));
  NS_ENSURE_TRUE(prompt, JS_TRUE);

  // Get localizable strings
  nsCOMPtr<nsIStringBundleService>
    stringService(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (!stringService)
    return JS_TRUE;

  nsCOMPtr<nsIStringBundle> bundle;
  stringService->CreateBundle(kDOMStringBundleURL, getter_AddRefs(bundle));
  if (!bundle)
    return JS_TRUE;
  
  nsXPIDLString title, msg, stopButton, waitButton, neverShowDlg;

  nsresult rv;

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("KillScriptMessage").get(),
                                  getter_Copies(msg));
  rv |= bundle->GetStringFromName(NS_LITERAL_STRING("KillScriptTitle").get(),
                                  getter_Copies(title));
  rv |= bundle->GetStringFromName(NS_LITERAL_STRING("StopScriptButton").get(),
                                  getter_Copies(stopButton));
  rv |= bundle->GetStringFromName(NS_LITERAL_STRING("WaitForScriptButton").get(),
                                  getter_Copies(waitButton));
  rv |= bundle->GetStringFromName(NS_LITERAL_STRING("NeverShowDialogAgain").get(),
                                  getter_Copies(neverShowDlg));

  //GetStringFromName can return NS_OK and still give NULL string
  if (NS_FAILED(rv) || !title || !msg || !stopButton || !waitButton ||
      !neverShowDlg) {
    NS_ERROR("Failed to get localized strings.");
    return JS_TRUE;
  }

  PRInt32 buttonPressed = 1; //In case user exits dialog by clicking X
  PRBool neverShowDlgChk = PR_FALSE;

  // Open the dialog.
  rv = prompt->ConfirmEx(title, msg,
                         (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          nsIPrompt::BUTTON_POS_0) +
                         (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          nsIPrompt::BUTTON_POS_1),
                         stopButton, waitButton,
                         nsnull, neverShowDlg, &neverShowDlgChk,
                         &buttonPressed);

  if (NS_FAILED(rv) || (buttonPressed == 1)) {
    // Allow the script to continue running

    if (neverShowDlgChk) {
      nsIPrefBranch *prefBranch = nsContentUtils::GetPrefBranch();

      if (prefBranch) {
        prefBranch->SetIntPref("dom.max_script_run_time", 0);
      }
    }

    ctx->mBranchCallbackTime = PR_Now();
    return JS_TRUE;
  }

  return JS_FALSE;
}

#define JS_OPTIONS_DOT_STR "javascript.options."

static const char js_options_dot_str[]   = JS_OPTIONS_DOT_STR;
static const char js_strict_option_str[] = JS_OPTIONS_DOT_STR "strict";
static const char js_werror_option_str[] = JS_OPTIONS_DOT_STR "werror";

int PR_CALLBACK
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsJSContext *context = NS_REINTERPRET_CAST(nsJSContext *, data);
  PRUint32 oldDefaultJSOptions = context->mDefaultJSOptions;
  PRUint32 newDefaultJSOptions = oldDefaultJSOptions;

  PRBool strict = nsContentUtils::GetBoolPref(js_strict_option_str);
  if (strict)
    newDefaultJSOptions |= JSOPTION_STRICT;
  else
    newDefaultJSOptions &= ~JSOPTION_STRICT;

  PRBool werror = nsContentUtils::GetBoolPref(js_werror_option_str);
  if (werror)
    newDefaultJSOptions |= JSOPTION_WERROR;
  else
    newDefaultJSOptions &= ~JSOPTION_WERROR;

  if (newDefaultJSOptions != oldDefaultJSOptions) {
    // Set options only if we used the old defaults; otherwise the page has
    // customized some via the options object and we defer to its wisdom.
    if (::JS_GetOptions(context->mContext) == oldDefaultJSOptions)
      ::JS_SetOptions(context->mContext, newDefaultJSOptions);

    // Save the new defaults for the next page load (InitContext).
    context->mDefaultJSOptions = newDefaultJSOptions;
  }
  return 0;
}

static jsuword
GetThreadStackLimit()
{
  // Store the thread stack limit in a static local to ensure that all
  // contexts get the same stack limit (they're all on the same thread
  // anyways), and this also helps prevent returning a stack limit
  // that is beyond the end of the stack if this method is called way
  // deep on the stack.

  static jsuword sThreadStackLimit;

  if (sThreadStackLimit == 0) {
    int stackDummy;
    jsuword currentStackAddr = (jsuword)&stackDummy;

    const jsuword kStackSize = 0x80000;   // 512k

#if JS_STACK_GROWTH_DIRECTION < 0
    sThreadStackLimit = (currentStackAddr > kStackSize)
                        ? currentStackAddr - kStackSize
                        : 0;
#else
    sThreadStackLimit = (currentStackAddr + kStackSize > currentStackAddr)
                        ? currentStackAddr + kStackSize
                        : (jsuword) -1;
#endif
  }

  return sThreadStackLimit;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime) : mGCOnDestruction(PR_TRUE)
{

  ++sContextCount;

  mDefaultJSOptions = JSOPTION_PRIVATE_IS_NSISUPPORTS
                    | JSOPTION_NATIVE_BRANCH_CALLBACK
#ifdef DEBUG
                    | JSOPTION_STRICT   // lint catching for development
#endif
    ;

  // Let xpconnect resync its JSContext tracker. We do this before creating
  // a new JSContext just in case the heap manager recycles the JSContext
  // struct.
  nsContentUtils::XPConnect()->SyncJSContexts();

  mContext = ::JS_NewContext(aRuntime, gStackSize);
  if (mContext) {
    ::JS_SetContextPrivate(mContext, NS_STATIC_CAST(nsIScriptContext *, this));

    ::JS_SetThreadStackLimit(mContext, GetThreadStackLimit());

    // Make sure the new context gets the default context options
    ::JS_SetOptions(mContext, mDefaultJSOptions);

    // Check for the JS strict option, which enables extra error checks
    nsContentUtils::RegisterPrefCallback(js_options_dot_str,
                                         JSOptionChangedCallback,
                                         this);
    JSOptionChangedCallback(js_options_dot_str, this);

    ::JS_SetBranchCallback(mContext, DOMBranchCallback);

    static JSLocaleCallbacks localeCallbacks =
      {
        LocaleToUpperCase,
        LocaleToLowerCase,
        LocaleCompare,
        LocaleToUnicode
      };

    ::JS_SetLocaleCallbacks(mContext, &localeCallbacks);
  }
  mIsInitialized = PR_FALSE;
  mNumEvaluations = 0;
  mOwner = nsnull;
  mTerminations = nsnull;
  mScriptsEnabled = PR_TRUE;
  mBranchCallbackCount = 0;
  mBranchCallbackTime = LL_ZERO;
  mProcessingScriptTag=PR_FALSE;

  InvalidateContextAndWrapperCache();
}

nsJSContext::~nsJSContext()
{
  NS_PRECONDITION(!mTerminations, "Shouldn't have termination funcs by now");
                  
  // Cope with JS_NewContext failure in ctor (XXXbe move NewContext to Init?)
  if (!mContext)
    return;

  // Clear our entry in the JSContext, bugzilla bug 66413
  ::JS_SetContextPrivate(mContext, nsnull);

  // Clear the branch callback, bugzilla bug 238218
  ::JS_SetBranchCallback(mContext, nsnull);

  // Unregister our "javascript.options.*" pref-changed callback.
  nsContentUtils::UnregisterPrefCallback(js_options_dot_str,
                                         JSOptionChangedCallback,
                                         this);

  // Release mGlobalWrapperRef before the context is destroyed
  mGlobalWrapperRef = nsnull;

  // Let xpconnect destroy the JSContext when it thinks the time is right.
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (xpc) {
    PRBool do_gc = mGCOnDestruction && !sGCTimer && sReadyForGC;

    xpc->ReleaseJSContext(mContext, !do_gc);
  } else {
    ::JS_DestroyContext(mContext);
  }

  --sContextCount;

  if (!sContextCount && sDidShutdown) {
    // The last context is being deleted, and we're already in the
    // process of shutting down, release the JS runtime service, and
    // the security manager.

    NS_IF_RELEASE(sRuntimeService);
    NS_IF_RELEASE(sSecurityManager);
    NS_IF_RELEASE(gCollation);
    NS_IF_RELEASE(gDecoder);
  }
}

// QueryInterface implementation for nsJSContext
NS_INTERFACE_MAP_BEGIN(nsJSContext)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContext)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptNotify)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptContext)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsJSContext)
NS_IMPL_RELEASE(nsJSContext)


nsresult
nsJSContext::EvaluateStringWithValue(const nsAString& aScript,
                                     void *aScopeObject,
                                     nsIPrincipal *aPrincipal,
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     const char* aVersion,
                                     void* aRetValue,
                                     PRBool* aIsUndefined)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    return NS_OK;
  }

  nsresult rv;
  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  JSPrincipals *jsprin;
  nsIPrincipal *principal = aPrincipal;
  if (!aPrincipal) {
    nsIScriptGlobalObject *global = GetGlobalObject();
    if (!global)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
      do_QueryInterface(global, &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    principal = objPrincipal->GetPrincipal();
    if (!principal)
      return NS_ERROR_FAILURE;
  }

  principal->GetJSPrincipals(mContext, &jsprin);

  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;

  rv = sSecurityManager->CanExecuteScripts(mContext, principal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  // Push our JSContext on the current thread's context stack so JS called
  // from native code via XPConnect uses the right context.  Do this whether
  // or not the SecurityManager said "ok", in order to simplify control flow
  // below where we pop before returning.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  jsval val;

  nsJSContext::TerminationFuncHolder holder(this);
  if (ok) {
    JSVersion newVersion = JSVERSION_UNKNOWN;

    // SecurityManager said "ok", but don't execute if aVersion is specified
    // and unknown.  Do execute with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    ok = (!aVersion ||
          (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN);
    if (ok) {
      JSVersion oldVersion = JSVERSION_UNKNOWN;

      if (aVersion)
        oldVersion = ::JS_SetVersion(mContext, newVersion);
      ok = ::JS_EvaluateUCScriptForPrincipals(mContext,
                                              (JSObject *)aScopeObject,
                                              jsprin,
                                              (jschar*)PromiseFlatString(aScript).get(),
                                              aScript.Length(),
                                              aURL,
                                              aLineNo,
                                              &val);

      if (aVersion) {
        ::JS_SetVersion(mContext, oldVersion);
      }

      if (!ok) {
        // Tell XPConnect about any pending exceptions. This is needed
        // to avoid dropping JS exceptions in case we got here through
        // nested calls through XPConnect.

        nsContentUtils::NotifyXPCIfExceptionPending(mContext);
      }
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
    if (aIsUndefined) {
      *aIsUndefined = JSVAL_IS_VOID(val);
    }

    *NS_STATIC_CAST(jsval*, aRetValue) = val;
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;

}

// Helper function to convert a jsval to an nsAString, and set
// exception flags if the conversion fails.
static nsresult
JSValueToAString(JSContext *cx, jsval val, nsAString *result,
                 PRBool *isUndefined)
{
  if (isUndefined) {
    *isUndefined = JSVAL_IS_VOID(val);
  }

  if (!result) {
    return NS_OK;
  }

  JSString* jsstring = ::JS_ValueToString(cx, val);
  if (jsstring) {
    result->Assign(NS_REINTERPRET_CAST(const PRUnichar*,
                                       ::JS_GetStringChars(jsstring)),
                   ::JS_GetStringLength(jsstring));
  } else {
    result->Truncate();

    // We failed to convert val to a string. We're either OOM, or the
    // security manager denied access to .toString(), or somesuch, on
    // an object. Treat this case as if the result were undefined.

    if (isUndefined) {
      *isUndefined = PR_TRUE;
    }

    if (!::JS_IsExceptionPending(cx)) {
      // JS_ValueToString() returned null w/o an exception
      // pending. That means we're OOM.

      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Tell XPConnect about any pending exceptions. This is needed to
    // avoid dropping JS exceptions in case we got here through nested
    // calls through XPConnect.

    nsContentUtils::NotifyXPCIfExceptionPending(cx);
  }

  return NS_OK;
}

nsresult
nsJSContext::EvaluateString(const nsAString& aScript,
                            void *aScopeObject,
                            nsIPrincipal *aPrincipal,
                            const char *aURL,
                            PRUint32 aLineNo,
                            const char* aVersion,
                            nsAString *aRetValue,
                            PRBool* aIsUndefined)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    *aIsUndefined = PR_TRUE;

    if (aRetValue) {
      aRetValue->Truncate();
    }

    return NS_OK;
  }

  nsresult rv;
  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  JSPrincipals *jsprin;
  nsIPrincipal *principal = aPrincipal;
  if (aPrincipal) {
    aPrincipal->GetJSPrincipals(mContext, &jsprin);
  }
  else {
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
      do_QueryInterface(GetGlobalObject(), &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    principal = objPrincipal->GetPrincipal();
    if (!principal)
      return NS_ERROR_FAILURE;
    principal->GetJSPrincipals(mContext, &jsprin);
  }

  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;

  rv = sSecurityManager->CanExecuteScripts(mContext, principal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  // Push our JSContext on the current thread's context stack so JS called
  // from native code via XPConnect uses the right context.  Do this whether
  // or not the SecurityManager said "ok", in order to simplify control flow
  // below where we pop before returning.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  // The result of evaluation, used only if there were no errors.  This need
  // not be a GC root currently, provided we run the GC only from the branch
  // callback or from ScriptEvaluated.  TODO: use JS_Begin/EndRequest to keep
  // the GC from racing with JS execution on any thread.
  jsval val;

  nsJSContext::TerminationFuncHolder holder(this);
  if (ok) {
    JSVersion newVersion = JSVERSION_UNKNOWN;

    // SecurityManager said "ok", but don't execute if aVersion is specified
    // and unknown.  Do execute with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    ok = (!aVersion ||
          (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN);
    if (ok) {
      JSVersion oldVersion = JSVERSION_UNKNOWN;

      if (aVersion)
        oldVersion = ::JS_SetVersion(mContext, newVersion);
      ok = ::JS_EvaluateUCScriptForPrincipals(mContext,
                                              (JSObject *)aScopeObject,
                                              jsprin,
                                              (jschar*)PromiseFlatString(aScript).get(),
                                              aScript.Length(),
                                              aURL,
                                              aLineNo,
                                              &val);

      if (aVersion) {
        ::JS_SetVersion(mContext, oldVersion);
      }

      if (!ok) {
        // Tell XPConnect about any pending exceptions. This is needed
        // to avoid dropping JS exceptions in case we got here through
        // nested calls through XPConnect.

        nsContentUtils::NotifyXPCIfExceptionPending(mContext);
      }
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
    rv = JSValueToAString(mContext, val, aRetValue, aIsUndefined);
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;
}

nsresult
nsJSContext::CompileScript(const PRUnichar* aText,
                           PRInt32 aTextLength,
                           void *aScopeObject,
                           nsIPrincipal *aPrincipal,
                           const char *aURL,
                           PRUint32 aLineNo,
                           const char* aVersion,
                           void** aScriptObject)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  NS_ENSURE_ARG_POINTER(aPrincipal);

  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  JSPrincipals *jsprin;
  aPrincipal->GetJSPrincipals(mContext, &jsprin);
  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;

  rv = sSecurityManager->CanExecuteScripts(mContext, aPrincipal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  *aScriptObject = nsnull;
  if (ok) {
    JSVersion newVersion = JSVERSION_UNKNOWN;

    // SecurityManager said "ok", but don't compile if aVersion is specified
    // and unknown.  Do compile with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    if (!aVersion ||
        (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN) {
      JSVersion oldVersion = JSVERSION_UNKNOWN;
      if (aVersion)
        oldVersion = ::JS_SetVersion(mContext, newVersion);

      JSScript* script =
        ::JS_CompileUCScriptForPrincipals(mContext,
                                          (JSObject*) aScopeObject,
                                          jsprin,
                                          (jschar*) aText,
                                          aTextLength,
                                          aURL,
                                          aLineNo);
      if (script) {
        *aScriptObject = (void*) ::JS_NewScriptObject(mContext, script);
        if (! *aScriptObject) {
          ::JS_DestroyScript(mContext, script);
          script = nsnull;
        }
      }
      if (!script)
        rv = NS_ERROR_OUT_OF_MEMORY;

      if (aVersion)
        ::JS_SetVersion(mContext, oldVersion);
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);
  return rv;
}

nsresult
nsJSContext::ExecuteScript(void* aScriptObject,
                           void *aScopeObject,
                           nsAString* aRetValue,
                           PRBool* aIsUndefined)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }

    return NS_OK;
  }

  nsresult rv;

  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  // Push our JSContext on our thread's context stack, in case native code
  // called from JS calls back into JS via XPConnect.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  // The result of evaluation, used only if there were no errors.  This need
  // not be a GC root currently, provided we run the GC only from the branch
  // callback or from ScriptEvaluated.  TODO: use JS_Begin/EndRequest to keep
  // the GC from racing with JS execution on any thread.
  jsval val;
  JSBool ok;

  nsJSContext::TerminationFuncHolder holder(this);
  ok = ::JS_ExecuteScript(mContext,
                          (JSObject*) aScopeObject,
                          (JSScript*) ::JS_GetPrivate(mContext,
                                                    (JSObject*)aScriptObject),
                          &val);

  if (ok) {
    // If all went well, convert val to a string (XXXbe unless undefined?).

    rv = JSValueToAString(mContext, val, aRetValue, aIsUndefined);
  } else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }

    // Tell XPConnect about any pending exceptions. This is needed to
    // avoid dropping JS exceptions in case we got here through nested
    // calls through XPConnect.

    nsContentUtils::NotifyXPCIfExceptionPending(mContext);
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;
}


static inline const char *
AtomToEventHandlerName(nsIAtom *aName)
{
  const char *name;

  aName->GetUTF8String(&name);

#ifdef DEBUG
  const char *cp;
  char c;
  for (cp = name; *cp != '\0'; ++cp)
  {
    c = *cp;
    NS_ASSERTION (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'),
                  "non-ASCII non-alphabetic event handler name");
  }
#endif

  return name;
}

nsresult
nsJSContext::CompileEventHandler(void *aTarget, nsIAtom *aName,
                                 const char *aEventName,
                                 const nsAString& aBody,
                                 const char *aURL, PRUint32 aLineNo,
                                 PRBool aShared, void** aHandler)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!sSecurityManager) {
    NS_ERROR("Huh, we need a script security manager to compile "
             "an event handler!");

    return NS_ERROR_UNEXPECTED;
  }

  JSObject *target = (JSObject*)aTarget;

  JSPrincipals *jsprin = nsnull;

  if (target) {
    // Get the principal of the event target (the object principal),
    // don't get the principal of the global object in this context
    // since that opens up security exploits with delayed event
    // handler compilation on stale DOM objects (objects that live in
    // a document that has already been unloaded).
    nsCOMPtr<nsIPrincipal> prin;
    nsresult rv = sSecurityManager->GetObjectPrincipal(mContext, target,
                                                       getter_AddRefs(prin));
    NS_ENSURE_SUCCESS(rv, rv);

    prin->GetJSPrincipals(mContext, &jsprin);
    NS_ENSURE_TRUE(jsprin, NS_ERROR_NOT_AVAILABLE);
  }

  const char *charName = AtomToEventHandlerName(aName);

  const char *argList[] = { aEventName };

  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipals(mContext,
                                          aShared ? nsnull : target, jsprin,
                                          charName, 1, argList,
                                          (jschar*)PromiseFlatString(aBody).get(),
                                          aBody.Length(),
                                          aURL, aLineNo);

  if (jsprin) {
    JSPRINCIPALS_DROP(mContext, jsprin);
  }
  if (!fun) {
    return NS_ERROR_FAILURE;
  }

  JSObject *handler = ::JS_GetFunctionObject(fun);
  if (aHandler)
    *aHandler = (void*) handler;
  return NS_OK;
}

nsresult
nsJSContext::CompileFunction(void* aTarget,
                             const nsACString& aName,
                             PRUint32 aArgCount,
                             const char** aArgArray,
                             const nsAString& aBody,
                             const char* aURL,
                             PRUint32 aLineNo,
                             PRBool aShared,
                             void** aFunctionObject)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  JSPrincipals *jsprin = nsnull;

  nsIScriptGlobalObject *global = GetGlobalObject();
  if (global) {
    // XXXbe why the two-step QI? speed up via a new GetGlobalObjectData func?
    nsCOMPtr<nsIScriptObjectPrincipal> globalData = do_QueryInterface(global);
    if (globalData) {
      nsIPrincipal *prin = globalData->GetPrincipal();
      if (!prin)
        return NS_ERROR_FAILURE;
      prin->GetJSPrincipals(mContext, &jsprin);
    }
  }

  JSObject *target = (JSObject*)aTarget;
  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipals(mContext,
                                          aShared ? nsnull : target, jsprin,
                                          PromiseFlatCString(aName).get(),
                                          aArgCount, aArgArray,
                                          (jschar*)PromiseFlatString(aBody).get(),
                                          aBody.Length(),
                                          aURL, aLineNo);

  if (jsprin)
    JSPRINCIPALS_DROP(mContext, jsprin);
  if (!fun)
    return NS_ERROR_FAILURE;

  JSObject *handler = ::JS_GetFunctionObject(fun);
  if (aFunctionObject)
    *aFunctionObject = (void*) handler;
  return NS_OK;
}

nsresult
nsJSContext::CallEventHandler(JSObject *aTarget, JSObject *aHandler,
                              uintN argc, jsval *argv, jsval *rval)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  *rval = JSVAL_VOID;

  if (!mScriptsEnabled) {
    return NS_OK;
  }

  // This one's a lot easier than EvaluateString because we don't have to
  // hassle with principals: they're already compiled into the JS function.
  nsresult rv;

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext)))
    return NS_ERROR_FAILURE;

  // check if the event handler can be run on the object in question
  rv = sSecurityManager->CheckFunctionAccess(mContext, aHandler, aTarget);

  nsJSContext::TerminationFuncHolder holder(this);

  if (NS_SUCCEEDED(rv)) {
    jsval funval = OBJECT_TO_JSVAL(aHandler);
    PRBool ok = ::JS_CallFunctionValue(mContext, aTarget, funval, argc, argv,
                                       rval);

    if (!ok) {
      // Tell XPConnect about any pending exceptions. This is needed
      // to avoid dropping JS exceptions in case we got here through
      // nested calls through XPConnect.

      nsContentUtils::NotifyXPCIfExceptionPending(mContext);

      // Don't pass back results from failed calls.
      *rval = JSVAL_VOID;

      // Tell the caller that the handler threw an error.
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED(stack->Pop(nsnull)))
    return NS_ERROR_FAILURE;

  // Need to lock, since ScriptEvaluated can GC.
  PRBool locked = PR_FALSE;
  if (NS_SUCCEEDED(rv) && JSVAL_IS_GCTHING(*rval)) {
    locked = ::JS_LockGCThing(mContext, JSVAL_TO_GCTHING(*rval));
    if (!locked) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  if (locked) {
    ::JS_UnlockGCThing(mContext, JSVAL_TO_GCTHING(*rval));
  }

  return rv;
}

nsresult
nsJSContext::BindCompiledEventHandler(void *aTarget, nsIAtom *aName,
                                      void *aHandler)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  const char *charName = AtomToEventHandlerName(aName);

  JSObject *funobj = (JSObject*) aHandler;
  JSObject *target = (JSObject*) aTarget;

  nsresult rv;

  // Push our JSContext on our thread's context stack, in case native code
  // called from JS calls back into JS via XPConnect.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the handler function is parented by its event target object
  if (funobj && ::JS_GetParent(mContext, funobj) != target) {
    funobj = ::JS_CloneFunctionObject(mContext, funobj, target);
    if (!funobj)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_SUCCEEDED(rv) &&
      !::JS_DefineProperty(mContext, target, charName,
                           OBJECT_TO_JSVAL(funobj), nsnull, nsnull,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
    rv = NS_ERROR_FAILURE;
  }

  if (NS_FAILED(stack->Pop(nsnull)) && NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

void
nsJSContext::SetDefaultLanguageVersion(const char* aVersion)
{
  ::JS_SetVersion(mContext, ::JS_StringToVersion(aVersion));
}

nsIScriptGlobalObject *
nsJSContext::GetGlobalObject()
{
  JSObject *global = ::JS_GetGlobalObject(mContext);

  if (!global) {
    NS_WARNING("Context has no global.");
    return nsnull;
  }

  JSClass *c = JS_GET_CLASS(mContext, global);

  if (!c || ((~c->flags) & (JSCLASS_HAS_PRIVATE |
                            JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
    NS_WARNING("Global is not an nsISupports.");
    return nsnull;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  nsISupports *priv =
    (nsISupports *)::JS_GetPrivate(mContext, global);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapped_native =
    do_QueryInterface(priv);

  if (wrapped_native) {
    // The global object is a XPConnect wrapped native, the native in
    // the wrapper might be the nsIScriptGlobalObject

    sgo = do_QueryWrappedNative(wrapped_native);
  } else {
    sgo = do_QueryInterface(priv);
  }

  // This'll return a pointer to something we're about to release, but
  // that's ok, the JS object will hold it alive long enough.
  return sgo;
}

void *
nsJSContext::GetNativeContext()
{
  return mContext;
}

const JSClass* NS_DOMClassInfo_GetXPCNativeWrapperClass();
void NS_DOMClassInfo_SetXPCNativeWrapperClass(JSClass* aClass);

nsresult
nsJSContext::InitContext(nsIScriptGlobalObject *aGlobalObject)
{
  // Make sure callers of this use
  // WillInitializeContext/DidInitializeContext around this call.
  NS_ENSURE_TRUE(!mIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  if (!mContext)
    return NS_ERROR_OUT_OF_MEMORY;

  InvalidateContextAndWrapperCache();

  nsresult rv;

  if (!gNameSpaceManager) {
    gNameSpaceManager = new nsScriptNameSpaceManager;
    NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_OUT_OF_MEMORY);

    rv = gNameSpaceManager->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  ::JS_SetErrorReporter(mContext, NS_ScriptErrorReporter);

  if (!aGlobalObject) {
    // If we don't get a global object then there's nothing more to do here.

    return NS_OK;
  }

  nsIXPConnect *xpc = nsContentUtils::XPConnect();

  JSObject *global = ::JS_GetGlobalObject(mContext);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  // If there's already a global object in mContext we won't tell
  // XPConnect to wrap aGlobalObject since it's already wrapped.

  if (!global) {
    nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(aGlobalObject));
    PRUint32 flags = 0;
    
    if (chromeWindow) {
      // Flag this object and scripts compiled against it as "system", for
      // optional automated XPCNativeWrapper construction when chrome views
      // a content DOM.
      flags = nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT;

      // Always enable E4X for XUL and other chrome content -- there is no
      // need to preserve the <!-- script hiding hack from JS-in-HTML daze
      // (introduced in 1995 for graceful script degradation in Netscape 1,
      // Mosaic, and other pre-JS browsers).
      ::JS_SetOptions(mContext, ::JS_GetOptions(mContext) | JSOPTION_XML);
    }

    rv = xpc->InitClassesWithNewWrappedGlobal(mContext, aGlobalObject,
                                              NS_GET_IID(nsISupports),
                                              flags,
                                              getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now check whether we need to grab a pointer to the
    // XPCNativeWrapper class
    if (!NS_DOMClassInfo_GetXPCNativeWrapperClass()) {
      rv = FindXPCNativeWrapperClass(holder);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    // If there's already a global object in mContext we're called
    // after ::JS_ClearScope() was called. We'll have to tell
    // XPConnect to re-initialize the global object to do things like
    // define the Components object on the global again and forget all
    // old prototypes in this scope.
    rv = xpc->InitClasses(mContext, global);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIClassInfo> ci(do_QueryInterface(aGlobalObject));

    if (ci) {
      rv = xpc->WrapNative(mContext, global, aGlobalObject,
                           NS_GET_IID(nsISupports),
                           getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(holder));
      NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

      rv = wrapper->RefreshPrototype();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Hold a strong reference to the wrapper for the global to avoid
  // rooting and unrooting the global object every time its AddRef()
  // or Release() methods are called
  mGlobalWrapperRef = holder;

  holder->GetJSObject(&global);

  rv = InitClasses(global); // this will complete global object initialization
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsJSContext::InitializeExternalClasses()
{
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return gNameSpaceManager->InitForContext(this);
}

nsresult
nsJSContext::InitializeLiveConnectClasses(JSObject *aGlobalObj)
{
  nsresult rv = NS_OK;

#ifdef OJI
  nsCOMPtr<nsIJVMManager> jvmManager =
    do_GetService(nsIJVMManager::GetCID(), &rv);

  if (NS_SUCCEEDED(rv) && jvmManager) {
    PRBool javaEnabled = PR_FALSE;

    rv = jvmManager->GetJavaEnabled(&javaEnabled);

    if (NS_SUCCEEDED(rv) && javaEnabled) {
      nsCOMPtr<nsILiveConnectManager> liveConnectManager =
        do_QueryInterface(jvmManager);

      if (liveConnectManager) {
        rv = liveConnectManager->InitLiveConnectClasses(mContext, aGlobalObj);
      }
    }
  }
#endif /* OJI */

  // return all is well until things are stable.
  return NS_OK;
}

nsresult
nsJSContext::FindXPCNativeWrapperClass(nsIXPConnectJSObjectHolder *aHolder)
{
  NS_ASSERTION(!NS_DOMClassInfo_GetXPCNativeWrapperClass(),
               "Why was this called?");

  JSObject *globalObj;
  aHolder->GetJSObject(&globalObj);
  NS_ASSERTION(globalObj, "Must have global by now!");
      
  const char* arg = "arg";
  NS_NAMED_LITERAL_STRING(body, "return new XPCNativeWrapper(arg);");

  // Can't use CompileFunction() here because our principal isn't
  // inited yet and a null principal makes it fail.
  JSFunction *fun =
    ::JS_CompileUCFunction(mContext,
                           globalObj,
                           "_XPCNativeWrapperCtor",
                           1, &arg,
                           (jschar*)body.get(),
                           body.Length(),
                           "javascript:return new XPCNativeWrapper(arg);",
                           1 // lineno
                           );
  NS_ENSURE_TRUE(fun, NS_ERROR_FAILURE);

  jsval globalVal = OBJECT_TO_JSVAL(globalObj);
  jsval wrapper;
      
  JSBool ok = ::JS_CallFunction(mContext, globalObj, fun,
                                1, &globalVal, &wrapper);
  if (!ok) {
    // No need to notify about pending exceptions here; we don't
    // expect any other than out of memory, really.
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(JSVAL_IS_OBJECT(wrapper), "This should be an object!");

  NS_DOMClassInfo_SetXPCNativeWrapperClass(
    ::JS_GetClass(mContext, JSVAL_TO_OBJECT(wrapper)));
  return NS_OK;
}

static JSPropertySpec OptionsProperties[] = {
  {"strict",    JSOPTION_STRICT,    JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {"werror",    JSOPTION_WERROR,    JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {0}
};

static JSBool JS_DLL_CALLBACK
GetOptionsProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (JSVAL_IS_INT(id)) {
    uint32 optbit = (uint32) JSVAL_TO_INT(id);
    if ((optbit & (optbit - 1)) == 0 && optbit <= JSOPTION_WERROR)
      *vp = (JS_GetOptions(cx) & optbit) ? JSVAL_TRUE : JSVAL_FALSE;
  }
  return JS_TRUE;
}

static JSBool JS_DLL_CALLBACK
SetOptionsProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (JSVAL_IS_INT(id)) {
    uint32 optbit = (uint32) JSVAL_TO_INT(id);

    // Don't let options other than strict and werror be set -- it would be
    // bad if web page script could clear JSOPTION_PRIVATE_IS_NSISUPPORTS!
    if ((optbit & (optbit - 1)) == 0 && optbit <= JSOPTION_WERROR) {
      JSBool optval;
      if (! ::JS_ValueToBoolean(cx, *vp, &optval))
        return JS_FALSE;

      uint32 optset = ::JS_GetOptions(cx);
      if (optval)
        optset |= optbit;
      else
        optset &= ~optbit;
      ::JS_SetOptions(cx, optset);
    }
  }
  return JS_TRUE;
}

static JSClass OptionsClass = {
  "JSOptions",
  0,
  JS_PropertyStub, JS_PropertyStub, GetOptionsProperty, SetOptionsProperty,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

#ifdef NS_TRACE_MALLOC

#include <errno.h>              // XXX assume Linux if NS_TRACE_MALLOC
#include <fcntl.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#ifdef XP_WIN32
#include <io.h>
#endif
#include "nsTraceMalloc.h"

static JSBool
TraceMallocDisable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    NS_TraceMallocDisable();
    return JS_TRUE;
}

static JSBool
TraceMallocEnable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    NS_TraceMallocEnable();
    return JS_TRUE;
}

static JSBool
TraceMallocOpenLogFile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int fd;
    JSString *str;
    char *filename;

    if (argc == 0) {
        fd = -1;
    } else {
        str = JS_ValueToString(cx, argv[0]);
        if (!str)
            return JS_FALSE;
        filename = JS_GetStringBytes(str);
        fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            JS_ReportError(cx, "can't open %s: %s", filename, strerror(errno));
            return JS_FALSE;
        }
    }
    *rval = INT_TO_JSVAL(fd);
    return JS_TRUE;
}

static JSBool
TraceMallocChangeLogFD(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int32 fd, oldfd;

    if (argc == 0) {
        oldfd = -1;
    } else {
        if (!JS_ValueToECMAInt32(cx, argv[0], &fd))
            return JS_FALSE;
        oldfd = NS_TraceMallocChangeLogFD(fd);
        if (oldfd == -2) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
    }
    *rval = INT_TO_JSVAL(oldfd);
    return JS_TRUE;
}

static JSBool
TraceMallocCloseLogFD(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int32 fd;

    if (argc == 0)
        return JS_TRUE;
    if (!JS_ValueToECMAInt32(cx, argv[0], &fd))
        return JS_FALSE;
    NS_TraceMallocCloseLogFD((int) fd);
    return JS_TRUE;
}

static JSBool
TraceMallocLogTimestamp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    const char *caption;

    str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;
    caption = JS_GetStringBytes(str);
    NS_TraceMallocLogTimestamp(caption);
    return JS_TRUE;
}

static JSBool
TraceMallocDumpAllocations(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    const char *pathname;

    str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;
    pathname = JS_GetStringBytes(str);
    if (NS_TraceMallocDumpAllocations(pathname) < 0) {
        JS_ReportError(cx, "can't dump to %s: %s", pathname, strerror(errno));
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec TraceMallocFunctions[] = {
    {"TraceMallocDisable",         TraceMallocDisable,         0, 0, 0},
    {"TraceMallocEnable",          TraceMallocEnable,          0, 0, 0},
    {"TraceMallocOpenLogFile",     TraceMallocOpenLogFile,     1, 0, 0},
    {"TraceMallocChangeLogFD",     TraceMallocChangeLogFD,     1, 0, 0},
    {"TraceMallocCloseLogFD",      TraceMallocCloseLogFD,      1, 0, 0},
    {"TraceMallocLogTimestamp",    TraceMallocLogTimestamp,    1, 0, 0},
    {"TraceMallocDumpAllocations", TraceMallocDumpAllocations, 1, 0, 0},
    {nsnull,                       nsnull,                     0, 0, 0}
};

#endif /* NS_TRACE_MALLOC */

#ifdef MOZ_JPROF

#include <signal.h>

inline PRBool
IsJProfAction(struct sigaction *action)
{
    return (action->sa_sigaction &&
            action->sa_flags == SA_RESTART | SA_SIGINFO);
}

static JSBool
JProfStartProfiling(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *rval)
{
    // Figure out whether we're dealing with SIGPROF, SIGALRM, or
    // SIGPOLL profiling (SIGALRM for JP_REALTIME, SIGPOLL for
    // JP_RTC_HZ)
    struct sigaction action;

    sigaction(SIGALRM, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning real-time jprof profiling.\n");
        raise(SIGALRM);
        return JS_TRUE;
    }

    sigaction(SIGPROF, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning process-time jprof profiling.\n");
        raise(SIGPROF);
        return JS_TRUE;
    }

    sigaction(SIGPOLL, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning rtc-based jprof profiling.\n");
        raise(SIGPOLL);
        return JS_TRUE;
    }

    printf("Could not start jprof-profiling since JPROF_FLAGS was not set.\n");
    return JS_TRUE;
}

static JSBool
JProfStopProfiling(JSContext *cx, JSObject *obj,
                   uintN argc, jsval *argv, jsval *rval)
{
    raise(SIGUSR1);
    printf("Stopped jprof profiling.\n");
    return JS_TRUE;
}

static JSFunctionSpec JProfFunctions[] = {
    {"JProfStartProfiling",        JProfStartProfiling,        0, 0, 0},
    {"JProfStopProfiling",         JProfStopProfiling,         0, 0, 0},
    {nsnull,                       nsnull,                     0, 0, 0}
};

#endif /* defined(MOZ_JPROF) */

nsresult
nsJSContext::InitClasses(JSObject *aGlobalObj)
{
  nsresult rv = NS_OK;

  rv = InitializeExternalClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitializeLiveConnectClasses(aGlobalObj);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the options object and set default options in mContext
  JSObject *optionsObj = ::JS_DefineObject(mContext, aGlobalObj, "_options",
                                           &OptionsClass, nsnull, 0);
  if (optionsObj &&
      ::JS_DefineProperties(mContext, optionsObj, OptionsProperties)) {
    ::JS_SetOptions(mContext, mDefaultJSOptions);
  } else {
    rv = NS_ERROR_FAILURE;
  }

#ifdef NS_TRACE_MALLOC
  // Attempt to initialize TraceMalloc functions
  ::JS_DefineFunctions(mContext, aGlobalObj, TraceMallocFunctions);
#endif

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(mContext, aGlobalObj, JProfFunctions);
#endif

  return rv;
}

void
nsJSContext::WillInitializeContext()
{
  mIsInitialized = PR_FALSE;
}

void
nsJSContext::DidInitializeContext()
{
  mIsInitialized = PR_TRUE;
}

PRBool
nsJSContext::IsContextInitialized()
{
  return mIsInitialized;
}

void
nsJSContext::GC()
{
  FireGCTimer();
}

void
nsJSContext::ScriptEvaluated(PRBool aTerminated)
{
  if (aTerminated && mTerminations) {
    // Make sure to null out mTerminations before doing anything that
    // might cause new termination funcs to be added!
    nsJSContext::TerminationFuncClosure* start = mTerminations;
    mTerminations = nsnull;
    
    for (nsJSContext::TerminationFuncClosure* cur = start;
         cur;
         cur = cur->mNext) {
      (*(cur->mTerminationFunc))(cur->mTerminationFuncArg);
    }
    delete start;
  }

  mNumEvaluations++;

#ifdef WAY_TOO_MUCH_GC
  ::JS_MaybeGC(mContext);
#else
  if (mNumEvaluations > 20) {
    mNumEvaluations = 0;
    ::JS_MaybeGC(mContext);
  }
#endif

  mBranchCallbackCount = 0;
  mBranchCallbackTime = LL_ZERO;
}

void
nsJSContext::SetOwner(nsIScriptContextOwner* owner)
{
  // The owner should not be addrefed!! We'll be told
  // when the owner goes away.
  mOwner = owner;
}

nsIScriptContextOwner *
nsJSContext::GetOwner()
{
  return mOwner;
}

nsresult
nsJSContext::SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsISupports* aRef)
{
  nsJSContext::TerminationFuncClosure* newClosure =
    new nsJSContext::TerminationFuncClosure(aFunc, aRef, mTerminations);
  if (!newClosure) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mTerminations = newClosure;
  return NS_OK;
}

PRBool
nsJSContext::GetScriptsEnabled()
{
  return mScriptsEnabled;
}

void
nsJSContext::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
  mScriptsEnabled = aEnabled;

  nsIScriptGlobalObject *global = GetGlobalObject();

  if (global) {
    global->SetScriptsEnabled(aEnabled, aFireTimeouts);
  }
}


PRBool
nsJSContext::GetProcessingScriptTag()
{
  return mProcessingScriptTag;
}

void
nsJSContext::SetProcessingScriptTag(PRBool aFlag)
{
  mProcessingScriptTag = aFlag;
}

void
nsJSContext::SetGCOnDestruction(PRBool aGCOnDestruction)
{
  mGCOnDestruction = aGCOnDestruction;
}

NS_IMETHODIMP
nsJSContext::ScriptExecuted()
{
  ScriptEvaluated(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::PreserveWrapper(nsIXPConnectWrappedNative *aWrapper)
{
  return nsDOMClassInfo::PreserveNodeWrapper(aWrapper);
}

NS_IMETHODIMP
nsJSContext::Notify(nsITimer *timer)
{
  NS_ASSERTION(mContext, "No context in nsJSContext::Notify()!");

  ::JS_GC(mContext);

  sReadyForGC = PR_TRUE;

  NS_RELEASE(sGCTimer);
  return NS_OK;
}

void
nsJSContext::FireGCTimer()
{
  // Always clear the newborn roots.  If there's already a timer, this
  // will let the GC from that timer clean up properly.  If we're going
  // to create a timer, we still want to do this now so that XPCOM
  // shutdown can clean up properly.
  ::JS_ClearNewbornRoots(mContext);

  if (sGCTimer) {
    // There's already a timer for GC'ing, just return
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sGCTimer);

  if (!sGCTimer) {
    NS_WARNING("Failed to create timer");

    ::JS_GC(mContext);

    return;
  }

  static PRBool first = PR_TRUE;

  sGCTimer->InitWithCallback(this,
                             first ? NS_FIRST_GC_DELAY : NS_GC_DELAY,
                             nsITimer::TYPE_ONE_SHOT);

  first = PR_FALSE;
}

static JSBool JS_DLL_CALLBACK
DOMGCCallback(JSContext *cx, JSGCStatus status)
{
  JSBool result = gOldJSGCCallback ? gOldJSGCCallback(cx, status) : JS_TRUE;

  if (status == JSGC_BEGIN && !NS_IsMainThread())
    return JS_FALSE;

  // XPCJSRuntime::GCCallback does marking from the JSGC_MARK_END callback.
  // we need to call EndGCMark *after* marking is finished.
  // XXX This relies on our callback being registered after
  // XPCJSRuntime's, although if they were registered the other way
  // around the ordering there would be correct.
  if (status == JSGC_MARK_END)
    nsDOMClassInfo::EndGCMark();

  return result;
}

//static
void
nsJSEnvironment::Startup()
{
  // initialize all our statics, so that we can restart XPCOM
  sGCTimer = nsnull;
  sReadyForGC = PR_FALSE;
  gNameSpaceManager = nsnull;
  sRuntimeService = nsnull;
  sRuntime = nsnull;
  gOldJSGCCallback = nsnull;
  sIsInitialized = PR_FALSE;
  sDidShutdown = PR_FALSE;
  sContextCount = 0;
  sSecurityManager = nsnull;
  gCollation = nsnull;
}

static int PR_CALLBACK
MaxScriptRunTimePrefChangedCallback(const char *aPrefName, void *aClosure)
{
  // Default limit on script run time to 5 seconds. 0 means let
  // scripts run forever.
  PRInt32 time = nsContentUtils::GetIntPref(aPrefName, 5);

  if (time <= 0) {
    // Let scripts run for a really, really long time.
    sMaxScriptRunTime = LL_INIT(0x40000000, 0);
  } else {
    PRTime usec_per_sec;
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_I2L(sMaxScriptRunTime, time);
    LL_MUL(sMaxScriptRunTime, sMaxScriptRunTime, usec_per_sec);
  }

  return 0;
}

JS_STATIC_DLL_CALLBACK(JSPrincipals *)
ObjectPrincipalFinder(JSContext *cx, JSObject *obj)
{
  if (!sSecurityManager)
    return nsnull;

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv =
    sSecurityManager->GetObjectPrincipal(cx, obj,
                                         getter_AddRefs(principal));

  if (NS_FAILED(rv) || !principal) {
    return nsnull;
  }

  JSPrincipals *jsPrincipals = nsnull;
  principal->GetJSPrincipals(cx, &jsPrincipals);

  // nsIPrincipal::GetJSPrincipals() returns a strong reference to the
  // JS principals, but the caller of this function expects a weak
  // reference. So we need to release here.

  JSPRINCIPALS_DROP(cx, jsPrincipals);

  return jsPrincipals;
}

// static
nsresult
nsJSEnvironment::Init()
{
  if (sIsInitialized) {
    if (!nsContentUtils::XPConnect())
      return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
  }

  nsresult rv = CallGetService(kJSRuntimeServiceContractID, &sRuntimeService);
  // get the JSRuntime from the runtime svc, if possible
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sRuntimeService->GetRuntime(&sRuntime);
  NS_ENSURE_SUCCESS(rv, rv);

  // Let's make sure that our main thread is the same as the xpcom main thread.
  NS_ASSERTION(NS_IsMainThread(), "bad");

  NS_ASSERTION(!gOldJSGCCallback,
               "nsJSEnvironment initialized more than once");

  // Save the old GC callback to chain to it, for GC-observing generality.
  gOldJSGCCallback = ::JS_SetGCCallbackRT(sRuntime, DOMGCCallback);

  // No chaining to a pre-existing callback here, we own this problem space.
#ifdef NS_DEBUG
  JSObjectPrincipalsFinder oldfop =
#endif
    ::JS_SetObjectPrincipalsFinder(sRuntime, ObjectPrincipalFinder);
  NS_ASSERTION(!oldfop, " fighting over the findObjectPrincipals callback!");

  // Set these global xpconnect options...
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  xpc->SetCollectGarbageOnMainThreadOnly(PR_TRUE);
  xpc->SetDeferReleasesUntilAfterGarbageCollection(PR_TRUE);

#ifdef OJI
  // Initialize LiveConnect.  XXXbe use contractid rather than GetCID
  // NOTE: LiveConnect is optional so initialisation will still succeed
  //       even if the service is not present.
  nsCOMPtr<nsILiveConnectManager> manager =
           do_GetService(nsIJVMManager::GetCID());

  // Should the JVM manager perhaps define methods for starting up
  // LiveConnect?
  if (manager) {
    PRBool started = PR_FALSE;
    rv = manager->StartupLiveConnect(sRuntime, started);
    // XXX Did somebody mean to check |rv| ?
  }
#endif /* OJI */

  nsContentUtils::RegisterPrefCallback("dom.max_script_run_time",
                                       MaxScriptRunTimePrefChangedCallback,
                                       nsnull);
  MaxScriptRunTimePrefChangedCallback("dom.max_script_run_time", nsnull);

  rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &sSecurityManager);

  sIsInitialized = NS_SUCCEEDED(rv);

  return rv;
}

// static
void nsJSEnvironment::ShutDown()
{
  if (sGCTimer) {
    // We're being shut down, if we have a GC timer scheduled, cancel
    // it. The DOM factory will do one final GC once it's shut down.

    sGCTimer->Cancel();

    NS_RELEASE(sGCTimer);
  }

  delete gNameSpaceManager;
  gNameSpaceManager = nsnull;

  if (!sContextCount) {
    // We're being shutdown, and there are no more contexts
    // alive, release the JS runtime service and the security manager.

    if (sRuntimeService && sSecurityManager) {
      // No chaining to a pre-existing callback here, we own this problem space.
#ifdef NS_DEBUG
      JSObjectPrincipalsFinder oldfop =
#endif
        ::JS_SetObjectPrincipalsFinder(sRuntime, nsnull);
      NS_ASSERTION(oldfop == ObjectPrincipalFinder, " fighting over the findObjectPrincipals callback!");
    }
    NS_IF_RELEASE(sRuntimeService);
    NS_IF_RELEASE(sSecurityManager);
    NS_IF_RELEASE(gCollation);
    NS_IF_RELEASE(gDecoder);
  }

  sDidShutdown = PR_TRUE;
}

// static
nsresult
nsJSEnvironment::CreateNewContext(nsIScriptContext **aContext)
{
  *aContext = new nsJSContext(sRuntime);
  NS_ENSURE_TRUE(*aContext, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aContext);
  return NS_OK;
}

nsresult
NS_CreateScriptContext(nsIScriptGlobalObject *aGlobal,
                       nsIScriptContext **aContext)
{
  nsresult rv = nsJSEnvironment::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptContext> scriptContext;
  rv = nsJSEnvironment::CreateNewContext(getter_AddRefs(scriptContext));
  NS_ENSURE_SUCCESS(rv, rv);

  scriptContext->WillInitializeContext();

  // Bind the script context and the global object
  rv = scriptContext->InitContext(aGlobal);
  NS_ENSURE_SUCCESS(rv, rv);

  scriptContext->DidInitializeContext();

  if (aGlobal) {
    aGlobal->SetContext(scriptContext);
  }

  *aContext = scriptContext;

  NS_ADDREF(*aContext);

  return rv;
}

