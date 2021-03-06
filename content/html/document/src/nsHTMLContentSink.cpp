/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Daniel Glazman <glazman@netscape.com>
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

#include "nsContentSink.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIHTMLContentSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIParser.h"
#include "nsParserUtils.h"
#include "nsIScriptLoader.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsINodeInfo.h"
#include "nsHTMLTokens.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "prtime.h"
#include "prlog.h"
#include "nsInt64.h"

#include "nsGenericHTMLElement.h"
#include "nsITextContent.h"

#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIScriptElement.h"

#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsHTMLAtoms.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsStubDocumentObserver.h"
#include "nsIHTMLDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsICookieService.h"
#include "nsVoidArray.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsTextFragment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"

#include "nsIParserService.h"
#include "nsISelectElement.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsTimer.h"
#include "nsITimer.h"
#include "nsDOMError.h"
#include "nsContentPolicyUtils.h"
#include "nsIScriptContext.h"
#include "nsStyleLinkElement.h"

#include "nsReadableUtils.h"
#include "nsWeakReference.h" // nsHTMLElementFactory supports weak references
#include "nsIPrompt.h"
#include "nsLayoutCID.h"
#include "nsIDocShellTreeItem.h"

#include "nsEscape.h"
#include "nsIElementObserver.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

//----------------------------------------------------------------------

static void
FavorPerformanceHint(PRBool perfOverStarvation, PRUint32 starvationDelay)
{
  static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell)
    appShell->FavorPerformanceHint(perfOverStarvation, starvationDelay);
}

//----------------------------------------------------------------------

#ifdef NS_DEBUG
static PRLogModuleInfo* gSinkLogModuleInfo;

#define SINK_TRACE_CALLS              0x1
#define SINK_TRACE_REFLOW             0x2
#define SINK_ALWAYS_REFLOW            0x4

#define SINK_LOG_TEST(_lm, _bit) (PRIntn((_lm)->level) & (_bit))

#define SINK_TRACE(_bit, _args)                       \
  PR_BEGIN_MACRO                                      \
    if (SINK_LOG_TEST(gSinkLogModuleInfo, _bit)) {    \
      PR_LogPrint _args;                              \
    }                                                 \
  PR_END_MACRO

#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj) \
  _obj->SinkTraceNode(_bit, _msg, _tag, _sp, this)

#else
#define SINK_TRACE(_bit, _args)
#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj)
#endif

#undef SINK_NO_INCREMENTAL

//----------------------------------------------------------------------

#define NS_SINK_FLAG_SCRIPT_ENABLED       0x00000008

#define NS_SINK_FLAG_FRAMES_ENABLED       0x00000010

// Interrupt parsing when mMaxTokenProcessingTime is exceeded
#define NS_SINK_FLAG_CAN_INTERRUPT_PARSER 0x00000020

// Lower the value for mNotificationInterval and
// mMaxTokenProcessingTime
#define NS_SINK_FLAG_DYNAMIC_LOWER_VALUE  0x00000040

#define NS_SINK_FLAG_FORM_ON_STACK        0x00000080

#define NS_SINK_FLAG_PARSING              0x00000100

#define NS_SINK_FLAG_DROPPED_TIMER        0x00000200

// 1/2 second fudge factor for window creation
#define NS_DELAY_FOR_WINDOW_CREATION  500000

// 200 determined empirically to provide good user response without
// sampling the clock too often.
#define NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE 200

typedef nsGenericHTMLElement* (*contentCreatorCallback)(nsINodeInfo*, PRBool aFromParser);

nsGenericHTMLElement*
NS_NewHTMLNOTUSEDElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
{
  NS_NOTREACHED("The element ctor should never be called");
  return nsnull;
}

#define HTML_TAG(_tag, _classname) NS_NewHTML##_classname##Element,
#define HTML_OTHER(_tag) NS_NewHTMLNOTUSEDElement,
static const contentCreatorCallback sContentCreatorCallbacks[] = {
  NS_NewHTMLUnknownElement,
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_OTHER
  NS_NewHTMLUnknownElement
};

class SinkContext;
class HTMLContentSink;

static void MaybeSetForm(nsGenericHTMLElement*, nsHTMLTag, HTMLContentSink*);

class HTMLContentSink : public nsContentSink,
                        public nsIHTMLContentSink,
                        public nsITimerCallback,
#ifdef DEBUG
                        public nsIDebugDumpContent,
#endif
                        public nsStubDocumentObserver
{
public:
  friend class SinkContext;
  friend class DummyParserRequest;
  friend void MaybeSetForm(nsGenericHTMLElement*, nsHTMLTag, HTMLContentSink*);

  HTMLContentSink();
  virtual ~HTMLContentSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsresult Init(nsIDocument* aDoc, nsIURI* aURI, nsISupports* aContainer,
                nsIChannel* aChannel);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(void);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  virtual void FlushPendingNotifications(mozFlushType aType);
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget();

  // nsIHTMLContentSink
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD CloseMalformedContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  NS_IMETHOD WillProcessTokens(void);
  NS_IMETHOD DidProcessTokens(void);
  NS_IMETHOD WillProcessAToken(void);
  NS_IMETHOD DidProcessAToken(void);
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode);
  NS_IMETHOD BeginContext(PRInt32 aID);
  NS_IMETHOD EndContext(PRInt32 aID);
  NS_IMETHOD OpenHead();
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn);
  NS_IMETHOD_(PRBool) IsFormOnStack();

  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK
  
  // nsIDocumentObserver
  virtual void BeginUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType);
  virtual void EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType);

#ifdef DEBUG
  // nsIDebugDumpContent
  NS_IMETHOD DumpContentModel();
#endif

protected:
  PRBool IsTimeToNotify();

  nsresult UpdateDocumentTitle();
  // If aCheckIfPresent is true, will only set an attribute in cases
  // when it's not already set.
  nsresult AddAttributes(const nsIParserNode& aNode, nsIContent* aContent,
                         PRBool aNotify = PR_FALSE,
                         PRBool aCheckIfPresent = PR_FALSE);
  already_AddRefed<nsGenericHTMLElement>
  CreateContentObject(const nsIParserNode& aNode, nsHTMLTag aNodeType);

  inline PRInt32 GetNotificationInterval()
  {
    if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
      return 1000;
    }

    return mNotificationInterval;
  }

  inline PRInt32 GetMaxTokenProcessingTime()
  {
    if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
      return 3000;
    }

    return mMaxTokenProcessingTime;
  }

#ifdef NS_DEBUG
  void SinkTraceNode(PRUint32 aBit,
                     const char* aMsg,
                     const nsHTMLTag aTag,
                     PRInt32 aStackPos,
                     void* aThis);
#endif

  nsIHTMLDocument* mHTMLDocument;

  // back off timer notification after count
  PRInt32 mBackoffCount;

  // Notification interval in microseconds
  PRInt32 mNotificationInterval;

  // Time of last notification
  PRTime mLastNotificationTime;

  // Timer used for notification
  nsCOMPtr<nsITimer> mNotificationTimer;

  // The maximum length of a text run
  PRInt32 mMaxTextRun;

  nsGenericHTMLElement* mRoot;
  nsGenericHTMLElement* mBody;
  nsRefPtr<nsGenericHTMLElement> mFrameset;
  nsGenericHTMLElement* mHead;

  // Do we notify based on time?
  PRPackedBool mNotifyOnTimer;

  PRPackedBool mLayoutStarted;
  PRPackedBool mScrolledToRefAlready;
  PRPackedBool mInTitle;

  nsString mTitleString;
  PRInt32 mInNotification;
  nsRefPtr<nsGenericHTMLElement> mCurrentForm;

  nsAutoVoidArray mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;
  PRInt32 mNumOpenIFRAMES;
  nsCOMPtr<nsIRequest> mDummyParserRequest;

  nsCOMPtr<nsIURI> mBaseHref;
  nsCOMPtr<nsIAtom> mBaseTarget;

  // depth of containment within <noembed>, <noframes> etc
  PRInt32 mInsideNoXXXTag;
  PRInt32 mInMonolithicContainer;
  PRUint32 mFlags;

  // -- Can interrupt parsing members --
  PRUint32 mDelayTimerStart;

  // Interrupt parsing during token procesing after # of microseconds
  PRInt32 mMaxTokenProcessingTime;

  // Switch between intervals when time is exceeded
  PRInt32 mDynamicIntervalSwitchThreshold;

  PRInt32 mBeginLoadTime;

  // Last mouse event or keyboard event time sampled by the content
  // sink
  PRUint32 mLastSampledUserEventTime;

  // The number of tokens that have been processed while in the low
  // frequency parser interrupt mode without falling through to the
  // logic which decides whether to switch to the high frequency
  // parser interrupt mode.
  PRUint8 mDeflectedCount;

  // Boolean indicating whether we've notified insertion of our root content
  // yet.  We want to make sure to only do this once.
  PRPackedBool mNotifiedRootInsertion;

  // Boolean indicating whether we've seen a <head> tag that might have had
  // attributes once already.
  PRPackedBool mHaveSeenHead;

  nsCOMPtr<nsIObserverEntry> mObservers;

  void StartLayout();

  void TryToScrollToRef();

  /**
   * AddBaseTagInfo adds the "current" base URI and target to the content node
   * in the form of bogo-attributes.  This MUST be called before attributes are
   * added to the content node, since the way URI attributes are treated may
   * depend on the value of the base URI
   */
  void AddBaseTagInfo(nsIContent* aContent);

  // Routines for tags that require special handling
  nsresult CloseHTML();
  nsresult OpenFrameset(const nsIParserNode& aNode);
  nsresult CloseFrameset();
  nsresult OpenBody(const nsIParserNode& aNode);
  nsresult CloseBody();
  nsresult OpenForm(const nsIParserNode& aNode);
  nsresult CloseForm();
  void ProcessBASEElement(nsGenericHTMLElement* aElement);
  nsresult ProcessLINKTag(const nsIParserNode& aNode);

  // Routines for tags that require special handling when we reach their end
  // tag.
  nsresult ProcessSCRIPTEndTag(nsGenericHTMLElement* content,
                               PRBool aHaveNotified,
                               PRBool aMalformed);
  nsresult ProcessSTYLEEndTag(nsGenericHTMLElement* content);

  nsresult OpenHeadContext();
  void CloseHeadContext();

  // nsContentSink overrides
  virtual void PreEvaluateScript();
  virtual void PostEvaluateScript(nsIScriptElement *aElement);

  void UpdateAllContexts();
  void NotifyAppend(nsIContent* aContent,
                    PRUint32 aStartIndex);
  void NotifyInsert(nsIContent* aContent,
                    nsIContent* aChildContent,
                    PRInt32 aIndexInContainer);
  PRBool IsMonolithicContainer(nsHTMLTag aTag);

  // CanInterrupt parsing related routines
  nsresult AddDummyParserRequest(void);
  nsresult RemoveDummyParserRequest(void);

#ifdef NS_DEBUG
  void ForceReflow();
#endif

  // Measures content model creation time for current document
  MOZ_TIMER_DECLARE(mWatch)
};


//----------------------------------------------------------------------
//
// DummyParserRequest
//
//   This is a dummy request implementation that we add to the document's load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't fire
//   before we've finished all of parsing and tokenizing of the document.
//

class DummyParserRequest : public nsIRequest
{
protected:
  DummyParserRequest(nsIHTMLContentSink* aSink);

  nsIHTMLContentSink* mSink; // Weak reference

public:
  static nsresult
  Create(nsIRequest** aResult, nsIHTMLContentSink* aSink);

  NS_DECL_ISUPPORTS

  // nsIRequest
  NS_IMETHOD GetName(nsACString &result)
  {
    result.AssignLiteral("about:layout-dummy-request");
    return NS_OK;
  }

  NS_IMETHOD IsPending(PRBool *_retval)
  {
    *_retval = PR_TRUE;
    return NS_OK;
  }

  NS_IMETHOD GetStatus(nsresult *status)
  {
    *status = NS_OK;
    return NS_OK;
  }

  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend(void)
  {
    return NS_OK;
  }

  NS_IMETHOD Resume(void)
  {
    return NS_OK;
  }

  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup)
  {
    *aLoadGroup = nsnull;

    return NS_OK;
  }

  NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup)
  {
    return NS_OK;
  }

  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags)
  {
    *aLoadFlags = nsIRequest::LOAD_NORMAL;

    return NS_OK;
  }

  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags)
  {
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(DummyParserRequest, nsIRequest)

nsresult
DummyParserRequest::Create(nsIRequest** aResult, nsIHTMLContentSink* aSink)
{
  *aResult = new DummyParserRequest(aSink);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}


DummyParserRequest::DummyParserRequest(nsIHTMLContentSink* aSink)
{
  mSink = aSink;
}

NS_IMETHODIMP
DummyParserRequest::Cancel(nsresult status)
{
  // Cancel parser
  nsresult rv = NS_OK;
  HTMLContentSink* sink = NS_STATIC_CAST(HTMLContentSink*, mSink);
  if ((sink) && (sink->mParser)) {
    sink->mParser->CancelParsingEvents();
  }
  return rv;
}


class SinkContext
{
public:
  SinkContext(HTMLContentSink* aSink);
  ~SinkContext();

  nsresult Begin(nsHTMLTag aNodeType, nsGenericHTMLElement* aRoot,
                 PRUint32 aNumFlushed, PRInt32 aInsertionPoint);
  nsresult OpenContainer(const nsIParserNode& aNode);
  nsresult CloseContainer(const nsHTMLTag aTag, PRBool aMalformed);
  nsresult AddLeaf(const nsIParserNode& aNode);
  nsresult AddLeaf(nsGenericHTMLElement* aContent);
  nsresult AddComment(const nsIParserNode& aNode);
  nsresult End();

  nsresult GrowStack();
  nsresult AddText(const nsAString& aText);
  nsresult FlushText(PRBool* aDidFlush = nsnull,
                     PRBool aReleaseLast = PR_FALSE);
  nsresult FlushTextAndRelease(PRBool* aDidFlush = nsnull)
  {
    return FlushText(aDidFlush, PR_TRUE);
  }

  nsresult FlushTags(PRBool aNotify);

  PRBool   IsCurrentContainer(nsHTMLTag mType);
  PRBool   IsAncestorContainer(nsHTMLTag mType);

  void DidAddContent(nsIContent* aContent);
  void UpdateChildCounts();

private:
  // Function to check whether we've notified for the current content.
  // What this actually does is check whether we've notified for all
  // of the parent's kids.
  PRBool HaveNotifiedForCurrentContent() const;
  
public:
  HTMLContentSink* mSink;
  PRInt32 mNotifyLevel;
  nsCOMPtr<nsITextContent> mLastTextNode;
  PRInt32 mLastTextNodeSize;

  struct Node {
    nsHTMLTag mType;
    nsGenericHTMLElement* mContent;
    PRUint32 mNumFlushed;
    PRInt32 mInsertionPoint;
  };

  Node* mStack;
  PRInt32 mStackSize;
  PRInt32 mStackPos;

  PRUnichar* mText;
  PRInt32 mTextLength;
  PRInt32 mTextSize;
};

//----------------------------------------------------------------------

#ifdef NS_DEBUG
void
HTMLContentSink::SinkTraceNode(PRUint32 aBit,
                               const char* aMsg,
                               const nsHTMLTag aTag,
                               PRInt32 aStackPos,
                               void* aThis)
{
  if (SINK_LOG_TEST(gSinkLogModuleInfo, aBit)) {
    nsIParserService *parserService = nsContentUtils::GetParserService();
    if (!parserService)
      return;

    NS_ConvertUTF16toUTF8 tag(parserService->HTMLIdToStringTag(aTag));
    PR_LogPrint("%s: this=%p node='%s' stackPos=%d", 
                aMsg, aThis, tag.get(), aStackPos);
  }
}
#endif

nsresult
HTMLContentSink::AddAttributes(const nsIParserNode& aNode,
                               nsIContent* aContent, PRBool aNotify,
                               PRBool aCheckIfPresent)
{
  // Add tag attributes to the content attributes

  PRInt32 ac = aNode.GetAttributeCount();

  if (ac == 0) {
    // No attributes, nothing to do. Do an early return to avoid
    // constructing the nsAutoString object for nothing.

    return NS_OK;
  }

  nsCAutoString k;
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

  // The attributes are on the parser node in the order they came in in the
  // source.  What we want to happen if a single attribute is set multiple
  // times on an element is that the first time should "win".  That is, <input
  // value="foo" value="bar"> should show "foo".  So we loop over the
  // attributes backwards; this ensures that the first attribute in the set
  // wins.  This does mean that we do some extra work in the case when the same
  // attribute is set multiple times, but we save a HasAttr call in the much
  // more common case of reasonable HTML.  Note that if aCheckIfPresent is set
  // then we actually want to loop _forwards_ to preserve the "first attribute
  // wins" behavior.  That does mean that when aCheckIfPresent is set the order
  // of attributes will get "reversed" from the point of view of the
  // serializer.  But aCheckIfPresent is only true for malformed documents with
  // multiple <html>, <head>, or <body> tags, so we're doing fixup anyway at
  // that point.

  PRInt32 i, limit, step;
  if (aCheckIfPresent) {
    i = 0;
    limit = ac;
    step = 1;
  } else {
    i = ac - 1;
    limit = -1;
    step = -1;
  }
  
  for (; i != limit; i += step) {
    // Get lower-cased key
    const nsAString& key = aNode.GetKeyAt(i);
    // Copy up-front to avoid shared-buffer overhead (and convert to UTF-8
    // at the same time since that's what the atom table uses).
    CopyUTF16toUTF8(key, k);
    ToLowerCase(k);

    nsCOMPtr<nsIAtom> keyAtom = do_GetAtom(k);

    if (aCheckIfPresent && aContent->HasAttr(kNameSpaceID_None, keyAtom)) {
      continue;
    }

    // Get value and remove mandatory quotes
    static const char* kWhitespace = "\n\r\t\b";

    // Bug 114997: Don't trim whitespace on <input value="...">:
    // Using ?: outside the function call would be more efficient, but
    // we don't trust ?: with references.
    const nsAString& v =
      nsContentUtils::TrimCharsInSet(
        (nodeType == eHTMLTag_input &&
          keyAtom == nsHTMLAtoms::value) ?
        "" : kWhitespace, aNode.GetValueAt(i));

    if (nodeType == eHTMLTag_a && keyAtom == nsHTMLAtoms::name) {
      NS_ConvertUTF16toUTF8 cname(v);
      NS_ConvertUTF8toUTF16 uv(nsUnescape(cname.BeginWriting()));

      // Add attribute to content
      aContent->SetAttr(kNameSpaceID_None, keyAtom, uv, aNotify);
    } else {
      // Add attribute to content
      aContent->SetAttr(kNameSpaceID_None, keyAtom, v, aNotify);
    }
  }

  return NS_OK;
}

static void
MaybeSetForm(nsGenericHTMLElement* aContent, nsHTMLTag aNodeType,
             HTMLContentSink* aSink)
{
  nsGenericHTMLElement* form = aSink->mCurrentForm;

  if (!form || aSink->mInsideNoXXXTag) {
    return;
  }

  switch (aNodeType) {
    case eHTMLTag_button:
    case eHTMLTag_fieldset:
    case eHTMLTag_label:
    case eHTMLTag_legend:
    case eHTMLTag_object:
    case eHTMLTag_input:
    case eHTMLTag_select:
    case eHTMLTag_textarea:
      break;
    default:
      return;
  }
  
  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(aContent));
  NS_ASSERTION(formControl,
               "nsGenericHTMLElement didn't implement nsIFormControl");
  nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(form));
  NS_ASSERTION(!form || formElement,
               "nsGenericHTMLElement didn't implement nsIDOMHTMLFormElement");

  formControl->SetForm(formElement);
}

static already_AddRefed<nsGenericHTMLElement>
MakeContentObject(nsHTMLTag aNodeType, nsINodeInfo *aNodeInfo,
                  PRBool aFromParser);

/**
 * Factory subroutine to create all of the html content objects.
 */
already_AddRefed<nsGenericHTMLElement>
HTMLContentSink::CreateContentObject(const nsIParserNode& aNode,
                                     nsHTMLTag aNodeType)
{
  // Find/create atom for the tag name

  nsCOMPtr<nsINodeInfo> nodeInfo;

  if (aNodeType == eHTMLTag_userdefined) {
    NS_ConvertUTF16toUTF8 tmp(aNode.GetText());
    ToLowerCase(tmp);

    nsCOMPtr<nsIAtom> name = do_GetAtom(tmp);
    mNodeInfoManager->GetNodeInfo(name, nsnull, kNameSpaceID_None,
                                  getter_AddRefs(nodeInfo));
  } else {
    nsIParserService *parserService = nsContentUtils::GetParserService();
    if (!parserService)
      return nsnull;

    nsIAtom *name = parserService->HTMLIdToAtomTag(aNodeType);
    NS_ASSERTION(name, "What? Reverse mapping of id to string broken!!!");

    mNodeInfoManager->GetNodeInfo(name, nsnull, kNameSpaceID_None,
                                  getter_AddRefs(nodeInfo));
  }

  NS_ENSURE_TRUE(nodeInfo, nsnull);

  // Make the content object
  nsGenericHTMLElement* result = MakeContentObject(aNodeType, nodeInfo,
                                                   PR_TRUE).get();
  if (!result) {
    return nsnull;
  }

  return result;
}

nsresult
NS_NewHTMLElement(nsIContent** aResult, nsINodeInfo *aNodeInfo)
{
  *aResult = nsnull;

  nsIParserService* parserService = nsContentUtils::GetParserService();
  if (!parserService)
    return NS_ERROR_OUT_OF_MEMORY;

  nsIAtom *name = aNodeInfo->NameAtom();

  nsHTMLTag id;
  nsRefPtr<nsGenericHTMLElement> result;
  if (aNodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    // Find tag in tag table
    id = nsHTMLTag(parserService->HTMLCaseSensitiveAtomTagToId(name));

    result = MakeContentObject(id, aNodeInfo, PR_FALSE);
  }
  else {
    // Find tag in tag table
    id = nsHTMLTag(parserService->HTMLAtomTagToId(name));

    // Reverse map id to name to get the correct character case in
    // the tag name.

    nsCOMPtr<nsINodeInfo> kungFuDeathGrip;
    nsINodeInfo *nodeInfo = aNodeInfo;

    if (id != eHTMLTag_userdefined) {
      nsIAtom *tag = parserService->HTMLIdToAtomTag(id);
      NS_ASSERTION(tag, "What? Reverse mapping of id to string broken!!!");

      if (name != tag) {
        nsresult rv =
          nsContentUtils::NameChanged(aNodeInfo, tag,
                                      getter_AddRefs(kungFuDeathGrip));
        NS_ENSURE_SUCCESS(rv, rv);

        nodeInfo = kungFuDeathGrip;
      }
    }

    result = MakeContentObject(id, nodeInfo, PR_FALSE);
  }

  return result ? CallQueryInterface(result.get(), aResult)
    : NS_ERROR_OUT_OF_MEMORY;
}

already_AddRefed<nsGenericHTMLElement>
MakeContentObject(nsHTMLTag aNodeType, nsINodeInfo *aNodeInfo,
                  PRBool aFromParser)
{
  contentCreatorCallback cb = sContentCreatorCallbacks[aNodeType];

  NS_ASSERTION(cb != NS_NewHTMLNOTUSEDElement,
               "Don't know how to construct tag element!");

  nsGenericHTMLElement* result = cb(aNodeInfo, aFromParser);
  if (!result) {
    return nsnull;
  }

  NS_ADDREF(result);

  return result;
}

//----------------------------------------------------------------------

SinkContext::SinkContext(HTMLContentSink* aSink)
  : mSink(aSink),
    mNotifyLevel(0),
    mLastTextNodeSize(0),
    mStack(nsnull),
    mStackSize(0),
    mStackPos(0),
    mText(nsnull),
    mTextLength(0),
    mTextSize(0)
{
  MOZ_COUNT_CTOR(SinkContext);
}

SinkContext::~SinkContext()
{
  MOZ_COUNT_DTOR(SinkContext);

  if (mStack) {
    for (PRInt32 i = 0; i < mStackPos; i++) {
      NS_RELEASE(mStack[i].mContent);
    }
    delete [] mStack;
  }

  delete [] mText;
}

nsresult
SinkContext::Begin(nsHTMLTag aNodeType,
                   nsGenericHTMLElement* aRoot,
                   PRUint32 aNumFlushed,
                   PRInt32 aInsertionPoint)
{
  if (mStackSize < 1) {
    nsresult rv = GrowStack();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mStack[0].mType = aNodeType;
  mStack[0].mContent = aRoot;
  mStack[0].mNumFlushed = aNumFlushed;
  mStack[0].mInsertionPoint = aInsertionPoint;
  NS_ADDREF(aRoot);
  mStackPos = 1;
  mTextLength = 0;

  return NS_OK;
}

PRBool
SinkContext::IsCurrentContainer(nsHTMLTag aTag)
{
  if (aTag == mStack[mStackPos - 1].mType) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
SinkContext::IsAncestorContainer(nsHTMLTag aTag)
{
  PRInt32 stackPos = mStackPos - 1;

  while (stackPos >= 0) {
    if (aTag == mStack[stackPos].mType) {
      return PR_TRUE;
    }
    stackPos--;
  }

  return PR_FALSE;
}

void
SinkContext::DidAddContent(nsIContent* aContent)
{
  if ((mStackPos == 2) && (mSink->mBody == mStack[1].mContent ||
                           mSink->mFrameset == mStack[1].mContent)) {
    // We just finished adding something to the body
    mNotifyLevel = 0;
  }

  // If we just added content to a node for which
  // an insertion happen, we need to do an immediate
  // notification for that insertion.
  if (0 < mStackPos &&
      mStack[mStackPos - 1].mInsertionPoint != -1 &&
      mStack[mStackPos - 1].mNumFlushed <
      mStack[mStackPos - 1].mContent->GetChildCount()) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;

#ifdef NS_DEBUG
    // Tracing code
    nsIParserService *parserService = nsContentUtils::GetParserService();
    if (parserService) {
      nsHTMLTag tag = nsHTMLTag(mStack[mStackPos - 1].mType);
      NS_ConvertUTF16toUTF8 str(parserService->HTMLIdToStringTag(tag));

      SINK_TRACE(SINK_TRACE_REFLOW,
                 ("SinkContext::DidAddContent: Insertion notification for "
                  "parent=%s at position=%d and stackPos=%d",
                  str.get(), mStack[mStackPos - 1].mInsertionPoint - 1,
                  mStackPos - 1));
    }
#endif

    mSink->NotifyInsert(parent, aContent,
                        mStack[mStackPos - 1].mInsertionPoint - 1);
    mStack[mStackPos - 1].mNumFlushed = parent->GetChildCount();
  } else if (mSink->IsTimeToNotify()) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("SinkContext::DidAddContent: Notification as a result of the "
                "interval expiring; backoff count: %d", mSink->mBackoffCount));
    FlushTags(PR_TRUE);
  }
}

nsresult
SinkContext::OpenContainer(const nsIParserNode& aNode)
{
  FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::OpenContainer", 
                  nsHTMLTag(aNode.GetNodeType()), 
                  mStackPos, 
                  mSink);

  if (mStackPos <= 0) {
    NS_ERROR("container w/o parent");

    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (mStackPos + 1 > mStackSize) {
    rv = GrowStack();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Create new container content object
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  nsGenericHTMLElement* content =
    mSink->CreateContentObject(aNode, nodeType).get();
  if (!content) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mStack[mStackPos].mType = nodeType;
  mStack[mStackPos].mContent = content;
  mStack[mStackPos].mNumFlushed = 0;
  mStack[mStackPos].mInsertionPoint = -1;
  ++mStackPos;

  // XXX Need to do this before we start adding attributes.
  if (nodeType == eHTMLTag_style) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle = do_QueryInterface(content);
    NS_ASSERTION(ssle, "Style content isn't a style sheet?");
    ssle->SetLineNumber(aNode.GetSourceLineNumber());

    // Now disable updates so that every time we add an attribute or child
    // text token, we don't try to update the style sheet.
    if (!mSink->mInsideNoXXXTag) {
      ssle->InitStyleLinkElement(mSink->mParser, PR_FALSE);
    }
    else {
      // We're not going to be evaluating this style anyway.
      ssle->InitStyleLinkElement(nsnull, PR_TRUE);
    }

    ssle->SetEnableUpdates(PR_FALSE);
  }

  // Make sure to add base tag info, if needed, before setting any other
  // attributes -- what URI attrs do will depend on the base URI.  Only do this
  // for elements that have useful URI attributes.
  // See bug 18478 and bug 30617 for why we need to do this.
  switch (nodeType) {
    // Containers with "href="
    case eHTMLTag_a:
    case eHTMLTag_map:

    // Containers with "src="
    case eHTMLTag_script:
    
    // Containers with "action="
    case eHTMLTag_form:

    // Containers with "data="
    case eHTMLTag_object:

    // Containers with "background="
    case eHTMLTag_table:
    case eHTMLTag_thead:
    case eHTMLTag_tbody:
    case eHTMLTag_tfoot:
    case eHTMLTag_tr:
    case eHTMLTag_td:
    case eHTMLTag_th:
      mSink->AddBaseTagInfo(content);

      break;
    default:
      break;    
  }
  
  rv = mSink->AddAttributes(aNode, content);
  MaybeSetForm(content, nodeType, mSink);

  nsGenericHTMLElement* parent = mStack[mStackPos - 2].mContent;

  if (mStack[mStackPos - 2].mInsertionPoint != -1) {
    parent->InsertChildAt(content,
                          mStack[mStackPos - 2].mInsertionPoint++,
                          PR_FALSE);
  } else {
    parent->AppendChildTo(content, PR_FALSE);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  if (mSink->IsMonolithicContainer(nodeType)) {
    mSink->mInMonolithicContainer++;
  }

  // Special handling for certain tags
  switch (nodeType) {
    case eHTMLTag_form:
      mSink->mCurrentForm = content;
      break;

    case eHTMLTag_frameset:
      if (!mSink->mFrameset &&
          mSink->mFlags & NS_SINK_FLAG_FRAMES_ENABLED) {
        mSink->mFrameset = content;
      }
      break;

    case eHTMLTag_noembed:
    case eHTMLTag_noframes:
      mSink->mInsideNoXXXTag++;
      break;

    case eHTMLTag_iframe:
      mSink->mNumOpenIFRAMES++;
      break;

    case eHTMLTag_script:
      {
        nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(content);
        NS_ASSERTION(sele, "Script content isn't a script element?");
        sele->SetScriptLineNumber(aNode.GetSourceLineNumber());
      }
      break;

    case eHTMLTag_title:
      if (mSink->mDocument->GetDocumentTitle().IsVoid()) {
        // The first title wins.
        mSink->mInTitle = PR_TRUE;
      }
      break;

    default:
      break;
  }

  return NS_OK;
}

PRBool
SinkContext::HaveNotifiedForCurrentContent() const
{
  if (0 < mStackPos) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;
    return mStack[mStackPos-1].mNumFlushed == parent->GetChildCount();
  }

  return PR_TRUE;
}

nsresult
SinkContext::CloseContainer(const nsHTMLTag aTag, PRBool aMalformed)
{
  nsresult result = NS_OK;

  // Flush any collected text content. Release the last text
  // node to indicate that no more should be added to it.
  FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::CloseContainer", 
                  aTag, mStackPos - 1, mSink);

  NS_ASSERTION(mStackPos > 0,
               "stack out of bounds. wrong context probably!");

  if (mStackPos <= 0) {
    return NS_OK; // Fix crash - Ref. bug 45975 or 45007
  }

  --mStackPos;
  nsHTMLTag nodeType = mStack[mStackPos].mType;

  NS_ASSERTION(nodeType == aTag,
               "Tag mismatch.  Closing tag on wrong context or something?");

  nsGenericHTMLElement* content = mStack[mStackPos].mContent;

  content->Compact();

  // If we're in a state where we do append notifications as
  // we go up the tree, and we're at the level where the next
  // notification needs to be done, do the notification.
  if (mNotifyLevel >= mStackPos) {
    // Check to see if new content has been added after our last
    // notification

    if (mStack[mStackPos].mNumFlushed < content->GetChildCount()) {
#ifdef NS_DEBUG
      {
        // Tracing code
        const char *tagStr;
        mStack[mStackPos].mContent->Tag()->GetUTF8String(&tagStr);

        SINK_TRACE(SINK_TRACE_REFLOW,
                   ("SinkContext::CloseContainer: reflow on notifyImmediate "
                    "tag=%s newIndex=%d stackPos=%d",
                    tagStr,
                    mStack[mStackPos].mNumFlushed, mStackPos));
      }
#endif
      mSink->NotifyAppend(content, mStack[mStackPos].mNumFlushed);
      mStack[mStackPos].mNumFlushed = content->GetChildCount();
    }

    // Indicate that notification has now happened at this level
    mNotifyLevel = mStackPos - 1;
  }

  if (mSink->IsMonolithicContainer(nodeType)) {
    --mSink->mInMonolithicContainer;
  }

  DidAddContent(content);

  // Special handling for certain tags
  switch (nodeType) {
  case eHTMLTag_noembed:
  case eHTMLTag_noframes:
    // Fix bug 40216
    NS_ASSERTION((mSink->mInsideNoXXXTag > 0), "mInsideNoXXXTag underflow");
    if (mSink->mInsideNoXXXTag > 0) {
      mSink->mInsideNoXXXTag--;
    }

    break;
  case eHTMLTag_form:
    {
      mSink->mFlags &= ~NS_SINK_FLAG_FORM_ON_STACK;
      // If there's a FORM on the stack, but this close tag doesn't
      // close the form, then close out the form *and* close out the
      // next container up. This is since the parser doesn't do fix up
      // of invalid form nesting. When the end FORM tag comes through,
      // we'll ignore it.
      if (aTag != nodeType) {
        result = CloseContainer(aTag, PR_FALSE);
      }
    }

    break;
  case eHTMLTag_iframe:
    mSink->mNumOpenIFRAMES--;

    break;

  case eHTMLTag_select:
  case eHTMLTag_textarea:
  case eHTMLTag_object:
  case eHTMLTag_applet:
    content->DoneAddingChildren(HaveNotifiedForCurrentContent());
    break;

  case eHTMLTag_script:
    result = mSink->ProcessSCRIPTEndTag(content,
                                        HaveNotifiedForCurrentContent(),
                                        aMalformed);
    break;

  case eHTMLTag_style:
    result = mSink->ProcessSTYLEEndTag(content);
    break;

  case eHTMLTag_title:
    if (mSink->mInTitle) {
      mSink->UpdateDocumentTitle();
      mSink->mInTitle = PR_FALSE;
    }
    break;

  default:
    break;
  }

  NS_IF_RELEASE(content);

#ifdef DEBUG
  if (SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return result;
}

nsresult
SinkContext::AddLeaf(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::AddLeaf", 
                  nsHTMLTag(aNode.GetNodeType()), 
                  mStackPos, mSink);

  nsresult rv = NS_OK;

  switch (aNode.GetTokenType()) {
  case eToken_start:
    {
      FlushTextAndRelease();

      // Create new leaf content object
      nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
      nsRefPtr<nsGenericHTMLElement> content =
        mSink->CreateContentObject(aNode, nodeType);
      NS_ENSURE_TRUE(content, NS_ERROR_OUT_OF_MEMORY);

      // Make sure to add base tag info, if needed, before setting any other
      // attributes -- what URI attrs do will depend on the base URI.  Only do
      // this for elements that have useful URI attributes.
      // See bug 18478 and bug 30617 for why we need to do this.
      switch (nodeType) {
      case eHTMLTag_area:
      case eHTMLTag_meta:
      case eHTMLTag_img:
      case eHTMLTag_frame:
      case eHTMLTag_input:
      case eHTMLTag_embed:
        mSink->AddBaseTagInfo(content);
        break;

      // <form> can end up as a leaf if it's misnested with table elements
      case eHTMLTag_form:
        mSink->AddBaseTagInfo(content);
        mSink->mCurrentForm = content;

        break;
      default:
        break;
      }

      rv = mSink->AddAttributes(aNode, content);

      NS_ENSURE_SUCCESS(rv, rv);

      MaybeSetForm(content, nodeType, mSink);

      // Add new leaf to its parent
      AddLeaf(content);

      // Additional processing needed once the element is in the tree
      switch (nodeType) {
      case eHTMLTag_base:
        if (!mSink->mInsideNoXXXTag) {
          mSink->ProcessBASEElement(content);
        }
        break;

      case eHTMLTag_meta:
        // XXX It's just not sufficient to check if the parent is head. Also
        // check for the preference.
        // Bug 40072: Don't evaluate METAs after FRAMESET.
        if (!mSink->mInsideNoXXXTag && !mSink->mFrameset) {
          rv = mSink->ProcessMETATag(content);
        }
        break;

      case eHTMLTag_input:
      case eHTMLTag_button:
        content->DoneCreatingElement();

        break;

      default:
        break;
      }
    }
    break;

  case eToken_text:
  case eToken_whitespace:
  case eToken_newline:
    rv = AddText(aNode.GetText());

    break;
  case eToken_entity:
    {
      nsAutoString tmp;
      PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
      if (unicode < 0) {
        rv = AddText(aNode.GetText());
      } else {
        // Map carriage returns to newlines
        if (!tmp.IsEmpty()) {
          if (tmp.CharAt(0) == '\r') {
            tmp.Assign((PRUnichar)'\n');
          }
          rv = AddText(tmp);
        }
      }
    }

    break;
  default:
    break;
  }

  return rv;
}

nsresult
SinkContext::AddLeaf(nsGenericHTMLElement* aContent)
{
  NS_ASSERTION(mStackPos > 0, "leaf w/o container");
  if (mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }

  nsGenericHTMLElement* parent = mStack[mStackPos - 1].mContent;

  // If the parent has an insertion point, insert rather than append.
  if (mStack[mStackPos - 1].mInsertionPoint != -1) {
    parent->InsertChildAt(aContent,
                          mStack[mStackPos - 1].mInsertionPoint++,
                          PR_FALSE);
  } else {
    parent->AppendChildTo(aContent, PR_FALSE);
  }

  DidAddContent(aContent);

#ifdef DEBUG
  if (SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return NS_OK;
}

nsresult
SinkContext::AddComment(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::AddLeaf", 
                  nsHTMLTag(aNode.GetNodeType()), 
                  mStackPos, mSink);
  FlushTextAndRelease();

  if (!mSink) {
    return NS_ERROR_UNEXPECTED;
  }
  
  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment),
                                  mSink->mNodeInfoManager);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMComment> domComment(do_QueryInterface(comment));
  NS_ENSURE_TRUE(domComment, NS_ERROR_UNEXPECTED);

  domComment->AppendData(aNode.GetText());

  NS_ASSERTION(mStackPos > 0, "stack out of bounds");
  if (mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }

  nsGenericHTMLElement* parent;
  if (!mSink->mBody && !mSink->mFrameset && mSink->mHead) {
    // XXXbz but this will make DidAddContent use the wrong parent for
    // the notification!  That seems so bogus it's not even funny.
    parent = mSink->mHead;
  } else {
    parent = mStack[mStackPos - 1].mContent;
  }

  // If the parent has an insertion point, insert rather than append.
  if (mStack[mStackPos - 1].mInsertionPoint != -1) {
    parent->InsertChildAt(comment,
                          mStack[mStackPos - 1].mInsertionPoint++,
                          PR_FALSE);
  } else {
    parent->AppendChildTo(comment, PR_FALSE);
  }

  DidAddContent(comment);

#ifdef DEBUG
  if (SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return rv;
}

nsresult
SinkContext::End()
{
  for (PRInt32 i = 0; i < mStackPos; i++) {
    NS_RELEASE(mStack[i].mContent);
  }

  mStackPos = 0;
  mTextLength = 0;

  return NS_OK;
}

nsresult
SinkContext::GrowStack()
{
  PRInt32 newSize = mStackSize * 2;
  if (newSize == 0) {
    newSize = 32;
  }

  Node* stack = new Node[newSize];
  if (!stack) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mStackPos != 0) {
    memcpy(stack, mStack, sizeof(Node) * mStackPos);
    delete [] mStack;
  }

  mStack = stack;
  mStackSize = newSize;

  return NS_OK;
}

/**
 * Add textual content to the current running text buffer. If the text buffer
 * overflows, flush out the text by creating a text content object and adding
 * it to the content tree.
 */

// XXX If we get a giant string grow the buffer instead of chopping it
// up???
nsresult
SinkContext::AddText(const nsAString& aText)
{
  PRInt32 addLen = aText.Length();
  if (addLen == 0) {
    return NS_OK;
  }
  
  if (mSink->mInTitle) {
    // Hang onto the title text specially.
    mSink->mTitleString.Append(aText);
  }

  // Create buffer when we first need it
  if (mTextSize == 0) {
    mText = new PRUnichar[4096];
    if (!mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = 4096;
  }

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
  PRBool  isLastCharCR = PR_FALSE;

  while (addLen != 0) {
    PRInt32 amount = mTextSize - mTextLength;

    if (amount > addLen) {
      amount = addLen;
    }

    if (amount == 0) {
      // Don't release last text node so we can add to it again
      nsresult rv = FlushText();
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    mTextLength +=
      nsContentUtils::CopyNewlineNormalizedUnicodeTo(aText, offset,
                                                     &mText[mTextLength],
                                                     amount, isLastCharCR);
    offset += amount;
    addLen -= amount;
  }

  return NS_OK;
}

/**
 * Flush all elements that have been seen so far such that
 * they are visible in the tree. Specifically, make sure
 * that they are all added to their respective parents.
 * Also, do notification at the top for all content that
 * has been newly added so that the frame tree is complete.
 */
nsresult
SinkContext::FlushTags(PRBool aNotify)
{
  // Don't release last text node in case we need to add to it again
  FlushText();

  if (aNotify) {
    // Start from the base of the stack (growing downward) and do
    // a notification from the node that is closest to the root of
    // tree for any content that has been added.

    // Note that we can start at stackPos == 0 here, because it's the caller's
    // responsibility to handle flushing interactions between contexts (see
    // HTMLContentSink::BeginContext).
    PRInt32 stackPos = 0;
    PRBool flushed = PR_FALSE;
    PRUint32 childCount;
    nsGenericHTMLElement* content;

    while (stackPos < mStackPos) {
      content = mStack[stackPos].mContent;
      childCount = content->GetChildCount();

      if (!flushed && (mStack[stackPos].mNumFlushed < childCount)) {
#ifdef NS_DEBUG
        {
          // Tracing code
          const char* tagStr;
          mStack[stackPos].mContent->Tag()->GetUTF8String(&tagStr);

          SINK_TRACE(SINK_TRACE_REFLOW,
                     ("SinkContext::FlushTags: tag=%s from newindex=%d at "
                      "stackPos=%d", tagStr,
                      mStack[stackPos].mNumFlushed, stackPos));
        }
#endif
        if ((mStack[stackPos].mInsertionPoint != -1) &&
            (mStackPos > (stackPos + 1))) {
          nsIContent* child = mStack[stackPos + 1].mContent;
          mSink->NotifyInsert(content,
                              child,
                              mStack[stackPos].mInsertionPoint);
        } else {
          mSink->NotifyAppend(content, mStack[stackPos].mNumFlushed);
        }

        flushed = PR_TRUE;
      }

      mStack[stackPos].mNumFlushed = childCount;
      stackPos++;
    }
    mNotifyLevel = mStackPos - 1;
  }

  return NS_OK;
}

void
SinkContext::UpdateChildCounts()
{
  // Start from the top of the stack (growing upwards) and see if any
  // new content has been appended. If so, we recognize that reflows
  // have been generated for it and we should make sure that no
  // further reflows occur.  Note that we have to include stackPos == 0
  // to properly notify on kids of <html>.
  PRInt32 stackPos = mStackPos - 1;
  while (stackPos >= 0) {
    Node & node = mStack[stackPos];
    node.mNumFlushed = node.mContent->GetChildCount();

    stackPos--;
  }

  mNotifyLevel = mStackPos - 1;
}

/**
 * Flush any buffered text out by creating a text content object and
 * adding it to the content.
 */
nsresult
SinkContext::FlushText(PRBool* aDidFlush, PRBool aReleaseLast)
{
  nsresult rv = NS_OK;
  PRBool didFlush = PR_FALSE;

  if (mTextLength != 0) {
    if (mLastTextNode) {
      if ((mLastTextNodeSize + mTextLength) > mSink->mMaxTextRun) {
        mLastTextNodeSize = 0;
        mLastTextNode = nsnull;
        FlushText(aDidFlush, aReleaseLast);
      } else {
        nsCOMPtr<nsIDOMCharacterData> cdata(do_QueryInterface(mLastTextNode));

        if (cdata) {
          rv = cdata->AppendData(Substring(mText, mText + mTextLength));

          mLastTextNodeSize += mTextLength;
          mTextLength = 0;
          didFlush = PR_TRUE;
        }
      }
    } else {
      nsCOMPtr<nsITextContent> textContent;
      rv = NS_NewTextNode(getter_AddRefs(textContent),
                          mSink->mNodeInfoManager);
      NS_ENSURE_SUCCESS(rv, rv);

      mLastTextNode = textContent;

      // Set the text in the text node
      mLastTextNode->SetText(mText, mTextLength, PR_FALSE);

      // Eat up the rest of the text up in state.
      mLastTextNodeSize += mTextLength;
      mTextLength = 0;

      // Add text to its parent
      NS_ASSERTION(mStackPos > 0, "leaf w/o container");
      if (mStackPos <= 0) {
        return NS_ERROR_FAILURE;
      }

      nsGenericHTMLElement* parent = mStack[mStackPos - 1].mContent;
      if (mStack[mStackPos - 1].mInsertionPoint != -1) {
        parent->InsertChildAt(mLastTextNode,
                              mStack[mStackPos - 1].mInsertionPoint++,
                              PR_FALSE);
      } else {
        parent->AppendChildTo(mLastTextNode, PR_FALSE);
      }

      didFlush = PR_TRUE;

      DidAddContent(mLastTextNode);
    }
  }

  if (aDidFlush) {
    *aDidFlush = didFlush;
  }

  if (aReleaseLast) {
    mLastTextNodeSize = 0;
    mLastTextNode = nsnull;
  }

#ifdef DEBUG
  if (didFlush &&
      SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return rv;
}


nsresult
NS_NewHTMLContentSink(nsIHTMLContentSink** aResult,
                      nsIDocument* aDoc,
                      nsIURI* aURI,
                      nsISupports* aContainer,
                      nsIChannel* aChannel)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsRefPtr<HTMLContentSink> it = new HTMLContentSink();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aDoc, aURI, aContainer, aChannel);

  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = it;
  NS_ADDREF(*aResult);

  return NS_OK;
}

HTMLContentSink::HTMLContentSink()
{
  // Note: operator new zeros our memory


#ifdef NS_DEBUG
  if (!gSinkLogModuleInfo) {
    gSinkLogModuleInfo = PR_NewLogModule("htmlcontentsink");
  }
#endif
}

HTMLContentSink::~HTMLContentSink()
{
  NS_IF_RELEASE(mHead);
  NS_IF_RELEASE(mBody);
  NS_IF_RELEASE(mRoot);

  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
  NS_IF_RELEASE(mHTMLDocument);

  if (mNotificationTimer) {
    mNotificationTimer->Cancel();
  }

  PRInt32 numContexts = mContextStack.Count();

  if (mCurrentContext == mHeadContext && numContexts > 0) {
    // Pop off the second html context if it's not done earlier
    mContextStack.RemoveElementAt(--numContexts);
  }

  for (PRInt32 i = 0; i < numContexts; i++) {
    SinkContext* sc = (SinkContext*)mContextStack.ElementAt(i);
    if (sc) {
      sc->End();
      if (sc == mCurrentContext) {
        mCurrentContext = nsnull;
      }

      delete sc;
    }
  }

  if (mCurrentContext == mHeadContext) {
    mCurrentContext = nsnull;
  }

  delete mCurrentContext;

  delete mHeadContext;
}

#if DEBUG
NS_IMPL_ISUPPORTS_INHERITED5(HTMLContentSink,
                             nsContentSink,
                             nsIContentSink,
                             nsIHTMLContentSink,
                             nsITimerCallback,
                             nsIDocumentObserver,
                             nsIDebugDumpContent)
#else
NS_IMPL_ISUPPORTS_INHERITED4(HTMLContentSink,
                             nsContentSink,
                             nsIContentSink,
                             nsIHTMLContentSink,
                             nsITimerCallback,
                             nsIDocumentObserver)
#endif

static PRBool
IsScriptEnabled(nsIDocument *aDoc, nsIDocShell *aContainer)
{
  NS_ENSURE_TRUE(aDoc && aContainer, PR_TRUE);

  nsCOMPtr<nsIScriptGlobalObject> globalObject = aDoc->GetScriptGlobalObject();

  // Getting context is tricky if the document hasn't had it's
  // GlobalObject set yet
  if (!globalObject) {
    nsCOMPtr<nsIScriptGlobalObjectOwner> owner = do_GetInterface(aContainer);
    NS_ENSURE_TRUE(owner, PR_TRUE);

    globalObject = owner->GetScriptGlobalObject();
    NS_ENSURE_TRUE(globalObject, PR_TRUE);
  }

  nsIScriptContext *scriptContext = globalObject->GetContext();
  NS_ENSURE_TRUE(scriptContext, PR_TRUE);

  JSContext* cx = (JSContext *) scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, PR_TRUE);

  PRBool enabled = PR_TRUE;
  nsContentUtils::GetSecurityManager()->
    CanExecuteScripts(cx, aDoc->NodePrincipal(), &enabled);
  return enabled;
}

nsresult
HTMLContentSink::Init(nsIDocument* aDoc,
                      nsIURI* aURI,
                      nsISupports* aContainer,
                      nsIChannel* aChannel)
{
  NS_ENSURE_TRUE(aContainer, NS_ERROR_NULL_POINTER);


  MOZ_TIMER_DEBUGLOG(("Reset and start: nsHTMLContentSink::Init(), this=%p\n",
                      this));
  MOZ_TIMER_RESET(mWatch);
  MOZ_TIMER_START(mWatch);

  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }

  aDoc->AddObserver(this);
  CallQueryInterface(aDoc, &mHTMLDocument);

  mObservers = nsnull;
  nsIParserService* service = nsContentUtils::GetParserService();
  if (!service) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  service->GetTopicObservers(NS_LITERAL_STRING("text/html"),
                             getter_AddRefs(mObservers));

  NS_ASSERTION(mDocShell, "oops no docshell!");

  // Find out if subframes are enabled
  if (mDocShell) {
    PRBool subFramesEnabled = PR_TRUE;
    mDocShell->GetAllowSubframes(&subFramesEnabled);
    if (subFramesEnabled) {
      mFlags |= NS_SINK_FLAG_FRAMES_ENABLED;
    }
  }

  // Find out if scripts are enabled, if not, show <noscript> content
  if (IsScriptEnabled(aDoc, mDocShell)) {
    mFlags |= NS_SINK_FLAG_SCRIPT_ENABLED;
  }

  mNotifyOnTimer =
    nsContentUtils::GetBoolPref("content.notify.ontimer", PR_TRUE);

  // -1 means never
  mBackoffCount =
    nsContentUtils::GetIntPref("content.notify.backoffcount", -1);

  // The mNotificationInterval has a dramatic effect on how long it
  // takes to initially display content for slow connections.
  // The current value provides good
  // incremental display of content without causing an increase
  // in page load time. If this value is set below 1/10 of second
  // it starts to impact page load performance.
  // see bugzilla bug 72138 for more info.
  mNotificationInterval =
    nsContentUtils::GetIntPref("content.notify.interval", 120000);

  // The mMaxTokenProcessingTime controls how long we stay away from
  // the event loop when processing token. A lower value makes the app
  // more responsive, but may increase page load time.  The content
  // sink mNotificationInterval gates how frequently the content is
  // processed so it will also affect how interactive the app is
  // during page load also. The mNotification prevents contents
  // flushes from happening too frequently. while
  // mMaxTokenProcessingTime prevents flushes from happening too
  // infrequently.

  // The current ratio of 3 to 1 was determined to be the lowest
  // mMaxTokenProcessingTime which does not impact page load
  // performance.  See bugzilla bug 76722 for details.

  mMaxTokenProcessingTime =
    nsContentUtils::GetIntPref("content.max.tokenizing.time",
                               mNotificationInterval * 3);

  // 3/4 second (750000us) default for switching
  mDynamicIntervalSwitchThreshold =
    nsContentUtils::GetIntPref("content.switch.threshold", 750000);

  if (nsContentUtils::GetBoolPref("content.interrupt.parsing", PR_TRUE)) {
    mFlags |= NS_SINK_FLAG_CAN_INTERRUPT_PARSER;
  }

  // Changed from 8192 to greatly improve page loading performance on
  // large pages.  See bugzilla bug 77540.
  mMaxTextRun = nsContentUtils::GetIntPref("content.maxtextrun", 8191);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::html, nsnull,
                                     kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // Make root part
  nsIContent *doc_root = mDocument->GetRootContent();

  if (doc_root) {
    // If the document already has a root we'll use it. This will
    // happen when we do document.open()/.write()/.close()...

    NS_ADDREF(mRoot = NS_STATIC_CAST(nsGenericHTMLElement*, doc_root));
  } else {
    mRoot = NS_NewHTMLHtmlElement(nodeInfo);
    if (!mRoot) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
      MOZ_TIMER_STOP(mWatch);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mRoot);

    NS_ASSERTION(mDocument->GetChildCount() == 0,
                 "Document should have no kids here!");
    rv = mDocument->AppendChildTo(mRoot, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Make head part
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::head,
                                     nsnull, kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  mHead = NS_NewHTMLHeadElement(nodeInfo);
  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(mHead);

  mRoot->AppendChildTo(mHead, PR_FALSE);

  mCurrentContext = new SinkContext(this);
  NS_ENSURE_TRUE(mCurrentContext, NS_ERROR_OUT_OF_MEMORY);
  mCurrentContext->Begin(eHTMLTag_html, mRoot, 0, -1);
  mContextStack.AppendElement(mCurrentContext);

#ifdef NS_DEBUG
  nsCAutoString spec;
  (void)aURI->GetSpec(spec);
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::Init: this=%p url='%s'",
              this, spec.get()));
#endif

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillBuildModel(void)
{
  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
    nsresult rv = AddDummyParserRequest();
    if (NS_FAILED(rv)) {
      NS_ERROR("Adding dummy parser request failed");

      // Don't return the error result, just reset flag which
      // indicates that it can interrupt parsing. If
      // AddDummyParserRequests fails it should not affect
      // WillBuildModel.
      mFlags &= ~NS_SINK_FLAG_CAN_INTERRUPT_PARSER;
    }

    mBeginLoadTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  mScrolledToRefAlready = PR_FALSE;

  if (mHTMLDocument) {
    NS_ASSERTION(mParser, "no parser");
    nsCompatibility mode = eCompatibility_NavQuirks;
    if (mParser) {
      nsDTDMode dtdMode = mParser->GetParseMode();
      switch (dtdMode) {
        case eDTDMode_full_standards:
          mode = eCompatibility_FullStandards;

          break;
        case eDTDMode_almost_standards:
          mode = eCompatibility_AlmostStandards;

          break;
        default:
          mode = eCompatibility_NavQuirks;

          break;
      }
    }
    mHTMLDocument->SetCompatibilityMode(mode);
  }

  // Notify document that the load is beginning
  mDocument->BeginLoad();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidBuildModel(void)
{

  // NRA Dump stopwatch stop info here
#ifdef MOZ_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::DidBuildModel(), this=%p\n",
                      this));
  MOZ_TIMER_STOP(mWatch);
  MOZ_TIMER_LOG(("Content creation time (this=%p): ", this));
  MOZ_TIMER_PRINT(mWatch);
#endif

  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: canceling notification "
                "timeout"));
    mNotificationTimer->Cancel();
    mNotificationTimer = 0;
  }

  if (mDocument->GetDocumentTitle().IsVoid()) {
    nsCOMPtr<nsIDOMNSDocument> domDoc(do_QueryInterface(mDocument));
    domDoc->SetTitle(EmptyString());
  }

  // Reflow the last batch of content
  if (mBody || mFrameset) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: layout final content"));
    mCurrentContext->FlushTags(PR_TRUE);
  } else if (!mLayoutStarted) {
    // We never saw the body, and layout never got started. Force
    // layout *now*, to get an initial reflow.
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: forcing reflow on empty "
                "document"));

    // NOTE: only force the layout if we are NOT destroying the
    // docshell. If we are destroying it, then starting layout will
    // likely cause us to crash, or at best waste a lot of time as we
    // are just going to tear it down anyway.
    PRBool bDestroying = PR_TRUE;
    if (mDocShell) {
      mDocShell->IsBeingDestroyed(&bDestroying);
    }

    if (!bDestroying) {
      StartLayout();
    }
  }

  if (mDocShell) {
    PRUint32 LoadType = 0;
    mDocShell->GetLoadType(&LoadType);

    if (ScrollToRef(!(LoadType & nsIDocShell::LOAD_CMD_HISTORY))) {
      mScrolledToRefAlready = PR_TRUE;
    }
  }

  nsIScriptLoader *loader = mDocument->GetScriptLoader();
  if (loader) {
    loader->RemoveObserver(this);
  }

  mDocument->EndLoad();

  // Ref. Bug 49115
  // Do this hack to make sure that the parser
  // doesn't get destroyed, accidently, before
  // the circularity, between sink & parser, is
  // actually borken.
  nsCOMPtr<nsIParser> kungFuDeathGrip(mParser);

  // Drop our reference to the parser to get rid of a circular
  // reference.
  mParser = nsnull;

  if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
    // Reset the performance hint which was set to FALSE
    // when NS_SINK_FLAG_DYNAMIC_LOWER_VALUE was set. 
    FavorPerformanceHint(PR_TRUE , 0);
  }

  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
    // Note: Don't return value from RemoveDummyParserRequest,
    // If RemoveDummyParserRequests fails it should not affect
    // DidBuildModel. The remove can fail if the parser request
    // was already removed by a DummyParserRequest::Cancel
    RemoveDummyParserRequest();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::Notify(nsITimer *timer)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_START(mWatch);

  if (mFlags & NS_SINK_FLAG_PARSING) {
    // We shouldn't interfere with our normal DidProcessAToken logic
    mFlags |= NS_SINK_FLAG_DROPPED_TIMER;
    return NS_OK;
  }
  
#ifdef MOZ_DEBUG
  {
    PRTime now = PR_Now();
    PRInt64 diff, interval;
    PRInt32 delay;

    LL_I2L(interval, GetNotificationInterval());
    LL_SUB(diff, now, mLastNotificationTime);

    LL_SUB(diff, diff, interval);
    LL_L2I(delay, diff);
    delay /= PR_USEC_PER_MSEC;

    mBackoffCount--;
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::Notify: reflow on a timer: %d milliseconds "
                "late, backoff count: %d", delay, mBackoffCount));
  }
#endif

  if (mCurrentContext) {
    mCurrentContext->FlushTags(PR_TRUE);
  }

  // Now try and scroll to the reference
  // XXX Should we scroll unconditionally for history loads??
  TryToScrollToRef();

  mNotificationTimer = 0;
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillInterrupt()
{
  nsresult result = NS_OK;

  SINK_TRACE(SINK_TRACE_CALLS, ("HTMLContentSink::WillInterrupt: this=%p",
                                this));
#ifndef SINK_NO_INCREMENTAL
  if (mNotifyOnTimer && mLayoutStarted) {
    if (mBackoffCount && !mInMonolithicContainer) {
      nsInt64 now(PR_Now());
      nsInt64 interval(GetNotificationInterval());
      nsInt64 lastNotification(mLastNotificationTime);
      nsInt64 diff(now - lastNotification);

      // If it's already time for us to have a notification
      if (diff > interval || (mFlags & NS_SINK_FLAG_DROPPED_TIMER)) {
        mBackoffCount--;
        SINK_TRACE(SINK_TRACE_REFLOW,
                 ("HTMLContentSink::WillInterrupt: flushing tags since we've "
                  "run out time; backoff count: %d", mBackoffCount));
        result = mCurrentContext->FlushTags(PR_TRUE);
        if (mFlags & NS_SINK_FLAG_DROPPED_TIMER) {
          TryToScrollToRef();
          mFlags &= ~NS_SINK_FLAG_DROPPED_TIMER;
        }
      } else if (!mNotificationTimer) {
        interval -= diff;
        PRInt32 delay = interval;
        
        // Convert to milliseconds
        delay /= PR_USEC_PER_MSEC;

        mNotificationTimer = do_CreateInstance("@mozilla.org/timer;1",
                                               &result);
        if (NS_SUCCEEDED(result)) {
          SINK_TRACE(SINK_TRACE_REFLOW,
                     ("HTMLContentSink::WillInterrupt: setting up timer with "
                      "delay %d", delay));

          result =
            mNotificationTimer->InitWithCallback(this, delay,
                                                 nsITimer::TYPE_ONE_SHOT);
          if (NS_FAILED(result)) {
            mNotificationTimer = nsnull;
          }
        }
      }
    }
  } else {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::WillInterrupt: flushing tags "
                "unconditionally"));

    result = mCurrentContext->FlushTags(PR_TRUE);
  }
#endif

  mFlags &= ~NS_SINK_FLAG_PARSING;
  
  return result;
}

NS_IMETHODIMP
HTMLContentSink::WillResume()
{
  SINK_TRACE(SINK_TRACE_CALLS, ("HTMLContentSink::WillResume: this=%p", this));

  mFlags |= NS_SINK_FLAG_PARSING;

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::SetParser(nsIParser* aParser)
{
  mParser = aParser;
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
HTMLContentSink::IsFormOnStack()
{
  return mFlags & NS_SINK_FLAG_FORM_ON_STACK;
}

NS_IMETHODIMP
HTMLContentSink::BeginContext(PRInt32 aPosition)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::BeginContext()\n"));
  MOZ_TIMER_START(mWatch);
  NS_PRECONDITION(aPosition > -1, "out of bounds");

  // Create new context
  SinkContext* sc = new SinkContext(this);
  if (!sc) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::BeginContext()\n"));
    MOZ_TIMER_STOP(mWatch);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!mCurrentContext) {
    NS_ERROR("Non-existing context");

    return NS_ERROR_FAILURE;
  }

  // Flush everything in the current context so that we don't have
  // to worry about insertions resulting in inconsistent frame creation.
  mCurrentContext->FlushTags(PR_TRUE);

  // Sanity check.
  if (mCurrentContext->mStackPos <= aPosition) {
    NS_ERROR("Out of bounds position");
    return NS_ERROR_FAILURE;
  }

  PRInt32 insertionPoint = -1;
  nsHTMLTag nodeType      = mCurrentContext->mStack[aPosition].mType;
  nsGenericHTMLElement* content = mCurrentContext->mStack[aPosition].mContent;

  // If the content under which the new context is created
  // has a child on the stack, the insertion point is
  // before the last child.
  if (aPosition < (mCurrentContext->mStackPos - 1)) {
    insertionPoint = content->GetChildCount() - 1;
  }

  sc->Begin(nodeType,
            content,
            mCurrentContext->mStack[aPosition].mNumFlushed,
            insertionPoint);
  NS_ADDREF(sc->mSink);

  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = sc;

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::BeginContext()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::EndContext(PRInt32 aPosition)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::EndContext()\n"));
  MOZ_TIMER_START(mWatch);
  NS_PRECONDITION(mCurrentContext && aPosition > -1, "non-existing context");

  PRInt32 n = mContextStack.Count() - 1;
  SinkContext* sc = (SinkContext*) mContextStack.ElementAt(n);

  NS_ASSERTION(sc->mStack[aPosition].mType == mCurrentContext->mStack[0].mType,
               "ending a wrong context");

  mCurrentContext->FlushTextAndRelease();

  sc->mStack[aPosition].mNumFlushed = mCurrentContext->mStack[0].mNumFlushed;

  for (PRInt32 i = 0; i<mCurrentContext->mStackPos; i++) {
    NS_IF_RELEASE(mCurrentContext->mStack[i].mContent);
  }

  delete [] mCurrentContext->mStack;

  mCurrentContext->mStack      = nsnull;
  mCurrentContext->mStackPos   = 0;
  mCurrentContext->mStackSize  = 0;

  delete [] mCurrentContext->mText;

  mCurrentContext->mText       = nsnull;
  mCurrentContext->mTextLength = 0;
  mCurrentContext->mTextSize   = 0;

  NS_IF_RELEASE(mCurrentContext->mSink);

  delete mCurrentContext;

  mCurrentContext = sc;
  mContextStack.RemoveElementAt(n);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::EndContext()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

nsresult
HTMLContentSink::CloseHTML()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseHTML()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                 "HTMLContentSink::CloseHTML", 
                 eHTMLTag_html, 0, this);

  if (mHeadContext) {
    if (mCurrentContext == mHeadContext) {
      PRInt32 numContexts = mContextStack.Count();

      // Pop off the second html context if it's not done earlier
      mCurrentContext = (SinkContext*)mContextStack.ElementAt(--numContexts);
      mContextStack.RemoveElementAt(numContexts);
    }

    NS_ASSERTION(mHeadContext->mTextLength == 0, "Losing text");

    mHeadContext->End();

    delete mHeadContext;
    mHeadContext = nsnull;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseHTML()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

nsresult
HTMLContentSink::OpenHead()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenHead()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = OpenHeadContext();

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

nsresult
HTMLContentSink::OpenBody(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenBody()\n"));
  MOZ_TIMER_START(mWatch);

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenBody", 
                  eHTMLTag_body,
                  mCurrentContext->mStackPos, 
                  this);

  CloseHeadContext();  // do this just in case if the HEAD was left open!

  // Add attributes, if any, to the current BODY node
  if (mBody) {
    AddAttributes(aNode, mBody, PR_TRUE, PR_TRUE);

    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return NS_OK;
  }

  nsresult rv = mCurrentContext->OpenContainer(aNode);

  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return rv;
  }

  mBody = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;

  NS_ADDREF(mBody);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
  MOZ_TIMER_STOP(mWatch);

  if (mCurrentContext->mStackPos > 1) {
    PRInt32 parentIndex    = mCurrentContext->mStackPos - 2;
    nsGenericHTMLElement *parent = mCurrentContext->mStack[parentIndex].mContent;
    PRInt32 numFlushed     = mCurrentContext->mStack[parentIndex].mNumFlushed;
    PRInt32 childCount = parent->GetChildCount();
    NS_ASSERTION(numFlushed < childCount, "Already notified on the body?");
    
    PRInt32 insertionPoint =
      mCurrentContext->mStack[parentIndex].mInsertionPoint;

    // XXX: I have yet to see a case where numFlushed is non-zero and
    // insertionPoint is not -1, but this code will try to handle
    // those cases too.

    if (insertionPoint != -1) {
      NotifyInsert(parent, mBody, insertionPoint - 1);
    } else {
      NotifyAppend(parent, numFlushed);
    }
    mCurrentContext->mStack[parentIndex].mNumFlushed = childCount;
  }

  StartLayout();

  return NS_OK;
}

nsresult
HTMLContentSink::CloseBody()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseBody()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseBody", 
                  eHTMLTag_body,
                  mCurrentContext->mStackPos - 1, 
                  this);

  PRBool didFlush;
  nsresult rv = mCurrentContext->FlushTextAndRelease(&didFlush);
  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return rv;
  }

  // Flush out anything that's left
  SINK_TRACE(SINK_TRACE_REFLOW,
             ("HTMLContentSink::CloseBody: layout final body content"));

  mCurrentContext->FlushTags(PR_TRUE);
  mCurrentContext->CloseContainer(eHTMLTag_body, PR_FALSE);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseBody()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

nsresult
HTMLContentSink::OpenForm(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenForm()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult result = NS_OK;

  mCurrentContext->FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenForm", 
                  eHTMLTag_form,
                  mCurrentContext->mStackPos, 
                  this);

  // Close out previous form if it's there. If there is one
  // around, it's probably because the last one wasn't well-formed.
  mCurrentForm = nsnull;

  // Check if the parent is a table, tbody, thead, tfoot, tr, col or
  // colgroup. If so, we fix up by making the form leaf content.
  if (mCurrentContext->IsCurrentContainer(eHTMLTag_table) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tbody) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_thead) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tfoot) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tr) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_col) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_colgroup)) {
    result = mCurrentContext->AddLeaf(aNode);
  } else {
    mFlags |= NS_SINK_FLAG_FORM_ON_STACK;
    // Otherwise the form can be a content parent.
    result = mCurrentContext->OpenContainer(aNode);
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenForm()\n"));
  MOZ_TIMER_STOP(mWatch);

  return result;
}

nsresult
HTMLContentSink::CloseForm()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseForm()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult result = NS_OK;

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseForm",
                  eHTMLTag_form,
                  mCurrentContext->mStackPos - 1, 
                  this);

  if (mCurrentForm) {
    // if this is a well-formed form, close it too
    if (mCurrentContext->IsCurrentContainer(eHTMLTag_form)) {
      result = mCurrentContext->CloseContainer(eHTMLTag_form, PR_FALSE);
      mFlags &= ~NS_SINK_FLAG_FORM_ON_STACK;
    }

    mCurrentForm = nsnull;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseForm()\n"));
  MOZ_TIMER_STOP(mWatch);

  return result;
}

nsresult
HTMLContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenFrameset()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenFrameset", 
                  eHTMLTag_frameset,
                  mCurrentContext->mStackPos, 
                  this);

  CloseHeadContext(); // do this just in case if the HEAD was left open!

  // Need to keep track of whether OpenContainer changes mFrameset
  nsGenericHTMLElement* oldFrameset = mFrameset;
  nsresult rv = mCurrentContext->OpenContainer(aNode);
  PRBool isFirstFrameset = NS_SUCCEEDED(rv) && mFrameset != oldFrameset;

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);

  if (isFirstFrameset && mCurrentContext->mStackPos > 1) {
    NS_ASSERTION(mFrameset, "Must have frameset!");
    // Have to notify for the frameset now, since we never actually
    // close out <html>, so won't notify for it then.
    PRInt32 parentIndex    = mCurrentContext->mStackPos - 2;
    nsGenericHTMLElement *parent = mCurrentContext->mStack[parentIndex].mContent;
    PRInt32 numFlushed     = mCurrentContext->mStack[parentIndex].mNumFlushed;
    PRInt32 childCount = parent->GetChildCount();
    NS_ASSERTION(numFlushed < childCount, "Already notified on the frameset?");

    PRInt32 insertionPoint =
      mCurrentContext->mStack[parentIndex].mInsertionPoint;

    // XXX: I have yet to see a case where numFlushed is non-zero and
    // insertionPoint is not -1, but this code will try to handle
    // those cases too.

    if (insertionPoint != -1) {
      NotifyInsert(parent, mFrameset, insertionPoint - 1);
    } else {
      NotifyAppend(parent, numFlushed);
    }
    mCurrentContext->mStack[parentIndex].mNumFlushed = childCount;
  }
  
  return rv;
}

nsresult
HTMLContentSink::CloseFrameset()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseFrameset()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                   "HTMLContentSink::CloseFrameset", 
                   eHTMLTag_frameset,
                   mCurrentContext->mStackPos - 1,
                   this);

  SinkContext* sc = mCurrentContext;
  nsGenericHTMLElement* fs = sc->mStack[sc->mStackPos - 1].mContent;
  PRBool done = fs == mFrameset;

  nsresult rv;
  if (done) {
    PRBool didFlush;
    rv = sc->FlushTextAndRelease(&didFlush);
    if (NS_FAILED(rv)) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseFrameset()\n"));
      MOZ_TIMER_STOP(mWatch);

      return rv;
    }

    // Flush out anything that's left
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::CloseFrameset: layout final content"));

    sc->FlushTags(PR_TRUE);
  }

  rv = sc->CloseContainer(eHTMLTag_frameset, PR_FALSE);    

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);

  if (done && (mFlags & NS_SINK_FLAG_FRAMES_ENABLED)) {
    StartLayout();
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::IsEnabled(PRInt32 aTag, PRBool* aReturn)
{
  nsHTMLTag theHTMLTag = nsHTMLTag(aTag);

  if (theHTMLTag == eHTMLTag_script) {
    *aReturn = mFlags & NS_SINK_FLAG_SCRIPT_ENABLED ? PR_TRUE : PR_FALSE;
  } else if (theHTMLTag == eHTMLTag_frameset) {
    *aReturn = mFlags & NS_SINK_FLAG_FRAMES_ENABLED ? PR_TRUE : PR_FALSE;
  } else {
    *aReturn = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenContainer()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = NS_OK;

  switch (aNode.GetNodeType()) {
    case eHTMLTag_frameset:
      rv = OpenFrameset(aNode);
      break;
    case eHTMLTag_head:
      rv = OpenHeadContext();
      if (NS_SUCCEEDED(rv)) {
        rv = AddAttributes(aNode, mHead, PR_FALSE, mHaveSeenHead);
        mHaveSeenHead = PR_TRUE;
      }
      break;
    case eHTMLTag_body:
      rv = OpenBody(aNode);
      break;
    case eHTMLTag_html:
      if (mRoot) {
        // If we've already hit this code once, need to check for
        // already-present attributes on the root.
        AddAttributes(aNode, mRoot, PR_TRUE, mNotifiedRootInsertion);
        if (!mNotifiedRootInsertion) {
          NS_ASSERTION(!mLayoutStarted,
                       "How did we start layout without notifying on root?");
          // Now make sure to notify that we have now inserted our root.  If
          // there has been no initial reflow yet it'll be a no-op, but if
          // there has been one we need this to get its frames constructed.
          // Note that if mNotifiedRootInsertion is true we don't notify here,
          // since that just means there are multiple <html> tags in the
          // document; in those cases we just want to put all the attrs on one
          // tag.
          mNotifiedRootInsertion = PR_TRUE;
          PRInt32 index = mDocument->IndexOf(mRoot);
          NS_ASSERTION(index != -1, "mRoot not child of document?");
          NotifyInsert(nsnull, mRoot, index);

          // Now update the notification information in all our
          // contexts, since we just inserted the root and notified on
          // our whole tree
          UpdateAllContexts();          
        }
      }
      break;
    case eHTMLTag_form:
      rv = OpenForm(aNode);
      break;
    default:
      rv = mCurrentContext->OpenContainer(aNode);
      break;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenContainer()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const eHTMLTags aTag)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseContainer()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = NS_OK;

  switch (aTag) {
    case eHTMLTag_frameset:
      rv = CloseFrameset();
      break;
    case eHTMLTag_head:
      CloseHeadContext();
      break;
    case eHTMLTag_body:
      rv = CloseBody();
      break;
    case eHTMLTag_html:
      rv = CloseHTML();
      break;
    case eHTMLTag_form:
      rv = CloseForm();
      break;
    default:
      rv = mCurrentContext->CloseContainer(aTag, PR_FALSE);
      break;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseContainer()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseMalformedContainer(const eHTMLTags aTag)
{
  return mCurrentContext->CloseContainer(aTag, PR_TRUE);
}

NS_IMETHODIMP
HTMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddLeaf()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv;

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  switch (nodeType) {
  case eHTMLTag_link:
    mCurrentContext->FlushTextAndRelease();
    rv = ProcessLINKTag(aNode);

    break;
  default:
    rv = mCurrentContext->AddLeaf(aNode);

    break;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddLeaf()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

nsresult 
HTMLContentSink::UpdateDocumentTitle()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::UpdateDocumentTitle()\n"));
  MOZ_TIMER_START(mWatch);
  NS_ASSERTION(mCurrentContext == mHeadContext, "title not in head");

  if (!mDocument->GetDocumentTitle().IsVoid()) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::UpdateDocumentTitle()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_OK;
  }

  // Use mTitleString.
  mTitleString.CompressWhitespace(PR_TRUE, PR_TRUE);

  nsCOMPtr<nsIDOMNSDocument> domDoc(do_QueryInterface(mDocument));
  domDoc->SetTitle(mTitleString);

  mTitleString.Truncate();

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::UpdateDocumentTitle()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

/**
 * This gets called by the parsing system when we find a comment
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
nsresult
HTMLContentSink::AddComment(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddComment()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = mCurrentContext->AddComment(aNode);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddComment()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

/**
 * This gets called by the parsing system when we find a PI
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
nsresult
HTMLContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  MOZ_TIMER_START(mWatch);
  // Implementation of AddProcessingInstruction() should start here

  MOZ_TIMER_STOP(mWatch);

  return result;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
HTMLContentSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddDocTypeDecl()\n"));
  MOZ_TIMER_START(mWatch);

  nsAutoString docTypeStr(aNode.GetText());
  nsresult rv = NS_OK;

  PRInt32 publicStart = docTypeStr.Find("PUBLIC", PR_TRUE);
  PRInt32 systemStart = docTypeStr.Find("SYSTEM", PR_TRUE);
  nsAutoString name, publicId, systemId;

  if (publicStart >= 0 || systemStart >= 0) {
    /*
     * If we find the keyword 'PUBLIC' after the keyword 'SYSTEM' we assume
     * that we got a system id that happens to contain the string "PUBLIC"
     * and we ignore that as the start of the public id.
     */
    if (systemStart >= 0 && (publicStart > systemStart)) {
      publicStart = -1;
    }

    /*
     * We found 'PUBLIC' or 'SYSTEM' in the doctype, put everything before
     * the first one of those in name.
     */
    docTypeStr.Mid(name, 0, publicStart >= 0 ? publicStart : systemStart);

    if (publicStart >= 0) {
      // We did find 'PUBLIC'
      docTypeStr.Mid(publicId, publicStart + 6,
                     docTypeStr.Length() - publicStart);
      publicId.Trim(" \t\n\r");

      // Strip quotes
      PRUnichar ch = publicId.IsEmpty() ? '\0' : publicId.First();

      PRBool hasQuote = PR_FALSE;
      if (ch == '"' || ch == '\'') {
        publicId.Cut(0, 1);

        PRInt32 end = publicId.FindChar(ch);

        if (end < 0) {
          /*
           * We didn't find an end quote, so just make sure we cut off
           * the '>' on the end of the doctype declaration
           */

          end = publicId.FindChar('>');
        } else {
          hasQuote = PR_TRUE;
        }

        /*
         * If we didn't find a closing quote or a '>' we leave publicId as
         * it is.
         */
        if (end >= 0) {
          publicId.Truncate(end);
        }
      } else {
        // No quotes, ignore the public id
        publicId.Truncate();
      }

      /*
       * Make sure the 'SYSTEM' word we found is not inside the pubilc id
       */
      PRInt32 pos = docTypeStr.Find(publicId);

      if (systemStart > 0) {
        if (systemStart < pos + (PRInt32)publicId.Length()) {
          systemStart = docTypeStr.Find("SYSTEM", PR_TRUE,
                                        pos + publicId.Length());
        }
      }

      /*
       * If we didn't find 'SYSTEM' we treat everything after the public id
       * as the system id.
       */
      if (systemStart < 0) {
        // 1 is the end quote
        systemStart = pos + publicId.Length() + (hasQuote ? 1 : 0);
      }
    }

    if (systemStart >= 0) {
      // We either found 'SYSTEM' or we have data after the public id
      docTypeStr.Mid(systemId, systemStart,
                     docTypeStr.Length() - systemStart);

      // Strip off 'SYSTEM' if we have it.
      if (StringBeginsWith(systemId, NS_LITERAL_STRING("SYSTEM")))
        systemId.Cut(0, 6);

      systemId.Trim(" \t\n\r");

      // Strip quotes
      PRUnichar ch = systemId.IsEmpty() ? '\0' : systemId.First();

      if (ch == '"' || ch == '\'') {
        systemId.Cut(0, 1);

        PRInt32 end = systemId.FindChar(ch);

        if (end < 0) {
          // We didn't find an end quote, then we just make sure we
          // cut of the '>' on the end of the doctype declaration

          end = systemId.FindChar('>');
        }

        // If we found an closing quote nor a '>' we truncate systemId
        // at that length.
        if (end >= 0) {
          systemId.Truncate(end);
        }
      } else {
        systemId.Truncate();
      }
    }
  } else {
    name.Assign(docTypeStr);
  }

  // Cut out "<!DOCTYPE" or "DOCTYPE" from the name.
  if (StringBeginsWith(name, NS_LITERAL_STRING("<!DOCTYPE"), nsCaseInsensitiveStringComparator())) {
    name.Cut(0, 9);
  } else if (StringBeginsWith(name, NS_LITERAL_STRING("DOCTYPE"), nsCaseInsensitiveStringComparator())) {
    name.Cut(0, 7);
  }

  name.Trim(" \t\n\r");

  // Check if name contains whitespace chars. If it does and the first
  // char is not a quote, we set the name to contain the characters
  // before the whitespace
  PRInt32 nameEnd = 0;

  if (name.IsEmpty() || (name.First() != '"' && name.First() != '\'')) {
    nameEnd = name.FindCharInSet(" \n\r\t");
  }

  // If we didn't find a public id we grab everything after the name
  // and use that as public id.
  if (publicStart < 0) {
    name.Mid(publicId, nameEnd, name.Length() - nameEnd);
    publicId.Trim(" \t\n\r");

    PRUnichar ch = publicId.IsEmpty() ? '\0' : publicId.First();

    if (ch == '"' || ch == '\'') {
      publicId.Cut(0, 1);

      PRInt32 publicEnd = publicId.FindChar(ch);

      if (publicEnd < 0) {
        publicEnd = publicId.FindChar('>');
      }

      if (publicEnd < 0) {
        publicEnd = publicId.Length();
      }

      publicId.Truncate(publicEnd);
    } else {
      // No quotes, no public id
      publicId.Truncate();
    }
  }

  if (nameEnd >= 0) {
    name.Truncate(nameEnd);
  } else {
    nameEnd = name.FindChar('>');

    if (nameEnd >= 0) {
      name.Truncate(nameEnd);
    }
  }

  if (!publicId.IsEmpty() || !systemId.IsEmpty() || !name.IsEmpty()) {
    nsCOMPtr<nsIDOMDocumentType> oldDocType;
    nsCOMPtr<nsIDOMDocumentType> docType;

    nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mHTMLDocument));
    doc->GetDoctype(getter_AddRefs(oldDocType));

    nsCOMPtr<nsIDOMDOMImplementation> domImpl;

    rv = doc->GetImplementation(getter_AddRefs(domImpl));

    if (NS_FAILED(rv) || !domImpl) {
      return rv;
    }

    if (name.IsEmpty()) {
      name.AssignLiteral("HTML");
    }

    rv = domImpl->CreateDocumentType(name, publicId, systemId,
                                     getter_AddRefs(docType));

    if (NS_FAILED(rv) || !docType) {
      return rv;
    }

    if (oldDocType) {
      // If we already have a doctype we replace the old one.
      nsCOMPtr<nsIDOMNode> tmpNode;
      rv = doc->ReplaceChild(oldDocType, docType, getter_AddRefs(tmpNode));
    } else {
      // If we don't already have one we insert it as the first child,
      // this might not be 100% correct but since this is called from
      // the content sink we assume that this is what we want.
      nsCOMPtr<nsIContent> content = do_QueryInterface(docType);
      NS_ASSERTION(content, "Doctype isn't content?");
      
      mDocument->InsertChildAt(content, 0, PR_TRUE);
    }
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddDocTypeDecl()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}


NS_IMETHODIMP
HTMLContentSink::WillProcessTokens(void)
{
  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
    mDelayTimerStart = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidProcessTokens(void)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillProcessAToken(void)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidProcessAToken(void)
{
  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
    // There is both a high frequency interrupt mode and a low
    // frequency interupt mode controlled by the flag
    // NS_SINK_FLAG_DYNAMIC_LOWER_VALUE The high frequency mode
    // interupts the parser frequently to provide UI responsiveness at
    // the expense of page load time. The low frequency mode
    // interrupts the parser and samples the system clock infrequently
    // to provide fast page load time. When the user moves the mouse,
    // clicks or types the mode switches to the high frequency
    // interrupt mode. If the user stops moving the mouse or typing
    // for a duration of time (mDynamicIntervalSwitchThreshold) it
    // switches to low frequency interrupt mode.

    // Get the current user event time
    nsIPresShell *shell = mDocument->GetShellAt(0);

    if (!shell) {
      // If there's no pres shell in the document, return early since
      // we're not laying anything out here.

      return NS_OK;
    }

    nsIViewManager* vm = shell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
    PRUint32 eventTime;
    nsCOMPtr<nsIWidget> widget;
    nsresult rv = vm->GetWidget(getter_AddRefs(widget));
    if (!widget || NS_FAILED(widget->GetLastInputEventTime(eventTime))) {
        // If we can't get the last input time from the widget
        // then we will get it from the viewmanager.
        rv = vm->GetLastUserEventTime(eventTime);
        NS_ENSURE_SUCCESS(rv , NS_ERROR_FAILURE);
    }

    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if ((!(mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE)) &&
      (mLastSampledUserEventTime == eventTime)) {
      // The magic value of NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE
      // was selected by empirical testing. It provides reasonable
      // user response and prevents us from sampling the clock too
      // frequently.
      if (mDeflectedCount < NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE) {
        mDeflectedCount++;

        // return early to prevent sampling the clock. Note: This
        // prevents us from switching to higher frequency (better UI
        // responsive) mode, so limit ourselves to doing for no more
        // than NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE tokens.

        return NS_OK;
      }

      // reset count and drop through to the code which samples the
      // clock and does the dynamic switch between the high
      // frequency and low frequency interruption of the parser.

      mDeflectedCount = 0;
    }

    mLastSampledUserEventTime = eventTime;

    PRUint32 currentTime = PR_IntervalToMicroseconds(PR_IntervalNow());

    // Get the last user event time and compare it with the current
    // time to determine if the lower value for content notification
    // and max token processing should be used. But only consider
    // using the lower value if the document has already been loading
    // for 2 seconds. 2 seconds was chosen because it is greater than
    // the default 3/4 of second that is used to determine when to
    // switch between the modes and it gives the document a little
    // time to create windows.  This is important because on some
    // systems (Windows, for example) when a window is created and the
    // mouse is over it, a mouse move event is sent, which will kick
    // us into interactive mode otherwise. It also suppresses reaction
    // to pressing the ENTER key in the URL bar...

    PRUint32 delayBeforeLoweringThreshold =
      NS_STATIC_CAST(PRUint32, ((2 * mDynamicIntervalSwitchThreshold) +
                                NS_DELAY_FOR_WINDOW_CREATION));

    if ((currentTime - mBeginLoadTime) > delayBeforeLoweringThreshold) {
      if ((currentTime - eventTime) <
          NS_STATIC_CAST(PRUint32, mDynamicIntervalSwitchThreshold)) {
    
        if (! (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE)) {
          // lower the dynamic values to favor application
          // responsiveness over page load time.
          mFlags |= NS_SINK_FLAG_DYNAMIC_LOWER_VALUE;
          // Set the performance hint to prevent event starvation when
          // dispatching PLEvents. This improves application responsiveness 
          // during page loads.
          FavorPerformanceHint(PR_FALSE, 0);
        }

      } else {
      
        if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
          // raise the content notification and MaxTokenProcessing time
          // to favor overall page load speed over responsiveness.
          mFlags &= ~NS_SINK_FLAG_DYNAMIC_LOWER_VALUE;
          // Reset the hint that to favoring performance for PLEvent dispatch.
          FavorPerformanceHint(PR_TRUE, 0);
        }

      }
    }

    if ((currentTime - mDelayTimerStart) >
        NS_STATIC_CAST(PRUint32, GetMaxTokenProcessingTime())) {
      return NS_ERROR_HTMLPARSER_INTERRUPTED;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::NotifyTagObservers(nsIParserNode* aNode)
{
  // Bug 125317
  // Inform observers that we're handling a document.write().
  // This information is necessary for the charset observer, atleast,
  // to make a decision whether a new charset loading is required or not.

  if (!mObservers) {
    return NS_OK;
  }

  PRUint32 flag = 0;

  if (mHTMLDocument && mHTMLDocument->IsWriting()) {
    flag = nsIElementObserver::IS_DOCUMENT_WRITE;
  }

  return mObservers->Notify(aNode, mParser, mDocShell, flag);
}

void
HTMLContentSink::StartLayout()
{
  if (mLayoutStarted) {
    return;
  }

  mLayoutStarted = PR_TRUE;

  mLastNotificationTime = PR_Now();

  mHTMLDocument->SetIsFrameset(mFrameset != nsnull);

  nsContentSink::StartLayout(mFrameset != nsnull);
}

void
HTMLContentSink::TryToScrollToRef()
{
  if (mRef.IsEmpty()) {
    return;
  }

  if (mScrolledToRefAlready) {
    return;
  }

  if (ScrollToRef(PR_TRUE)) {
    mScrolledToRefAlready = PR_TRUE;
  }
}

void
HTMLContentSink::AddBaseTagInfo(nsIContent* aContent)
{
  nsresult rv;
  if (mBaseHref) {
    rv = aContent->SetProperty(nsHTMLAtoms::htmlBaseHref, mBaseHref,
                               nsPropertyTable::SupportsDtorFunc);
    if (NS_SUCCEEDED(rv)) {
      // circumvent nsDerivedSafe
      NS_ADDREF(NS_STATIC_CAST(nsIURI*, mBaseHref));
    }
  }
  if (mBaseTarget) {
    rv = aContent->SetProperty(nsHTMLAtoms::htmlBaseTarget, mBaseTarget,
                               nsPropertyTable::SupportsDtorFunc);
    if (NS_SUCCEEDED(rv)) {
      // circumvent nsDerivedSafe
      NS_ADDREF(NS_STATIC_CAST(nsIAtom*, mBaseTarget));
    }
  }
}

nsresult
HTMLContentSink::OpenHeadContext()
{
  if (mCurrentContext && mCurrentContext->IsCurrentContainer(eHTMLTag_head))
    return NS_OK;

  // Flush everything in the current context so that we don't have
  // to worry about insertions resulting in inconsistent frame creation.
  //
  // Try to do this only if needed (costly), i.e., only if we are sure
  // we are changing contexts from some other context to the head.
  //
  // PERF: This call causes approximately a 2% slowdown in page load time
  // according to jrgm's page load tests, but seems to be a necessary evil
  if (mCurrentContext && (mCurrentContext != mHeadContext)) {
    mCurrentContext->FlushTags(PR_TRUE);
  }

  if (!mHeadContext) {
    mHeadContext = new SinkContext(this);
    NS_ENSURE_TRUE(mHeadContext, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = mHeadContext->Begin(eHTMLTag_head, mHead, 0, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = mHeadContext;

  return NS_OK;
}

void
HTMLContentSink::CloseHeadContext()
{
  if (mCurrentContext) {
    if (!mCurrentContext->IsCurrentContainer(eHTMLTag_head))
      return;

    mCurrentContext->FlushTextAndRelease();
  }

  NS_ASSERTION(mContextStack.Count() > 0, "Stack should not be empty");
  
  PRInt32 n = mContextStack.Count() - 1;
  mCurrentContext = (SinkContext*) mContextStack.ElementAt(n);
  mContextStack.RemoveElementAt(n);
}

void
HTMLContentSink::ProcessBASEElement(nsGenericHTMLElement* aElement)
{
  // href attribute
  nsAutoString attrValue;
  if (aElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, attrValue)) {
    //-- Make sure this page is allowed to load this URI
    nsresult rv;
    nsCOMPtr<nsIURI> baseHrefURI;
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(baseHrefURI),
                                                   attrValue, mDocument,
                                                   nsnull);
    if (NS_FAILED(rv))
      return;

    // Setting "BASE URI" from the last BASE tag appearing in HEAD.
    if (!mBody) {
      // The document checks if it is legal to set this base. Failing here is
      // ok, we just won't set a new base.
      rv = mDocument->SetBaseURI(baseHrefURI);
      if (NS_SUCCEEDED(rv)) {
        mDocumentBaseURI = mDocument->GetBaseURI();
      }
    } else {
      // NAV compatibility quirk

      nsIScriptSecurityManager *securityManager =
        nsContentUtils::GetSecurityManager();

      rv = securityManager->
        CheckLoadURIWithPrincipal(mDocument->NodePrincipal(), baseHrefURI,
                                  nsIScriptSecurityManager::STANDARD);
      if (NS_SUCCEEDED(rv)) {
        mBaseHref = baseHrefURI;
      }
    }
  }

  // target attribute
  if (aElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, attrValue)) {
    if (!mBody) {
      // still in real HEAD
      mDocument->SetBaseTarget(attrValue);
    } else {
      // NAV compatibility quirk
      mBaseTarget = do_GetAtom(attrValue);
    }
  }
}

nsresult
HTMLContentSink::ProcessLINKTag(const nsIParserNode& aNode)
{
  nsresult  result = NS_OK;
  nsGenericHTMLElement* parent = nsnull;

  if (mCurrentContext) {
    parent = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  }

  if (parent) {
    // Create content object
    nsCOMPtr<nsIContent> element;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::link, nsnull, kNameSpaceID_None,
                                  getter_AddRefs(nodeInfo));

    result = NS_NewHTMLElement(getter_AddRefs(element), nodeInfo);
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(element));

    if (ssle) {
      // XXX need prefs. check here.
      if (!mInsideNoXXXTag) {
        ssle->InitStyleLinkElement(mParser, PR_FALSE);
        ssle->SetEnableUpdates(PR_FALSE);
      } else {
        ssle->InitStyleLinkElement(nsnull, PR_TRUE);
      }
    }

    // Add in the attributes and add the style content object to the
    // head container.
    AddBaseTagInfo(element);
    result = AddAttributes(aNode, element);
    if (NS_FAILED(result)) {
      return result;
    }
    parent->AppendChildTo(element, PR_FALSE);

    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      result = ssle->UpdateStyleSheet(nsnull, nsnull);

      // look for <link rel="next" href="url">
      nsAutoString relVal;
      element->GetAttr(kNameSpaceID_None, nsHTMLAtoms::rel, relVal);
      if (!relVal.IsEmpty()) {
        // XXX seems overkill to generate this string array
        nsStringArray linkTypes;
        nsStyleLinkElement::ParseLinkTypes(relVal, linkTypes);
        PRBool hasPrefetch = (linkTypes.IndexOf(NS_LITERAL_STRING("prefetch")) != -1);
        if (hasPrefetch || linkTypes.IndexOf(NS_LITERAL_STRING("next")) != -1) {
          nsAutoString hrefVal;
          element->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, hrefVal);
          if (!hrefVal.IsEmpty()) {
            PrefetchHref(hrefVal, hasPrefetch);
          }
        }
      }
    }
  }

  return result;
}

#ifdef DEBUG
void
HTMLContentSink::ForceReflow()
{
  mCurrentContext->FlushTags(PR_TRUE);
}
#endif

void
HTMLContentSink::NotifyAppend(nsIContent* aContainer, PRUint32 aStartIndex)
{
  if (aContainer->GetCurrentDoc() != mDocument) {
    // aContainer is not actually in our document anymore.... Just bail out of
    // here; notifying on our document for this append would be wrong.
    return;
  }

  mInNotification++;

  MOZ_TIMER_DEBUGLOG(("Save and stop: nsHTMLContentSink::NotifyAppend()\n"));
  MOZ_TIMER_SAVE(mWatch)
  MOZ_TIMER_STOP(mWatch);

  mDocument->ContentAppended(aContainer, aStartIndex);
  mLastNotificationTime = PR_Now();

  MOZ_TIMER_DEBUGLOG(("Restore: nsHTMLContentSink::NotifyAppend()\n"));
  MOZ_TIMER_RESTORE(mWatch);

  mInNotification--;
}

void
HTMLContentSink::NotifyInsert(nsIContent* aContent,
                              nsIContent* aChildContent,
                              PRInt32 aIndexInContainer)
{
  if (aContent && aContent->GetCurrentDoc() != mDocument) {
    // aContent is not actually in our document anymore.... Just bail out of
    // here; notifying on our document for this insert would be wrong.
    return;
  }

  mInNotification++;

  MOZ_TIMER_DEBUGLOG(("Save and stop: nsHTMLContentSink::NotifyInsert()\n"));
  MOZ_TIMER_SAVE(mWatch)
  MOZ_TIMER_STOP(mWatch);

  mDocument->ContentInserted(aContent, aChildContent, aIndexInContainer);
  mLastNotificationTime = PR_Now();

  MOZ_TIMER_DEBUGLOG(("Restore: nsHTMLContentSink::NotifyInsert()\n"));
  MOZ_TIMER_RESTORE(mWatch);

  mInNotification--;
}

PRBool
HTMLContentSink::IsMonolithicContainer(nsHTMLTag aTag)
{
  if (aTag == eHTMLTag_tr     ||
      aTag == eHTMLTag_select ||
      aTag == eHTMLTag_applet ||
      aTag == eHTMLTag_object) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
HTMLContentSink::IsTimeToNotify()
{
  if (!mNotifyOnTimer || !mLayoutStarted || !mBackoffCount ||
      mInMonolithicContainer) {
    return PR_FALSE;
  }

  PRTime now = PR_Now();
  PRInt64 interval, diff;

  LL_I2L(interval, GetNotificationInterval());
  LL_SUB(diff, now, mLastNotificationTime);

  if (LL_CMP(diff, >, interval)) {
    mBackoffCount--;
    return PR_TRUE;
  }

  return PR_FALSE;
}

void
HTMLContentSink::UpdateAllContexts()
{
  PRInt32 numContexts = mContextStack.Count();
  for (PRInt32 i = 0; i < numContexts; i++) {
    SinkContext* sc = (SinkContext*)mContextStack.ElementAt(i);

    sc->UpdateChildCounts();
  }

  mCurrentContext->UpdateChildCounts();
}

void
HTMLContentSink::BeginUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType)
{
  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Since this could result in frame
  // creation, make sure we've flushed everything before we
  // continue.

  // Note that UPDATE_CONTENT_STATE notifications never cause
  // synchronous frame construction, so we never have to worry about
  // them here.  The code that handles the async event these
  // notifications post will flush us out if it needs to.

  // Also, if this is not an UPDATE_CONTENT_STATE notification,
  // increment mInNotification to make sure we don't flush again until
  // the end of this update, even if nested updates or
  // FlushPendingNotifications calls happen during it.
  NS_ASSERTION(aUpdateType && (aUpdateType & UPDATE_ALL) == aUpdateType,
               "Weird update type bitmask");
  if (aUpdateType != UPDATE_CONTENT_STATE && !mInNotification++ &&
      mCurrentContext) {
    mCurrentContext->FlushTags(PR_TRUE);
  }
}

void
HTMLContentSink::EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType)
{
  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Update our notion of how much
  // has been flushed to include any new content if ending
  // this update leaves us not inside a notification.  Note that we
  // exclude UPDATE_CONTENT_STATE notifications here, since those
  // never affect the frame model directly while inside the
  // notification.
  NS_ASSERTION(aUpdateType && (aUpdateType & UPDATE_ALL) == aUpdateType,
               "Weird update type bitmask");
  if (aUpdateType != UPDATE_CONTENT_STATE && !--mInNotification) {
    UpdateAllContexts();
  }
}

void
HTMLContentSink::PreEvaluateScript()
{
  // Eagerly append all pending elements (including the current body child)
  // to the body (so that they can be seen by scripts) and force reflow.
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::PreEvaluateScript: flushing tags before "
              "evaluating script"));

  mCurrentContext->FlushTags(PR_FALSE);
}

void
HTMLContentSink::PostEvaluateScript(nsIScriptElement *aElement)
{
  mHTMLDocument->ScriptExecuted(aElement);
}

nsresult
HTMLContentSink::ProcessSCRIPTEndTag(nsGenericHTMLElement *content,
                                     PRBool aHaveNotified,
                                     PRBool aMalformed)
{
  nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(content);
  NS_ASSERTION(sele, "Not really closing a script tag?");

  nsRefPtr<nsGenericHTMLElement> parent =
    mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;

  if (aMalformed) {
    // Make sure to serialize this script correctly, for nice round tripping.
    sele->SetIsMalformed();
  }

  nsCOMPtr<nsIScriptLoader> loader;
  if (mFrameset) {
    // Fix bug 82498
    // We don't want to evaluate scripts in a frameset document.
    if (mDocument) {
      loader = mDocument->GetScriptLoader();
      if (loader) {
        loader->SetEnabled(PR_FALSE);
      }
    }
  } else if (parent->GetCurrentDoc() == mDocument) {
    // We test the current doc of |parent| because if it doesn't have one we
    // won't actually try to evaluate the script, so we shouldn't be blocking
    // or appending to mScriptElements or anything.
    
    // Don't include script loading and evaluation in the stopwatch
    // that is measuring content creation time
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::ProcessSCRIPTEndTag()\n"));
    MOZ_TIMER_STOP(mWatch);

    // Assume that we're going to block the parser with a script load.
    // If it's an inline script, we'll be told otherwise in the call
    // to our ScriptAvailable method.
    mNeedToBlockParser = PR_TRUE;

    mScriptElements.AppendObject(sele);
  }

  // Notify our document that we're loading this script.
  mHTMLDocument->ScriptLoading(sele);

  // Now tell the script that it's ready to go. This will execute the script
  // and call our ScriptAvailable method.
  content->DoneAddingChildren(aHaveNotified);
  
  // To prevent script evaluation in a frameset document we suspended the
  // script loader. Now that the script content has been handled, let's resume
  // the script loader.
  if (loader) {
    loader->SetEnabled(PR_TRUE);
  }

  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  // Note: If the script is malformed, we'll get a ScriptAvailable call to
  // take care of this test.
  if (mNeedToBlockParser || (mParser && !mParser->IsParserEnabled())) {
    return NS_ERROR_HTMLPARSER_BLOCK;
  }

  return NS_OK;
}

// 3 ways to load a style sheet: inline, style src=, link tag
// XXX What does nav do if we have SRC= and some style data inline?

nsresult
HTMLContentSink::ProcessSTYLEEndTag(nsGenericHTMLElement* content)
{
  nsCOMPtr<nsIStyleSheetLinkingElement> ssle = do_QueryInterface(content);

  NS_ASSERTION(ssle,
               "html:style doesn't implement nsIStyleSheetLinkingElement");

  nsresult rv = NS_OK;

  if (ssle) {
    // Note: if we are inside a noXXX tag, then we init'ed this style element
    // with mDontLoadStyle = PR_TRUE, so these two calls will have no effect.
    ssle->SetEnableUpdates(PR_TRUE);
    rv = ssle->UpdateStyleSheet(nsnull, nsnull);
  }

  return rv;
}

void
HTMLContentSink::FlushPendingNotifications(mozFlushType aType)
{
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant)
  if (mCurrentContext && !mInNotification) {
    PRBool notify = ((aType & Flush_SinkNotifications) != 0);
    mCurrentContext->FlushTags(notify);
    if (aType & Flush_OnlyReflow) {
      // Make sure that layout has started so that the reflow flush
      // will actually happen.
      StartLayout();
    }
  }
}

NS_IMETHODIMP
HTMLContentSink::SetDocumentCharset(nsACString& aCharset)
{
  if (mDocShell) {
    // the following logic to get muCV is copied from
    // nsHTMLDocument::StartDocumentLoad
    // We need to call muCV->SetPrevDocCharacterSet here in case
    // the charset is detected by parser DetectMetaTag
    nsCOMPtr<nsIMarkupDocumentViewer> muCV;
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
       muCV = do_QueryInterface(cv);
    } else {
      // in this block of code, if we get an error result, we return
      // it but if we get a null pointer, that's perfectly legal for
      // parent and parentContentViewer

      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
        do_QueryInterface(mDocShell);
      NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
      docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));

      nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));
      if (parent) {
        nsCOMPtr<nsIContentViewer> parentContentViewer;
        nsresult rv =
          parent->GetContentViewer(getter_AddRefs(parentContentViewer));
        if (NS_SUCCEEDED(rv) && parentContentViewer) {
          muCV = do_QueryInterface(parentContentViewer);
        }
      }
    }

    if (muCV) {
      muCV->SetPrevDocCharacterSet(aCharset);
    }
  }

  if (mDocument) {
    mDocument->SetDocumentCharacterSet(aCharset);
  }

  return NS_OK;
}

nsISupports *
HTMLContentSink::GetTarget()
{
  return mDocument;
}

#ifdef DEBUG
/**
 *  This will dump content model into the output file.
 *
 *  @update  harishd 05/25/00
 *  @param
 *  @return  NS_OK all went well, error on failure
 */

NS_IMETHODIMP
HTMLContentSink::DumpContentModel()
{
  FILE* out = ::fopen("rtest_html.txt", "a");
  if (out) {
    if (mDocument) {
      nsIContent* root = mDocument->GetRootContent();
      if (root) {
        if (mDocumentURI) {
          nsCAutoString buf;
          mDocumentURI->GetSpec(buf);
          fputs(buf.get(), out);
        }

        fputs(";", out);
        root->DumpContent(out, 0, PR_FALSE);
        fputs(";\n", out);
      }
    }

    fclose(out);
  }

  return NS_OK;
}
#endif

// If the content sink can interrupt the parser (@see mCanInteruptParsing)
// then it needs to schedule a dummy parser request to delay the document
// from firing onload handlers and other document done actions until all of the
// parsing has completed.

nsresult
HTMLContentSink::AddDummyParserRequest(void)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(!mDummyParserRequest, "Already have a dummy parser request");

  rv = DummyParserRequest::Create(getter_AddRefs(mDummyParserRequest), this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    loadGroup = mDocument->GetDocumentLoadGroup();
  }

  if (loadGroup) {
    rv = mDummyParserRequest->SetLoadGroup(loadGroup);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = loadGroup->AddRequest(mDummyParserRequest, nsnull);
  }

  return rv;
}

nsresult
HTMLContentSink::RemoveDummyParserRequest(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    loadGroup = mDocument->GetDocumentLoadGroup();
  }

  if (loadGroup && mDummyParserRequest) {
    rv = loadGroup->RemoveRequest(mDummyParserRequest, nsnull, NS_OK);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mDummyParserRequest = nsnull;
  }

  return rv;
}

