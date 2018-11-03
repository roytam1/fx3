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
 * The Original Code is Mozilla Communicator client code.
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
#include "nsIDOMHTMLObjectElement.h"
#include "nsGenericHTMLElement.h"
#include "nsImageLoadingContent.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"

#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIWebNavigation.h"
#include "nsIFormControl.h"
#include "nsIEventStateManager.h"
#include "nsCSSPseudoClasses.h"
#include "nsLayoutAtoms.h"


class nsHTMLObjectElement : public nsGenericHTMLFormElement,
                            public nsImageLoadingContent,
                            public nsIDOMHTMLObjectElement
{
public:
  nsHTMLObjectElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT
  
  // nsIContent
  // Let plugin decide whether it wants focus from mouse clicks
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull)
    { if (aTabIndex) GetTabIndex(aTabIndex); return PR_TRUE; }

  // Overriden nsIFormControl methods
  NS_IMETHOD_(PRInt32) GetType()
  {
    return NS_FORM_OBJECT;
  }

  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  virtual PRBool RestoreState(nsPresState* aState);

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual PRInt32 IntrinsicState() const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Object)


nsHTMLObjectElement::nsHTMLObjectElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
{
}

nsHTMLObjectElement::~nsHTMLObjectElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLObjectElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLObjectElement, nsGenericElement) 

// QueryInterface implementation for nsHTMLObjectElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLObjectElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLObjectElement)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIImageLoadingContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLObjectElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

// nsIDOMHTMLObjectElement


NS_IMPL_DOM_CLONENODE(nsHTMLObjectElement)


NS_IMETHODIMP
nsHTMLObjectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLObjectElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                       nsIContent* aSubmitElement)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::SaveState()
{
  return NS_OK;
}

PRBool
nsHTMLObjectElement::RestoreState(nsPresState* aState)
{
  return PR_FALSE;
}

NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Code, code)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Archive, archive)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Border, border)
NS_IMPL_URI_ATTR(nsHTMLObjectElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, CodeType, codetype)
NS_IMPL_URI_ATTR(nsHTMLObjectElement, Data, data)
NS_IMPL_BOOL_ATTR(nsHTMLObjectElement, Declare, declare)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Height, height)
NS_IMPL_INT_ATTR(nsHTMLObjectElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Standby, standby)
NS_IMPL_INT_ATTR(nsHTMLObjectElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, UseMap, usemap)
NS_IMPL_INT_ATTR(nsHTMLObjectElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Width, width)


NS_IMETHODIMP
nsHTMLObjectElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);

  *aContentDocument = nsnull;

  if (!mDocument) {
    return NS_OK;
  }

  nsIDocument *sub_doc = mDocument->GetSubDocumentFor(this);

  if (!sub_doc) {
    return NS_OK;
  }

  return CallQueryInterface(sub_doc, aContentDocument);
}

PRBool
nsHTMLObjectElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsHTMLAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return PR_TRUE;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLObjectElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
    sImageAlignAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}


nsMapRuleToAttributesFunc
nsHTMLObjectElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

PRInt32
nsHTMLObjectElement::IntrinsicState() const
{
  PRInt32 state = nsGenericHTMLFormElement::IntrinsicState();

  void* image = GetProperty(nsLayoutAtoms::imageFrame);
  if (NS_PTR_TO_INT32(image)) {
    state |= nsImageLoadingContent::ImageState();
  }

  void* broken = GetProperty(nsCSSPseudoClasses::mozBroken);
  if (NS_PTR_TO_INT32(broken)) {
    state |= NS_EVENT_STATE_BROKEN;
  }

  return state;
}
