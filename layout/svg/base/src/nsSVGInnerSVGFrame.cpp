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

#include "nsContainerFrame.h"
#include "nsIDOMSVGGElement.h"
#include "nsPresContext.h"
#include "nsISVGChildFrame.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsISVGSVGElement.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsSVGLength.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsSVGMatrix.h"
#include "nsLayoutAtoms.h"
#include "nsSVGFilterFrame.h"
#include "nsISVGValueUtils.h"
#include "nsSVGUtils.h"
#include "nsSVGClipPathFrame.h"
#include "nsISVGRendererSurface.h"
#include "nsGkAtoms.h"

typedef nsContainerFrame nsSVGInnerSVGFrameBase;

class nsSVGInnerSVGFrame : public nsSVGInnerSVGFrameBase,
                           public nsISVGChildFrame,
                           public nsISVGContainerFrame,
                           public nsISVGValueObserver,
                           public nsSupportsWeakReference,
                           public nsISVGSVGFrame
{
  friend nsIFrame*
  NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
protected:
  nsSVGInnerSVGFrame(nsStyleContext* aContext);
  nsresult Init();
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:
  
  NS_IMETHOD  AppendFrames(nsIAtom*       aListName,
                           nsIFrame*      aFrameList);
  NS_IMETHOD  InsertFrames(nsIAtom*       aListName,
                           nsIFrame*      aPrevFrame,
                           nsIFrame*      aFrameList);
  NS_IMETHOD  RemoveFrame(nsIAtom*       aListName,
                          nsIFrame*      aOldFrame);
  NS_IMETHOD Init(nsIContent*     aContent,
                  nsIFrame*       aParent,
                  nsIFrame*       aPrevInFlow);
  NS_IMETHOD DidSetStyleContext();

  // We don't define an AttributeChanged method since changes to the
  // 'x', 'y', 'width' and 'height' attributes of our content object
  // are handled in nsSVGSVGElement::DidModifySVGObservable

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgInnerSVGFrame
   */
  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGInnerSVG"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas);
  NS_IMETHOD GetFrameForPointSVG(float x, float y, nsIFrame** hit);  
  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>) GetCoveredRegion();
  NS_IMETHOD InitialUpdate();
  NS_IMETHOD NotifyCanvasTMChanged(PRBool suppressInvalidation);
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate);
  NS_IMETHOD SetOverrideCTM(nsIDOMSVGMatrix *aCTM);
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISVGContainerFrame interface:
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

  // nsISVGSVGFrame interface:
  NS_IMETHOD SuspendRedraw();
  NS_IMETHOD UnsuspendRedraw();
  NS_IMETHOD NotifyViewportChange();

protected:
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;

  nsCOMPtr<nsIDOMSVGLength> mX;
  nsCOMPtr<nsIDOMSVGLength> mY;

  nsCOMPtr<nsIDOMSVGMatrix> mOverrideCTM;

  PRPackedBool mPropagateTransform;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGInnerSVGFrame(aContext);
}

nsSVGInnerSVGFrame::nsSVGInnerSVGFrame(nsStyleContext* aContext) :
  nsSVGInnerSVGFrameBase(aContext), mPropagateTransform(PR_TRUE)
{
#ifdef DEBUG
//  printf("nsSVGInnerSVGFrame CTOR\n");
#endif
}

nsresult nsSVGInnerSVGFrame::Init()
{
  NS_ASSERTION(mParent, "no parent");
  
  // hook up CoordContextProvider chain:
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ASSERTION(SVGElement, "wrong content element");
  SVGElement->SetParentCoordCtxProvider(nsRefPtr<nsSVGCoordCtxProvider>(containerFrame->GetCoordContextProvider()));

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    SVGElement->GetX(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no x");
    if (!mX) return NS_ERROR_FAILURE;
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    SVGElement->GetY(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no y");
    if (!mY) return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGInnerSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGInnerSVGFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGInnerSVGFrame::Init(nsIContent*     aContent,
                         nsIFrame*       aParent,
                         nsIFrame*       aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGInnerSVGFrameBase::Init(aContent, aParent, aPrevInFlow);

  Init();
  
  return rv;
}


NS_IMETHODIMP
nsSVGInnerSVGFrame::AppendFrames(nsIAtom*       aListName,
                                 nsIFrame*      aFrameList)
{
  // append == insert at end:
  return InsertFrames(aListName, mFrames.LastChild(), aFrameList);  
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::InsertFrames(nsIAtom*       aListName,
                                 nsIFrame*      aPrevFrame,
                                 nsIFrame*      aFrameList)
{
  nsIFrame* lastNewFrame = nsnull;
  {
    nsFrameList tmpList(aFrameList);
    lastNewFrame = tmpList.LastChild();
  }
  
  // Insert the new frames
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  // call InitialUpdate() on all new frames:
  nsIFrame* end = nsnull;
  if (lastNewFrame)
    end = lastNewFrame->GetNextSibling();

  for (nsIFrame*kid=aFrameList; kid != end; kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate(); 
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::RemoveFrame(nsIAtom*       aListName,
                                nsIFrame*      aOldFrame)
{
  nsCOMPtr<nsISVGRendererRegion> dirty_region;
  
  nsISVGChildFrame* SVGFrame=nsnull;
  aOldFrame->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);

  if (SVGFrame)
    dirty_region = SVGFrame->GetCoveredRegion();

  PRBool result = mFrames.DestroyFrame(aOldFrame);

  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  NS_ASSERTION(outerSVGFrame, "no outer svg frame");
  if (dirty_region && outerSVGFrame)
    outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);

  NS_ASSERTION(result, "didn't find frame to delete");
  return result ? NS_OK : NS_ERROR_FAILURE;
}

nsIAtom *
nsSVGInnerSVGFrame::GetType() const
{
  return nsLayoutAtoms::svgInnerSVGFrame;
}

PRBool
nsSVGInnerSVGFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~nsIFrame::eSVG);
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGInnerSVGFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
#ifdef DEBUG
//  printf("nsSVGInnerSVG(%p)::Paint\n", this);
#endif

  canvas->PushClip();

  if (GetStyleDisplay()->IsScrollableOverflow()) {
    nsCOMPtr<nsIDOMSVGAnimatedLength> anim;
    nsCOMPtr<nsIDOMSVGLength> val;
    nsCOMPtr<nsISVGSVGElement> svg = do_QueryInterface(mContent);

    float x, y, width, height;
    mX->GetValue(&x);
    mY->GetValue(&y);
    svg->GetWidth(getter_AddRefs(anim));
    anim->GetAnimVal(getter_AddRefs(val));
    val->GetValue(&width);
    svg->GetHeight(getter_AddRefs(anim));
    anim->GetAnimVal(getter_AddRefs(val));
    val->GetValue(&height);

    nsCOMPtr<nsIDOMSVGMatrix> clipTransform;
    if (!mPropagateTransform) {
      NS_NewSVGMatrix(getter_AddRefs(clipTransform));
    } else {
      nsISVGContainerFrame *parent;
      CallQueryInterface(mParent, &parent);
      if (parent)
        clipTransform = parent->GetCanvasTM();
    }

    if (clipTransform)
      canvas->SetClipRect(clipTransform, x, y, width, height);
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintChildWithEffects(canvas, kid);
  }

  canvas->PopClip();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
  nsSVGUtils::HitTestChildren(this, x, y, hit);

  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGInnerSVGFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *accu_region=nsnull;
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsCOMPtr<nsISVGRendererRegion> dirty_region = SVGFrame->GetCoveredRegion();
      if (dirty_region) {
        if (accu_region) {
          nsCOMPtr<nsISVGRendererRegion> temp = dont_AddRef(accu_region);
          dirty_region->Combine(temp, &accu_region);
        }
        else {
          accu_region = dirty_region;
          NS_IF_ADDREF(accu_region);
        }
      }
    }
    kid = kid->GetNextSibling();
  }
  
  return accu_region;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::InitialUpdate()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate();
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}  

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  // make sure our cached transform matrix gets (lazily) updated
  mCanvasTM = nsnull;
  
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged(suppressInvalidation);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyRedrawSuspended()
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyRedrawUnsuspended()
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
  }
 return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}


NS_IMETHODIMP
nsSVGInnerSVGFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  float minx, miny, maxx, maxy;
  minx = miny = FLT_MAX;
  maxx = maxy = -1.0 * FLT_MAX;

  nsCOMPtr<nsIDOMSVGRect> unionRect;

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsCOMPtr<nsIDOMSVGRect> box;
      SVGFrame->GetBBox(getter_AddRefs(box));

      if (box) {
        float bminx, bminy, bmaxx, bmaxy, width, height;
        box->GetX(&bminx);
        box->GetY(&bminy);
        box->GetWidth(&width);
        box->GetHeight(&height);
        bmaxx = bminx+width;
        bmaxy = bminy+height;

        if (!unionRect)
          unionRect = box;
        minx = PR_MIN(minx, bminx);
        miny = PR_MIN(miny, bminy);
        maxx = PR_MAX(maxx, bmaxx);
        maxy = PR_MAX(maxy, bmaxy);
      }
    }
    kid = kid->GetNextSibling();
  }

  if (unionRect) {
    unionRect->SetX(minx);
    unionRect->SetY(miny);
    unionRect->SetWidth(maxx-minx);
    unionRect->SetHeight(maxy-miny);
    *_retval = unionRect;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:

NS_IMETHODIMP
nsSVGInnerSVGFrame::SuspendRedraw()
{
  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("no outer svg frame");
    return NS_ERROR_FAILURE;
  }
  return outerSVGFrame->SuspendRedraw();
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::UnsuspendRedraw()
{
  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("no outer svg frame");
    return NS_ERROR_FAILURE;
  }
  return outerSVGFrame->UnsuspendRedraw();
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyViewportChange()
{
  // make sure canvas transform matrix gets (lazily) recalculated:
  mCanvasTM = nsnull;
  
  // inform children
  SuspendRedraw();
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->NotifyCanvasTMChanged(PR_FALSE); 
    kid = kid->GetNextSibling();
  }
  UnsuspendRedraw();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGInnerSVGFrame::GetCanvasTM()
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

  // parentTM * Translate(x,y) * viewboxToViewportTM

  if (!mCanvasTM) {
    // get the transform from our parent's coordinate system to ours:
    NS_ASSERTION(mParent, "null parent");
    nsISVGContainerFrame *containerFrame;
    mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
    if (!containerFrame) {
      NS_ERROR("invalid parent");
      return nsnull;
    }
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // append the transform due to the 'x' and 'y' attributes:
    float x, y;
    mX->GetValue(&x);
    mY->GetValue(&y);
    nsCOMPtr<nsIDOMSVGMatrix> xyTM;
    parentTM->Translate(x, y, getter_AddRefs(xyTM));

    // append the viewbox to viewport transform:
    nsCOMPtr<nsIDOMSVGMatrix> viewBoxToViewportTM;
    nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(mContent);
    NS_ASSERTION(svgElement, "wrong content element");
    svgElement->GetViewboxToViewportTransform(getter_AddRefs(viewBoxToViewportTM));
    xyTM->Multiply(viewBoxToViewportTM, getter_AddRefs(mCanvasTM));
  }    

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGInnerSVGFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mContent, "null parent");

  // Our <svg> content element is the CoordContextProvider for our children:
  nsSVGCoordCtxProvider *provider;
  mContent->QueryInterface(NS_GET_IID(nsSVGCoordCtxProvider), (void**)&provider);

  NS_IF_ADDREF(provider);

  return provider;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGInnerSVGFrame::WillModifySVGObservable(nsISVGValue* observable,
                                            nsISVGValue::modificationType aModType)
{
  nsSVGUtils::WillModifyEffects(this, observable, aModType);
  return NS_OK;
}
	
NS_IMETHODIMP
nsSVGInnerSVGFrame::DidModifySVGObservable (nsISVGValue* observable,
                                            nsISVGValue::modificationType aModType)
{
  nsISVGFilterFrame *filter;
  CallQueryInterface(observable, &filter);

  nsSVGUtils::DidModifyEffects(this, observable, aModType);

  if (!filter) {
    NotifyViewportChange();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::DidSetStyleContext()
{
  nsSVGUtils::StyleEffects(this);
  return NS_OK;
}
