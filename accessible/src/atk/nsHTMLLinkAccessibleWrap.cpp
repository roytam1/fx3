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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gilbert Fang (gilbert.fang@sun.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsHTMLLinkAccessibleWrap.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDocument.h"
#include "nsILink.h"
#include "nsIDOMText.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

// --------------------------------------------------------
// nsHTMLLinkAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLLinkAccessibleWrap, nsHTMLLinkAccessible, nsIAccessibleHyperLink)

nsHTMLLinkAccessibleWrap::nsHTMLLinkAccessibleWrap(nsIDOMNode* aDomNode, nsIArray* aTextNodes, nsIWeakReference* aShell, nsIFrame *aFrame):
nsHTMLLinkAccessible(aDomNode, aShell, aFrame)
{ 
  mTextNodes = aTextNodes;
}

//-------------------------- nsIAccessibleHyperLink -------------------------
/* readonly attribute long anchors; */
NS_IMETHODIMP nsHTMLLinkAccessibleWrap::GetAnchors(PRInt32 *aAnchors)
{
  if (!mIsLink)
    return NS_ERROR_FAILURE;
  
  *aAnchors = 1;
  return NS_OK;
}

/* readonly attribute long startIndex; */
NS_IMETHODIMP nsHTMLLinkAccessibleWrap::GetStartIndex(PRInt32 *aStartIndex)
{
  PRInt32 endIndex;
  return GetLinkOffset(aStartIndex, &endIndex);
}

/* readonly attribute long endIndex; */
NS_IMETHODIMP nsHTMLLinkAccessibleWrap::GetEndIndex(PRInt32 *aEndIndex)
{
  PRInt32 startIndex;
  return GetLinkOffset(&startIndex, aEndIndex);
}

/* nsIURI getURI (in long i); */
NS_IMETHODIMP nsHTMLLinkAccessibleWrap::GetURI(PRInt32 i, nsIURI **aURI)
{
  //I do not know why we have to return a nsIURI instead of
  //nsILink or just a string of url. Anyway, maybe nsIURI is
  //more powerful for the future.
  *aURI = nsnull;

  if (!mIsLink)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILink> link(do_QueryInterface(mActionContent));
  if (link) {
    return link->GetHrefURI(aURI);
  }

  return NS_ERROR_FAILURE;
}

/* nsIAccessible getObject (in long i); */
NS_IMETHODIMP nsHTMLLinkAccessibleWrap::GetObject(PRInt32 aIndex,
                                              nsIAccessible **aAccessible)
{
  if (0 != aIndex)
    return NS_ERROR_FAILURE;
  
  return QueryInterface(NS_GET_IID(nsIAccessible), (void **)aAccessible);
}

/* boolean isValid (); */
NS_IMETHODIMP nsHTMLLinkAccessibleWrap::IsValid(PRBool *aIsValid)
{
  // I have not found the cause which makes this attribute false.
  *aIsValid = PR_TRUE;
  return NS_OK;
}

/* boolean isSelected (); */
NS_IMETHODIMP nsHTMLLinkAccessibleWrap::IsSelected(PRBool *aIsSelected)
{
  *aIsSelected = (gLastFocusedNode == mDOMNode);
  return NS_OK;
}

nsresult nsHTMLLinkAccessibleWrap::GetLinkOffset(PRInt32* aStartOffset, PRInt32* aEndOffset)
{
  NS_ENSURE_TRUE(mTextNodes, NS_ERROR_FAILURE);

  if (!mIsLink)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILink> currentLink(do_QueryInterface(mActionContent));
  NS_ENSURE_TRUE(currentLink, NS_ERROR_FAILURE);

  PRUint32 index, count = 0;
  PRUint32 totalLength = 0, textLength = 0;

  mTextNodes->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextNodes, index));
    nsCOMPtr<nsIDOMText> domText(do_QueryInterface(domNode));
    if (domText) {
      domText->GetLength(&textLength);
      totalLength += textLength;
    }

    // text node maybe a child (or grandchild, ...) of a link node
    nsCOMPtr<nsIDOMNode> parentNode;
    nsCOMPtr<nsILink> link = nsnull;
    domNode->GetParentNode(getter_AddRefs(parentNode));
    while (parentNode) {
      link = do_QueryInterface(parentNode);
      if (link)
        break;
      nsCOMPtr<nsIDOMNode> temp = parentNode;
      temp->GetParentNode(getter_AddRefs(parentNode));
    }

    if (link == currentLink) {
      *aEndOffset = totalLength;
      *aStartOffset = totalLength - textLength;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

// --------------------------------------------------------
// nsHTMLImageMapAccessible Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLImageMapAccessible, nsHTMLImageAccessible, nsIAccessibleHyperLink)

nsHTMLImageMapAccessible::nsHTMLImageMapAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsHTMLImageAccessible(aDomNode, aShell)
{
}

/* readonly attribute long anchors; */
NS_IMETHODIMP nsHTMLImageMapAccessible::GetAnchors(PRInt32 *aAnchors)
{
  return GetChildCount(aAnchors);
}

/* readonly attribute long startIndex; */
NS_IMETHODIMP nsHTMLImageMapAccessible::GetStartIndex(PRInt32 *aStartIndex)
{
  //should not be supported in image map hyperlink
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long endIndex; */
NS_IMETHODIMP nsHTMLImageMapAccessible::GetEndIndex(PRInt32 *aEndIndex)
{
  //should not be supported in image map hyperlink
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIURI getURI (in long i); */
NS_IMETHODIMP nsHTMLImageMapAccessible::GetURI(PRInt32 aIndex, nsIURI **aURI)
{
  *aURI = nsnull;

  nsCOMPtr<nsIDOMHTMLCollection> mapAreas;
  mMapElement->GetAreas(getter_AddRefs(mapAreas));
  if (!mapAreas)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> domNode;
  mapAreas->Item(aIndex,getter_AddRefs(domNode));
  if (!domNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (content) {
    nsCOMPtr<nsIURI> baseURI = content->GetBaseURI();

    nsCOMPtr<nsIDOMElement> area(do_QueryInterface(domNode));
    nsAutoString hrefValue;
    if (NS_SUCCEEDED(area->GetAttribute(NS_LITERAL_STRING("href"), hrefValue))) {
      return NS_NewURI(aURI, hrefValue, nsnull, baseURI);
    }
  }

  return NS_ERROR_FAILURE;
}

/* nsIAccessible getObject (in long i); */
NS_IMETHODIMP nsHTMLImageMapAccessible::GetObject(PRInt32 aIndex,
                                                  nsIAccessible **aAccessible)
{
  *aAccessible = nsnull;
  nsCOMPtr<nsIAccessible> areaAccessible;
  nsresult rv = GetChildAt(aIndex, getter_AddRefs(areaAccessible));
  areaAccessible.swap(*aAccessible);
  return rv;
}

/* boolean isValid (); */
NS_IMETHODIMP nsHTMLImageMapAccessible::IsValid(PRBool *aIsValid)
{
  *aIsValid = PR_TRUE;
  return NS_OK;
}

/* boolean isSelected (); */
NS_IMETHODIMP nsHTMLImageMapAccessible::IsSelected(PRBool *aIsSelected)
{
  *aIsSelected = PR_FALSE;
  return NS_OK;
}
