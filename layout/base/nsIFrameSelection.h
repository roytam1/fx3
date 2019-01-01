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

/*
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTE!!  This is not a general class, but specific to layout and frames.
 * Consumers looking for the general selection interface should look at
 * nsISelection.
 */

#ifndef nsIFrameSelection_h___
#define nsIFrameSelection_h___

#include "nsISupports.h"
#include "nsIFrame.h"
#include "nsISelection.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsISelectionController.h"

class nsIPresShell;

// IID for the nsIFrameSelection interface
// cdfa6280-eba6-4938-9406-427818da8ce3
#define NS_IFRAMESELECTION_IID      \
{ 0xcdfa6280, 0xeba6, 0x4938, \
  { 0x94, 0x06, 0x42, 0x78, 0x18, 0xda, 0x8c, 0xe3 } }

//----------------------------------------------------------------------

// Selection interface

struct SelectionDetails
{
  PRInt32 mStart;
  PRInt32 mEnd;
  SelectionType mType;
  SelectionDetails *mNext;
};

/*PeekOffsetStruct
   *  @param mShell is used to get the PresContext useful for measuring text etc.
   *  @param mDesiredX is the "desired" location of the new caret
   *  @param mAmount eWord, eCharacter, eLine
   *  @param mDirection enum defined in this file to be eForward or eBackward
   *  @param mStartOffset start offset to start the peek. 0 == beginning -1 = end
   *  @param mResultContent content that actually is the next/previous
   *  @param mResultOffset offset for result content
   *  @param mResultFrame resulting frame for peeking
   *  @param mEatingWS boolean to tell us the state of our search for Next/Prev
   *  @param mPreferLeft true = prev line end, false = next line begin
   *  @param mJumpLines if this is true then it's ok to cross lines while peeking
   *  @param mScrollViewStop if this is true then stop peeking across scroll view boundary
*/
struct nsPeekOffsetStruct
{
  void SetData(nsIPresShell *aShell,
               nscoord aDesiredX, 
               nsSelectionAmount aAmount,
               nsDirection aDirection,
               PRInt32 aStartOffset, 
               PRBool aEatingWS,
               PRBool aPreferLeft,
               PRBool aJumpLines,
               PRBool aScrollViewStop,
               PRBool aIsKeyboardSelect,
               PRBool aVisual)
      {
       mShell=aShell;
       mDesiredX=aDesiredX;
       mAmount=aAmount;
       mDirection=aDirection;
       mStartOffset=aStartOffset;
       mEatingWS=aEatingWS;
       mPreferLeft=aPreferLeft;
       mJumpLines = aJumpLines;
       mScrollViewStop = aScrollViewStop;
       mIsKeyboardSelect = aIsKeyboardSelect;
       mVisual = aVisual;
      }
  nsIPresShell *mShell;
  nscoord mDesiredX;
  nsSelectionAmount mAmount;
  nsDirection mDirection;
  PRInt32 mStartOffset;
  nsCOMPtr<nsIContent> mResultContent;
  PRInt32 mContentOffset;
  PRInt32 mContentOffsetEnd;
  nsIFrame *mResultFrame;
  PRBool mEatingWS;
  PRBool mPreferLeft;
  PRBool mJumpLines;
  PRBool mScrollViewStop;
  PRBool mIsKeyboardSelect;
  PRBool mVisual;
};

class nsIScrollableView;


class nsIFrameSelection : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFRAMESELECTION_IID)
  enum HINT { HINTLEFT = 0, HINTRIGHT = 1};  //end of this line or beginning of next

  /** Init will initialize the frame selector with the necessary pres shell to 
   *  be used by most of the methods
   *  @param aShell is the parameter to be used for most of the other calls for callbacks etc
   *  @param aLimiter limits the selection to nodes with aLimiter parents
   */
  NS_IMETHOD Init(nsIPresShell *aShell, nsIContent *aLimiter) = 0; //default since this isn't used for embedding

  /* SetScrollableView sets the scroll view
   *  @param aScrollView is the scroll view for this selection.
   */
  NS_IMETHOD SetScrollableView(nsIScrollableView *aScrollView) =0;

  /* GetScrollableView gets the current scroll view
   *  @param aScrollView is the scroll view for this selection.
   */
  NS_IMETHOD GetScrollableView(nsIScrollableView **aScrollView) =0;

  /** ShutDown will be called when the owner of the frame selection is shutting down
   *  this should be the time to release all member variable interfaces. all methods
   *  called after ShutDown should return NS_ERROR_FAILURE
   */
  NS_IMETHOD ShutDown() = 0;

  /** HandleKeyEvent will accept an event.
   *  <P>DOES NOT ADDREF<P>
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleTextEvent(nsGUIEvent *aGuiEvent) = 0;

  /** HandleKeyEvent will accept an event and a PresContext.
   *  <P>DOES NOT ADDREF<P>
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleKeyEvent(nsPresContext* aPresContext, nsGUIEvent *aGuiEvent) = 0;

  /** HandleClick will take the focus to the new frame at the new offset and 
   *  will either extend the selection from the old anchor, or replace the old anchor.
   *  the old anchor and focus position may also be used to deselect things
   *  @param aNewfocus is the content that wants the focus
   *  @param aContentOffset is the content offset of the parent aNewFocus
   *  @param aContentOffsetEnd is the content offset of the parent aNewFocus and is specified different
   *                           when you need to select to and include both start and end points
   *  @param aContinueSelection is the flag that tells the selection to keep the old anchor point or not.
   *  @param aMultipleSelection will tell the frame selector to replace /or not the old selection. 
   *         cannot coexist with aContinueSelection
   *  @param aHint will tell the selection which direction geometrically to actually show the caret on. 
   *         1 = end of this line 0 = beginning of this line
   */
  NS_IMETHOD HandleClick(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset , 
                       PRBool aContinueSelection, PRBool aMultipleSelection, PRBool aHint) = 0; 

  /** HandleDrag extends the selection to contain the frame closest to aPoint.
   *  @param aPresContext is the context to use when figuring out what frame contains the point.
   *  @param aFrame is the parent of all frames to use when searching for the closest frame to the point.
   *  @param aPoint is relative to aFrame
   */
  NS_IMETHOD HandleDrag(nsPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint) = 0;

  /** HandleTableSelection will set selection to a table, cell, etc
   *   depending on information contained in aFlags
   *  @param aParentContent is the paretent of either a table or cell that user clicked or dragged the mouse in
   *  @param aContentOffset is the offset of the table or cell
   *  @param aTarget indicates what to select (defined in nsISelectionPrivate.idl/nsISelectionPrivate.h):
   *    TABLESELECTION_CELL      We should select a cell (content points to the cell)
   *    TABLESELECTION_ROW       We should select a row (content points to any cell in row)
   *    TABLESELECTION_COLUMN    We should select a row (content points to any cell in column)
   *    TABLESELECTION_TABLE     We should select a table (content points to the table)
   *    TABLESELECTION_ALLCELLS  We should select all cells (content points to any cell in table)
   *  @param aMouseEvent         passed in so we can get where event occurred and what keys are pressed
   */
  NS_IMETHOD HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRInt32 aTarget, nsMouseEvent *aMouseEvent) = 0;

  /** StartAutoScrollTimer is responsible for scrolling views so that aPoint is always
   *  visible, and for selecting any frame that contains aPoint. The timer will also reset
   *  itself to fire again if we have not scrolled to the end of the document.
   *  @param aPresContext is the context to use when figuring out what frame contains the point.
   *  @param aView is view to use when searching for the closest frame to the point,
   *  which is the view that is capturing the mouse
   *  @param aPoint is relative to the view.
   *  @param aDelay is the timer's interval.
   */
  NS_IMETHOD StartAutoScrollTimer(nsPresContext *aPresContext, nsIView* aFrame, nsPoint& aPoint, PRUint32 aDelay) = 0;

  /** StopAutoScrollTimer stops any active auto scroll timer.
   */
  NS_IMETHOD StopAutoScrollTimer() = 0;

  /** EnableFrameNotification
   *  mutch like start batching, except all dirty calls are ignored. no notifications will go 
   *  out until enableNotifications with a PR_TRUE is called
   */
  NS_IMETHOD EnableFrameNotification(PRBool aEnable) = 0;

  /** Lookup Selection
   *  returns in frame coordinates the selection beginning and ending with the type of selection given
   * @param aContent is the content asking
   * @param aContentOffset is the starting content boundary
   * @param aContentLength is the length of the content piece asking
   * @param aReturnDetails linkedlist of return values for the selection. 
   * @param aSlowCheck will check using slow method with no shortcuts
   */
  NS_IMETHOD LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails, PRBool aSlowCheck) = 0;

  /** SetMouseDownState(PRBool);
   *  sets the mouse state to aState for resons of drag state.
   * @param aState is the new state of mousedown
   */
  NS_IMETHOD SetMouseDownState(PRBool aState)=0;

  /** GetMouseDownState(PRBool *);
   *  gets the mouse state to aState for resons of drag state.
   * @param aState will hold the state of mousedown
   */
  NS_IMETHOD GetMouseDownState(PRBool *aState)=0;

  /**
    if we are in table cell selection mode. aka ctrl click in table cell
   */
  NS_IMETHOD GetTableCellSelection(PRBool *aState)=0;

  /** GetSelection
   * no query interface for selection. must use this method now.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  NS_IMETHOD GetSelection(SelectionType aSelectionType, nsISelection **aSelection)=0;

  /**
   * ScrollSelectionIntoView scrolls a region of the selection,
   * so that it is visible in the scrolled view.
   *
   * @param aType the selection to scroll into view.
   * @param aRegion the region inside the selection to scroll into view.
   * @param aIsSynchronous when PR_TRUE, scrolls the selection into view
   * at some point after the method returns.request which is processed
   */
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aSelectionType, SelectionRegion aRegion, PRBool aIsSynchronous)=0;

  /** RepaintSelection repaints the selected frames that are inside the selection
   *  specified by aSelectionType.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  NS_IMETHOD RepaintSelection(nsPresContext* aPresContext, SelectionType aSelectionType)=0;

  /** GetFrameForNodeOffset given a node and its child offset, return the nsIFrame and
   *  the offset into that frame. 
   * @param aNode input parameter for the node to look at
   * @param aOffset offset into above node.
   * @param aReturnFrame will contain the return frame. MUST NOT BE NULL or will return error
   * @param aReturnOffset will contain offset into frame.
   */
  NS_IMETHOD GetFrameForNodeOffset(nsIContent *aNode, PRInt32 aOffset, HINT aHint, nsIFrame **aReturnFrame, PRInt32 *aReturnOffset)=0;

  NS_IMETHOD GetHint(HINT *aHint)=0;
  NS_IMETHOD SetHint(HINT aHint)=0;

  /** CharacterMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one character left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend)=0;

  /** WordMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one word left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend)=0;

  /** LineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one line up or down.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD LineMove(PRBool aForward, PRBool aExtend)=0;

  /** IntraLineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move to beginning or end of line
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD IntraLineMove(PRBool aForward, PRBool aExtend)=0;

  /**
   * Scrolling then moving caret placement code in common to text areas and 
   * content areas should be located in the implementer
   * This method will accept the following parameters and perform the scroll 
   * and caret movement.  It remains for the caller to call the final 
   * ScrollCaretIntoView if that called wants to be sure the caret is always
   * visible.
   *
   * @param aForward if PR_TRUE, scroll forward if not scroll backward
   *
   * @param aExtend  if PR_TRUE, extend selection to the new point
   *
   * @param aScrollableView the view that needs the scrolling
   *
   * @param aFrameSel the nsIFrameSelection of the caller.
   *
   * @return  always NS_OK
   */
  NS_IMETHOD CommonPageMove(PRBool aForward, 
                            PRBool aExtend, 
                            nsIScrollableView *aScrollableView, 
                            nsIFrameSelection *aFrameSel)=0;

  /** Select All will generally be called from the nsiselectioncontroller implementations.
   *  it will select the whole doc
   */
  NS_IMETHOD SelectAll()=0;

  /** Sets/Gets The display selection enum.
   */
  NS_IMETHOD SetDisplaySelection(PRInt16 aState)=0;
  NS_IMETHOD GetDisplaySelection(PRInt16 *aState)=0;

  /** Allow applications to specify how we should place the caret
   *  when the user clicks over an existing selection. A aDelay
   *  value of PR_TRUE means delay clearing the selection and
   *  placing the caret until MouseUp, when the user clicks over
   *  an existing selection. This is especially useful when applications
   *  want to support Drag & Drop of the current selection. A value
   *  of PR_FALSE means place the caret immediately. If the application
   *  never calls this method, the nsIFrameSelection implementation
   *  assumes the default value is PR_TRUE.
   * @param aDelay PR_TRUE if we should delay caret placement.
   */
  NS_IMETHOD SetDelayCaretOverExistingSelection(PRBool aDelay)=0;

  /** Get the current delay caret setting. If aDelay contains
   *  a return value of PR_TRUE, the caret is placed on MouseUp
   *  when clicking over an existing selection. If PR_FALSE,
   *  the selection is cleared and caret is placed immediately
   *  in all cases.
   * @param aDelay will contain the return value.
   */
  NS_IMETHOD GetDelayCaretOverExistingSelection(PRBool *aDelay)=0;

  /** If we are delaying caret placement til MouseUp (see
   *  Set/GetDelayCaretOverExistingSelection()), this method
   *  can be used to store the data received during the MouseDown
   *  so that we can place the caret during the MouseUp event.
   * @aMouseEvent the event received by the selection MouseDown
   *  handling method. A NULL value can be use to tell this method
   *  that any data is storing is no longer valid.
   */
  NS_IMETHOD SetDelayedCaretData(nsMouseEvent *aMouseEvent)=0;

  /** Get the delayed MouseDown event data necessary to place the
   *  caret during MouseUp processing.
   * @aMouseEvent will contain a pointer to the event received
   *  by the selection during MouseDown processing. It can be NULL
   *  if the data is no longer valid.
   */
  NS_IMETHOD GetDelayedCaretData(nsMouseEvent **aMouseEvent)=0;


  /** Get the content node that limits the selection
   *  When searching up a nodes for parents, as in a text edit field
   *    in an browser page, we must stop at this node else we reach into the 
   *    parent page, which is very bad!
   */
  NS_IMETHOD GetLimiter(nsIContent **aLimiterContent)=0;

  /** This will tell the frame selection that a double click has been pressed 
   *  so it can track abort future drags if inside the same selection
   *  @aDoubleDown has the double click down happened
   */
  NS_IMETHOD SetMouseDoubleDown(PRBool aDoubleDown)=0;

  /** This will return whether the double down flag was set.
   *  @aDoubleDown is the return boolean value
   */
  NS_IMETHOD GetMouseDoubleDown(PRBool *aDoubleDown)=0;

  /**
   * MaintainSelection will track the current selection as being "sticky".
   * Dragging or extending selection will never allow for a subset
   * (or the whole) of the maintained selection to become unselected.
   * Primary use: double click selecting then dragging on second click
   */
  NS_IMETHOD MaintainSelection()=0;

#ifdef IBMBIDI
  /** GetPrevNextBidiLevels will return the frames and associated Bidi levels of the characters
   *   logically before and after a (collapsed) selection.
   *  @param aPresContext is the context to use
   *  @param aNode is the node containing the selection
   *  @param aContentOffset is the offset of the selection in the node
   *  @param aJumpLines If PR_TRUE, look across line boundaries.
   *                    If PR_FALSE, behave as if there were base-level frames at line edges.  
   *  @param aPrevFrame will hold the frame of the character before the selection
   *  @param aNextFrame will hold the frame of the character after the selection
   *  @param aPrevLevel will hold the Bidi level of the character before the selection
   *  @param aNextLevel will hold the Bidi level of the character after the selection
   *
   *  At the beginning and end of each line there is assumed to be a frame with Bidi level equal to the
   *   paragraph embedding level. In these cases aPrevFrame and aNextFrame respectively will return nsnull.
   */
  NS_IMETHOD GetPrevNextBidiLevels(nsPresContext *aPresContext, nsIContent *aNode, PRUint32 aContentOffset, PRBool aJumpLines,
                                   nsIFrame **aPrevFrame, nsIFrame **aNextFrame, PRUint8 *aPrevLevel, PRUint8 *aNextLevel)=0;

  /** GetFrameFromLevel will scan in a given direction
   *   until it finds a frame with a Bidi level less than or equal to a given level.
   *   It will return the last frame before this.
   *  @param aPresContext is the context to use
   *  @param aFrameIn is the frame to start from
   *  @param aDirection is the direction to scan
   *  @param aBidiLevel is the level to search for
   *  @param aFrameOut will hold the frame returned
   */
  NS_IMETHOD GetFrameFromLevel(nsPresContext *aPresContext, nsIFrame *aFrameIn, nsDirection aDirection, PRUint8 aBidiLevel,
                               nsIFrame **aFrameOut)=0;

#endif // IBMBIDI
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFrameSelection, NS_IFRAMESELECTION_IID)

#endif /* nsIFrameSelection_h___ */
