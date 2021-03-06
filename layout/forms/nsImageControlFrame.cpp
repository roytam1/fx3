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
#include "nsCOMPtr.h"
#include "nsIImageControlFrame.h"
#include "nsImageFrame.h"
#include "nsFormControlHelper.h"
#include "nsIFormControlFrame.h"
#include "nsIFormControl.h"
#include "nsHTMLParts.h"
#include "nsIRenderingContext.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIImage.h"
#include "nsStyleConsts.h"
#include "nsFormControlFrame.h"
#include "nsGUIEvent.h"
#include "nsLayoutAtoms.h"
#include "nsIServiceManager.h"
#include "nsContainerFrame.h"
#include "nsLayoutUtils.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

#define nsImageControlFrameSuper nsImageFrame
class nsImageControlFrame : public nsImageControlFrameSuper,
                            public nsIFormControlFrame,
                            public nsIImageControlFrame
{
public:
  nsImageControlFrame(nsStyleContext* aContext);
  ~nsImageControlFrame();

  virtual void Destroy();
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  virtual nsIAtom* GetType() const;

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("ImageControl"), aResult);
  }
#endif

  NS_IMETHOD GetCursor(const nsPoint&    aPoint,
                       nsIFrame::Cursor& aCursor);
  // nsIFormContromFrame
  virtual void SetFocus(PRBool aOn, PRBool aRepaint);
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 

  // nsIImageControlFrame
  NS_IMETHOD GetClickedX(PRInt32* aX);
  NS_IMETHOD GetClickedY(PRInt32* aY);

protected:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  nsPoint mLastClickPoint; 
};


nsImageControlFrame::nsImageControlFrame(nsStyleContext* aContext):
  nsImageControlFrameSuper(aContext)
{
  mLastClickPoint = nsPoint(0,0);
}

nsImageControlFrame::~nsImageControlFrame()
{
}

void
nsImageControlFrame::Destroy()
{
  nsFormControlFrame::RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  nsImageControlFrameSuper::Destroy();
}

nsIFrame*
NS_NewImageControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsImageControlFrame(aContext);
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsImageControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  } 
  if (aIID.Equals(NS_GET_IID(nsIImageControlFrame))) {
    *aInstancePtr = (void*) ((nsIImageControlFrame*) this);
    return NS_OK;
  }

  return nsImageControlFrameSuper::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsImageControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    if (mContent->Tag() == nsHTMLAtoms::button) {
      return accService->CreateHTML4ButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    }
    else if (mContent->Tag() == nsHTMLAtoms::input) {
      return accService->CreateHTMLButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    }
  }

  return NS_ERROR_FAILURE;
}
#endif

nsrefcnt nsImageControlFrame::AddRef(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsrefcnt nsImageControlFrame::Release(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsIAtom*
nsImageControlFrame::GetType() const
{
  return nsLayoutAtoms::imageControlFrame; 
}

NS_METHOD
nsImageControlFrame::Reflow(nsPresContext*         aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  if (aReflowState.reason == eReflowReason_Initial) {
    nsFormControlFrame::RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
  }
  return nsImageControlFrameSuper::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_METHOD 
nsImageControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                 nsGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Don't do anything if the event has already been handled by someone
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  // do we have user-input style?
  const nsStyleUserInterface* uiStyle = GetStyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  if (nsFormControlHelper::GetDisabled(mContent)) { // XXX cache disabled
    return NS_OK;
  }

  *aEventStatus = nsEventStatus_eIgnore;

  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP:
    {
      // Store click point for GetNamesValues
      // Do this on MouseUp because the specs don't say and that's what IE does
      nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
      TranslateEventCoords(pt, mLastClickPoint);
      break;
    }
  }
  return nsImageControlFrameSuper::HandleEvent(aPresContext, aEvent,
                                               aEventStatus);
}

void 
nsImageControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
}

NS_IMETHODIMP
nsImageControlFrame::GetCursor(const nsPoint&    aPoint,
                               nsIFrame::Cursor& aCursor)
{
  // Use style defined cursor if one is provided, otherwise when
  // the cursor style is "auto" we use the pointer cursor.
  FillCursorInformationFromStyle(GetStyleUserInterface(), aCursor);

  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    aCursor.mCursor = NS_STYLE_CURSOR_POINTER;
  }

  return NS_OK;
}

nsresult
nsImageControlFrame::SetFormProperty(nsIAtom* aName,
                                     const nsAString& aValue)
{
  return NS_OK;
}

nsresult
nsImageControlFrame::GetFormProperty(nsIAtom* aName,
                                     nsAString& aValue) const
{
  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsImageControlFrame::GetClickedX(PRInt32* aX)
{
  *aX = mLastClickPoint.x;
  return NS_OK;
}

NS_IMETHODIMP
nsImageControlFrame::GetClickedY(PRInt32* aY)
{
  *aY = mLastClickPoint.y;
  return NS_OK;
}
