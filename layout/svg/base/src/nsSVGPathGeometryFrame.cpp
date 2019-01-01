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

#include "nsSVGPathGeometryFrame.h"
#include "nsIDOMSVGDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsISVGRenderer.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGValueUtils.h"
#include "nsISVGContainerFrame.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsSVGAtoms.h"
#include "nsCRT.h"
#include "prdtoa.h"
#include "nsSVGMarkerFrame.h"
#include "nsISVGMarkable.h"
#include "nsIViewManager.h"
#include "nsSVGMatrix.h"
#include "nsSVGClipPathFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIViewManager.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsISVGRendererSurface.h"
#include "nsINameSpaceManager.h"
#include "nsSVGGraphicElement.h"

struct nsSVGMarkerProperty {
  nsISVGMarkerFrame *mMarkerStart;
  nsISVGMarkerFrame *mMarkerMid;
  nsISVGMarkerFrame *mMarkerEnd;

  nsSVGMarkerProperty() 
      : mMarkerStart(nsnull),
        mMarkerMid(nsnull),
        mMarkerEnd(nsnull)
  {}
};

////////////////////////////////////////////////////////////////////////
// nsSVGPathGeometryFrame

nsSVGPathGeometryFrame::nsSVGPathGeometryFrame(nsStyleContext* aContext)
  : nsSVGPathGeometryFrameBase(aContext),
    mUpdateFlags(0),
    mPropagateTransform(PR_TRUE)
{
#ifdef DEBUG
//  printf("nsSVGPathGeometryFrame %p CTOR\n", this);
#endif
}

nsSVGPathGeometryFrame::~nsSVGPathGeometryFrame()
{
#ifdef DEBUG
//  printf("~nsSVGPathGeometryFrame %p\n", this);
#endif
  
  if (GetStateBits() & NS_STATE_SVG_HAS_MARKERS) {
    DeleteProperty(nsGkAtoms::marker);
  }
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGPathGeometryFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGPathGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPathGeometryFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods
  
NS_IMETHODIMP
nsSVGPathGeometryFrame::Init(nsIContent*      aContent,
                             nsIFrame*        aParent,
                             nsIFrame*        aPrevInFlow)
{
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  if (mContent) {
    mContent->SetMayHaveFrame(PR_TRUE);
  }
  
  InitSVG();
  DidSetStyleContext();
    
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::transform)
    UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_CANVAS_TM);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::DidSetStyleContext()
{
  nsSVGPathGeometryFrameBase::DidSetStyleContext();

  nsSVGUtils::StyleEffects(this);

  if (GetStateBits() & NS_STATE_SVG_HAS_MARKERS) {
    DeleteProperty(nsGkAtoms::marker);
    RemoveStateBits(NS_STATE_SVG_HAS_MARKERS);
  }

  // XXX: we'd like to use the style_hint mechanism and the
  // ContentStateChanged/AttributeChanged functions for style changes
  // to get slightly finer granularity, but unfortunately the
  // style_hints don't map very well onto svg. Here seems to be the
  // best place to deal with style changes:

  UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_ALL);

  return NS_OK;
}

nsIAtom *
nsSVGPathGeometryFrame::GetType() const
{
  return nsLayoutAtoms::svgPathGeometryFrame;
}

PRBool
nsSVGPathGeometryFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~nsIFrame::eSVG);
}

// marker helper
static void
RemoveMarkerObserver(nsSVGMarkerProperty *property,
                     nsIFrame            *aFrame,
                     nsISVGMarkerFrame   *marker)
{
  if (!marker) return;
  if (property->mMarkerStart == marker)
    property->mMarkerStart = nsnull;
  if (property->mMarkerMid == marker)
    property->mMarkerMid = nsnull;
  if (property->mMarkerEnd == marker)
    property->mMarkerEnd = nsnull;
  nsSVGUtils::RemoveObserver(aFrame, marker);
}

static void
MarkerPropertyDtor(void *aObject, nsIAtom *aPropertyName,
                   void *aPropertyValue, void *aData)
{
  nsSVGMarkerProperty *property = NS_STATIC_CAST(nsSVGMarkerProperty *,
                                                 aPropertyValue);
  nsIFrame *frame = NS_STATIC_CAST(nsIFrame *, aObject);
  RemoveMarkerObserver(property, frame, property->mMarkerStart);
  RemoveMarkerObserver(property, frame, property->mMarkerMid);
  RemoveMarkerObserver(property, frame, property->mMarkerEnd);
  delete property;
}

void
nsSVGPathGeometryFrame::GetMarkerFrames(nsISVGMarkerFrame **markerStart,
                                        nsISVGMarkerFrame **markerMid,
                                        nsISVGMarkerFrame **markerEnd)
{
  *markerStart = *markerMid = *markerEnd = nsnull;

  if (GetStateBits() & NS_STATE_SVG_HAS_MARKERS) {
    nsSVGMarkerProperty *property;
    property = NS_STATIC_CAST(nsSVGMarkerProperty *,
                              GetProperty(nsGkAtoms::marker));

    if (property) {
      *markerStart = property->mMarkerStart;
      *markerMid = property->mMarkerMid;
      *markerEnd = property->mMarkerEnd;
    }
  }
}

void
nsSVGPathGeometryFrame::GetMarkerFromStyle(nsISVGMarkerFrame   **aResult,
                                           nsSVGMarkerProperty *property,
                                           nsIURI              *aURI)
{
  if (aURI && !*aResult) {
    nsISVGMarkerFrame *marker;
    NS_GetSVGMarkerFrame(&marker, aURI, GetContent());
    if (marker) {
      if (property->mMarkerStart != marker &&
          property->mMarkerMid != marker &&
          property->mMarkerEnd != marker)
        nsSVGUtils::AddObserver(NS_STATIC_CAST(nsIFrame *, this), marker);
      *aResult = marker;
    }
  }
}

void
nsSVGPathGeometryFrame::UpdateMarkerProperty()
{
  const nsStyleSVG *style = GetStyleSVG();

  if (style->mMarkerStart || style->mMarkerMid || style->mMarkerEnd) {

    nsSVGMarkerProperty *property;
    if (GetStateBits() & NS_STATE_SVG_HAS_MARKERS) {
      property = NS_STATIC_CAST(nsSVGMarkerProperty *,
                                GetProperty(nsGkAtoms::marker));
    } else {
      property = new nsSVGMarkerProperty;
      if (!property) {
        NS_ERROR("Could not create marker property");
        return;
      }
      SetProperty(nsGkAtoms::marker, property, MarkerPropertyDtor);
      AddStateBits(NS_STATE_SVG_HAS_MARKERS);
    }
    GetMarkerFromStyle(&property->mMarkerStart, property, style->mMarkerStart);
    GetMarkerFromStyle(&property->mMarkerMid, property, style->mMarkerMid);
    GetMarkerFromStyle(&property->mMarkerEnd, property, style->mMarkerEnd);
  }
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGPathGeometryFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  /* render */
  GetGeometry()->Render(this, canvas);

  nsISVGMarkable *markable;
  CallQueryInterface(this, &markable);

  if (markable) {
    // Marker Property is added lazily and may have been removed by a restyle
    UpdateMarkerProperty();
    nsISVGMarkerFrame *markerEnd, *markerMid, *markerStart;
    GetMarkerFrames(&markerStart, &markerMid, &markerEnd);
      
    if (markerEnd || markerMid || markerStart) {
      // need to set this up with the first draw
      if (!mMarkerRegion)
        mMarkerRegion = GetCoveredRegion();
        
      float strokeWidth = GetStrokeWidth();
        
      nsVoidArray marks;
      markable->GetMarkPoints(&marks);
        
      PRUint32 num = marks.Count();
        
      if (num && markerStart)
        markerStart->PaintMark(canvas, this, (nsSVGMark *)marks[0], strokeWidth);
        
      if (num && markerMid)
        for (PRUint32 i = 1; i < num - 1; i++)
          markerMid->PaintMark(canvas, this, (nsSVGMark *)marks[i], strokeWidth);
        
      if (num && markerEnd)
        markerEnd->PaintMark(canvas, this, (nsSVGMark *)marks[num-1], strokeWidth);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
  //printf("nsSVGPathGeometryFrame(%p)::GetFrameForPoint\n", this);
#endif

  // test for hit:
  *hit = nsnull;
  PRBool isHit;
  GetGeometry()->ContainsPoint(this, x, y, &isHit);

  if (isHit && nsSVGUtils::HitTestClip(this, x, y))
    *hit = this;
  
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGPathGeometryFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *region = nsnull;
  if (!GetGeometry())
    return region;

  GetGeometry()->GetCoveredRegion(this, &region);

  nsISVGMarkable *markable;
  CallQueryInterface(this, &markable);

  if (markable) {
    nsISVGMarkerFrame *markerEnd, *markerMid, *markerStart;
    GetMarkerFrames(&markerStart, &markerMid, &markerEnd);

    if (!markerEnd && !markerMid && !markerStart)
      return region;

    float strokeWidth = GetStrokeWidth();

    nsVoidArray marks;
    markable->GetMarkPoints(&marks);

    PRUint32 num = marks.Count();

    if (num && markerStart) {
      nsCOMPtr<nsISVGRendererRegion> mark;
      mark = markerStart->RegionMark(this, (nsSVGMark *)marks[0], strokeWidth);

      if (mark) {
        nsCOMPtr<nsISVGRendererRegion> tmp = dont_AddRef(region);
        mark->Combine(tmp, &region);
      }
    }

    if (num && markerMid)
      for (PRUint32 i = 1; i < num - 1; i++) {
        nsCOMPtr<nsISVGRendererRegion> mark;
        mark = markerMid->RegionMark(this, (nsSVGMark *)marks[i], strokeWidth);

        if (mark) {
          nsCOMPtr<nsISVGRendererRegion> tmp = dont_AddRef(region);
          mark->Combine(tmp, &region);
        }
      }

    if (num && markerEnd) {
      nsCOMPtr<nsISVGRendererRegion> mark;
      mark = markerEnd->RegionMark(this, (nsSVGMark *)marks[num-1], strokeWidth);

      if (mark) {
        nsCOMPtr<nsISVGRendererRegion> tmp = dont_AddRef(region);
        mark->Combine(tmp, &region);
      }
    }
  }

  return region;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::InitialUpdate()
{
  UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_ALL);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_CANVAS_TM,
                suppressInvalidation);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::NotifyRedrawSuspended()
{
  // XXX should we cache the fact that redraw is suspended?
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::NotifyRedrawUnsuspended()
{
  if (mUpdateFlags != 0)
    UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_NOTHING);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  if (GetGeometry())
    return GetGeometry()->GetBoundingBox(this, _retval);
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGPathGeometryFrame::WillModifySVGObservable(nsISVGValue* observable,
                                                nsISVGValue::modificationType aModType)
{
  nsSVGUtils::WillModifyEffects(this, observable, aModType);

  return NS_OK;
}


NS_IMETHODIMP
nsSVGPathGeometryFrame::DidModifySVGObservable (nsISVGValue* observable,
                                                nsISVGValue::modificationType aModType)
{
  nsSVGUtils::DidModifyEffects(this, observable, aModType);

  nsSVGPathGeometryFrameBase::DidModifySVGObservable(observable, aModType);

  nsISVGFilterFrame *filter;
  CallQueryInterface(observable, &filter);

  if (filter) {
    UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_STROKE_PAINT |
                  nsSVGGeometryFrame::UPDATEMASK_FILL_PAINT);
    return NS_OK;
  }

  nsISVGMarkerFrame *marker;
  CallQueryInterface(observable, &marker);

  if (marker) {
    if (aModType == nsISVGValue::mod_die)
      RemoveMarkerObserver(NS_STATIC_CAST(nsSVGMarkerProperty *, 
                                          GetProperty(nsGkAtoms::marker)),
                           this,
                           marker);
    UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_NOTHING);
    return NS_OK;
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGGeometryFrame methods:

/* readonly attribute nsIDOMSVGMatrix canvasTM; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetCanvasTM(nsIDOMSVGMatrix * *aCTM)
{
  *aCTM = nsnull;

  if (!mPropagateTransform) {
    if (mOverrideCTM) {
      *aCTM = mOverrideCTM;
      NS_ADDREF(*aCTM);
      return NS_OK;
    }
    return NS_NewSVGMatrix(aCTM);
  }

  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
  NS_ASSERTION(parentTM, "null TM");

  // append our local transformations if we have any:
  nsSVGGraphicElement *element =
    NS_STATIC_CAST(nsSVGGraphicElement*, mContent);
  nsCOMPtr<nsIDOMSVGMatrix> localTM = element->GetLocalTransformMatrix();

  if (localTM)
    return parentTM->Multiply(localTM, aCTM);

  *aCTM = parentTM;
  NS_ADDREF(*aCTM);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetHittestMask(PRUint16 *aHittestMask)
{
  *aHittestMask=0;

  switch(GetStyleSVG()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
      if (GetStyleVisibility()->IsVisible()) {
        if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)
          *aHittestMask |= HITTEST_MASK_FILL;
        if (GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
          *aHittestMask |= HITTEST_MASK_STROKE;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
      if (GetStyleVisibility()->IsVisible()) {
        *aHittestMask |= HITTEST_MASK_FILL;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
      if (GetStyleVisibility()->IsVisible()) {
        *aHittestMask |= HITTEST_MASK_STROKE;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (GetStyleVisibility()->IsVisible()) {
        *aHittestMask |= HITTEST_MASK_FILL;
        *aHittestMask |= HITTEST_MASK_STROKE;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)
        *aHittestMask |= HITTEST_MASK_FILL;
      if (GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
        *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    case NS_STYLE_POINTER_EVENTS_FILL:
      *aHittestMask |= HITTEST_MASK_FILL;
      break;
    case NS_STYLE_POINTER_EVENTS_STROKE:
      *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    case NS_STYLE_POINTER_EVENTS_ALL:
      *aHittestMask |= HITTEST_MASK_FILL;
      *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }
  
  return NS_OK;
}

/* readonly attribute unsigned short shapeRendering; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetShapeRendering(PRUint16 *aShapeRendering)
{
  *aShapeRendering = GetStyleSVG()->mShapeRendering;
  return NS_OK;
}

//---------------------------------------------------------------------- 

NS_IMETHODIMP
nsSVGPathGeometryFrame::InitSVG()
{
  // construct a pathgeometry object:
  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("Null outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  if (!renderer) return NS_ERROR_FAILURE;

  renderer->CreatePathGeometry(getter_AddRefs(mGeometry));
  
  if (!mGeometry) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsISVGRendererPathGeometry *
nsSVGPathGeometryFrame::GetGeometry()
{
#ifdef DEBUG
  NS_ASSERTION(mGeometry, "invalid geometry object");
#endif
  return mGeometry;
}

nsresult
nsSVGPathGeometryFrame::UpdateGraphic(PRUint32 flags,
                                      PRBool suppressInvalidation)
{
  mUpdateFlags |= flags;

  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return NS_ERROR_FAILURE;
  }

  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (!suspended) {
    nsCOMPtr<nsISVGRendererRegion> dirty_region;
    if (GetGeometry())
      GetGeometry()->Update(this, mUpdateFlags, getter_AddRefs(dirty_region));

    mUpdateFlags = 0;

    if (suppressInvalidation)
      return NS_OK;

    nsCOMPtr<nsISVGRendererRegion> filter_region;
    nsSVGUtils::FindFilterInvalidation(this,
                                       getter_AddRefs(filter_region));
    if (filter_region) {
      outerSVGFrame->InvalidateRegion(filter_region, PR_TRUE);
    } else {
      if (mMarkerRegion) {
        outerSVGFrame->InvalidateRegion(mMarkerRegion, PR_TRUE);
        mMarkerRegion = nsnull;
      }

      nsISVGMarkable *markable;
      CallQueryInterface(this, &markable);
      
      if (markable) {
        nsISVGMarkerFrame *markerEnd, *markerMid, *markerStart;
        GetMarkerFrames(&markerStart, &markerMid, &markerEnd);

        if (markerEnd || markerMid || markerStart) {
          mMarkerRegion = GetCoveredRegion();
          if (mMarkerRegion)
            outerSVGFrame->InvalidateRegion(mMarkerRegion, PR_TRUE);
          return NS_OK;
        }
      }

      if (dirty_region)
        outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);
    }
  }

  return NS_OK;
}


