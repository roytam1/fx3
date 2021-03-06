/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
 */

#ifndef nsFrameSelection_h___
#define nsFrameSelection_h___
 
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsISelectionController.h"

#include "nsITableLayout.h"
#include "nsITableCellLayout.h"
#include "nsIDOMElement.h"
#include "nsGUIEvent.h"

// IID for the nsFrameSelection interface
// cdfa6280-eba6-4938-9406-427818da8ce3
#define NS_FRAME_SELECTION_IID      \
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

class nsIPresShell;

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

struct nsPrevNextBidiLevels
{
  void SetData(nsIFrame* aFrameBefore,
               nsIFrame* aFrameAfter,
               PRUint8 aLevelBefore,
               PRUint8 aLevelAfter)
  {
    mFrameBefore = aFrameBefore;
    mFrameAfter = aFrameAfter;
    mLevelBefore = aLevelBefore;
    mLevelAfter = aLevelAfter;
  }
  nsIFrame* mFrameBefore;
  nsIFrame* mFrameAfter;
  PRUint8 mLevelBefore;
  PRUint8 mLevelAfter;
};

class nsTypedSelection;
class nsIScrollableView;

class nsFrameSelection : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_FRAME_SELECTION_IID)
  enum HINT { HINTLEFT = 0, HINTRIGHT = 1};  //end of this line or beginning of next
  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_ISUPPORTS

  /** Init will initialize the frame selector with the necessary pres shell to 
   *  be used by most of the methods
   *  @param aShell is the parameter to be used for most of the other calls for callbacks etc
   *  @param aLimiter limits the selection to nodes with aLimiter parents
   */
  void Init(nsIPresShell *aShell, nsIContent *aLimiter);

  /* SetScrollableView sets the scroll view
   *  @param aScrollView is the scroll view for this selection.
   */
  void SetScrollableView(nsIScrollableView *aScrollView) { mScrollView = aScrollView; }

  /* GetScrollableView gets the current scroll view
   */
  nsIScrollableView* GetScrollableView() { return mScrollView; }

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
  nsresult HandleClick(nsIContent *aNewFocus,
                       PRUint32 aContentOffset,
                       PRUint32 aContentEndOffset,
                       PRBool aContinueSelection,
                       PRBool aMultipleSelection,
                       PRBool aHint);

  /** HandleDrag extends the selection to contain the frame closest to aPoint.
   *  @param aPresContext is the context to use when figuring out what frame contains the point.
   *  @param aFrame is the parent of all frames to use when searching for the closest frame to the point.
   *  @param aPoint is relative to aFrame
   */
  void HandleDrag(nsIFrame *aFrame, nsPoint aPoint);

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
  nsresult HandleTableSelection(nsIContent *aParentContent,
                                PRInt32 aContentOffset,
                                PRInt32 aTarget,
                                nsMouseEvent *aMouseEvent);

  /** StartAutoScrollTimer is responsible for scrolling views so that aPoint is always
   *  visible, and for selecting any frame that contains aPoint. The timer will also reset
   *  itself to fire again if we have not scrolled to the end of the document.
   *  @param aPresContext is the context to use when figuring out what frame contains the point.
   *  @param aView is view to use when searching for the closest frame to the point,
   *  which is the view that is capturing the mouse
   *  @param aPoint is relative to the view.
   *  @param aDelay is the timer's interval.
   */
  nsresult StartAutoScrollTimer(nsIView *aView,
                                nsPoint aPoint,
                                PRUint32 aDelay);

  /** StopAutoScrollTimer stops any active auto scroll timer.
   */
  void StopAutoScrollTimer();

  /** Lookup Selection
   *  returns in frame coordinates the selection beginning and ending with the type of selection given
   * @param aContent is the content asking
   * @param aContentOffset is the starting content boundary
   * @param aContentLength is the length of the content piece asking
   * @param aReturnDetails linkedlist of return values for the selection. 
   * @param aSlowCheck will check using slow method with no shortcuts
   */
  SelectionDetails* LookUpSelection(nsIContent *aContent,
                                    PRInt32 aContentOffset,
                                    PRInt32 aContentLength,
                                    PRBool aSlowCheck);

  /** SetMouseDownState(PRBool);
   *  sets the mouse state to aState for resons of drag state.
   * @param aState is the new state of mousedown
   */
  void SetMouseDownState(PRBool aState);

  /** GetMouseDownState(PRBool *);
   *  gets the mouse state to aState for resons of drag state.
   * @param aState will hold the state of mousedown
   */
  PRBool GetMouseDownState() { return mMouseDownState; }

  /**
    if we are in table cell selection mode. aka ctrl click in table cell
   */
  PRBool GetTableCellSelection() { return mSelectingTableCellMode != 0; }
  void ClearTableCellSelection(){ mSelectingTableCellMode = 0; }

  /** GetSelection
   * no query interface for selection. must use this method now.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  nsISelection* GetSelection(SelectionType aType);

  /**
   * ScrollSelectionIntoView scrolls a region of the selection,
   * so that it is visible in the scrolled view.
   *
   * @param aType the selection to scroll into view.
   * @param aRegion the region inside the selection to scroll into view.
   * @param aIsSynchronous when PR_TRUE, scrolls the selection into view
   * at some point after the method returns.request which is processed
   */
  nsresult ScrollSelectionIntoView(SelectionType aType,
                                   SelectionRegion aRegion,
                                   PRBool aIsSynchronous);

  /** RepaintSelection repaints the selected frames that are inside the selection
   *  specified by aSelectionType.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  nsresult RepaintSelection(SelectionType aType);

  /** GetFrameForNodeOffset given a node and its child offset, return the nsIFrame and
   *  the offset into that frame. 
   * @param aNode input parameter for the node to look at
   * @param aOffset offset into above node.
   * @param aReturnOffset will contain offset into frame.
   */
  nsIFrame* GetFrameForNodeOffset(nsIContent *aNode,
                                  PRInt32     aOffset,
                                  HINT        aHint,
                                  PRInt32    *aReturnOffset);

  /**
   * Scrolling then moving caret placement code in common to text areas and 
   * content areas should be located in the implementer
   * This method will accept the following parameters and perform the scroll 
   * and caret movement.  It remains for the caller to call the final 
   * ScrollCaretIntoView if that called wants to be sure the caret is always
   * visible.
   *
   * @param aForward if PR_TRUE, scroll forward if not scroll backward
   * @param aExtend  if PR_TRUE, extend selection to the new point
   * @param aScrollableView the view that needs the scrolling
   */
  void CommonPageMove(PRBool aForward,
                      PRBool aExtend,
                      nsIScrollableView *aScrollableView);

  void SetHint(HINT aHintRight) { mHint = aHintRight; }
  HINT GetHint() { return mHint; }

  /** CharacterMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one character left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  nsresult CharacterMove(PRBool aForward, PRBool aExtend);

  /** WordMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one word left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  nsresult WordMove(PRBool aForward, PRBool aExtend);

  /** LineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one line up or down.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  nsresult LineMove(PRBool aForward, PRBool aExtend);

  /** IntraLineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move to beginning or end of line
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  nsresult IntraLineMove(PRBool aForward, PRBool aExtend); 

  /** Select All will generally be called from the nsiselectioncontroller implementations.
   *  it will select the whole doc
   */
  nsresult SelectAll();

  /** Sets/Gets The display selection enum.
   */
  void SetDisplaySelection(PRInt16 aState) { mDisplaySelection = aState; }
  PRInt16 GetDisplaySelection() { return mDisplaySelection; }

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
  void SetDelayCaretOverExistingSelection(PRBool aDelay);

  /** Get the current delay caret setting.
   */
  PRBool GetDelayCaretOverExistingSelection() { return mDelayCaretOverExistingSelection; }

  /** If we are delaying caret placement til MouseUp (see
   *  Set/GetDelayCaretOverExistingSelection()), this method
   *  can be used to store the data received during the MouseDown
   *  so that we can place the caret during the MouseUp event.
   * @aMouseEvent the event received by the selection MouseDown
   *  handling method. A NULL value can be use to tell this method
   *  that any data is storing is no longer valid.
   */
  void SetDelayedCaretData(nsMouseEvent *aMouseEvent);

  /** Get the delayed MouseDown event data necessary to place the
   *  caret during MouseUp processing.
   * @return a pointer to the event received
   *  by the selection during MouseDown processing. It can be NULL
   *  if the data is no longer valid.
   */
  nsMouseEvent* GetDelayedCaretData();

  /** Get the content node that limits the selection
   *  When searching up a nodes for parents, as in a text edit field
   *    in an browser page, we must stop at this node else we reach into the 
   *    parent page, which is very bad!
   */
  nsIContent* GetLimiter() { return mLimiter; }

  /** This will tell the frame selection that a double click has been pressed 
   *  so it can track abort future drags if inside the same selection
   *  @aDoubleDown has the double click down happened
   */
  void SetMouseDoubleDown(PRBool aDoubleDown) { mMouseDoubleDownState = aDoubleDown; }
  
  /** This will return whether the double down flag was set.
   *  @return whether the double down flag was set
   */
  PRBool GetMouseDoubleDown() { return mMouseDoubleDownState; }

  /** GetPrevNextBidiLevels will return the frames and associated Bidi levels of the characters
   *   logically before and after a (collapsed) selection.
   *  @param aNode is the node containing the selection
   *  @param aContentOffset is the offset of the selection in the node
   *  @param aJumpLines If PR_TRUE, look across line boundaries.
   *                    If PR_FALSE, behave as if there were base-level frames at line edges.  
   *
   *  @return A struct holding the before/after frame and the before/after level.
   *
   *  At the beginning and end of each line there is assumed to be a frame with
   *   Bidi level equal to the paragraph embedding level.
   *  In these cases the before frame and after frame respectively will be 
   *   nsnull.
   *
   *  This method is virtual since it gets called from outside of layout. 
   */
  virtual nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent *aNode,
                                                     PRUint32 aContentOffset,
                                                     PRBool aJumpLines);

  /** GetFrameFromLevel will scan in a given direction
   *   until it finds a frame with a Bidi level less than or equal to a given level.
   *   It will return the last frame before this.
   *  @param aPresContext is the context to use
   *  @param aFrameIn is the frame to start from
   *  @param aDirection is the direction to scan
   *  @param aBidiLevel is the level to search for
   *  @param aFrameOut will hold the frame returned
   */
  nsresult GetFrameFromLevel(nsIFrame *aFrameIn,
                             nsDirection aDirection,
                             PRUint8 aBidiLevel,
                             nsIFrame **aFrameOut);

  /**
   * MaintainSelection will track the current selection as being "sticky".
   * Dragging or extending selection will never allow for a subset
   * (or the whole) of the maintained selection to become unselected.
   * Primary use: double click selecting then dragging on second click
   */
  nsresult MaintainSelection();


  nsFrameSelection();
  virtual ~nsFrameSelection();

  void StartBatchChanges();
  void EndBatchChanges();
  nsresult DeleteFromDocument();

  nsIPresShell *GetShell() {return mShell;}

private:
  nsresult TakeFocus(nsIContent *aNewFocus,
                     PRUint32 aContentOffset,
                     PRUint32 aContentEndOffset, 
                     PRBool aContinueSelection,
                     PRBool aMultipleSelection);

  void BidiLevelFromMove(nsIPresShell* aPresShell,
                         nsIContent *aNode,
                         PRUint32 aContentOffset,
                         PRUint32 aKeycode,
                         HINT aHint);
  void BidiLevelFromClick(nsIContent *aNewFocus, PRUint32 aContentOffset);
  nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent *aNode,
                                             PRUint32 aContentOffset,
                                             HINT aHint,
                                             PRBool aJumpLines);
#ifdef VISUALSELECTION
  NS_IMETHOD VisualSelectFrames(nsIFrame* aCurrentFrame,
                                nsPeekOffsetStruct aPos);
  NS_IMETHOD VisualSequence(nsIFrame* aSelectFrame,
                            nsIFrame* aCurrentFrame,
                            nsPeekOffsetStruct* aPos,
                            PRBool* aNeedVisualSelection);
  NS_IMETHOD SelectToEdge(nsIFrame *aFrame,
                          nsIContent *aContent,
                          PRInt32 aOffset,
                          PRInt32 aEdge,
                          PRBool aMultipleSelection);
  NS_IMETHOD SelectLines(nsDirection aSelectionDirection,
                         nsIDOMNode *aAnchorNode,
                         nsIFrame* aAnchorFrame,
                         PRInt32 aAnchorOffset,
                         nsIDOMNode *aCurrentNode,
                         nsIFrame* aCurrentFrame,
                         PRInt32 aCurrentOffset,
                         nsPeekOffsetStruct aPos);
#endif // VISUALSELECTION

  PRBool AdjustForMaintainedSelection(nsIContent *aContent, PRInt32 aOffset);

// post and pop reasons for notifications. we may stack these later
  void    PostReason(PRInt16 aReason) { mSelectionChangeReason = aReason; }
  PRInt16 PopReason()
  {
    PRInt16 retval = mSelectionChangeReason;
    mSelectionChangeReason = 0;
    return retval;
  }

  friend class nsTypedSelection; 
#ifdef DEBUG
  void printSelection();       // for debugging
#endif /* DEBUG */

  void ResizeBuffer(PRUint32 aNewBufSize);
/*HELPER METHODS*/
  nsresult     MoveCaret(PRUint32 aKeycode, PRBool aContinueSelection, nsSelectionAmount aAmount);

  nsresult     FetchDesiredX(nscoord &aDesiredX); //the x position requested by the Key Handling for up down
  void         InvalidateDesiredX(); //do not listen to mDesiredX you must get another.
  void         SetDesiredX(nscoord aX); //set the mDesiredX

  nsresult     GetRootForContentSubtree(nsIContent *aContent, nsIContent **aParent);
  nsresult     ConstrainFrameAndPointToAnchorSubtree(nsIFrame *aFrame, nsPoint& aPoint, nsIFrame **aRetFrame, nsPoint& aRetPoint);

  PRUint32     GetBatching(){return mBatching;}
  PRBool       GetNotifyFrames(){return mNotifyFrames;}
  void         SetDirty(PRBool aDirty=PR_TRUE){if (mBatching) mChangesDuringBatching = aDirty;}

  nsresult     NotifySelectionListeners(SelectionType aType);     // add parameters to say collapsed etc?

  nsTypedSelection *mDomSelections[nsISelectionController::NUM_SELECTIONTYPES];

  // Table selection support.
  // Interfaces that let us get info based on cellmap locations
  nsITableLayout* GetTableLayout(nsIContent *aTableContent);
  nsITableCellLayout* GetCellLayout(nsIContent *aCellContent);

  nsresult SelectBlockOfCells(nsIContent *aStartNode, nsIContent *aEndNode);
  nsresult SelectRowOrColumn(nsIContent *aCellContent, PRUint32 aTarget);
  nsresult GetCellIndexes(nsIContent *aCell, PRInt32 &aRowIndex, PRInt32 &aColIndex);

  nsresult GetFirstSelectedCellAndRange(nsIDOMNode **aCell, nsIDOMRange **aRange);
  nsresult GetNextSelectedCellAndRange(nsIDOMNode **aCell, nsIDOMRange **aRange);
  nsresult GetFirstCellNodeInRange(nsIDOMRange *aRange, nsIDOMNode **aCellNode);
  // aTableNode may be null if table isn't needed to be returned
  PRBool   IsInSameTable(nsIContent *aContent1, nsIContent *aContent2, nsIContent **aTableNode);
  nsresult GetParentTable(nsIContent *aCellNode, nsIContent **aTableNode);
  nsresult SelectCellElement(nsIDOMElement* aCellElement);
  nsresult CreateAndAddRange(nsIDOMNode *aParentNode, PRInt32 aOffset);
  nsresult ClearNormalSelection();

  nsCOMPtr<nsIDOMNode> mCellParent; //used to snap to table selection
  nsCOMPtr<nsIContent> mStartSelectedCell;
  nsCOMPtr<nsIContent> mEndSelectedCell;
  nsCOMPtr<nsIContent> mAppendStartSelectedCell;
  nsCOMPtr<nsIContent> mUnselectCellOnMouseUp;
  PRInt32  mSelectingTableCellMode;
  PRInt32  mSelectedCellIndex;

  // maintain selection
  nsCOMPtr<nsIDOMRange> mMaintainRange;

  //batching
  PRInt32 mBatching;
    
  nsIContent *mLimiter;     //limit selection navigation to a child of this node.
  nsIPresShell *mShell;

  PRInt16 mSelectionChangeReason; // reason for notifications of selection changing
  PRInt16 mDisplaySelection; //for visual display purposes.

  HINT  mHint;   //hint to tell if the selection is at the end of this line or beginning of next

  PRInt32 mDesiredX;
  nsIScrollableView *mScrollView;

  nsMouseEvent mDelayedMouseEvent;

  PRPackedBool mDelayCaretOverExistingSelection;
  PRPackedBool mDelayedMouseEventValid;

  PRPackedBool mChangesDuringBatching;
  PRPackedBool mNotifyFrames;
  PRPackedBool mIsEditor;
  PRPackedBool mDragSelectingCells;
  PRPackedBool mMouseDownState;   //for drag purposes
  PRPackedBool mMouseDoubleDownState; //has the doubleclick down happened
  PRPackedBool mDesiredXSet;

  PRInt8 mCaretMovementStyle;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsFrameSelection, NS_FRAME_SELECTION_IID)

#endif /* nsFrameSelection_h___ */
