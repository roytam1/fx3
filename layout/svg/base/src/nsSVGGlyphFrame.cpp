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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsFrame.h"
#include "nsISVGRendererGlyphGeometry.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsISVGRenderer.h"
#include "nsISVGGlyphGeometrySource.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsITextContent.h"
#include "nsISVGChildFrame.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGTextFrame.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGTextContainerFrame.h"
#include "nsSVGGradient.h"
#include "nsSVGPattern.h"
#include "nsISVGValueUtils.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prdtoa.h"
#include "nsIDOMSVGRect.h"
#include "nsILookAndFeel.h"
#include "nsTextFragment.h"
#include "nsSVGRect.h"
#include "nsSVGPoint.h"
#include "nsSVGAtoms.h"
#include "nsIViewManager.h"
#include "nsINameSpaceManager.h"
#include "nsContainerFrame.h"
#include "nsLayoutAtoms.h"
#include "nsSVGUtils.h"
#include "nsISVGPathFlatten.h"

typedef nsFrame nsSVGGlyphFrameBase;

class nsSVGGlyphFrame : public nsSVGGlyphFrameBase,
                        public nsISVGValueObserver,
                        public nsSupportsWeakReference,
                        public nsISVGGlyphGeometrySource, // : nsISVGGlyphMetricsSource : nsISVGGeometrySource
                        public nsISVGGlyphFragmentLeaf, // : nsISVGGlyphFragmentNode
                        public nsISVGChildFrame
{
protected:
  friend nsIFrame*
  NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                      nsIFrame* parentFrame, nsStyleContext* aContext);
  nsSVGGlyphFrame(nsStyleContext* aContext);
  virtual ~nsSVGGlyphFrame();

public:
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsIFrame interface:
  NS_IMETHOD
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD  CharacterDataChanged(nsPresContext*  aPresContext,
                                   nsIContent*     aChild,
                                   PRBool          aAppend);

  NS_IMETHOD  DidSetStyleContext();

  NS_IMETHOD  SetSelected(nsPresContext* aPresContext,
                          nsIDOMRange*    aRange,
                          PRBool          aSelected,
                          nsSpread        aSpread);
  NS_IMETHOD  GetSelected(PRBool *aSelected) const;
  NS_IMETHOD  IsSelectable(PRBool* aIsSelectable, PRUint8* aSelectStyle) const;

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgGlyphFrame
   */
  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGlyph"), aResult);
  }
#endif

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas);
  NS_IMETHOD GetFrameForPointSVG(float x, float y, nsIFrame** hit);
  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>) GetCoveredRegion();
  NS_IMETHOD InitialUpdate();
  NS_IMETHOD NotifyCanvasTMChanged(PRBool suppressInvalidation);
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate) { return NS_OK; }
  NS_IMETHOD SetOverrideCTM(nsIDOMSVGMatrix *aCTM) { return NS_ERROR_FAILURE; }
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISVGGeometrySource interface: 
  NS_DECL_NSISVGGEOMETRYSOURCE

  // nsISVGGlyphMetricsSource interface:
  NS_DECL_NSISVGGLYPHMETRICSSOURCE

  // nsISVGGlyphGeometrySource interface:
  NS_DECL_NSISVGGLYPHGEOMETRYSOURCE

  // nsISVGGlyphFragmentLeaf interface:
  NS_IMETHOD_(void) SetGlyphPosition(float x, float y);
  NS_IMETHOD_(float) GetGlyphPositionX();
  NS_IMETHOD_(float) GetGlyphPositionY();  
  NS_IMETHOD GetGlyphMetrics(nsISVGRendererGlyphMetrics** metrics);
  NS_IMETHOD_(PRBool) IsStartOfChunk(); // == is new absolutely positioned chunk.
  NS_IMETHOD_(void) GetAdjustedPosition(/* inout */ float &x, /* inout */ float &y);
  NS_IMETHOD_(PRUint32) GetNumberOfChars();
  NS_IMETHOD_(PRUint32) GetCharNumberOffset();

  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetX();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetY();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDx();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDy();
  NS_IMETHOD_(PRUint16) GetTextAnchor();
  NS_IMETHOD_(PRBool) IsAbsolutelyPositioned();

  // nsISVGGlyphFragmentNode interface:
  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetFirstGlyphFragment();
  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetNextGlyphFragment();
  NS_IMETHOD_(PRUint32) BuildGlyphFragmentTree(PRUint32 charNum, PRBool lastBranch);
  NS_IMETHOD_(void) NotifyMetricsSuspended();
  NS_IMETHOD_(void) NotifyMetricsUnsuspended();
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeSuspended();
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeUnsuspended();
  
protected:
  void UpdateGeometry(PRUint32 flags, PRBool bRedraw,
                      PRBool suppressInvalidation);
  void UpdateMetrics(PRUint32 flags);
  void UpdateFragmentTree();
  nsISVGTextFrame *GetTextFrame();
  NS_IMETHOD Update(PRUint32 aFlags);
  
  nsCOMPtr<nsISVGRendererGlyphGeometry> mGeometry;
  nsCOMPtr<nsISVGRendererGlyphMetrics> mMetrics;
  float mX, mY;
  PRUint32 mGeometryUpdateFlags;
  PRUint32 mMetricsUpdateFlags;
  PRUint32 mCharOffset;
  nsString mCharacterData;
  nsISVGGradient* mFillGradient;
  nsISVGGradient* mStrokeGradient;
  nsISVGPattern* mFillPattern;
  nsISVGPattern* mStrokePattern;
  PRPackedBool mFragmentTreeDirty;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parentFrame, nsStyleContext* aContext)
{
#ifdef DEBUG
  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextContainerFrame *text_container;
  parentFrame->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void**)&text_container);
  NS_ASSERTION(text_container, "trying to construct an SVGGlyphFrame for an invalid container");
  
  nsCOMPtr<nsITextContent> tc = do_QueryInterface(aContent);
  NS_ASSERTION(tc, "trying to construct an SVGGlyphFrame for wrong content element");
#endif

  return new (aPresShell) nsSVGGlyphFrame(aContext);
}

nsSVGGlyphFrame::nsSVGGlyphFrame(nsStyleContext* aContext)
    : nsSVGGlyphFrameBase(aContext),
      mGeometryUpdateFlags(0), mMetricsUpdateFlags(0),
      mCharOffset(0),
      mFillGradient(nsnull), mStrokeGradient(nsnull),
      mFillPattern(nsnull), mStrokePattern(nsnull),
      mFragmentTreeDirty(PR_FALSE)
{
}

nsSVGGlyphFrame::~nsSVGGlyphFrame()
{
  if (mFillGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillGradient);
  }
  if (mStrokeGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokeGradient);
  }
  if (mFillPattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillPattern);
  }
  if (mStrokePattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokePattern);
  }
}


//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGlyphFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphMetricsSource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentLeaf)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentNode)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGlyphFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
//  rv = nsSVGGlyphFrameBase::Init(aPresContext, aContent, aParent, aPrevInFlow);

  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  if (mContent) {
    mContent->SetMayHaveFrame(PR_TRUE);
  }
  
  // construct our glyphmetrics & glyphgeometry objects:
  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("No outerSVGFrame");
    DidSetStyleContext();
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  if (renderer) {
    renderer->CreateGlyphMetrics(this, getter_AddRefs(mMetrics));
    renderer->CreateGlyphGeometry(this, getter_AddRefs(mGeometry));
  }

  DidSetStyleContext();

  if (!renderer || !mMetrics || !mGeometry)
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::CharacterDataChanged(nsPresContext*  aPresContext,
                                      nsIContent*     aChild,
                                      PRBool          aAppend)
{
	return Update(nsISVGGeometrySource::UPDATEMASK_ALL);
}

NS_IMETHODIMP
nsSVGGlyphFrame::Update(PRUint32 aFlags)
{
#ifdef DEBUG
//  printf("** nsSVGGlyphFrame::Update\n");
#endif
  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("No outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  
  outerSVGFrame->SuspendRedraw();
  UpdateFragmentTree();
  UpdateMetrics(aFlags);
  UpdateGeometry(aFlags, PR_TRUE, PR_FALSE);
  outerSVGFrame->UnsuspendRedraw();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::DidSetStyleContext()
{
  // One of the styles that might have been changed are the urls that
  // point to gradients, etc.  Drop our cached values to those
  if (mFillGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillGradient);
    mFillGradient = nsnull;
  }
  if (mStrokeGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokeGradient);
    mStrokeGradient = nsnull;
  }
  if (mFillPattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillPattern);
    mFillPattern = nsnull;
  }
  if (mStrokePattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokePattern);
    mStrokePattern = nsnull;
  }

  return CharacterDataChanged(nsnull, nsnull, PR_FALSE);
}

NS_IMETHODIMP
nsSVGGlyphFrame::SetSelected(nsPresContext* aPresContext,
                             nsIDOMRange*    aRange,
                             PRBool          aSelected,
                             nsSpread        aSpread)
{
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::SetSelected()\n", this);
#endif
//  return nsSVGGlyphFrameBase::SetSelected(aPresContext, aRange, aSelected, aSpread);

  // check whether style allows selection
  PRBool  selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return NS_OK;
  
  if ( aSelected ){
    mState |=  NS_FRAME_SELECTED_CONTENT;
  }
  else
    mState &= ~NS_FRAME_SELECTED_CONTENT;

  UpdateGeometry(nsISVGGlyphGeometrySource::UPDATEMASK_HIGHLIGHT |
                 nsISVGGlyphGeometrySource::UPDATEMASK_HAS_HIGHLIGHT,
                 PR_FALSE, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetSelected(PRBool *aSelected) const
{
  nsresult rv = nsSVGGlyphFrameBase::GetSelected(aSelected);
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::GetSelected()=%d\n", this, *aSelected);
#endif
  return rv;
}

NS_IMETHODIMP
nsSVGGlyphFrame::IsSelectable(PRBool* aIsSelectable,
                              PRUint8* aSelectStyle) const
{
  nsresult rv = nsSVGGlyphFrameBase::IsSelectable(aIsSelectable, aSelectStyle);
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::IsSelectable()=(%d,%d)\n", this, *aIsSelectable, aSelectStyle);
#endif
  return rv;
}

nsIAtom *
nsSVGGlyphFrame::GetType() const
{
  return nsLayoutAtoms::svgGlyphFrame;
}

PRBool
nsSVGGlyphFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~nsIFrame::eSVG);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGGlyphFrame::WillModifySVGObservable(nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGGlyphFrame::DidModifySVGObservable (nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  // Is this a gradient?
  nsISVGGradient *val;
  CallQueryInterface(observable, &val);

  if (val) {
    // Yes, we need to handle this differently
    if (mFillGradient == val) {
      if (aModType == nsISVGValue::mod_die) {
        mFillGradient = nsnull;
      }
      return Update(nsISVGGeometrySource::UPDATEMASK_FILL_PAINT);
    } else {
      // No real harm in assuming a stroke gradient at this point
      if (aModType == nsISVGValue::mod_die) {
        mStrokeGradient = nsnull;
      }
      return Update(nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT);
    }
  } else {
    nsISVGPattern *pval;
    CallQueryInterface(observable, &pval);
    if (pval) {
      // Handle Patterns
      if (mFillPattern == pval) {
        if (aModType == nsISVGValue::mod_die) {
          mFillPattern = nsnull;
        }
        return Update(nsISVGGeometrySource::UPDATEMASK_FILL_PAINT);
      } else {
        // Assume stroke pattern
        if (aModType == nsISVGValue::mod_die) {
          mStrokePattern = nsnull;
        }
        return Update(nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT);
      }
    } else {
      // No, all of our other observables update the canvastm by default
      return Update(nsISVGGeometrySource::UPDATEMASK_CANVAS_TM);
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
#ifdef DEBUG
  //printf("nsSVGGlyphFrame(%p)::Paint\n", this);
#endif
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  mGeometry->Render(canvas);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
  //printf("nsSVGGlyphFrame(%p)::GetFrameForPoint\n", this);
#endif
  // test for hit:
  *hit = nsnull;

  PRBool events = PR_FALSE;
  switch (GetStyleSVG()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
      if (GetStyleVisibility()->IsVisible() &&
          (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None ||
           GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None))
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (GetStyleVisibility()->IsVisible())
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None ||
          GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_FILL:
    case NS_STYLE_POINTER_EVENTS_STROKE:
    case NS_STYLE_POINTER_EVENTS_ALL:
      events = PR_TRUE;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }

  if (!events)
    return NS_OK;

  PRBool isHit;
  mGeometry->ContainsPoint(x, y, &isHit);
  if (isHit) 
    *hit = this;
  
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGGlyphFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *region = nsnull;
  if (mGeometry)
    mGeometry->GetCoveredRegion(&region);
  return region;
}

NS_IMETHODIMP
nsSVGGlyphFrame::InitialUpdate()
{
  return Update(nsISVGGeometrySource::UPDATEMASK_ALL);
}  

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  UpdateGeometry(nsISVGGeometrySource::UPDATEMASK_CANVAS_TM,
                 PR_TRUE, suppressInvalidation);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyRedrawSuspended()
{
  // XXX should we cache the fact that redraw is suspended?
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyRedrawUnsuspended()
{
  NS_ASSERTION(!mMetricsUpdateFlags, "dirty metrics in nsSVGGlyphFrame::NotifyRedrawUnsuspended");
  NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::NotifyRedrawUnsuspended");
    
  if (mGeometryUpdateFlags != 0) {
    nsCOMPtr<nsISVGRendererRegion> dirty_region;
    mGeometry->Update(mGeometryUpdateFlags, getter_AddRefs(dirty_region));
    if (dirty_region) {
      nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
      if (outerSVGFrame)
        outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);
    }
    mGeometryUpdateFlags = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  if (mGeometry)
    return mGeometry->GetBoundingBox(_retval);
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGGeometrySource methods:

/* [noscript] readonly attribute nsPresContext presContext; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetPresContext(nsPresContext * *aPresContext)
{
  // XXX gcc 3.2.2 requires the explicit 'nsSVGGlyphFrameBase::' qualification
  *aPresContext = nsSVGGlyphFrameBase::GetPresContext();
  NS_ADDREF(*aPresContext);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGMatrix canvasTM; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetCanvasTM(nsIDOMSVGMatrix * *aCTM)
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
  *aCTM = parentTM.get();
  NS_ADDREF(*aCTM);
  return NS_OK;
}

/* readonly attribute float strokeOpacity; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeOpacity(float *aStrokeOpacity)
{
  *aStrokeOpacity =
    GetStyleSVG()->mStrokeOpacity * GetStyleDisplay()->mOpacity;
  return NS_OK;
}

/* readonly attribute float strokeWidth; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeWidth(float *aStrokeWidth)
{
  *aStrokeWidth = 
    nsSVGUtils::CoordToFloat(nsSVGGlyphFrameBase::GetPresContext(),
                             mContent, GetStyleSVG()->mStrokeWidth);
  return NS_OK;
}

/* void getStrokeDashArray ([array, size_is (count)] out float arr, out unsigned long count); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeDashArray(float **arr, PRUint32 *count)
{
  const nsStyleCoord *dasharray = GetStyleSVG()->mStrokeDasharray;
  nsPresContext *presContext = nsSVGGlyphFrameBase::GetPresContext();
  float totalLength = 0.0f;

  *count = GetStyleSVG()->mStrokeDasharrayLength;
  *arr = nsnull;

  if (*count) {
    *arr = (float *) nsMemory::Alloc(*count * sizeof(float));
    if (*arr) {
      for (PRUint32 i = 0; i < *count; i++) {
        (*arr)[i] = nsSVGUtils::CoordToFloat(presContext, mContent, dasharray[i]);
        if ((*arr)[i] < 0.0f) {
          nsMemory::Free(*arr);
          *count = 0;
          *arr = nsnull;
          return NS_OK;
        }
        totalLength += (*arr)[i];
      }
    } else {
      *count = 0;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (totalLength == 0.0f) {
      nsMemory::Free(*arr);
      *count = 0;
      *arr = nsnull;
    }
  }

  return NS_OK;
}

/* readonly attribute float strokeDashoffset; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeDashoffset(float *aStrokeDashoffset)
{
  *aStrokeDashoffset = 
    nsSVGUtils::CoordToFloat(nsSVGGlyphFrameBase::GetPresContext(),
                             mContent, GetStyleSVG()->mStrokeDashoffset);
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinecap; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeLinecap(PRUint16 *aStrokeLinecap)
{
  *aStrokeLinecap = GetStyleSVG()->mStrokeLinecap;
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinejoin; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeLinejoin(PRUint16 *aStrokeLinejoin)
{
  *aStrokeLinejoin = GetStyleSVG()->mStrokeLinejoin;
  return NS_OK;
}

/* readonly attribute float strokeMiterlimit; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeMiterlimit(float *aStrokeMiterlimit)
{
  *aStrokeMiterlimit = GetStyleSVG()->mStrokeMiterlimit; 
  return NS_OK;
}

/* readonly attribute float fillOpacity; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillOpacity(float *aFillOpacity)
{
  *aFillOpacity =
    GetStyleSVG()->mFillOpacity * GetStyleDisplay()->mOpacity;
  return NS_OK;
}

/* readonly attribute unsigned short fillRule; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillRule(PRUint16 *aFillRule)
{
  *aFillRule = GetStyleSVG()->mFillRule;
  return NS_OK;
}

/* readonly attribute unsigned short clipRule; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetClipRule(PRUint16 *aClipRule)
{
  *aClipRule = GetStyleSVG()->mClipRule;
  return NS_OK;
}

/* readonly attribute unsigned short strokePaintType; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokePaintType(PRUint16 *aStrokePaintType)
{
  float strokeWidth;
  GetStrokeWidth(&strokeWidth);

  // cairo will stop rendering if stroke-width is less than or equal to zero
  *aStrokePaintType = strokeWidth <= 0 ?
                      nsISVGGeometrySource::PAINT_TYPE_NONE :
                      GetStyleSVG()->mStroke.mType;
  return NS_OK;
}

/* readonly attribute unsigned short strokePaintServerType; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokePaintServerType(PRUint16 *aStrokePaintServerType)
{
  return nsSVGUtils::GetPaintType(aStrokePaintServerType, GetStyleSVG()->mStroke, mContent,
                                  nsSVGGlyphFrameBase::GetPresContext()->PresShell());
}

/* [noscript] readonly attribute nscolor strokePaint; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokePaint(nscolor *aStrokePaint)
{
  *aStrokePaint = GetStyleSVG()->mStroke.mPaint.mColor;
  return NS_OK;
}

/* [noscript] void GetStrokeGradient(nsISVGGradient **aGrad); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokeGradient(nsISVGGradient **aGrad)
{
  nsresult rv = NS_OK;
  *aGrad = nsnull;
  if (!mStrokeGradient) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mStroke.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGGradient(&mStrokeGradient, aServer, mContent, 
                           nsSVGGlyphFrameBase::GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mStrokeGradient);
  }
  *aGrad = mStrokeGradient;
  return rv;
}

/* [noscript] void GetStrokePattern(nsISVGPattern **aPat); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetStrokePattern(nsISVGPattern **aPat)
{
  nsresult rv = NS_OK;
  *aPat = nsnull;
  if (!mStrokePattern) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mStroke.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGPattern(&mStrokePattern, aServer, mContent, 
                          nsSVGGlyphFrameBase::GetPresContext()->PresShell());
    if (mStrokePattern)
      NS_ADD_SVGVALUE_OBSERVER(mStrokePattern);
  }
  *aPat = mStrokePattern;
  return rv;
}

/* readonly attribute unsigned short fillPaintType; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillPaintType(PRUint16 *aFillPaintType)
{
  *aFillPaintType = GetStyleSVG()->mFill.mType;
  return NS_OK;
}

/* readonly attribute unsigned short fillPaintServerType; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillPaintServerType(PRUint16 *aFillPaintServerType)
{
  return nsSVGUtils::GetPaintType(aFillPaintServerType, GetStyleSVG()->mFill, mContent,
                                  nsSVGGlyphFrameBase::GetPresContext()->PresShell());
}

/* [noscript] readonly attribute nscolor fillPaint; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillPaint(nscolor *aFillPaint)
{
  *aFillPaint = GetStyleSVG()->mFill.mPaint.mColor;
  return NS_OK;
}

/* [noscript] void GetFillGradient(nsISVGGradient **aGrad); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillGradient(nsISVGGradient **aGrad)
{
  nsresult rv = NS_OK;
  *aGrad = nsnull;
  if (!mFillGradient) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mFill.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGGradient(&mFillGradient, aServer, mContent, 
                           nsSVGGlyphFrameBase::GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mFillGradient);
  }
  *aGrad = mFillGradient;
  return rv;
}

/* [noscript] void GetFillPattern(nsISVGPattern **aPat); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFillPattern(nsISVGPattern **aPat)
{
  nsresult rv = NS_OK;
  *aPat = nsnull;
  if (!mFillPattern) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mFill.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGPattern(&mFillPattern, aServer, mContent, 
                          nsSVGGlyphFrameBase::GetPresContext()->PresShell());
    if (mFillPattern)
      NS_ADD_SVGVALUE_OBSERVER(mFillPattern);
  }
  *aPat = mFillPattern;
  return rv;
}

/* [noscript] boolean isClipChild; */
NS_IMETHODIMP
nsSVGGlyphFrame::IsClipChild(PRBool *_retval)
{
  *_retval = PR_FALSE;
  nsCOMPtr<nsIContent> node(mContent);

  do {
    if (node->Tag() == nsSVGAtoms::clipPath) {
      *_retval = PR_TRUE;
      break;
    }
    node = node->GetParent();
  } while (node);
    
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGlyphMetricsSource methods:

/* [noscript] readonly attribute nsFont font; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetFont(nsFont *aFont)
{
  *aFont = GetStyleFont()->mFont;

  // XXX eventually we will have to treat decorations separately from
  // fonts, because they can have a different color than the current
  // glyph.
  
  NS_ASSERTION(mParent, "no parent");
  nsStyleContext *parentContext = mParent->GetStyleContext();
  NS_ASSERTION(parentContext, "no style context on parent");
  
  PRUint8 styleDecorations =
    parentContext->GetStyleTextReset()->mTextDecoration;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_UNDERLINE)
    aFont->decorations |= NS_FONT_DECORATION_UNDERLINE;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_OVERLINE)
    aFont->decorations |= NS_FONT_DECORATION_OVERLINE;
  if (styleDecorations & NS_STYLE_TEXT_DECORATION_LINE_THROUGH)
    aFont->decorations |= NS_FONT_DECORATION_LINE_THROUGH;    
  
  return NS_OK;
}

/* readonly attribute DOMString characterData; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetCharacterData(nsAString & aCharacterData)
{
  aCharacterData = mCharacterData;
  return NS_OK;
}

static void
FindPoint(nsSVGPathData *data,
          float aX, float aY, float aAdvance,
          nsSVGCharacterPosition *aCP)
{
  float x, y, length = 0;
  float midpoint = aX + aAdvance/2;
  for (PRUint32 i = 0; i < data->count; i++) {
    if (data->type[i] == NS_SVGPATHFLATTEN_LINE) {
      float dx = data->x[i] - x;
      float dy = data->y[i] - y;
      float sublength = sqrt(dx*dx + dy*dy);
      
      if (length + sublength > midpoint) {
        float ratio = (aX - length)/sublength;
        aCP->x = x * (1.0f - ratio) + data->x[i] * ratio;
        aCP->y = y * (1.0f - ratio) + data->y[i] * ratio;

        float dx = data->x[i] - x;
        float dy = data->y[i] - y;
        aCP->angle = atan2(dy, dx);

        float normalization = 1.0/sqrt(dx*dx+dy*dy);
        aCP->x += - aY * dy * normalization;
        aCP->y +=   aY * dx * normalization;
        return;
      }
      length += sublength;
    }
    x = data->x[i];
    y = data->y[i];
  }
}

/* readonly attribute nsSVGCharacterPostion characterPosition; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetCharacterPosition(nsSVGCharacterPosition **aCharacterPosition)
{
  *aCharacterPosition = nsnull;
  nsISVGPathFlatten *textPath = nsnull;

  /* check if we're the child of a textPath */
  for (nsIFrame *frame = this; frame != nsnull; frame = frame->GetParent())
    if (frame->GetType() == nsLayoutAtoms::svgTextPathFrame) {
      frame->QueryInterface(NS_GET_IID(nsISVGPathFlatten), (void **)&textPath);
      break;
    }

  /* we're an ordinary fragment - return */
  /* XXX: we might want to use this for individual x/y/dx/dy adjustment */
  if (!textPath)
    return NS_OK;

  nsSVGPathData *data;
  textPath->GetFlattenedPath(&data);

  /* textPath frame, but invalid target */
  if (!data)
    return NS_ERROR_FAILURE;

  float length = data->Length();
  PRUint32 strLength = mCharacterData.Length();

  nsSVGCharacterPosition *cp = new nsSVGCharacterPosition[strLength];

  for (PRUint32 k = 0; k < strLength; k++)
      cp[k].draw = PR_FALSE;

  float x = mX;
  for (PRUint32 i = 0; i < strLength; i++) {
    float advance;
    mMetrics->GetAdvanceOfChar(i, &advance);

    /* have we run off the end of the path? */
    if (x + advance/2 > length)
      break;

    /* check that we've advanced to the start of the path */
    if (x + advance/2 >= 0.0f) {
      cp[i].draw = PR_TRUE;

      // add y (normal)
      // add rotation
      // move point back along tangent
      FindPoint(data, x, mY, advance, &(cp[i]));
    }
    x += advance;
  }

  *aCharacterPosition = cp;

  delete data;

  return NS_OK;
}

/* readonly attribute unsigned short textRendering; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetTextRendering(PRUint16 *aTextRendering)
{
  *aTextRendering = GetStyleSVG()->mTextRendering;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGlyphGeometrySource methods:

/* readonly attribute nsISVGRendererGlyphMetrics metrics; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetMetrics(nsISVGRendererGlyphMetrics * *aMetrics)
{
  *aMetrics = mMetrics;
  NS_ADDREF(*aMetrics);
  return NS_OK;
}

/* readonly attribute float x; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}

/* readonly attribute float y; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}

/* readonly attribute boolean hasHighlight; */
NS_IMETHODIMP
nsSVGGlyphFrame::GetHasHighlight(PRBool *aHasHighlight)
{
  *aHasHighlight = (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;

  return NS_OK;
}


// Utilities for converting from indices in the uncompressed content
// element strings to compressed frame string and back:
int CompressIndex(int index, const nsTextFragment*fragment)
{
  int ci=0;
  if (fragment->Is2b()) {
    const PRUnichar *data=fragment->Get2b();
    while(*data && index) {
      if (XP_IS_SPACE_W(*data)){
        do {
          ++data;
          --index;
        }while(XP_IS_SPACE_W(*data) && index);
      }
      else {
        ++data;
        --index;
      }
      ++ci;
    }
  }
  else {
    const char *data=fragment->Get1b();
    while(*data && index) {
      if (XP_IS_SPACE_W(*data)){
        do {
          ++data;
          --index;
        }while(XP_IS_SPACE_W(*data) && index);
      }
      else {
        ++data;
        --index;
      }
      ++ci;
    }
  }
    
  return ci;
}

int UncompressIndex(int index, PRBool bRightAffinity, const nsTextFragment*fragment)
{
  // XXX
  return index;
}

/* [noscript] void getHighlight (out unsigned long charnum, out unsigned long nchars, out nscolor foreground, out nscolor background); */
NS_IMETHODIMP
nsSVGGlyphFrame::GetHighlight(PRUint32 *charnum, PRUint32 *nchars, nscolor *foreground, nscolor *background)
{
  *foreground = NS_RGB(255,255,255);
  *background = NS_RGB(0,0,0); 
  *charnum=0;
  *nchars=0;

    PRBool hasHighlight;
  GetHasHighlight(&hasHighlight);

  if (!hasHighlight) {
    NS_ERROR("nsSVGGlyphFrame::GetHighlight() called by renderer when there is no highlight");
    return NS_ERROR_FAILURE;
  }

  // XXX gcc 3.2.2 requires the explicit 'nsSVGGlyphFrameBase::' qualification
  nsPresContext *presContext = nsSVGGlyphFrameBase::GetPresContext();

  nsCOMPtr<nsITextContent> tc = do_QueryInterface(mContent);
  NS_ASSERTION(tc, "no textcontent interface");

  // The selection ranges are relative to the uncompressed text in
  // the content element. We'll need the text fragment:
  const nsTextFragment *fragment = tc->Text();
  
  // get the selection details 
  SelectionDetails *details = nsnull;
  {
    nsCOMPtr<nsIFrameSelection> frameSelection;
    {
      nsCOMPtr<nsISelectionController> controller;
      GetSelectionController(presContext, getter_AddRefs(controller));
      
      if (!controller) {
        NS_ERROR("no selection controller");
        return NS_ERROR_FAILURE;
      }
      frameSelection = do_QueryInterface(controller);
    }
    if (!frameSelection) {
      frameSelection = presContext->PresShell()->FrameSelection();
    }
    if (!frameSelection) {
      NS_ERROR("no frameselection interface");
      return NS_ERROR_FAILURE;
    }

    frameSelection->LookUpSelection(mContent, 0, fragment->GetLength(),
                                    &details, PR_FALSE);
  }

#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  {
    SelectionDetails *dp = details;
    printf("nsSVGGlyphFrame(%p)::GetHighlight() [\n", this);
    while (dp) {
      printf("selection detail: %d(%d)->%d(%d) type %d\n",
             dp->mStart, CompressIndex(dp->mStart, fragment),
             dp->mEnd, CompressIndex(dp->mEnd, fragment),
             dp->mType);
      dp = dp->mNext;
    }
    printf("]\n");
      
  }
#endif
  
  if (details) {
    NS_ASSERTION(details->mNext==nsnull, "can't do multiple selection ranges");

    *charnum=CompressIndex(details->mStart, fragment);
    *nchars=CompressIndex(details->mEnd, fragment)-*charnum;  
    
    nsILookAndFeel *look = presContext->LookAndFeel();

    look->GetColor(nsILookAndFeel::eColor_TextSelectBackground, *background);
    look->GetColor(nsILookAndFeel::eColor_TextSelectForeground, *foreground);

    SelectionDetails *dp = details;
    while ((dp=details->mNext) != nsnull) {
      delete details;
      details = dp;
    }
    delete details;
  }
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGGlyphFragmentLeaf interface:

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::SetGlyphPosition(float x, float y)
{
  mX = x;
  mY = y;
  UpdateGeometry(nsISVGGlyphGeometrySource::UPDATEMASK_X |
                 nsISVGGlyphGeometrySource::UPDATEMASK_Y, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetGlyphPositionX()
{
  return mX;
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetGlyphPositionY()
{
  return mY;
}


NS_IMETHODIMP
nsSVGGlyphFrame::GetGlyphMetrics(nsISVGRendererGlyphMetrics** metrics)
{
  *metrics = mMetrics;
  NS_IF_ADDREF(*metrics);
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsSVGGlyphFrame::IsStartOfChunk()
{
  // this fragment is a chunk if it has a corresponding absolute
  // position adjustment in an ancestors' x or y array. (At the moment
  // we don't map the full arrays, but only the first elements.)

  return PR_FALSE;
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::GetAdjustedPosition(/* inout */ float &x, /* inout */ float &y)
{
}

NS_IMETHODIMP_(PRUint32)
nsSVGGlyphFrame::GetNumberOfChars()
{
  return mCharacterData.Length();
}

NS_IMETHODIMP_(PRUint32)
nsSVGGlyphFrame::GetCharNumberOffset()
{
  return mCharOffset;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetX()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetX();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetY()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetY();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetDx()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetDx();
  return nsnull;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGGlyphFrame::GetDy()
{
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                          (void**)&containerFrame);
  if (containerFrame)
    return containerFrame->GetDy();
  return nsnull;
}

NS_IMETHODIMP_(PRUint16)
nsSVGGlyphFrame::GetTextAnchor()
{
  return GetStyleSVG()->mTextAnchor;
}

NS_IMETHODIMP_(PRBool)
nsSVGGlyphFrame::IsAbsolutelyPositioned()
{
  nsIFrame *lastFrame = this;

  for (nsIFrame *frame = this->GetParent();
       frame != nsnull;
       lastFrame = frame, frame = frame->GetParent()) {

    /* need to be the first child if we are absolutely positioned */
    if (!frame ||
        frame->GetFirstChild(nsnull) != lastFrame)
      break;

    // textPath is always absolutely positioned for our purposes
    if (frame->GetType() == nsLayoutAtoms::svgTextPathFrame)
      return PR_TRUE;
        
    if (frame &&
        (frame->GetContent()->HasAttr(kNameSpaceID_None, nsSVGAtoms::x) ||
         frame->GetContent()->HasAttr(kNameSpaceID_None, nsSVGAtoms::y)))
        return PR_TRUE;

    if (frame->GetType() == nsLayoutAtoms::svgTextFrame)
      break;
  }

  return PR_FALSE;
}


//----------------------------------------------------------------------
// nsISVGGlyphFragmentNode interface:

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGGlyphFrame::GetFirstGlyphFragment()
{
  return this;
}

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGGlyphFrame::GetNextGlyphFragment()
{
  nsIFrame* sibling = mNextSibling;
  while (sibling) {
    nsISVGGlyphFragmentNode *node = nsnull;
    sibling->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      return node->GetFirstGlyphFragment();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(mParent, "null parent");
  nsISVGGlyphFragmentNode *node = nsnull;
  mParent->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
  return node ? node->GetNextGlyphFragment() : nsnull;
}

NS_IMETHODIMP_(PRUint32)
nsSVGGlyphFrame::BuildGlyphFragmentTree(PRUint32 charNum, PRBool lastBranch)
{
  // XXX actually we should be building a new fragment for each chunk here...


  mCharOffset = charNum;
  nsCOMPtr<nsITextContent> tc = do_QueryInterface(mContent);

  if (tc->TextLength() == 0) {
#ifdef DEBUG
    printf("Glyph frame with zero length text\n");
#endif
    mCharacterData.AssignLiteral("");
    return charNum;
  }

  mCharacterData.Truncate();
  tc->AppendTextTo(mCharacterData);
  mCharacterData.CompressWhitespace(charNum==0, lastBranch);

  return charNum+mCharacterData.Length();
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyMetricsSuspended()
{
  // do nothing
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyMetricsUnsuspended()
{
  NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::NotifyMetricsUnsuspended");

  if (mMetricsUpdateFlags != 0) {
    PRBool metricsDirty = PR_FALSE;
    if (mMetrics)
      mMetrics->Update(mMetricsUpdateFlags, &metricsDirty);
    if (metricsDirty) {
      mGeometryUpdateFlags |= nsISVGGlyphGeometrySource::UPDATEMASK_METRICS;
      nsISVGTextFrame* text_frame = GetTextFrame();
      NS_ASSERTION(text_frame, "null text frame");
      if (text_frame)
        text_frame->NotifyGlyphMetricsChange(this);
    }
    mMetricsUpdateFlags = 0;
  }   
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyGlyphFragmentTreeSuspended()
{
  // do nothing
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::NotifyGlyphFragmentTreeUnsuspended()
{
  if (mFragmentTreeDirty) {
    nsISVGTextFrame* text_frame = GetTextFrame();
    NS_ASSERTION(text_frame, "null text frame");
    if (text_frame)
      text_frame->NotifyGlyphFragmentTreeChange(this);
    mFragmentTreeDirty = PR_FALSE;
  }
}



//----------------------------------------------------------------------
//

void nsSVGGlyphFrame::UpdateGeometry(PRUint32 flags, PRBool bRedraw,
                                     PRBool suppressInvalidation)
{
  mGeometryUpdateFlags |= flags;
  
  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }
  
  if (suppressInvalidation) {
    mGeometryUpdateFlags = 0;
    return;
  }

  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (!suspended) {
    NS_ASSERTION(!mMetricsUpdateFlags, "dirty metrics in nsSVGGlyphFrame::UpdateGeometry");
    NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::UpdateGeometry");
    nsCOMPtr<nsISVGRendererRegion> dirty_region;
    if (mGeometry)
      mGeometry->Update(mGeometryUpdateFlags, getter_AddRefs(dirty_region));

    mGeometryUpdateFlags = 0;

    if (suppressInvalidation)
      return;

    nsCOMPtr<nsISVGRendererRegion> filter_region;
    nsSVGUtils::FindFilterInvalidation(this,
                                       getter_AddRefs(filter_region));
    if (filter_region) {
      outerSVGFrame->InvalidateRegion(filter_region, bRedraw);
    } else {
      if (dirty_region)
        outerSVGFrame->InvalidateRegion(dirty_region, bRedraw);
    }
  }  
}

void nsSVGGlyphFrame::UpdateMetrics(PRUint32 flags)
{
  mMetricsUpdateFlags |= flags;

  nsISVGTextFrame* text_frame = GetTextFrame();
  if (!text_frame) {
    NS_ERROR("null text_frame");
    return;
  }
  
  PRBool suspended = text_frame->IsMetricsSuspended();
  if (!suspended) {
    NS_ASSERTION(!mFragmentTreeDirty, "dirty fragmenttree in nsSVGGlyphFrame::UpdateMetrics");
    PRBool metricsDirty;
    mMetrics->Update(mMetricsUpdateFlags, &metricsDirty);
    if (metricsDirty) {
      mGeometryUpdateFlags |= nsISVGGlyphGeometrySource::UPDATEMASK_METRICS;
      text_frame->NotifyGlyphMetricsChange(this);
    }
    mMetricsUpdateFlags = 0;
  }
}

void nsSVGGlyphFrame::UpdateFragmentTree()
{
  mFragmentTreeDirty = PR_TRUE;
    
  nsISVGTextFrame* text_frame = GetTextFrame();
  if (!text_frame) {
    NS_ERROR("null text_frame");
    return;
  }
  
  PRBool suspended = text_frame->IsGlyphFragmentTreeSuspended();
  if (!suspended) {
    text_frame->NotifyGlyphFragmentTreeChange(this);
    mFragmentTreeDirty = PR_FALSE;
  }
}

nsISVGTextFrame *
nsSVGGlyphFrame::GetTextFrame()
{
  NS_ASSERTION(mParent, "null parent");

  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetTextFrame();
}
