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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "nsITimer.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIFrameSelection.h"
#include "nsIFrame.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIFontMetrics.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMCharacterData.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIViewManager.h"
#include "nsPresContext.h"
#include "nsILookAndFeel.h"
#include "nsBlockFrame.h"
#include "nsISelectionController.h"

#include "nsCaret.h"

// The bidi indicator hangs off the caret to one side, to show which
// direction the typing is in. It needs to be at least 2x2 to avoid looking like 
// an insignificant dot
static const PRUint32 kMinBidiIndicatorPixels = 2;

#if !defined(MOZ_WIDGET_GTK2)
// Because of drawing issues, we currently always make a new RC. See bug 28068
// Before removing this, stuff will need to be fixed and tested on all platforms.
// For example, turning this off on Mac right now causes drawing problems on pages
// with form elements.
// Also turning this off caused problems on GTK1. See bug 254049.
#define DONT_REUSE_RENDERING_CONTEXT
#endif

#ifdef IBMBIDI
//-------------------------------IBM BIDI--------------------------------------
// Mamdouh : Modifiaction of the caret to work with Bidi in the LTR and RTL
#include "nsLayoutAtoms.h"
//------------------------------END OF IBM BIDI--------------------------------
#endif //IBMBIDI

//-----------------------------------------------------------------------------

nsCaret::nsCaret()
: mPresShell(nsnull)
, mBlinkRate(500)
, mVisible(PR_FALSE)
, mDrawn(PR_FALSE)
, mReadOnly(PR_FALSE)
, mShowDuringSelection(PR_FALSE)
, mLastCaretView(nsnull)
, mLastContentOffset(0)
, mLastHint(nsIFrameSelection::HINTLEFT)
#ifdef IBMBIDI
, mLastBidiLevel(0)
, mKeyboardRTL(PR_FALSE)
#endif
{
}


//-----------------------------------------------------------------------------
nsCaret::~nsCaret()
{
  KillTimer();
}

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::Init(nsIPresShell *inPresShell)
{
  NS_ENSURE_ARG(inPresShell);
  
  mPresShell = do_GetWeakReference(inPresShell);    // the presshell owns us, so no addref
  NS_ASSERTION(mPresShell, "Hey, pres shell should support weak refs");

  // get nsILookAndFeel from the pres context, which has one cached.
  nsILookAndFeel *lookAndFeel = nsnull;
  nsPresContext *presContext = inPresShell->GetPresContext();
  
  PRInt32 caretPixelsWidth = 1;
  if (presContext && (lookAndFeel = presContext->LookAndFeel())) {
    PRInt32 tempInt;
    if (NS_SUCCEEDED(lookAndFeel->GetMetric(nsILookAndFeel::eMetric_CaretWidth, tempInt)))
      caretPixelsWidth = (nscoord)tempInt;
    if (NS_SUCCEEDED(lookAndFeel->GetMetric(nsILookAndFeel::eMetric_CaretBlinkTime, tempInt)))
      mBlinkRate = (PRUint32)tempInt;
    if (NS_SUCCEEDED(lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ShowCaretDuringSelection, tempInt)))
      mShowDuringSelection = tempInt ? PR_TRUE : PR_FALSE;
  }
  
  float tDevUnitsToTwips;
  tDevUnitsToTwips = presContext->DeviceContext()->DevUnitsToTwips();
  mCaretTwipsWidth = (nscoord)(tDevUnitsToTwips * (float)caretPixelsWidth);
  mBidiIndicatorTwipsSize = (nscoord)(tDevUnitsToTwips * (float)kMinBidiIndicatorPixels);
  if (mBidiIndicatorTwipsSize < mCaretTwipsWidth) {
    mBidiIndicatorTwipsSize = mCaretTwipsWidth;
  }

  // get the selection from the pres shell, and set ourselves up as a selection
  // listener

  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShell);
  if (!selCon)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelection> domSelection;
  nsresult rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSelection));
  if (NS_FAILED(rv))
    return rv;
  if (!domSelection)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelectionPrivate> privateSelection = do_QueryInterface(domSelection);
  if (privateSelection)
    privateSelection->AddSelectionListener(this);
  mDomSelectionWeak = do_GetWeakReference(domSelection);
  
  // set up the blink timer
  if (mVisible)
  {
    rv = StartBlinking();
    if (NS_FAILED(rv))
      return rv;
  }

#ifdef IBMBIDI
  PRBool isRTL = PR_FALSE;
  mBidiKeyboard = do_GetService("@mozilla.org/widget/bidikeyboard;1");
  if (mBidiKeyboard)
	mBidiKeyboard->IsLangRTL(&isRTL);
  mKeyboardRTL = isRTL;
#endif
  
  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::Terminate()
{
  // this doesn't erase the caret if it's drawn. Should it? We might not have a good
  // drawing environment during teardown.
  
  KillTimer();
  mBlinkTimer = nsnull;
  
  mRendContext = nsnull;

  // unregiser ourselves as a selection listener
  nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSelection));
  if (privateSelection)
    privateSelection->RemoveSelectionListener(this);
  mDomSelectionWeak = nsnull;
  mPresShell = nsnull;

  mLastContent = nsnull;
  mLastCaretView = nsnull;
  
#ifdef IBMBIDI
  mBidiKeyboard = nsnull;
#endif

  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMPL_ISUPPORTS2(nsCaret, nsICaret, nsISelectionListener)

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::GetCaretDOMSelection(nsISelection **aDOMSel)
{
  nsCOMPtr<nsISelection> sel(do_QueryReferent(mDomSelectionWeak));
  
  NS_IF_ADDREF(*aDOMSel = sel);

  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::SetCaretDOMSelection(nsISelection *aDOMSel)
{
  NS_ENSURE_ARG_POINTER(aDOMSel);
  mDomSelectionWeak = do_GetWeakReference(aDOMSel);   // weak reference to pres shell
  if (mVisible)
  {
    // Stop the caret from blinking in its previous location.
    StopBlinking();
    // Start the caret blinking in the new location.
    StartBlinking();
  }
  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::SetCaretVisible(PRBool inMakeVisible)
{
  mVisible = inMakeVisible;
  nsresult  err = NS_OK;
  if (mVisible)
    err = StartBlinking();
  else
    err = StopBlinking();
    
  return err;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::GetCaretVisible(PRBool *outMakeVisible)
{
  NS_ENSURE_ARG_POINTER(outMakeVisible);
  *outMakeVisible = mVisible;
  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::SetCaretReadOnly(PRBool inMakeReadonly)
{
  mReadOnly = inMakeReadonly;
  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::GetCaretCoordinates(EViewCoordinates aRelativeToType, nsISelection *aDOMSel, nsRect *outCoordinates, PRBool *outIsCollapsed, nsIView **outView)
{
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;
  if (!outCoordinates || !outIsCollapsed)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISelection> domSelection = aDOMSel;
  nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSelection));
  if (!privateSelection)
    return NS_ERROR_NOT_INITIALIZED;    // no selection

  if (outView)
    *outView = nsnull;

  // fill in defaults for failure
  outCoordinates->x = -1;
  outCoordinates->y = -1;
  outCoordinates->width = -1;
  outCoordinates->height = -1;
  *outIsCollapsed = PR_FALSE;
  
  nsresult err = domSelection->GetIsCollapsed(outIsCollapsed);
  if (NS_FAILED(err)) 
    return err;
    
  nsCOMPtr<nsIDOMNode>  focusNode;
  
  err = domSelection->GetFocusNode(getter_AddRefs(focusNode));
  if (NS_FAILED(err))
    return err;
  if (!focusNode)
    return NS_ERROR_FAILURE;
  
  PRInt32 focusOffset;
  err = domSelection->GetFocusOffset(&focusOffset);
  if (NS_FAILED(err))
    return err;
    
/*
  // is this a text node?
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(focusNode);
  // note that we only work with text nodes here, unlike when drawing the caret.
  // this is because this routine is intended for IME support, which only cares about text.
  if (!nodeAsText)
    return NS_ERROR_UNEXPECTED;
*/  
  nsCOMPtr<nsIContent>contentNode = do_QueryInterface(focusNode);
  if (!contentNode)
    return NS_ERROR_FAILURE;

  // find the frame that contains the content node that has focus
  nsIFrame*       theFrame = nsnull;
  PRInt32         theFrameOffset = 0;

  nsCOMPtr<nsIFrameSelection> frameSelection;
  privateSelection->GetFrameSelection(getter_AddRefs(frameSelection));

  nsIFrameSelection::HINT hint;
  frameSelection->GetHint(&hint);

  PRUint8 bidiLevel;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  presShell->GetCaretBidiLevel(&bidiLevel);
  
  err = GetCaretFrameForNodeOffset(contentNode,
                                   focusOffset, hint,
                                   bidiLevel,
                                   &theFrame,
                                   &theFrameOffset);
  if (NS_FAILED(err) || !theFrame)
    return err;
  
  nsPoint   viewOffset(0, 0);
  nsRect    clipRect;
  nsIView   *drawingView;     // views are not refcounted

  GetViewForRendering(theFrame, aRelativeToType, viewOffset, clipRect, &drawingView, outView);
  if (!drawingView)
    return NS_ERROR_UNEXPECTED;
  // ramp up to make a rendering context for measuring text.
  // First, we get the pres context ...
  nsPresContext *presContext = presShell->GetPresContext();

  // ... then tell it to make a rendering context
  nsCOMPtr<nsIRenderingContext> rendContext;  
  err = presContext->DeviceContext()->
    CreateRenderingContext(drawingView, *getter_AddRefs(rendContext));
  if (NS_FAILED(err))
    return err;
  if (!rendContext)
    return NS_ERROR_UNEXPECTED;

  // now we can measure the offset into the frame.
  nsPoint   framePos(0, 0);
  theFrame->GetPointFromOffset(presContext, rendContext, theFrameOffset, &framePos);

  // we don't need drawingView anymore so reuse that; reset viewOffset values for our purposes
  if (aRelativeToType == eClosestViewCoordinates)
  {
    theFrame->GetOffsetFromView(viewOffset, &drawingView);
    if (outView)
      *outView = drawingView;
  }
  // now add the frame offset to the view offset, and we're done
  viewOffset += framePos;
  outCoordinates->x = viewOffset.x;
  outCoordinates->y = viewOffset.y;
  outCoordinates->height = theFrame->GetSize().height;
  outCoordinates->width  = mCaretTwipsWidth;
  
  return NS_OK;
}

void nsCaret::DrawCaretAfterBriefDelay()
{
  // Make sure readonly caret gets drawn again if it needs to be
  if (!mBlinkTimer) {
    nsresult  err;
    mBlinkTimer = do_CreateInstance("@mozilla.org/timer;1", &err);    
    if (NS_FAILED(err))
      return;
  }    

  mBlinkTimer->InitWithFuncCallback(CaretBlinkCallback, this, 0,
                                    nsITimer::TYPE_ONE_SHOT);
}

NS_IMETHODIMP nsCaret::EraseCaret()
{
  if (mDrawn) {
    DrawCaret();
    if (mReadOnly) {
      // If readonly we don't have a blink timer set, so caret won't
      // be redrawn automatically. We need to force the caret to get
      // redrawn right after the paint
      DrawCaretAfterBriefDelay();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsCaret::SetVisibilityDuringSelection(PRBool aVisibility) 
{
  mShowDuringSelection = aVisibility;
  return NS_OK;
}

NS_IMETHODIMP nsCaret::DrawAtPosition(nsIDOMNode* aNode, PRInt32 aOffset)
{
  NS_ENSURE_ARG(aNode);

  PRUint8 bidiLevel;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  presShell->GetCaretBidiLevel(&bidiLevel);
  
  // XXX we need to do more work here to get the correct hint.
  return DrawAtPositionWithHint(aNode, aOffset, nsIFrameSelection::HINTLEFT, bidiLevel) ?
    NS_OK : NS_ERROR_FAILURE;
}


#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::NotifySelectionChanged(nsIDOMDocument *, nsISelection *aDomSel, PRInt16 aReason)
{
  if (aReason & nsISelectionListener::MOUSEUP_REASON)//this wont do
    return NS_OK;

  nsCOMPtr<nsISelection> domSel(do_QueryReferent(mDomSelectionWeak));

  // The same caret is shared amongst the document and any text widgets it
  // may contain. This means that the caret could get notifications from
  // multiple selections.
  //
  // If this notification is for a selection that is not the one the
  // the caret is currently interested in (mDomSelectionWeak), then there
  // is nothing to do!

  if (domSel != aDomSel)
    return NS_OK;

  if (mVisible)
  {
    // Stop the caret from blinking in its previous location.
    StopBlinking();

    // Start the caret blinking in the new location.
    StartBlinking();
  }

  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
void nsCaret::KillTimer()
{
  if (mBlinkTimer)
  {
    mBlinkTimer->Cancel();
  }
}


//-----------------------------------------------------------------------------
nsresult nsCaret::PrimeTimer()
{
  // set up the blink timer
  if (!mReadOnly && mBlinkRate > 0)
  {
    if (!mBlinkTimer) {
      nsresult  err;
      mBlinkTimer = do_CreateInstance("@mozilla.org/timer;1", &err);    
      if (NS_FAILED(err))
        return err;
    }    

    mBlinkTimer->InitWithFuncCallback(CaretBlinkCallback, this, mBlinkRate,
                                      nsITimer::TYPE_REPEATING_SLACK);
  }

  return NS_OK;
}


//-----------------------------------------------------------------------------
nsresult nsCaret::StartBlinking()
{
  if (mReadOnly) {
    // Make sure the one draw command we use for a readonly caret isn't
    // done until the selection is set
    DrawCaretAfterBriefDelay();
    return NS_OK;
  }
  PrimeTimer();

  //NS_ASSERTION(!mDrawn, "Caret should not be drawn here");
  DrawCaret();    // draw it right away
  
  return NS_OK;
}


//-----------------------------------------------------------------------------
nsresult nsCaret::StopBlinking()
{
  if (mDrawn)     // erase the caret if necessary
    DrawCaret();
    
  KillTimer();
  
  return NS_OK;
}

PRBool
nsCaret::DrawAtPositionWithHint(nsIDOMNode*             aNode,
                                PRInt32                 aOffset,
                                nsIFrameSelection::HINT aFrameHint,
                                PRUint8                 aBidiLevel)
{  
  nsCOMPtr<nsIContent> contentNode = do_QueryInterface(aNode);
  if (!contentNode)
    return PR_FALSE;
      
  nsIFrame* theFrame = nsnull;
  PRInt32   theFrameOffset = 0;

  nsresult rv = GetCaretFrameForNodeOffset(contentNode, aOffset, aFrameHint, aBidiLevel,
                                           &theFrame, &theFrameOffset);
  if (NS_FAILED(rv) || !theFrame)
    return PR_FALSE;
  
  // now we have a frame, check whether it's appropriate to show the caret here
  const nsStyleUserInterface* userinterface = theFrame->GetStyleUserInterface();
  if (
#ifdef SUPPORT_USER_MODIFY
        // editable content still defaults to NS_STYLE_USER_MODIFY_READ_ONLY at present. See bug 15284
      (userinterface->mUserModify == NS_STYLE_USER_MODIFY_READ_ONLY) ||
#endif          
      (userinterface->mUserInput == NS_STYLE_USER_INPUT_NONE) ||
      (userinterface->mUserInput == NS_STYLE_USER_INPUT_DISABLED))
  {
    return PR_FALSE;
  }  

  if (!mDrawn)
  {
    // save stuff so we can erase the caret later
    mLastContent = contentNode;
    mLastContentOffset = aOffset;
    mLastHint = aFrameHint;
    mLastBidiLevel = aBidiLevel;

    // If there has been a reflow, set the caret Bidi level to the level of the current frame
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
    if (aBidiLevel & BIDI_LEVEL_UNDEFINED)
      presShell->SetCaretBidiLevel(NS_GET_EMBEDDING_LEVEL(theFrame));
  }

  GetCaretRectAndInvert(theFrame, theFrameOffset);

  return PR_TRUE;
}

NS_IMETHODIMP 
nsCaret::GetCaretFrameForNodeOffset (nsIContent*             aContentNode,
                                     PRInt32                 aOffset,
                                     nsIFrameSelection::HINT aFrameHint,
                                     PRUint8                 aBidiLevel,
                                     nsIFrame**              aReturnFrame,
                                     PRInt32*                aReturnOffset)
{

  //get frame selection and find out what frame to use...
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryReferent(mDomSelectionWeak));
  if (!privateSelection)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFrameSelection> frameSelection;
  privateSelection->GetFrameSelection(getter_AddRefs(frameSelection));

  nsIFrame* theFrame = nsnull;
  PRInt32   theFrameOffset = 0;

  nsresult rv = frameSelection->GetFrameForNodeOffset(aContentNode, aOffset, aFrameHint, &theFrame, &theFrameOffset);
  if (NS_FAILED(rv) || !theFrame)
    return NS_ERROR_FAILURE;

  // Mamdouh : modification of the caret to work at rtl and ltr with Bidi
  //
  // Direction Style from this->GetStyleData()
  // now in (visibility->mDirection)
  // ------------------
  // NS_STYLE_DIRECTION_LTR : LTR or Default
  // NS_STYLE_DIRECTION_RTL
  // NS_STYLE_DIRECTION_INHERIT
  nsPresContext *presContext = presShell->GetPresContext();
  if (presContext && presContext->BidiEnabled())
  {
    // If there has been a reflow, take the caret Bidi level to be the level of the current frame
    if (aBidiLevel & BIDI_LEVEL_UNDEFINED)
      aBidiLevel = NS_GET_EMBEDDING_LEVEL(theFrame);

    PRInt32 start;
    PRInt32 end;
    nsIFrame* frameBefore;
    nsIFrame* frameAfter;
    PRUint8 levelBefore;     // Bidi level of the character before the caret
    PRUint8 levelAfter;      // Bidi level of the character after the caret

    theFrame->GetOffsets(start, end);
    if (start == 0 || end == 0 || start == theFrameOffset || end == theFrameOffset)
    {
      /* Boundary condition, we need to know the Bidi levels of the characters before and after the caret */
      if (NS_SUCCEEDED(frameSelection->GetPrevNextBidiLevels(presContext, aContentNode, aOffset, PR_FALSE,
                                                             &frameBefore, &frameAfter,
                                                             &levelBefore, &levelAfter)))
      {
        if ((levelBefore != levelAfter) || (aBidiLevel != levelBefore))
        {
          aBidiLevel = PR_MAX(aBidiLevel, PR_MIN(levelBefore, levelAfter));                                  // rule c3
          aBidiLevel = PR_MIN(aBidiLevel, PR_MAX(levelBefore, levelAfter));                                  // rule c4
          if (aBidiLevel == levelBefore                                                                      // rule c1
              || aBidiLevel > levelBefore && aBidiLevel < levelAfter && !((aBidiLevel ^ levelBefore) & 1)    // rule c5
              || aBidiLevel < levelBefore && aBidiLevel > levelAfter && !((aBidiLevel ^ levelBefore) & 1))   // rule c9
          {
            if (theFrame != frameBefore)
            {
              if (frameBefore) // if there is a frameBefore, move into it
              {
                theFrame = frameBefore;
                theFrame->GetOffsets(start, end);
                theFrameOffset = end;
              }
              else 
              {
                // if there is no frameBefore, we must be at the beginning of the line
                // so we stay with the current frame.
                // Exception: when the first frame on the line has a different Bidi level from the paragraph level, there is no
                // real frame for the caret to be in. We have to find the first frame whose level is the same as the
                // paragraph level, and put the caret at the end of the frame before that.
                PRUint8 baseLevel = NS_GET_BASE_LEVEL(frameAfter);
                if (baseLevel != levelAfter)
                {
                  if (NS_SUCCEEDED(frameSelection->GetFrameFromLevel(presContext, frameAfter, eDirNext, baseLevel, &theFrame)))
                  {
                    theFrame->GetOffsets(start, end);
                    levelAfter = NS_GET_EMBEDDING_LEVEL(theFrame);
                    if (baseLevel & 1) // RTL paragraph: caret to the right of the rightmost character
                      theFrameOffset = (levelAfter & 1) ? start : end;
                    else               // LTR paragraph: caret to the left of the leftmost character
                      theFrameOffset = (levelAfter & 1) ? end : start;
                  }
                }
              }
            }
          }
          else if (aBidiLevel == levelAfter                                                                     // rule c2
                   || aBidiLevel > levelBefore && aBidiLevel < levelAfter && !((aBidiLevel ^ levelAfter) & 1)   // rule c6  
                   || aBidiLevel < levelBefore && aBidiLevel > levelAfter && !((aBidiLevel ^ levelAfter) & 1))  // rule c10
          {
            if (theFrame != frameAfter)
            {
              if (frameAfter)
              {
                // if there is a frameAfter, move into it
                theFrame = frameAfter;
                theFrame->GetOffsets(start, end);
                theFrameOffset = start;
              }
              else 
              {
                // if there is no frameAfter, we must be at the end of the line
                // so we stay with the current frame.
                //
                // Exception: when the last frame on the line has a different Bidi level from the paragraph level, there is no
                // real frame for the caret to be in. We have to find the last frame whose level is the same as the
                // paragraph level, and put the caret at the end of the frame after that.

                PRUint8 baseLevel = NS_GET_BASE_LEVEL(frameBefore);
                if (baseLevel != levelBefore)
                {
                  if (NS_SUCCEEDED(frameSelection->GetFrameFromLevel(presContext, frameBefore, eDirPrevious, baseLevel, &theFrame)))
                  {
                    theFrame->GetOffsets(start, end);
                    levelBefore = NS_GET_EMBEDDING_LEVEL(theFrame);
                    if (baseLevel & 1) // RTL paragraph: caret to the left of the leftmost character
                      theFrameOffset = (levelBefore & 1) ? end : start;
                    else               // RTL paragraph: caret to the right of the rightmost character
                      theFrameOffset = (levelBefore & 1) ? start : end;
                  }
                }
              }
            }
          }
          else if (aBidiLevel > levelBefore && aBidiLevel < levelAfter  // rule c7/8
                   && !((levelBefore ^ levelAfter) & 1)                 // before and after have the same parity
                   && ((aBidiLevel ^ levelAfter) & 1))                  // caret has different parity
          {
            if (NS_SUCCEEDED(frameSelection->GetFrameFromLevel(presContext, frameAfter, eDirNext, aBidiLevel, &theFrame)))
            {
              theFrame->GetOffsets(start, end);
              levelAfter = NS_GET_EMBEDDING_LEVEL(theFrame);
              if (aBidiLevel & 1) // c8: caret to the right of the rightmost character
                theFrameOffset = (levelAfter & 1) ? start : end;
              else               // c7: caret to the left of the leftmost character
                theFrameOffset = (levelAfter & 1) ? end : start;
            }
          }
          else if (aBidiLevel < levelBefore && aBidiLevel > levelAfter  // rule c11/12
                   && !((levelBefore ^ levelAfter) & 1)                 // before and after have the same parity
                   && ((aBidiLevel ^ levelAfter) & 1))                  // caret has different parity
          {
            if (NS_SUCCEEDED(frameSelection->GetFrameFromLevel(presContext, frameBefore, eDirPrevious, aBidiLevel, &theFrame)))
            {
              theFrame->GetOffsets(start, end);
              levelBefore = NS_GET_EMBEDDING_LEVEL(theFrame);
              if (aBidiLevel & 1) // c12: caret to the left of the leftmost character
                theFrameOffset = (levelBefore & 1) ? end : start;
              else               // c11: caret to the right of the rightmost character
                theFrameOffset = (levelBefore & 1) ? start : end;
            }
          }   
        }
      }
    }
  }
  *aReturnFrame = theFrame;
  *aReturnOffset = theFrameOffset;
  return NS_OK;
}


//-----------------------------------------------------------------------------
void nsCaret::GetViewForRendering(nsIFrame *caretFrame, EViewCoordinates coordType, nsPoint &viewOffset, nsRect& outClipRect, nsIView **outRenderingView, nsIView **outRelativeView)
{

  if (!caretFrame || !outRenderingView)
    return;

  // XXX by Masayuki Nakano:
  // Our this code is not good. This is adhoc approach.
  // Our best approach is to use the event fired widget related view.
  // But if we do so, we need large change for editor and this.
  if (coordType == eIMECoordinates)
#if defined(XP_MAC) || defined(XP_MACOSX) || defined(XP_WIN)
   // #59405 and #313918, on Mac and Windows, the coordinate for IME need to be
   // root view related.
   coordType = eTopLevelWindowCoordinates; 
#else
   // #59405, on unix, the coordinate for IME need to be view
   // (nearest native window) related.
   coordType = eRenderingViewCoordinates; 
#endif

  *outRenderingView = nsnull;
  if (outRelativeView)
    *outRelativeView = nsnull;
  
  NS_ASSERTION(caretFrame, "Should have frame here");
 
  viewOffset.x = 0;
  viewOffset.y = 0;
  
  nsPoint   withinViewOffset(0, 0);
  // get the offset of this frame from its parent view (walks up frame hierarchy)
  nsIView* theView = nsnull;
  caretFrame->GetOffsetFromView(withinViewOffset, &theView);
  if (theView == nsnull) return;

  if (outRelativeView && coordType == eClosestViewCoordinates)
    *outRelativeView = theView;

  nsIView*    returnView = nsnull;    // views are not refcounted
  
  // coorinates relative to the view we are going to use for drawing
  if (coordType == eRenderingViewCoordinates)
  {
    nsIScrollableView*  scrollableView = nsnull;
  
    nsPoint             drawViewOffset(0, 0);         // offset to the view we are using to draw
    
    // walk up to the first view with a widget
    do {
      //is this a scrollable view?
      if (!scrollableView)
        scrollableView = theView->ToScrollableView();

      if (theView->HasWidget())
      {
        returnView = theView;
        // account for the view's origin not lining up with the widget's (bug 190290)
        drawViewOffset += theView->GetPosition() - theView->GetBounds().TopLeft();
        break;
      }
      drawViewOffset += theView->GetPosition();
      theView = theView->GetParent();
    } while (theView);
    
    viewOffset = withinViewOffset;
    viewOffset += drawViewOffset;
    
    if (scrollableView)
    {
      nsRect  bounds = scrollableView->View()->GetBounds();
      scrollableView->GetScrollPosition(bounds.x, bounds.y);
      
      bounds += drawViewOffset;   // offset to coords of returned view
      outClipRect = bounds;
    }
    else
    {
      NS_ASSERTION(returnView, "bulletproofing, see bug #24329");
      if (returnView)
        outClipRect = returnView->GetBounds();
    }

    if (outRelativeView)
      *outRelativeView = returnView;
  }
  else
  {
    // window-relative coordinates (walk right to the top of the view hierarchy)
    // we don't do anything with clipping here
    viewOffset = withinViewOffset;

    do {
      if (!returnView && theView->HasWidget())
        returnView = theView;
      // is this right?
      viewOffset += theView->GetPosition();
      
      if (outRelativeView && coordType == eTopLevelWindowCoordinates)
        *outRelativeView = theView;

      theView = theView->GetParent();
    } while (theView);
  }
  
  *outRenderingView = returnView;
}


/*-----------------------------------------------------------------------------

  MustDrawCaret
  
  FInd out if we need to do any caret drawing. This returns true if
  either a) or b)
  a) caret has been drawn, and we need to erase it.
  b) caret is not drawn, and selection is collapsed
  
----------------------------------------------------------------------------- */
PRBool nsCaret::MustDrawCaret()
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (presShell) {
    PRBool isPaintingSuppressed;
    presShell->IsPaintingSuppressed(&isPaintingSuppressed);
    if (isPaintingSuppressed)
      return PR_FALSE;
  }

  if (mDrawn)
    return PR_TRUE;

  nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  if (!domSelection)
    return PR_FALSE;
  PRBool isCollapsed;

  if (NS_FAILED(domSelection->GetIsCollapsed(&isCollapsed)))
    return PR_FALSE;

  if (mShowDuringSelection)
    return PR_TRUE;      // show the caret even in selections

  return isCollapsed;
}


/*-----------------------------------------------------------------------------

  DrawCaret
    
----------------------------------------------------------------------------- */

void nsCaret::DrawCaret()
{
  // do we need to draw the caret at all?
  if (!MustDrawCaret())
    return;
  
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  nsIFrameSelection::HINT hint;
  PRUint8 bidiLevel;

  if (!mDrawn)
  {
    nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
    nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSelection));
    if (!privateSelection) return;
    
    PRBool isCollapsed = PR_FALSE;
    domSelection->GetIsCollapsed(&isCollapsed);
    if (!mShowDuringSelection && !isCollapsed)
      return;

    PRBool hintRight;
    privateSelection->GetInterlinePosition(&hintRight);//translate hint.
    hint = hintRight ? nsIFrameSelection::HINTRIGHT : nsIFrameSelection::HINTLEFT;

    // get the node and offset, which is where we want the caret to draw
    domSelection->GetFocusNode(getter_AddRefs(node));
    if (!node)
      return;
    
    if (NS_FAILED(domSelection->GetFocusOffset(&offset)))
      return;

    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
    presShell->GetCaretBidiLevel(&bidiLevel);
  }
  else
  {
    if (!mLastContent)
    {
      mDrawn = PR_FALSE;
      return;
    }
    if (!mLastContent->IsInDoc())
    {
      mLastContent = nsnull;
      mDrawn = PR_FALSE;
      return;
    }
    node = do_QueryInterface(mLastContent);
    offset = mLastContentOffset;
    hint = mLastHint;
    bidiLevel = mLastBidiLevel;
  }

  DrawAtPositionWithHint(node, offset, hint, bidiLevel);
}

void nsCaret::GetCaretRectAndInvert(nsIFrame* aFrame, PRInt32 aFrameOffset)
{
  NS_ASSERTION(aFrame, "Should have a frame here");

  nsRect frameRect = aFrame->GetRect();
  frameRect.x = 0;      // the origin is accounted for in GetViewForRendering()
  frameRect.y = 0;
  
  nsPoint   viewOffset(0, 0);
  nsRect    clipRect;
  nsIView   *drawingView;
  GetViewForRendering(aFrame, eRenderingViewCoordinates, viewOffset, clipRect, &drawingView, nsnull);
  
  if (!drawingView)
    return;
  
  frameRect += viewOffset;

  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) return;
  
  nsPresContext *presContext = presShell->GetPresContext();

  // if the view changed, or we don't have a rendering context, make one
  // because of drawing issues, always make a new RC at the moment. See bug 28068
  if (
#ifdef DONT_REUSE_RENDERING_CONTEXT
      PR_TRUE ||
#endif
      (mLastCaretView != drawingView) || !mRendContext)
  {
    mRendContext = nsnull;    // free existing one if we have one
    
    nsresult rv = presContext->DeviceContext()->
      CreateRenderingContext(drawingView, *getter_AddRefs(mRendContext));

    if (NS_FAILED(rv) || !mRendContext)
      return;      
  }

  // push a known good state
  mRendContext->PushState();

  // if we got a zero-height frame, it's probably a BR frame at the end of a non-empty line
  // (see BRFrame::Reflow). In that case, figure out a height. We have to do this
  // after we've got an RC.
  if (frameRect.height == 0)
  {
      const nsStyleFont* fontStyle = aFrame->GetStyleFont();
      const nsStyleVisibility* vis = aFrame->GetStyleVisibility();
      mRendContext->SetFont(fontStyle->mFont, vis->mLangGroup);

      nsCOMPtr<nsIFontMetrics> fm;
      mRendContext->GetFontMetrics(*getter_AddRefs(fm));
      if (fm)
      {
        nscoord ascent, descent;
        fm->GetMaxAscent(ascent);
        fm->GetMaxDescent(descent);
        frameRect.height = ascent + descent;
        frameRect.y -= ascent; // BR frames sit on the baseline of the text, so we need to subtract
                               // the ascent to account for the frame height.
      }
  }
  
  // views are not refcounted
  mLastCaretView = drawingView;

  if (!mDrawn)
  {
    nsPoint   framePos(0, 0);
    nsRect    caretRect = frameRect;
    nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
    nsCOMPtr<nsISelectionPrivate> privateSelection = do_QueryInterface(domSelection);

    // if cache in selection is available, apply it, else refresh it
    privateSelection->GetCachedFrameOffset(aFrame, aFrameOffset, framePos);

    caretRect += framePos;

    caretRect.width = mCaretTwipsWidth;

    // Check if the caret intersects with the right edge
    // of the frame. If it does, and the frame's right edge
    // is close to the right edge of the clipRect, we may
    // need to adjust the caret's x position so that it
    // remains visible.

    nscoord caretXMost = caretRect.XMost();
    nscoord frameXMost = frameRect.XMost();

    if (caretXMost > frameXMost)
    {
      nscoord clipXMost  = clipRect.XMost();

      if (caretRect.x == frameRect.x && caretRect.x <= clipXMost &&
          caretXMost > clipXMost)
      {
        // The left side of the caret is attached to the left edge of
        // the frame, and it is wider than the frame itself. It also
        // overlaps the right edge of the clipRect so we need to nudge
        // it to the left so that it remains visible.
        //
        // We usually hit this case when the caret is attached to a
        // br frame (which is about 1 twip in width) that is positioned
        // at the right edge of the content area because it is right aligned
        // or the right margin pushed it beyond the width of the view port.

        caretRect.x = clipXMost - caretRect.width;
      }
      else if (caretRect.x == frameXMost && frameXMost == clipXMost)
      {
        // The left side of the caret is attached to the right edge of
        // the frame, but it's going to get clipped because it's positioned
        // on the  right edge of the clipRect, so nudge it to the
        // left so it remains visible.
        //
        // We usually hit this case when the caret is after the last
        // character on the line, and the line exceeds the width of the
        // view port.

        caretRect.x = clipXMost - caretRect.width;
      }
    }

    mCaretRect.IntersectRect(clipRect, caretRect);
#ifdef IBMBIDI
    // Simon -- make a hook to draw to the left or right of the caret to show keyboard language direction
    PRBool bidiEnabled;
    nsRect hookRect;
    PRBool isCaretRTL=PR_FALSE;
    if (mBidiKeyboard)
      mBidiKeyboard->IsLangRTL(&isCaretRTL);
    if (isCaretRTL)
    {
      bidiEnabled = PR_TRUE;
      presContext->SetBidiEnabled(bidiEnabled);
    }
    else
      bidiEnabled = presContext->BidiEnabled();
    if (bidiEnabled)
    {
      if (isCaretRTL != mKeyboardRTL)
      {
        /* if the caret bidi level and the keyboard language direction are not in
         * synch, the keyboard language must have been changed by the
         * user, and if the caret is in a boundary condition (between left-to-right and
         * right-to-left characters) it may have to change position to
         * reflect the location in which the next character typed will
         * appear. We will call |SelectionLanguageChange| and exit
         * without drawing the caret in the old position.
         */ 
        mKeyboardRTL = isCaretRTL;
        nsCOMPtr<nsISelection> domSelection = do_QueryReferent(mDomSelectionWeak);
        if (domSelection)
        {
          if (NS_SUCCEEDED(domSelection->SelectionLanguageChange(mKeyboardRTL)))
          {
            mRendContext->PopState();
#ifdef DONT_REUSE_RENDERING_CONTEXT
            mRendContext = nsnull;
#endif
            return;
          }
        }
      }
      // If keyboard language is RTL, draw the hook on the left; if LTR, to the right
      // The height of the hook rectangle is the same as the width of the caret
      // rectangle.
      hookRect.SetRect(caretRect.x + ((isCaretRTL) ?
                                       mBidiIndicatorTwipsSize * -1 :
                                       caretRect.width),
                       caretRect.y + mBidiIndicatorTwipsSize,
                       mBidiIndicatorTwipsSize,
                       caretRect.width);
      mHookRect.IntersectRect(clipRect, hookRect);
    }
#endif //IBMBIDI
  }
  
  if (mReadOnly)
    mRendContext->SetColor(NS_RGB(85, 85, 85));   // we are drawing it; gray
  else
    mRendContext->SetColor(NS_RGB(255,255,255));

  mRendContext->InvertRect(mCaretRect);

  // Ensure the buffer is flushed (Cocoa needs this), since we're drawing
  // outside the normal painting process.
  mRendContext->FlushRect(mCaretRect);

#ifdef IBMBIDI
  if (!mHookRect.IsEmpty()) // if Bidi support is disabled, the rectangle remains empty and won't be drawn
    mRendContext->InvertRect(mHookRect);
#endif

  mRendContext->PopState();
  
  ToggleDrawnStatus();

  if (mDrawn) {
    aFrame->AddStateBits(NS_FRAME_EXTERNAL_REFERENCE);
  }

#ifdef DONT_REUSE_RENDERING_CONTEXT
  mRendContext = nsnull;
#endif
}

#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
/* static */
void nsCaret::CaretBlinkCallback(nsITimer *aTimer, void *aClosure)
{
  nsCaret   *theCaret = NS_REINTERPRET_CAST(nsCaret*, aClosure);
  if (!theCaret) return;
  
  theCaret->DrawCaret();
}


//-----------------------------------------------------------------------------
nsresult NS_NewCaret(nsICaret** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null ptr");
  
  nsCaret* caret = new nsCaret();
  if (nsnull == caret)
      return NS_ERROR_OUT_OF_MEMORY;
      
  return caret->QueryInterface(NS_GET_IID(nsICaret), (void**) aInstancePtrResult);
}

