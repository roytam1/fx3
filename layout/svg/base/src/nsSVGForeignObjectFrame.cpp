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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#include "nsSVGForeignObjectFrame.h"

#include "nsISVGRendererCanvas.h"
#include "nsISVGValue.h"
#include "nsIDOMSVGGElement.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGForeignObjectElem.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGPoint.h"
#include "nsSpaceManager.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGRenderer.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGValueUtils.h"
#include "nsRegion.h"
#include "nsLayoutAtoms.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsIURI.h"
#include "nsSVGPoint.h"
#include "nsSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsISVGRendererSurface.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject = do_QueryInterface(aContent);
  if (!foreignObject) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGForeignObjectFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGForeignObjectFrame(aContext);
}

nsSVGForeignObjectFrame::nsSVGForeignObjectFrame(nsStyleContext* aContext)
  : nsSVGForeignObjectFrameBase(aContext),
    mIsDirty(PR_TRUE), mPropagateTransform(PR_TRUE)
{
  AddStateBits(NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT |
               NS_FRAME_REFLOW_ROOT);
}

nsresult nsSVGForeignObjectFrame::Init()
{
  nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject = do_QueryInterface(mContent);
  NS_ASSERTION(foreignObject, "wrong content element");
  
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetX(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no x");
    if (!mX) return NS_ERROR_FAILURE;
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetY(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no y");
    if (!mY) return NS_ERROR_FAILURE;
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetWidth(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mWidth));
    NS_ASSERTION(mWidth, "no width");
    if (!mWidth) return NS_ERROR_FAILURE;
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetHeight(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mHeight));
    NS_ASSERTION(mHeight, "no height");
    if (!mHeight) return NS_ERROR_FAILURE;
  }
  
  // XXX for some reason updating fails when done here. Why is this too early?
  // anyway - we use a less desirable mechanism now of updating in paint().
//  Update(); 
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGForeignObjectFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGForeignObjectFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGForeignObjectFrame::Init(
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGForeignObjectFrameBase::Init(aContent, aParent, aPrevInFlow);

  Init();

  return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::Reflow(nsPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  nsHTMLReflowState &reflowState =
    NS_CONST_CAST(nsHTMLReflowState&, aReflowState);

  // We could do this in DoReflow, except that we set
  // NS_FRAME_REFLOW_ROOT, so we also need to do it when the pres shell
  // calls us directly.
  reflowState.mComputedHeight = mRect.height;
  reflowState.mComputedWidth = mRect.width;

  nsSpaceManager* spaceManager =
    new nsSpaceManager(aPresContext->PresShell(), this);
  if (!spaceManager) {
    NS_ERROR("Could not create space manager");
    return nsnull;
  }
  reflowState.mSpaceManager = spaceManager;
   
  nsresult rv = nsSVGForeignObjectFrameBase::Reflow(aPresContext, aDesiredSize,
                                                    aReflowState, aStatus);

  reflowState.mSpaceManager = nsnull;
  delete spaceManager;

  return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::AppendFrames(nsIAtom*        aListName,
                                      nsIFrame*       aFrameList)
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::AppendFrames()\n");
#endif
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::AppendFrames(aListName, aFrameList);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::InsertFrames(nsIAtom*        aListName,
                                      nsIFrame*       aPrevFrame,
                                      nsIFrame*       aFrameList)
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::InsertFrames()\n");
#endif
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::InsertFrames(aListName, aPrevFrame,
                                                   aFrameList);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::RemoveFrame(nsIAtom*        aListName,
                                     nsIFrame*       aOldFrame)
{
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::RemoveFrame(aListName, aOldFrame);
	Update();
	return rv;
}

// XXX Need to make sure that any of the code examining
// frametypes, particularly code looking at block and area
// also handles foreignObject before we return our own frametype
// nsIAtom *
// nsSVGForeignObjectFrame::GetType() const
// {
//   return nsLayoutAtoms::svgForeignObjectFrame;
// }

PRBool
nsSVGForeignObjectFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGForeignObject));
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::transform))
    Update();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::DidSetStyleContext()
{
  nsSVGUtils::StyleEffects(this);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGForeignObjectFrame::WillModifySVGObservable(nsISVGValue* observable,
                                                 nsISVGValue::modificationType aModType)
{
  nsSVGUtils::WillModifyEffects(this, observable, aModType);

  return NS_OK;
}


NS_IMETHODIMP
nsSVGForeignObjectFrame::DidModifySVGObservable (nsISVGValue* observable,
                                                 nsISVGValue::modificationType aModType)
{
  Update();
  nsSVGUtils::DidModifyEffects(this, observable, aModType);
   
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

/**
 * Transform a rectangle with a given matrix. Since the image of the
 * rectangle may not be a rectangle, the output rectangle is the
 * bounding box of the true image.
 */
static void
TransformRect(float* aX, float *aY, float* aWidth, float *aHeight,
              nsIDOMSVGMatrix* aMatrix)
{
  float x[4], y[4];
  x[0] = *aX;
  y[0] = *aY;
  x[1] = x[0] + *aWidth;
  y[1] = y[0];
  x[2] = x[0] + *aWidth;
  y[2] = y[0] + *aHeight;
  x[3] = x[0];
  y[3] = y[0] + *aHeight;
 
  int i;
  for (i = 0; i < 4; i++) {
    nsSVGUtils::TransformPoint(aMatrix, &x[i], &y[i]);
  }

  float xmin, xmax, ymin, ymax;
  xmin = xmax = x[0];
  ymin = ymax = y[0];
  for (i=1; i<4; i++) {
    if (x[i] < xmin)
      xmin = x[i];
    if (y[i] < ymin)
      ymin = y[i];
    if (x[i] > xmax)
      xmax = x[i];
    if (y[i] > ymax)
      ymax = y[i];
  }
 
  *aX = xmin;
  *aY = ymin;
  *aWidth = xmax - xmin;
  *aHeight = ymax - ymin;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
  nsresult rv = NS_OK;

  if (mIsDirty) {
    nsCOMPtr<nsISVGRendererRegion> region = DoReflow();
  }

  nsRect dirtyRect = nsRect(nsPoint(0, 0), GetSize());
  nsCOMPtr<nsIDOMSVGMatrix> tm = GetTMIncludingOffset();
#if 0  /// XX - broken by PaintSVG API change (bug 330498)
  nsCOMPtr<nsIDOMSVGMatrix> inverse;
  rv = tm->Inverse(getter_AddRefs(inverse));
  float pxPerTwips = GetPxPerTwips();
  float twipsPerPx = GetTwipsPerPx();
  if (NS_SUCCEEDED(rv)) {
    float x = dirtyRectTwips.x*pxPerTwips;
    float y = dirtyRectTwips.y*pxPerTwips;
    float w = dirtyRectTwips.width*pxPerTwips;
    float h = dirtyRectTwips.height*pxPerTwips;
    TransformRect(&x, &y, &w, &h, inverse);
 
    nsRect r;
    r.x = NSToCoordFloor(x*twipsPerPx);
    r.y = NSToCoordFloor(y*twipsPerPx);
    r.width = NSToCoordCeil((x + w)*twipsPerPx) - r.x;
    r.height = NSToCoordCeil((y + h)*twipsPerPx) - r.y;
    dirtyRect.IntersectRect(dirtyRect, r);
  }
#endif

  if (dirtyRect.IsEmpty())
    return NS_OK;
 
  nsCOMPtr<nsIRenderingContext> ctx;
  canvas->LockRenderingContext(tm, getter_AddRefs(ctx));
  
  if (!ctx) {
    NS_WARNING("Can't render foreignObject element!");
    return NS_ERROR_FAILURE;
  }
    
  rv = nsLayoutUtils::PaintFrame(ctx, this, nsRegion(dirtyRect),
                                 NS_RGBA(0,0,0,0));
  
  ctx = nsnull;
  canvas->UnlockRenderingContext();
  
  return rv;
}

nsresult
nsSVGForeignObjectFrame::TransformPointFromOuterPx(float aX, float aY, nsPoint* aOut)
{
  nsCOMPtr<nsIDOMSVGMatrix> tm = GetTMIncludingOffset();
  nsCOMPtr<nsIDOMSVGMatrix> inverse;
  nsresult rv = tm->Inverse(getter_AddRefs(inverse));
  if (NS_FAILED(rv))
    return rv;
   
  nsSVGUtils::TransformPoint(inverse, &aX, &aY);
  float twipsPerPx = GetTwipsPerPx();
  *aOut = nsPoint(NSToCoordRound(aX*twipsPerPx),
                  NSToCoordRound(aY*twipsPerPx));
  return NS_OK;
}
 
NS_IMETHODIMP
nsSVGForeignObjectFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
  nsPoint pt;
  nsresult rv = TransformPointFromOuterPx(x, y, &pt);
  if (NS_FAILED(rv))
    return rv;
  *hit = nsLayoutUtils::GetFrameForPoint(this, pt);
  return NS_OK;
}

nsPoint
nsSVGForeignObjectFrame::TransformPointFromOuter(nsPoint aPt)
{
  float pxPerTwips = GetPxPerTwips();
  nsPoint pt(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  TransformPointFromOuterPx(aPt.x*pxPerTwips, aPt.y*pxPerTwips, &pt);
  return pt;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGForeignObjectFrame::GetCoveredRegion()
{
  // get a region from our BBox
  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return nsnull;
  }
  
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  
  float x, y, w, h;
  GetBBoxInternal(&x, &y, &w, &h);
  
  nsISVGRendererRegion *region = nsnull;
  renderer->CreateRectRegion(x, y, w, h, &region);

  NS_ASSERTION(region, "could not create region");
  return region;  
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::InitialUpdate()
{
//  Update();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  mCanvasTM = nsnull;
  if (!suppressInvalidation)
    Update();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawSuspended()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawUnsuspended()
{
  if (mIsDirty) {
    nsCOMPtr<nsISVGRendererRegion> dirtyRegion = DoReflow();
    if (dirtyRegion) {
      nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
      if (outerSVGFrame)
        outerSVGFrame->InvalidateRegion(dirtyRegion, PR_TRUE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

void
nsSVGForeignObjectFrame::GetBBoxInternal(float* aX, float *aY, float* aWidth,
                                         float *aHeight)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm = GetCanvasTM();
  if (!ctm)
    return;
  
  mX->GetValue(aX);
  mY->GetValue(aY);
  mWidth->GetValue(aWidth);
  mHeight->GetValue(aHeight);
  
  TransformRect(aX, aY, aWidth, aHeight, ctm);
}
  
NS_IMETHODIMP
nsSVGForeignObjectFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  float x, y, w, h;
  GetBBoxInternal(&x, &y, &w, &h);
  return NS_NewSVGRect(_retval, x, y, w, h);
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGForeignObjectFrame::GetTMIncludingOffset()
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm = GetCanvasTM();
  if (!ctm)
    return nsnull;
  float svgX, svgY;
  mX->GetValue(&svgX);
  mY->GetValue(&svgY);
  nsIDOMSVGMatrix* matrix;
  ctm->Translate(svgX, svgY, &matrix);
  return matrix;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGForeignObjectFrame::GetCanvasTM()
{
  if (!mPropagateTransform) {
    nsIDOMSVGMatrix *retval;
    if (mOverrideCTM) {
      retval = mOverrideCTM;
      NS_ADDREF(retval);
    } else {
      NS_NewSVGMatrix(&retval);
    }
    return retval;
  }

  if (!mCanvasTM) {
    // get our parent's tm and append local transforms (if any):
    NS_ASSERTION(mParent, "null parent");
    nsISVGContainerFrame *containerFrame;
    mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
    if (!containerFrame) {
      NS_ERROR("invalid parent");
      return nsnull;
    }
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // got the parent tm, now check for local tm:
    nsCOMPtr<nsIDOMSVGMatrix> localTM;
    {
      nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
      NS_ASSERTION(transformable, "wrong content element");
      nsCOMPtr<nsIDOMSVGAnimatedTransformList> atl;
      transformable->GetTransform(getter_AddRefs(atl));
      NS_ASSERTION(atl, "null animated transform list");
      nsCOMPtr<nsIDOMSVGTransformList> transforms;
      atl->GetAnimVal(getter_AddRefs(transforms));
      NS_ASSERTION(transforms, "null transform list");
      PRUint32 numberOfItems;
      transforms->GetNumberOfItems(&numberOfItems);
      if (numberOfItems>0)
        transforms->GetConsolidationMatrix(getter_AddRefs(localTM));
    }
    
    if (localTM)
      parentTM->Multiply(localTM, getter_AddRefs(mCanvasTM));
    else
      mCanvasTM = parentTM;
  }

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGForeignObjectFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetCoordContextProvider();  
}


//----------------------------------------------------------------------
// Implementation helpers

void nsSVGForeignObjectFrame::Update()
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::Update()\n");
#endif

  mIsDirty = PR_TRUE;

  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }
  
  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (!suspended) {
    nsCOMPtr<nsISVGRendererRegion> dirtyRegion = DoReflow();
    if (dirtyRegion) {
      outerSVGFrame->InvalidateRegion(dirtyRegion, PR_TRUE);
    }
  }  
}

already_AddRefed<nsISVGRendererRegion>
nsSVGForeignObjectFrame::DoReflow()
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::DoReflow()\n");
#endif

  nsPresContext *presContext = GetPresContext();

  // remember the area we have to invalidate after this reflow:
  nsCOMPtr<nsISVGRendererRegion> area_before = GetCoveredRegion();
  NS_ASSERTION(area_before, "could not get covered region");
  
  // initiate a synchronous reflow here and now:  
  nsSize availableSpace(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsCOMPtr<nsIRenderingContext> renderingContext;
  nsIPresShell* presShell = presContext->PresShell();
  NS_ASSERTION(presShell, "null presShell");
  presShell->CreateRenderingContext(this,getter_AddRefs(renderingContext));
  NS_ENSURE_TRUE(renderingContext, nsnull);
  
  float twipsPerPx = GetTwipsPerPx();
  
  NS_ENSURE_TRUE(mX && mY && mWidth && mHeight, nsnull);

  float width, height;
  mWidth->GetValue(&width);
  mHeight->GetValue(&height);

  nsSize size(NSFloatPixelsToTwips(width, twipsPerPx),
              NSFloatPixelsToTwips(height, twipsPerPx));

  // move our frame to (0, 0), set our size to the untransformed
  // width and height. Our frame size and position are meaningless
  // given the possibility of non-rectilinear transforms; our size
  // is only useful for reflowing our content
  SetPosition(nsPoint(0, 0));
  SetSize(size);
  
  // create a new reflow state, setting our max size to (width,height):
  // Make up a potentially reasonable but perhaps too destructive reflow
  // reason.
  nsReflowReason reason = (GetStateBits() & NS_FRAME_FIRST_REFLOW)
                            ? eReflowReason_Initial
                            : eReflowReason_StyleChange;
  nsHTMLReflowState reflowState(presContext, this, reason,
                                renderingContext, size);
  nsHTMLReflowMetrics desiredSize(nsnull);
  nsReflowStatus status;
  
  WillReflow(presContext);
  Reflow(presContext, desiredSize, reflowState, status);
  NS_ASSERTION(size.width == desiredSize.width &&
               size.height == desiredSize.height, "unexpected size");
  DidReflow(presContext, &reflowState, NS_FRAME_REFLOW_FINISHED);

  mIsDirty = PR_FALSE;

  nsCOMPtr<nsISVGRendererRegion> area_after = GetCoveredRegion();
  nsISVGRendererRegion *dirtyRegion;
  area_before->Combine(area_after, &dirtyRegion);

  return dirtyRegion;
}

float nsSVGForeignObjectFrame::GetPxPerTwips()
{
  float val = GetTwipsPerPx();
  
  NS_ASSERTION(val!=0.0f, "invalid px/twips");  
  if (val == 0.0) val = 1e-20f;
  
  return 1.0f/val;
}

float nsSVGForeignObjectFrame::GetTwipsPerPx()
{
  return GetPresContext()->ScaledPixelsToTwips();
}
