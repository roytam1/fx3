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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIXTFXMLVisualWrapper.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h"
#include "nsISchema.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsSubstring.h"

#include "nsIDOM3EventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXPathResult.h"

#include "nsXFormsControlStub.h"
#include "nsIXFormsContextControl.h"
#include "nsIXFormsRepeatElement.h"
#include "nsIXFormsRepeatUIElement.h"
#include "nsIXFormsRepeatItemElement.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsModelElement.h"
#include "nsXFormsUtils.h"
#include "nsXFormsDelegateStub.h"

#ifdef DEBUG
//#define DEBUG_XF_REPEAT
#endif

/**
 * Implementation of the XForms \<repeat\> control.
 * @see http://www.w3.org/TR/xforms/slice9.html#id2632123
 *
 * There are two main functions of repeat: 1) "Expanding its children" and 2)
 * Maintaining the repeat-index. These are described here:
 *
 * <h2>Expanding its children</h2>
 *
 * On Refresh(), nsXFormsRepeatElement, does the following for each node in
 * the nodeset the \<repeat\> tag is bound to:
 *
 * 1) Creates a new \<contextcontainer\> (nsXFormsContextContainer)
 *
 * 2) Clones all its children (that is children of its mElement)
 *    and appends them as children to the nsXFormsContextContainer
 * 
 * 3) Sets the context node and size for the nsXFormsContextContainer, so
 *    that children can retrieve this through nsIXFormsContextControl.
 *
 * 4) Inserts the nsXFormsContextContainer into its visual content node.
 *
 * For example, this instance data:
 * <pre>
 * <instance>
 *   <data>
 *     <n>val1</n>
 *     <n>val2</n>
 *   </data>
 * </instance>
 * </pre>
 *
 * and this repeat:
 * <pre>
 * <repeat nodeset="n">
 *   Val: <output ref="."/>
 * </repeat>
 * </pre>
 *
 * will be expanded to:
 * <pre>
 * <repeat nodeset="n">
 *   (anonymous content)          (XBL)
 *     <contextcontainer>         (contextNode == "n[0]" and contextPosition == 1)
 *       Val: <output ref="."/>   (that is: 'val1')
 *     </contextcontainer>
 *     <contextcontainer>         (contextNode == "n[1]" and contextPosition == 2)
 *       Val: <output ref="."/>   (that is: 'val2')
 *     </contextcontainer>
 *   (/anonymous content)         (XBL)
 * </repeat>
 * </pre>
 *
 * Besides being a practical way to implement \<repeat\>, it also means that it
 * is possible to CSS-style the individual "rows" in a \<repeat\>.
 *
 * <h2>Maintaining the repeat-index</h2>
 *
 * The repeat-index points to the current child contextcontainer selected,
 * which should be fairly easy, was it not for "nested repeats".
 *
 * If the DOM document has the following:
 * <pre>
 * <repeat id="r_outer">
 *   <repeat id="r_inner"/>
 * </repeat>
 * </pre>
 * r_inner is cloned like all other elements:
 * <pre>
 * <repeat id="r_outer">
 *   (anonymous content)
 *     <contextcontainer>
 *       <repeat id="r_inner"/>
 *     </contextcontainer>
 *     <contextcontainer>
 *       <repeat id="r_inner"/>
 *     </contextcontainer>
 *   (/anonymous content)

 *   (DOM content -- not shown)
 *     <repeat id="r_inner"/>
 *   (/DOM content)
 * </repeat>
 * </pre>
 *
 * So r_inner in fact exists three places; once in the DOM and twice in the
 * anonymous content of r_outer. The problem is that repeat-index can only be
 * set for one row for each repeat. The approach we use here is to check
 * whether we clone any \<repeat\> elements in Refresh() using our own
 * CloneNode() function. If a \<repeat\> is found, we mark the original (DOM)
 * \<repeat\> inactive with regards to content (mIsParent), and basically just
 * use it to store a pointer to the current cloned \<repeat\>
 * (mCurrentRepeat). We also store a pointer to the (DOM) parent in the cloned
 * repeat (mParent).
 * 
 * There are two ways the repeat-index can be changed, 1) by \<setindex\>
 * (nsXFormsSetIndexElement) and 2) by a \<contextcontainer\> getting
 * focus. We thus listen for focus-events in
 * nsXFormsContextContainer::HandleDefault().
 *
 * If a \<repeat\> changes the repeat-index, any nested repeats have their
 * repeat-index reset to their starting index (ResetInnerRepeats()).
 *
 * <h2>Notes / todo</h2>
 *
 * @todo Support attribute based repeats, as in: (XXX)
 *       \<html:table xforms:repeat-nodeset="..."\>
 *       @see http://www.w3.org/TR/xforms/index-all.html#ui.repeat.via.attrs
 *       @see http://bugzilla.mozilla.org/show_bug.cgi?id=280368
 *
 * @todo What happens if you set attributes on the parent repeat?
 *       Should they propagate to the cloned repeats? (XXX)
 *
 * @note Should we handle @number? The spec. says that it's a "Optional hint
 *       to the XForms Processor as to how many elements from the collection to
 *       display."
 *       @see https://bugzilla.mozilla.org/show_bug.cgi?id=302026
 */
class nsXFormsRepeatElement : public nsXFormsDelegateStub,
                              public nsIXFormsRepeatElement
{
protected:
  /** True while children are being added */
  PRBool mAddingChildren;

  /**
   * The current repeat-index, 0 if no row is selected (can happen for nested
   * repeats) or there are no rows at all.
   *
   * @note That is the child \<contextcontainer\> at position mCurrentIndex -
   * 1 (indexes go from 1, DOM from 0).
   */
  PRUint32 mCurrentIndex;

  /**
   * The maximum index value
   */
  PRUint32 mMaxIndex;

  /** The parent repeat (nested repeats) */
  nsCOMPtr<nsIXFormsRepeatElement> mParent;

  /**
   * The nested level of the repeat. That is, the number of \<repeat\>
   * elements above us in the anonymous content tree. Used by
   * ResetInnerRepeats().
   */
  PRUint32 mLevel;
  
  /**
   * Are we a parent for nested repeats
   */
  PRBool mIsParent;

  /**
   * The currently selected repeat (nested repeats)
   */
  nsCOMPtr<nsIXFormsRepeatElement> mCurrentRepeat;

  /**
   * Array of controls using the repeat-index
   */
  nsCOMArray<nsIXFormsControl>     mIndexUsers;
  
   /**
   * Retrieves an integer attribute and checks its type.
   * 
   * @param aName            The attribute to retrieve
   * @param aVal             The value
   * @param aType            The attribute (Schema) type
   * @return                 Normal error codes, and NS_ERROR_NOT_AVAILABLE if
   *                         the attribute was empty/nonexistant
   */
  nsresult GetIntAttr(const nsAString &aName,
                      PRInt32         *aVal,
                      const PRUint16   aType);

  /**
   * Set the repeat-index state for a given (nsIXFormsRepeatItemElement)
   * child.
   *
   * @param aPosition         The position of the child (1-based)
   * @param aState            The index state
   * @param aIsRefresh        Is this part of a refresh event
   */
  nsresult SetChildIndex(PRUint32 aPosition,
                         PRBool   aState,
                         PRBool   aIsRefresh = PR_FALSE);

  /**
   * Resets inner repeat indexes to 1 for first level of nested repeats of
   * |aNode|.
   *
   * @param aNode             The node to search for repeats
   * @param aIsRefresh        Is this part of a refresh event
   */
  nsresult ResetInnerRepeats(nsIDOMNode *aNode,
                             PRBool      aIsRefresh);

  /**
   * Deep clones aSrc to aTarget, with special handling of \<repeat\> elements
   * to take care of nested repeats.
   *
   * @param aSrc              The source node
   * @param aTarget           The target node
   */
  nsresult CloneNode(nsIDOMNode *aSrc, nsIDOMNode **aTarget);

  PRBool IsBindingAttribute(const nsIAtom *aAttr) const;

  /**
   * Make sure that an index value is inside the valid index range.
   *
   * @param aIndex            The index value to sanitize
   * @param aIsScroll         Send scroll events if first or last index?
   */
  void SanitizeIndex(PRUint32 *aIndex, PRBool aIsScroll = PR_FALSE);

  /**
   * Returns either the anonymous content of the repeat or null;
   */
  already_AddRefed<nsIDOMElement> GetAnonymousContent();

public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFBindableElement overrides
  NS_IMETHOD OnCreated(nsIXTFBindableElementWrapper *aWrapper);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  // nsIXFormsControl
  NS_IMETHOD Bind();
  NS_IMETHOD Refresh();
  NS_IMETHOD TryFocus(PRBool* aOK);
  NS_IMETHOD IsEventTarget(PRBool *aOK);

  // nsIXFormsRepeatElement
  NS_DECL_NSIXFORMSREPEATELEMENT

  // nsXFormsRepeatElement
  nsXFormsRepeatElement() :
    mAddingChildren(PR_FALSE),
    mCurrentIndex(0),
    mMaxIndex(0),
    mLevel(1),
    mIsParent(PR_FALSE)
    {}

#ifdef DEBUG_smaug
  virtual const char* Name() { return "repeat"; }
#endif
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsRepeatElement,
                             nsXFormsDelegateStub,
                             nsIXFormsRepeatElement)

// nsIXTFXMLVisual
NS_IMETHODIMP
nsXFormsRepeatElement::OnCreated(nsIXTFBindableElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsDelegateStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  aWrapper->SetNotificationMask(kStandardNotificationMask |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  return NS_OK;
}

// nsIXTFElement
NS_IMETHODIMP
nsXFormsRepeatElement::OnDestroyed()
{
  mIndexUsers.Clear();

  return nsXFormsDelegateStub::OnDestroyed();
}

NS_IMETHODIMP
nsXFormsRepeatElement::BeginAddingChildren()
{
  mAddingChildren = PR_TRUE;
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::DoneAddingChildren()
{
  mAddingChildren = PR_FALSE;

  return NS_OK;
}

// nsIXFormsRepeatElement

NS_IMETHODIMP
nsXFormsRepeatElement::SetIndex(PRUint32 *aIndex,
                                PRBool    aIsRefresh)
{
  NS_ENSURE_ARG(aIndex);
#ifdef DEBUG_XF_REPEAT
  printf("\tSetindex to %d (current: %d, max: %d), aIsRefresh=%d\n",
         *aIndex, mCurrentIndex, mMaxIndex, aIsRefresh);
#endif

  nsresult rv;

  // Set repeat-index
  if (mIsParent) {
    NS_ASSERTION(mCurrentRepeat,
                 "How can we be a repeat parent without a child?");
    // We're the parent of nested repeats, set through the correct repeat
    return mCurrentRepeat->SetIndex(aIndex, aIsRefresh);
  }
  
  // Do nothing if we are not showing anything
  if (mMaxIndex == 0) {
    // 9.3.6 states that the index position becomes 0 if there are
    // no elements in the repeat.
    mCurrentIndex = 0;

    // XXXbeaufour: handle scroll-first/last
    return NS_OK;
  }

  if (aIsRefresh && !mCurrentIndex) {
    // If we are refreshing, get existing index value from parent
    NS_ASSERTION(mParent,
                 "SetIndex with aIsRefresh == PR_TRUE for a non-nested repeat?!");
    rv = mParent->GetIndex(aIndex);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Check min. and max. value
  SanitizeIndex(aIndex, PR_TRUE);

  // Do nothing if setting to existing value
  if (!aIsRefresh && mCurrentIndex && *aIndex == mCurrentIndex)
    return NS_OK;
  
  
#ifdef DEBUG_XF_REPEAT
  printf("\tWill set index to %d\n",
         *aIndex);
#endif

  // Set the repeat-index
  rv = SetChildIndex(*aIndex, PR_TRUE, aIsRefresh);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Unset previous repeat-index
  if (mCurrentIndex) {
    // We had the previous selection, unset directly
    SetChildIndex(mCurrentIndex, PR_FALSE, aIsRefresh);
  }
  
  if (mParent) {
    // Selection is in another repeat, inform parent (it will inform the
    // previous owner of its new state)
    rv = mParent->SetCurrentRepeat(this, *aIndex);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set current index to new value
  mCurrentIndex = *aIndex;

  // Inform of index change
  mParent ? mParent->IndexHasChanged() : IndexHasChanged();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::GetIndex(PRUint32 *aIndex)
{
  NS_ENSURE_ARG(aIndex);
  if (mParent) {
    return mParent->GetIndex(aIndex);
  }

  *aIndex = mCurrentIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::Deselect(void)
{
  if (!mCurrentIndex)
    return NS_OK;
  
  nsresult rv = SetChildIndex(mCurrentIndex, PR_FALSE);
  if (NS_SUCCEEDED(rv)) {
    mCurrentIndex = 0;
  }
  return rv;
}

NS_IMETHODIMP
nsXFormsRepeatElement::GetStartingIndex(PRUint32 *aRes)
{
  NS_ENSURE_ARG(aRes);

  nsresult rv = GetIntAttr(NS_LITERAL_STRING("startindex"),
                           (PRInt32*) aRes,
                           nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER);
  if (NS_FAILED(rv)) {
    *aRes = 1;
  }
  SanitizeIndex(aRes);

  return NS_OK;
}


NS_IMETHODIMP
nsXFormsRepeatElement::SetCurrentRepeat(nsIXFormsRepeatElement *aRepeat,
                                        PRUint32                aIndex)
{
  // Deselect the previous owner
  if (mCurrentRepeat && aRepeat != mCurrentRepeat) {
    nsresult rv = mCurrentRepeat->Deselect();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mCurrentRepeat = aRepeat;

  // Check aIndex. If it is 0, we should intialize to the starting index
  if (!aIndex) {
    GetStartingIndex(&aIndex);
  }
  mCurrentIndex = aIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::GetCurrentRepeatRow(nsIDOMNode **aRow)
{
  if (mCurrentRepeat) {
    // nested repeats
    return mCurrentRepeat->GetCurrentRepeatRow(aRow);
  }

  nsCOMPtr<nsIDOMElement> anon = GetAnonymousContent();
  NS_ENSURE_STATE(anon);

  nsCOMPtr<nsIDOMNodeList> children;
  anon->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_STATE(children);

  nsCOMPtr<nsIDOMNode> child;
  children->Item(mCurrentIndex - 1, // Indexes are 1-based, the DOM is 0-based
                 getter_AddRefs(child));
  NS_IF_ADDREF(*aRow = child);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::AddIndexUser(nsIXFormsControl *aControl)
{
  nsresult rv = NS_OK;
  if (mIndexUsers.IndexOf(aControl) == -1 && !mIndexUsers.AppendObject(aControl))
    rv = NS_ERROR_FAILURE;
  
  return rv;
}

NS_IMETHODIMP
nsXFormsRepeatElement::RemoveIndexUser(nsIXFormsControl *aControl)
{
  return mIndexUsers.RemoveObject(aControl) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXFormsRepeatElement::IndexHasChanged()
{
  ///
  /// @bug We need to handle \<bind\> elements too (XXX)

  // copy the index array, as index users might add/remove themselves when
  // they are rebound and refreshed().
  nsCOMArray<nsIXFormsControl> indexes(mIndexUsers);

  for (PRInt32 i = 0; i < indexes.Count(); ++i) {
    nsCOMPtr<nsIXFormsControl> control = indexes[i];
    control->Bind();
    control->Refresh();
  }

  return NS_OK;
}

// NB: CloneNode() assumes that this always succeeds
NS_IMETHODIMP
nsXFormsRepeatElement::GetIsParent(PRBool *aIsParent)
{
  NS_ENSURE_ARG(aIsParent);
  *aIsParent = mIsParent;
  return NS_OK;
}

// NB: CloneNode() assumes that this always succeeds
NS_IMETHODIMP
nsXFormsRepeatElement::SetIsParent(PRBool aIsParent)
{
  mIsParent = aIsParent;
  return NS_OK;
}

// NB: CloneNode() assumes that this always succeeds
NS_IMETHODIMP
nsXFormsRepeatElement::SetParent(nsIXFormsRepeatElement *aParent)
{
  mParent = aParent;
  // We're an inner repeat owned by a parent, let it control whether we are
  // selected or not.
  Deselect();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::GetParent(nsIXFormsRepeatElement **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::SetLevel(PRUint32 aLevel)
{
  mLevel = aLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::GetLevel(PRUint32 *aLevel)
{
  NS_ENSURE_ARG(aLevel);
  *aLevel = mLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::HandleNodeInsert(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMElement> anon = GetAnonymousContent();
  if (!anon) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(aNode));
  NS_ENSURE_STATE(node);

  // XXX, badness^2: If it is a insert we have to refresh before we can
  // figure out whether the node is in our nodeset... refactor this so
  // repeat actually gets the nodeset in Bind() and then uses it refresh,
  // then we can "just" re-evaluate the nodeset, and only refresh if the
  // node actually hits this repeat

  // XXX, moreover it is also wrong to refresh at this point. It will happen
  // in insert processing (and possibly deferred...)
  nsresult rv = Refresh();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> child;
  anon->GetFirstChild(getter_AddRefs(child));

  PRUint32 index = 1;
  while (child) {
    nsCOMPtr<nsIXFormsContextControl> context(do_QueryInterface(child));
    NS_ASSERTION(context,
                 "repeat child not implementing nsIXFormsContextControl?!");

    nsAutoString modelID;
    PRInt32 position, size;
    nsCOMPtr<nsIDOMNode> boundNode;
    rv = context->GetContext(modelID, getter_AddRefs(boundNode), &position,
                             &size);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool sameNode = PR_FALSE;
    node->IsSameNode(boundNode, &sameNode);
    if (sameNode) {
      rv = SetIndex(&index, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      break;
    }

    nsCOMPtr<nsIDOMNode> tmp;
    child->GetNextSibling(getter_AddRefs(tmp));
    child.swap(tmp);
    ++index;
  }

  return NS_OK;
}

// nsXFormsControl

NS_IMETHODIMP
nsXFormsRepeatElement::Bind()
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!nsXFormsUtils::IsDocumentReadyForBind(domDoc)) {
    nsXFormsModelElement::DeferElementBind(domDoc, this);
    return NS_OK_XFORMS_DEFERRED;
  }
  return BindToModel(PR_TRUE);
}

NS_IMETHODIMP
nsXFormsRepeatElement::Refresh()
{
  if (!mElement || mAddingChildren || mIsParent) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMElement> anon = GetAnonymousContent();
  if (!anon) {
    return NS_OK;
  }

  nsPostRefresh postRefresh = nsPostRefresh();

  nsresult rv;

  // Clear any existing children
  nsCOMPtr<nsIDOMNode> cNode;
  anon->GetFirstChild(getter_AddRefs(cNode));
  while (cNode) {
    nsCOMPtr<nsIDOMNode> retNode;
    anon->RemoveChild(cNode, getter_AddRefs(retNode));
    anon->GetFirstChild(getter_AddRefs(cNode));
  }

  // Get the nodeset we are bound to
  nsCOMPtr<nsIDOMXPathResult> result;
  nsCOMPtr<nsIModelElementPrivate> model;
  rv = ProcessNodeBinding(NS_LITERAL_STRING("nodeset"),
                          nsIDOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                          getter_AddRefs(result),
                          getter_AddRefs(model));

  if (NS_FAILED(rv) | !result | !model)
    return rv;

  /// @todo The spec says: "This node-set must consist of contiguous child
  /// element nodes, with the same local name and namespace name of a common
  /// parent node. The behavior of element repeat with respect to
  /// non-homogeneous node-sets is undefined."
  /// @see http://www.w3.org/TR/xforms/slice9.html#ui-repeat
  ///
  /// Can/should we check this somehow? (XXX)

  PRUint32 contextSize;
  rv = result->GetSnapshotLength(&contextSize);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!contextSize)
    return NS_OK;

  // Get model ID
  nsCOMPtr<nsIDOMElement> modelElement = do_QueryInterface(model);
  NS_ENSURE_TRUE(modelElement, NS_ERROR_FAILURE);
  nsAutoString modelID;
  modelElement->GetAttribute(NS_LITERAL_STRING("id"), modelID);

  // Get DOM document
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  mMaxIndex = contextSize;
  for (PRUint32 i = 1; i < mMaxIndex + 1; ++i) {
    // Create <contextcontainer>
    nsCOMPtr<nsIDOMElement> riElement;
    rv = domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS),
                                 NS_LITERAL_STRING("contextcontainer"),
                                 getter_AddRefs(riElement));
    NS_ENSURE_SUCCESS(rv, rv);

    riElement->SetAttribute(NS_LITERAL_STRING("class"),
                            NS_LITERAL_STRING("xf-repeat-item"));

    // Set model as attribute
    if (!modelID.IsEmpty()) {
      riElement->SetAttribute(NS_LITERAL_STRING("model"), modelID);
    }

    // Get context node
    nsCOMPtr<nsIXFormsContextControl> riContext = do_QueryInterface(riElement);
    NS_ENSURE_TRUE(riContext, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNode> contextNode;
    rv = result->SnapshotItem(i - 1, getter_AddRefs(contextNode));
    NS_ENSURE_SUCCESS(rv, rv);

    // Set context node, position, and size
    rv = riContext->SetContext(contextNode, i, contextSize);
    NS_ENSURE_SUCCESS(rv, rv);

    // We need to insert the context node before adding the children, or the
    // children will fail to set up their proper XForms context.
    nsCOMPtr<nsIDOMNode> domNode;
    rv = anon->AppendChild(riElement, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    // Iterate over template children, clone them, and append them to
    // \<contextcontainer\>
    nsCOMPtr<nsIDOMNode> child;
    rv = mElement->GetFirstChild(getter_AddRefs(child));
    NS_ENSURE_SUCCESS(rv, rv);
    while (child) {
      /// XXX the node probably refreshes itself twice here, once on cloning
      /// and once when it's inserted ... that's not necessary.
      nsCOMPtr<nsIDOMNode> childClone;
      rv = CloneNode(child, getter_AddRefs(childClone));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIDOMNode> newNode;
      rv = riElement->AppendChild(childClone, getter_AddRefs(newNode));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = child->GetNextSibling(getter_AddRefs(newNode));
      NS_ENSURE_SUCCESS(rv, rv);
      child = newNode;
    }
  }

  if (mCurrentIndex) {
    // somebody might have been fooling around with our children since last
    // refresh (either using delete or through script, so check the index
    // value
    SanitizeIndex(&mCurrentIndex);
  } else if (mMaxIndex) {
    // repeat-index has not been initialized, set it.
    if (!mParent) {
      GetStartingIndex(&mCurrentIndex);
    } else if (mLevel > 1) {
      // Set repeat-index for inner repeats. If parent <contextcontainer/>
      // element is selected then mCurrentIndex is setted on starting index.

      nsCOMPtr<nsIDOMNode> temp = mElement;
      nsCOMPtr<nsIDOMNode> parent;
      nsCOMPtr<nsIXFormsRepeatItemElement> context;

      while (!context) {
        rv = temp->GetParentNode(getter_AddRefs(parent));
        NS_ENSURE_SUCCESS(rv, rv);
        if (!parent)
          break;
        context = do_QueryInterface(parent);
        temp.swap(parent);
      }

      if (context) {
        PRBool hasIndex = PR_FALSE;
        context->GetIndexState(&hasIndex);
        if (hasIndex) {
          PRUint32 index = 0;
          GetStartingIndex(&index);
          SetIndex(&index, PR_FALSE);
        }
      }
      return NS_OK;
    }
  }

  // If we have the repeat-index, set it.
  if (mCurrentIndex) {
    SetChildIndex(mCurrentIndex, PR_TRUE, PR_TRUE);
  }

  return NS_OK;
}

// nsXFormsRepeatElement

nsresult
nsXFormsRepeatElement::SetChildIndex(PRUint32 aPosition,
                                     PRBool   aState,
                                     PRBool   aIsRefresh)
{
#ifdef DEBUG_XF_REPEAT
  printf("\tTrying to set index #%d to state '%d', aIsRefresh=%d\n",
         aPosition, aState, aIsRefresh);
#endif

  nsCOMPtr<nsIDOMElement> anon = GetAnonymousContent();
  if (!anon)
    return NS_OK;

  nsCOMPtr<nsIDOMNodeList> children;
  anon->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_STATE(children);

  PRUint32 index = aPosition - 1; // Indexes are 1-based, the DOM is 0-based;

  nsCOMPtr<nsIDOMNode> child;
  children->Item(index,
                 getter_AddRefs(child));
  nsCOMPtr<nsIXFormsRepeatItemElement> repeatItem(do_QueryInterface(child));
  NS_ASSERTION(repeatItem,
               "repeat child not implementing nsIXFormsRepeatItemElement?!");

  nsresult rv;
  PRBool curState;
  rv = repeatItem->GetIndexState(&curState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (curState != aState) {
    rv = repeatItem->SetIndexState(aState);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aState) {
      // Reset inner repeats
      rv = ResetInnerRepeats(child, aIsRefresh);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void
nsXFormsRepeatElement::SanitizeIndex(PRUint32 *aIndex, PRBool aIsScroll)
{
  if (!aIndex)
    return;

  if (*aIndex < 1) {
    *aIndex = 1;
    if (aIsScroll)
      nsXFormsUtils::DispatchEvent(mElement, eEvent_ScrollFirst);
  } else if (*aIndex > mMaxIndex) {
    *aIndex = mMaxIndex;
    if (aIsScroll)
      nsXFormsUtils::DispatchEvent(mElement, eEvent_ScrollLast);
  }
}

nsresult
nsXFormsRepeatElement::ResetInnerRepeats(nsIDOMNode *aNode,
                                         PRBool      aIsRefresh)
{
#ifdef DEBUG_XF_REPEAT
  printf("\taIsRefresh: %d\n",
         aIsRefresh);
#endif

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);

  if (!element)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsresult rv;

  nsCOMPtr<nsIDOMElement> anon = GetAnonymousContent();
  NS_ENSURE_STATE(anon);

  rv = element->GetElementsByTagNameNS(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS),
                                       NS_LITERAL_STRING("repeat"),
                                       getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  nodeList->GetLength(&childCount);
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIXFormsRepeatElement> repeat;
  for (PRUint32 i = 0; i < childCount; ++i) {
    nodeList->Item(i, getter_AddRefs(node));
    repeat = do_QueryInterface(node);
    NS_ENSURE_STATE(repeat);
    PRUint32 level;
    repeat->GetLevel(&level);
    if (level == mLevel + 1) {
      PRUint32 index;
      repeat->GetStartingIndex(&index);
      repeat->SetIndex(&index, aIsRefresh);
    }
  }
  
  return NS_OK;
}

nsresult
nsXFormsRepeatElement::CloneNode(nsIDOMNode  *aSrc,
                                 nsIDOMNode **aTarget)
{
  NS_ENSURE_ARG(aSrc);
  NS_ENSURE_ARG_POINTER(aTarget);

  // Clone aSrc
  nsresult rv;
  rv = aSrc->CloneNode(PR_FALSE, aTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check whether we have cloned a repeat
  if (nsXFormsUtils::IsXFormsElement(aSrc, NS_LITERAL_STRING("repeat"))) {
    nsCOMPtr<nsIXFormsRepeatElement> repSource = do_QueryInterface(aSrc);
    NS_ENSURE_STATE(repSource);
    nsCOMPtr<nsIXFormsRepeatElement> repClone = do_QueryInterface(*aTarget);
    NS_ENSURE_STATE(repClone);

    // Find top-most parent of these repeats
    nsCOMPtr<nsIXFormsRepeatElement> parent = repSource;
    nsCOMPtr<nsIXFormsRepeatElement> temp;

    rv = parent->GetParent(getter_AddRefs(temp));
    NS_ENSURE_SUCCESS(rv, rv);
    while (temp) {
      temp.swap(parent);
      rv = parent->GetParent(getter_AddRefs(temp));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Set parent and level on clone
    PRUint32 level;
    repSource->GetLevel(&level);
    repClone->SetLevel(level + 1);
    repClone->SetParent(parent);

    // Inform parent of new status, if it does not know already
    PRBool isParent;
    parent->GetIsParent(&isParent);
    if (!isParent) {
      rv = parent->SetCurrentRepeat(repClone, 0);
      NS_ENSURE_SUCCESS(rv, rv);
      parent->SetIsParent(PR_TRUE);
    }
  }

  // Clone children of aSrc
  nsCOMPtr<nsIDOMNode> tmp;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  aSrc->GetChildNodes(getter_AddRefs(childNodes));

  PRUint32 count = 0;
  if (childNodes)
    childNodes->GetLength(&count);

  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMNode> child;
    childNodes->Item(i, getter_AddRefs(child));
    
    if (child) {
      nsCOMPtr<nsIDOMNode> clone;
      CloneNode(child, getter_AddRefs(clone));
      if (clone) {
        rv = (*aTarget)->AppendChild(clone, getter_AddRefs(tmp));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::TryFocus(PRBool *aOK)
{
  if (!mCurrentIndex) {
    *aOK = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMElement> anon = GetAnonymousContent();
  if (!anon)
    return NS_OK;

 /**
  * "Setting focus to a repeating structure sets the focus to
  *  the repeat item represented by the repeat index."
  *  @see http://www.w3.org/TR/xforms/slice10.html#action-setfocus
  */
  nsCOMPtr<nsIDOMNodeList> children;
  anon->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_STATE(children);

  nsCOMPtr<nsIDOMNode> child;
  children->Item(mCurrentIndex - 1, // Indexes are 1-based, the DOM is 0-based
                 getter_AddRefs(child));
  nsCOMPtr<nsIXFormsControl> control = do_QueryInterface(child);
  NS_ENSURE_STATE(control);

  return control->TryFocus(aOK);
}

NS_IMETHODIMP
nsXFormsRepeatElement::IsEventTarget(PRBool *aOK)
{
  *aOK = PR_FALSE;
  return NS_OK;
}

/**
 * @todo This function will be part of the general schema support, so it will
 * only live here until this is implemented there. (XXX)
 */
nsresult
nsXFormsRepeatElement::GetIntAttr(const nsAString &aName,
                                  PRInt32         *aVal,
                                  const PRUint16   aType)
{
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(aVal);

  nsAutoString attrVal;
  mElement->GetAttribute(aName, attrVal);

  /// @todo Is this the correct error to return? We need to distinguish between
  /// an empty attribute and other errors. (XXX)
  if (attrVal.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  PRInt32 errCode;
  /// @todo ToInteger is extremely large, "xxx23xxx" will be parsed with no errors
  /// as "23"... (XXX)
  *aVal = attrVal.ToInteger(&errCode);
  NS_ENSURE_TRUE(errCode == 0, NS_ERROR_FAILURE);

  ///
  /// @todo Check maximum values? (XXX)
  switch (aType) {
  case nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER:
    if (*aVal < 0) {
     rv = NS_ERROR_FAILURE;
    }
    break;
  
  case nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER:
    if (*aVal <= 0) {
      rv = NS_ERROR_FAILURE;
    }
    break;

  case nsISchemaBuiltinType::BUILTIN_TYPE_ANYTYPE:
    break;

  default:
    rv = NS_ERROR_INVALID_ARG; // or NOT_IMPLEMENTED?
    break;
  }
  
  return rv;
}

PRBool
nsXFormsRepeatElement::IsBindingAttribute(const nsIAtom *aAttr) const
{
  if (aAttr == nsXFormsAtoms::bind ||
      aAttr == nsXFormsAtoms::nodeset  ||
      aAttr == nsXFormsAtoms::model) {
    return PR_TRUE;
  }
  
  return PR_FALSE;
}

already_AddRefed<nsIDOMElement>
nsXFormsRepeatElement::GetAnonymousContent()
{
  nsIDOMElement* anon = nsnull;
  nsCOMPtr<nsIXFormsRepeatUIElement> uiElement(do_QueryInterface(mElement));
  if (uiElement) {
    // addrefs
    uiElement->GetAnonymousRepeatContent(&anon);
  }
  return anon;
}

// Factory
NS_HIDDEN_(nsresult)
NS_NewXFormsRepeatElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsRepeatElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
