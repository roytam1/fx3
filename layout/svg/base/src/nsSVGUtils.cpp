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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "nsSVGLength.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsStyleCoord.h"
#include "nsPresContext.h"
#include "nsSVGCoordCtxProvider.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIURI.h"
#include "nsStyleStruct.h"
#include "nsIPresShell.h"
#include "nsSVGUtils.h"
#include "nsISVGGeometrySource.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsNetUtil.h"
#include "nsIDOMSVGRect.h"
#include "nsFrameList.h"
#include "nsISVGChildFrame.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsISVGRenderer.h"
#include "nsSVGFilterFrame.h"
#include "nsINameSpaceManager.h"
#include "nsISVGChildFrame.h"
#include "nsIDOMSVGPoint.h"
#include "nsSVGPoint.h"
#include "nsDOMError.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsIDOMSVGPresAspectRatio.h"
#include "nsSVGMatrix.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsISVGRendererSurface.h"
#include "nsISVGContainerFrame.h"

struct nsSVGFilterProperty {
  nsCOMPtr<nsISVGRendererRegion> mFilterRegion;
  nsISVGFilterFrame *mFilter;
};

static PRBool gSVGEnabled;
static PRBool gSVGRendererAvailable = PR_FALSE;
static const char SVG_PREF_STR[] = "svg.enabled";

PR_STATIC_CALLBACK(int)
SVGPrefChanged(const char *aPref, void *aClosure)
{
  PRBool prefVal = nsContentUtils::GetBoolPref(SVG_PREF_STR);
  if (prefVal == gSVGEnabled)
    return 0;

  gSVGEnabled = prefVal;
  if (gSVGRendererAvailable) {
    if (gSVGEnabled)
      nsContentDLF::RegisterSVG();
    else
      nsContentDLF::UnregisterSVG();
  }

  return 0;
}

PRBool
nsSVGUtils::SVGEnabled()
{
  static PRBool sInitialized = PR_FALSE;
  
  if (!sInitialized) {
    gSVGRendererAvailable = PR_TRUE;

    /* check and register ourselves with the pref */
    gSVGEnabled = nsContentUtils::GetBoolPref(SVG_PREF_STR);
    nsContentUtils::RegisterPrefCallback(SVG_PREF_STR, SVGPrefChanged, nsnull);

    sInitialized = PR_TRUE;
  }

  return gSVGEnabled && gSVGRendererAvailable;
}

float
nsSVGUtils::CoordToFloat(nsPresContext *aPresContext, nsIContent *aContent,
			 const nsStyleCoord &aCoord)
{
  float val = 0.0f;

  switch (aCoord.GetUnit()) {
  case eStyleUnit_Factor:
    // user units
    val = aCoord.GetFactorValue();
    break;

  case eStyleUnit_Coord:
    val = aCoord.GetCoordValue() / aPresContext->ScaledPixelsToTwips();
    break;

  case eStyleUnit_Percent: {
      nsCOMPtr<nsIDOMSVGElement> element = do_QueryInterface(aContent);
      nsCOMPtr<nsIDOMSVGSVGElement> owner;
      element->GetOwnerSVGElement(getter_AddRefs(owner));
      nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(owner);
    
      nsCOMPtr<nsISVGLength> length;
      NS_NewSVGLength(getter_AddRefs(length), aCoord.GetPercentValue() * 100.0f,
                      nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    
      if (!ctx || !length)
        break;

      length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextUnspecified()));
      length->GetValue(&val);
      break;
    }
  default:
    break;
  }

  return val;
}

nsresult nsSVGUtils::GetReferencedFrame(nsIFrame **aRefFrame, nsIURI* aURI, nsIContent *aContent, 
                                        nsIPresShell *aPresShell)
{
  *aRefFrame = nsnull;

  nsIContent* content = nsContentUtils::GetReferencedElement(aURI, aContent);
  if (!content)
    return NS_ERROR_FAILURE;

  // Get the Primary Frame
  NS_ASSERTION(aPresShell, "Get referenced SVG frame -- no pres shell provided");
  if (!aPresShell)
    return NS_ERROR_FAILURE;

  *aRefFrame = aPresShell->GetPrimaryFrameFor(content);
  if (!(*aRefFrame)) return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult nsSVGUtils::GetPaintType(PRUint16 *aPaintType, const nsStyleSVGPaint& aPaint, 
                                  nsIContent *aContent, nsIPresShell *aPresShell )
{
  *aPaintType = aPaint.mType;
  // If the type is a Paint Server, determine what kind
  if (*aPaintType == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
    nsIURI *server = aPaint.mPaint.mPaintServer;
    if (server == nsnull)
      return NS_ERROR_FAILURE;

    // Get the frame
    nsIFrame *aFrame = nsnull;
    nsresult rv;
    rv = GetReferencedFrame(&aFrame, server, aContent, aPresShell);
    if (!NS_SUCCEEDED(rv) || !aFrame)
      return NS_ERROR_FAILURE;

    // Finally, figure out what type it is
    if (aFrame->GetType() == nsLayoutAtoms::svgLinearGradientFrame ||
        aFrame->GetType() == nsLayoutAtoms::svgRadialGradientFrame)
      *aPaintType = nsISVGGeometrySource::PAINT_TYPE_GRADIENT;
    else if (aFrame->GetType() == nsLayoutAtoms::svgPatternFrame)
      *aPaintType = nsISVGGeometrySource::PAINT_TYPE_PATTERN;
    else
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsSVGUtils::GetBBox(nsFrameList *aFrames, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  float minx, miny, maxx, maxy;
  minx = miny = FLT_MAX;
  maxx = maxy = -1.0 * FLT_MAX;

  nsCOMPtr<nsIDOMSVGRect> unionRect;

  nsIFrame* kid = aFrames->FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame), (void**)&SVGFrame);
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
    unionRect->SetWidth(maxx - minx);
    unionRect->SetHeight(maxy - miny);
    *_retval = unionRect;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsSVGUtils::GetNumberOfChars(nsISVGGlyphFragmentNode* node,
                             PRInt32 *_retval)
{
  PRUint32 nchars = 0;
  nsISVGGlyphFragmentLeaf *fragment = node->GetFirstGlyphFragment();

  while (fragment) {
    nchars += fragment->GetNumberOfChars();
    fragment = fragment->GetNextGlyphFragment();
  }

  *_retval = nchars;
  return NS_OK;
}

nsresult
nsSVGUtils::GetComputedTextLength(nsISVGGlyphFragmentNode* node,
                                  float *_retval)
{
  float length = 0.0;
  nsISVGGlyphFragmentLeaf *fragment = node->GetFirstGlyphFragment();

  while (fragment) {
    if (fragment->GetNumberOfChars() > 0) {
      // query the renderer metrics for the length of each fragment
      nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
      fragment->GetGlyphMetrics(getter_AddRefs(metrics));
      if (!metrics) return NS_ERROR_FAILURE;
      float fragmentLength;
      nsresult rv = metrics->GetComputedTextLength(&fragmentLength);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      length += fragmentLength;
    }

    fragment = fragment->GetNextGlyphFragment();
  }

  *_retval = length;
  return NS_OK;
}

nsresult
nsSVGUtils::GetSubStringLength(nsISVGGlyphFragmentNode* node,
                               PRUint32 charnum,
                               PRUint32 nchars,
                               float *_retval)
{
  float length = 0.0;
  nsISVGGlyphFragmentLeaf *fragment = node->GetFirstGlyphFragment();
  
  while (fragment && nchars) {
    PRUint32 count = fragment->GetNumberOfChars();
    if (count > charnum) {
      // query the renderer metrics for the length of the substring in each fragment
      nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
      fragment->GetGlyphMetrics(getter_AddRefs(metrics));
      if (!metrics) return NS_ERROR_FAILURE;
      PRUint32 fragmentChars = PR_MIN(nchars, count);
      float fragmentLength;
      nsresult rv = metrics->GetSubStringLength(charnum,
                                                fragmentChars,
                                                &fragmentLength);
      if (NS_FAILED(rv)) break;
      length += fragmentLength;
      nchars -= fragmentChars;
      if (nchars == 0) break;
    }
    charnum -= PR_MIN(charnum, count);
    fragment = fragment->GetNextGlyphFragment();
  }

  // substring too long
  if (nchars != 0) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  *_retval = length;
  return NS_OK;
}

nsresult
nsSVGUtils::GetStartPositionOfChar(nsISVGGlyphFragmentNode* node,
                                   PRUint32 charnum,
                                   nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum);
  if (!fragment) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // query the renderer metrics for the start position of the character
  nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
  fragment->GetGlyphMetrics(getter_AddRefs(metrics));
  if (!metrics) return NS_ERROR_FAILURE;
  nsresult rv = metrics->GetStartPositionOfChar(charnum-fragment->GetCharNumberOffset(),
                                                _retval);
  if (NS_FAILED(rv)) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // offset the bounds by the position of the fragment:
  float x,y;
  (*_retval)->GetX(&x);
  (*_retval)->GetY(&y);
  (*_retval)->SetX(x + fragment->GetGlyphPositionX());
  (*_retval)->SetY(y + fragment->GetGlyphPositionY());

  return NS_OK;
}

nsresult
nsSVGUtils::GetEndPositionOfChar(nsISVGGlyphFragmentNode* node,
                                 PRUint32 charnum,
                                 nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum);
  if (!fragment) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // query the renderer metrics for the end position of the character
  nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
  fragment->GetGlyphMetrics(getter_AddRefs(metrics));
  if (!metrics) return NS_ERROR_FAILURE;
  nsresult rv = metrics->GetEndPositionOfChar(charnum-fragment->GetCharNumberOffset(),
                                              _retval);
  if (NS_FAILED(rv)) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // offset the bounds by the position of the fragment:
  float x,y;
  (*_retval)->GetX(&x);
  (*_retval)->GetY(&y);
  (*_retval)->SetX(x + fragment->GetGlyphPositionX());
  (*_retval)->SetY(y + fragment->GetGlyphPositionY());

  return NS_OK;
}

nsresult
nsSVGUtils::GetExtentOfChar(nsISVGGlyphFragmentNode* node,
                            PRUint32 charnum,
                            nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum);
  if (!fragment) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // query the renderer metrics for the bounds of the character
  nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
  fragment->GetGlyphMetrics(getter_AddRefs(metrics));
  if (!metrics) return NS_ERROR_FAILURE;
  nsresult rv = metrics->GetExtentOfChar(charnum-fragment->GetCharNumberOffset(),
                                         _retval);
  if (NS_FAILED(rv)) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // offset the bounds by the position of the fragment:
  float x,y;
  (*_retval)->GetX(&x);
  (*_retval)->GetY(&y);
  (*_retval)->SetX(x + fragment->GetGlyphPositionX());
  (*_retval)->SetY(y + fragment->GetGlyphPositionY());

  return NS_OK;
}

nsresult
nsSVGUtils::GetRotationOfChar(nsISVGGlyphFragmentNode* node,
                              PRUint32 charnum,
                              float *_retval)
{
  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum);
  if (!fragment) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // query the renderer metrics for the rotation of the character
  nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
  fragment->GetGlyphMetrics(getter_AddRefs(metrics));
  if (!metrics) return NS_ERROR_FAILURE;
  nsresult rv = metrics->GetRotationOfChar(charnum-fragment->GetCharNumberOffset(),
                                           _retval);
  if (NS_FAILED(rv)) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  return NS_OK;
}

nsresult
nsSVGUtils::GetCharNumAtPosition(nsISVGGlyphFragmentNode* node,
                                     nsIDOMSVGPoint *point,
                                     PRInt32 *_retval)
{
  PRInt32 index = -1;
  nsISVGGlyphFragmentLeaf *fragment = node->GetFirstGlyphFragment();

  while (fragment) {
    if (fragment->GetNumberOfChars() > 0) {
      // query the renderer metrics for the character position
      nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
      fragment->GetGlyphMetrics(getter_AddRefs(metrics));
      if (!metrics) return NS_ERROR_FAILURE;

      // subtract the fragment offset from the position:
      float x,y;
      point->GetX(&x);
      point->GetY(&y);

      nsCOMPtr<nsIDOMSVGPoint> position;
      NS_NewSVGPoint(getter_AddRefs(position),
                     x - fragment->GetGlyphPositionX(),
                     y - fragment->GetGlyphPositionY());
      if (!position)
        return NS_ERROR_FAILURE;

      nsresult rv = metrics->GetCharNumAtPosition(position, &index);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      // Multiple characters may match, we must return the last one
      // so no break here
    }
    fragment = fragment->GetNextGlyphFragment();
  }

  *_retval = index;
  return NS_OK;
}

void
nsSVGUtils::FindFilterInvalidation(nsIFrame *aFrame,
                                   nsISVGRendererRegion **aRegion)
{
  *aRegion = nsnull;
  nsISVGRendererRegion *region = nsnull;

  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)
      break;

    if (aFrame->GetStateBits() & NS_STATE_SVG_FILTERED) {
      nsSVGFilterProperty *property;
      property = NS_STATIC_CAST(nsSVGFilterProperty *,
                                aFrame->GetProperty(nsGkAtoms::filter));
      region = property->mFilterRegion;
    }
    aFrame = aFrame->GetParent();
  }

  *aRegion = region;
  NS_IF_ADDREF(*aRegion);
}

float
nsSVGUtils::ObjectSpace(nsIDOMSVGRect *rect,
                        nsIDOMSVGLength *length,
                        ctxDirection direction)
{
  PRUint16 type;
  float fraction, axis;

  length->GetUnitType(&type);
  switch (direction) {
  case X:
    rect->GetWidth(&axis);
    break;
  case Y:
    rect->GetHeight(&axis);
    break;
  case XY:
  {
    float width, height;
    rect->GetWidth(&width);
    rect->GetHeight(&height);
    axis = sqrt(width * width + height * height)/sqrt(2.0f);
  }
  }

  if (type == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    length->GetValueInSpecifiedUnits(&fraction);
    fraction /= 100.0;
  } else
    length->GetValue(&fraction);

  return fraction * axis;
}

float
nsSVGUtils::UserSpace(nsIContent *content,
                      nsIDOMSVGLength *length,
                      ctxDirection direction)
{
  PRUint16 units;
  float value;

  length->GetUnitType(&units);
  length->GetValueInSpecifiedUnits(&value);

  nsCOMPtr<nsISVGLength> val;
  NS_NewSVGLength(getter_AddRefs(val), value, units);
 
  nsCOMPtr<nsIDOMSVGElement> element = do_QueryInterface(content);
  nsCOMPtr<nsIDOMSVGSVGElement> svg;
  element->GetOwnerSVGElement(getter_AddRefs(svg));
  nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(svg);

  if (ctx) {
    switch (direction) {
    case X:
      val->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
      break;
    case Y:
      val->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
      break;
    case XY:
      val->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextUnspecified()));
      break;
    }
  }

  val->GetValue(&value);
  return value;
}

void
nsSVGUtils::TransformPoint(nsIDOMSVGMatrix *matrix, 
                           float *x, float *y)
{
  nsCOMPtr<nsIDOMSVGPoint> point;
  NS_NewSVGPoint(getter_AddRefs(point), *x, *y);
  if (!point)
    return;

  nsCOMPtr<nsIDOMSVGPoint> xfpoint;
  point->MatrixTransform(matrix, getter_AddRefs(xfpoint));
  if (!xfpoint)
    return;

  xfpoint->GetX(x);
  xfpoint->GetY(y);
}

nsresult
nsSVGUtils::GetSurface(nsISVGOuterSVGFrame *aOuterSVGFrame,
                       nsISVGRendererCanvas *aCanvas,
                       nsISVGRendererSurface **aSurface)
{
  PRUint32 width, height;
  aCanvas->GetSurfaceSize(&width, &height);
  
  nsCOMPtr<nsISVGRenderer> renderer;
  aOuterSVGFrame->GetRenderer(getter_AddRefs(renderer));
  if (renderer)
    return renderer->CreateSurface(width, height, aSurface);
  else
    return NS_ERROR_FAILURE;
}

nsISVGGlyphFragmentLeaf *
nsSVGUtils::GetGlyphFragmentAtCharNum(nsISVGGlyphFragmentNode* node,
                                      PRUint32 charnum)
{
  nsISVGGlyphFragmentLeaf *fragment = node->GetFirstGlyphFragment();
  
  while (fragment) {
    PRUint32 count = fragment->GetNumberOfChars();
    if (count > charnum)
      return fragment;
    charnum -= count;
    fragment = fragment->GetNextGlyphFragment();
  }

  // not found
  return nsnull;
}

float
nsSVGUtils::AngleBisect(float a1, float a2)
{
  if (a2 - a1 < M_PI)
    return (a1+a2)/2;
  else
    return M_PI + (a1+a2)/2;
}

nsISVGOuterSVGFrame *
nsSVGUtils::GetOuterSVGFrame(nsIFrame *aFrame)
{
  nsISVGOuterSVGFrame *outerSVG = nsnull;

  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG) {
      CallQueryInterface(aFrame, &outerSVG);
      break;
    }
    aFrame = aFrame->GetParent();
  }

  return outerSVG;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGUtils::GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                                float aViewboxX, float aViewboxY,
                                float aViewboxWidth, float aViewboxHeight,
                                nsIDOMSVGAnimatedPreserveAspectRatio *aPreserveAspectRatio,
                                PRBool aIgnoreAlign)
{
  PRUint16 align, meetOrSlice;
  {
    nsCOMPtr<nsIDOMSVGPreserveAspectRatio> par;
    aPreserveAspectRatio->GetAnimVal(getter_AddRefs(par));
    NS_ASSERTION(par, "could not get preserveAspectRatio");
    par->GetAlign(&align);
    par->GetMeetOrSlice(&meetOrSlice);
  }

  // default to the defaults
  if (align == nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN)
    align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
  if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN)
    meetOrSlice = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;

  // alignment disabled for this matrix setup
  if (aIgnoreAlign)
    align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN;
    
  float a, d, e, f;
  a = aViewportWidth / aViewboxWidth;
  d = aViewportHeight / aViewboxHeight;
  e = 0.0f;
  f = 0.0f;

  if (align != nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE &&
      a != d) {
    if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
        a < d ||
        meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
        d < a) {
      d = a;
      switch (align) {
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
        f = (aViewportHeight - a * aViewboxHeight) / 2.0f;
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
        f = aViewportHeight - a * aViewboxHeight;
        break;
      default:
        NS_NOTREACHED("Unknown value for align");
      }
    }
    else if (
      meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
      d < a ||
      meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
      a < d) {
      a = d;
      switch (align) {
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
        e = (aViewportWidth - a * aViewboxWidth) / 2.0f;
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
        e = aViewportWidth - a * aViewboxWidth;
        break;
      default:
        NS_NOTREACHED("Unknown value for align");
      }
    }
    else NS_NOTREACHED("Unknown value for meetOrSlice");
  }
  
  if (aViewboxX) e += -a * aViewboxX;
  if (aViewboxY) f += -d * aViewboxY;
  
  nsIDOMSVGMatrix *retval;
  NS_NewSVGMatrix(&retval, a, 0.0f, 0.0f, d, e, f);
  return retval;
}


// This is ugly and roc will want to kill me...

already_AddRefed<nsIDOMSVGMatrix>
nsSVGUtils::GetCanvasTM(nsIFrame *aFrame)
{
  nsISVGContainerFrame *containerFrame = nsnull;
  CallQueryInterface(aFrame, &containerFrame);
  if (containerFrame) {
    return containerFrame->GetCanvasTM();
  }

  nsISVGGeometrySource *geometrySource = nsnull;
  CallQueryInterface(aFrame, &geometrySource);
  if (geometrySource) {
    nsCOMPtr<nsIDOMSVGMatrix> matrix;
    nsIDOMSVGMatrix *retval;
    geometrySource->GetCanvasTM(getter_AddRefs(matrix));
    retval = matrix.get();
    NS_IF_ADDREF(retval);
    return retval;
  }

  return nsnull;
}

void
nsSVGUtils::AddObserver(nsISupports *aObserver, nsISupports *aTarget)
{
  nsISVGValueObserver *observer = nsnull;
  nsISVGValue *v = nsnull;
  CallQueryInterface(aObserver, &observer);
  CallQueryInterface(aTarget, &v);
  if (observer && v)
    v->AddObserver(observer);
}

void
nsSVGUtils::RemoveObserver(nsISupports *aObserver, nsISupports *aTarget)
{
  nsISVGValueObserver *observer = nsnull;
  nsISVGValue *v = nsnull;
  CallQueryInterface(aObserver, &observer);
  CallQueryInterface(aTarget, &v);
  if (observer && v)
    v->RemoveObserver(observer);
}

// ************************************************************
// Effect helper functions

static void
FilterPropertyDtor(void *aObject, nsIAtom *aPropertyName,
                   void *aPropertyValue, void *aData)
{
  nsSVGFilterProperty *property = NS_STATIC_CAST(nsSVGFilterProperty *,
                                                 aPropertyValue);
  nsSVGUtils::RemoveObserver(NS_STATIC_CAST(nsIFrame *, aObject),
                                            property->mFilter);
  delete property;
}

static void
InvalidateFilterRegion(nsIFrame *aFrame)
{
  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(aFrame);
  if (outerSVGFrame) {
    nsCOMPtr<nsISVGRendererRegion> region;
    nsSVGUtils::FindFilterInvalidation(aFrame, getter_AddRefs(region));
    if (region)
      outerSVGFrame->InvalidateRegion(region, PR_TRUE);
  }
}

static void
AddEffectProperties(nsIFrame *aFrame)
{
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();

  if (style->mFilter && !(aFrame->GetStateBits() & NS_STATE_SVG_FILTERED)) {
    nsISVGFilterFrame *filter;
    NS_GetSVGFilterFrame(&filter, style->mFilter, aFrame->GetContent());
    if (filter) {
      nsSVGUtils::AddObserver(aFrame, filter);
      nsSVGFilterProperty *property = new nsSVGFilterProperty;
      if (!property) {
        NS_ERROR("Could not create filter property");
        return;
      }
      property->mFilter = filter;
      filter->GetInvalidationRegion(aFrame, getter_AddRefs(property->mFilterRegion));
      aFrame->SetProperty(nsGkAtoms::filter, property, FilterPropertyDtor);
      aFrame->AddStateBits(NS_STATE_SVG_FILTERED);
    }
  }

  if (style->mClipPath && !(aFrame->GetStateBits() & NS_STATE_SVG_CLIPPED_MASK)) {
    nsISVGClipPathFrame *clip;
    NS_GetSVGClipPathFrame(&clip, style->mClipPath, aFrame->GetContent());
    if (clip) {
      aFrame->SetProperty(nsGkAtoms::clipPath, clip);

      PRBool trivialClip;
      clip->IsTrivial(&trivialClip);
      if (trivialClip)
        aFrame->AddStateBits(NS_STATE_SVG_CLIPPED_TRIVIAL);
      else
        aFrame->AddStateBits(NS_STATE_SVG_CLIPPED_COMPLEX);
    }
  }

  if (style->mMask && !(aFrame->GetStateBits() & NS_STATE_SVG_MASKED)) {
    nsISVGMaskFrame *mask;
    NS_GetSVGMaskFrame(&mask, style->mMask, aFrame->GetContent());
    if (mask) {
      aFrame->SetProperty(nsGkAtoms::mask, mask);
      aFrame->AddStateBits(NS_STATE_SVG_MASKED);
    }
  }
}

static already_AddRefed<nsISVGRendererSurface>
GetComplexClipSurface(nsISVGRendererCanvas *aCanvas, nsIFrame *aFrame)
{
  nsISVGRendererSurface *surface = nsnull;

  if (aFrame->GetStateBits() & NS_STATE_SVG_CLIPPED_COMPLEX) {
    nsISVGChildFrame *svgChildFrame;
    CallQueryInterface(aFrame, &svgChildFrame);

    nsISVGClipPathFrame *clip;
    clip = NS_STATIC_CAST(nsISVGClipPathFrame *,
                          aFrame->GetProperty(nsGkAtoms::clipPath));

    nsSVGUtils::GetSurface(nsSVGUtils::GetOuterSVGFrame(aFrame),
                           aCanvas, &surface);
    if (surface) {
      nsCOMPtr<nsIDOMSVGMatrix> matrix = nsSVGUtils::GetCanvasTM(aFrame);
      if (NS_FAILED(clip->ClipPaint(aCanvas, surface, svgChildFrame,
                                    matrix))) {
        delete surface;
        surface = nsnull;
      }
    }
  }

  return surface;
}

static already_AddRefed<nsISVGRendererSurface>
GetMaskSurface(nsISVGRendererCanvas *aCanvas, nsIFrame *aFrame, float opacity)
{
  nsISVGRendererSurface *surface = nsnull;

  if (aFrame->GetStateBits() & NS_STATE_SVG_MASKED) {
    nsISVGChildFrame *svgChildFrame;
    CallQueryInterface(aFrame, &svgChildFrame);

    nsISVGMaskFrame *mask;
    mask = NS_STATIC_CAST(nsISVGMaskFrame *,
                          aFrame->GetProperty(nsGkAtoms::mask));

    nsSVGUtils::GetSurface(nsSVGUtils::GetOuterSVGFrame(aFrame),
                           aCanvas, &surface);
    if (surface) {
      nsCOMPtr<nsIDOMSVGMatrix> matrix = nsSVGUtils::GetCanvasTM(aFrame);
      if (NS_FAILED(mask->MaskPaint(aCanvas, surface, svgChildFrame,
                                    matrix, opacity))) {
        delete surface;
        surface = nsnull;
      }
    }
  }

  return surface;
}


// ************************************************************

void
nsSVGUtils::PaintChildWithEffects(nsISVGRendererCanvas *aCanvas,
                                  nsIFrame *aFrame)
{
  nsISVGChildFrame *svgChildFrame;
  CallQueryInterface(aFrame, &svgChildFrame);

  if (!svgChildFrame)
    return;

  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(aFrame);
  float opacity = aFrame->GetStyleDisplay()->mOpacity;

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */

  AddEffectProperties(aFrame);
  nsFrameState state = aFrame->GetStateBits();

  /* SVG defines the following rendering model:
   *
   *  1. Render geometry
   *  2. Apply filter
   *  3. Apply clipping, masking, group opacity
   *
   * We follow this, but perform a couple optimizations:
   *
   * + Use cairo's clipPath when representable natively (single object
   *   clip region).
   *
   * + Merge opacity and masking if both used together.
   */

  nsCOMPtr<nsISVGRendererSurface> offscreenSurface;

  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0 ||
      state & (NS_STATE_SVG_CLIPPED_COMPLEX | NS_STATE_SVG_MASKED)) {
    nsSVGUtils::GetSurface(outerSVGFrame,
                           aCanvas, getter_AddRefs(offscreenSurface));
    if (offscreenSurface) {
      aCanvas->PushSurface(offscreenSurface, PR_TRUE);
    } else {
      return;
    }
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (state & NS_STATE_SVG_CLIPPED_TRIVIAL) {
    nsISVGClipPathFrame *clip;
    clip = NS_STATIC_CAST(nsISVGClipPathFrame *,
                          aFrame->GetProperty(nsGkAtoms::clipPath));

    aCanvas->PushClip();
    nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM(aFrame);
    clip->ClipPaint(aCanvas, nsnull, svgChildFrame, matrix);
  }

  /* Paint the child */
  if (state & NS_STATE_SVG_FILTERED) {
    nsSVGFilterProperty *property;
    property = NS_STATIC_CAST(nsSVGFilterProperty *,
                              aFrame->GetProperty(nsGkAtoms::filter));
    property->mFilter->FilterPaint(aCanvas, svgChildFrame);
  } else {
    svgChildFrame->PaintSVG(aCanvas);
  }
  
  if (state & NS_STATE_SVG_CLIPPED_TRIVIAL) {
    aCanvas->PopClip();
  }

  /* No more effects, we're done. */
  if (!offscreenSurface)
    return;

  aCanvas->PopSurface();

  nsCOMPtr<nsISVGRendererSurface> clipMaskSurface, maskSurface;

  clipMaskSurface = GetComplexClipSurface(aCanvas, aFrame);
  maskSurface = GetMaskSurface(aCanvas, aFrame, opacity);

  nsCOMPtr<nsISVGRendererSurface> clippedSurface;

  if (clipMaskSurface) {
    // Still more set after clipping, so clip to another surface
    if (maskSurface || opacity != 1.0) {
      nsSVGUtils::GetSurface(outerSVGFrame, aCanvas,
                             getter_AddRefs(clippedSurface));
      
      if (clippedSurface) {
        aCanvas->PushSurface(clippedSurface, PR_TRUE);
        aCanvas->CompositeSurfaceWithMask(offscreenSurface, clipMaskSurface);
        aCanvas->PopSurface();
      }
    } else {
        aCanvas->CompositeSurfaceWithMask(offscreenSurface, clipMaskSurface);
    }
  }

  // No clipping or out of memory creating clip dest surface (skip clipping)
  if (!clippedSurface) {
    clippedSurface = offscreenSurface;
  }

  if (maskSurface)
    aCanvas->CompositeSurfaceWithMask(clippedSurface, maskSurface);
  else if (opacity != 1.0)
    aCanvas->CompositeSurface(clippedSurface, opacity);
}

void
nsSVGUtils::StyleEffects(nsIFrame *aFrame)
{
  nsFrameState state = aFrame->GetStateBits();

  /* clear out all effects */

  if (state & NS_STATE_SVG_CLIPPED_MASK) {
    aFrame->DeleteProperty(nsGkAtoms::clipPath);
  }

  if (state & NS_STATE_SVG_FILTERED) {
    aFrame->DeleteProperty(nsGkAtoms::filter);
  }

  if (state & NS_STATE_SVG_MASKED) {
    aFrame->DeleteProperty(nsGkAtoms::mask);
  }

  aFrame->RemoveStateBits(NS_STATE_SVG_CLIPPED_MASK |
                          NS_STATE_SVG_FILTERED |
                          NS_STATE_SVG_MASKED);
}

void
nsSVGUtils::WillModifyEffects(nsIFrame *aFrame, nsISVGValue *observable,
                              nsISVGValue::modificationType aModType)
{
  nsISVGFilterFrame *filter;
  CallQueryInterface(observable, &filter);

  if (filter)
    InvalidateFilterRegion(aFrame);
}

void
nsSVGUtils::DidModifyEffects(nsIFrame *aFrame, nsISVGValue *observable,
                             nsISVGValue::modificationType aModType)
{
  nsISVGFilterFrame *filter;
  CallQueryInterface(observable, &filter);

  if (filter) {
    InvalidateFilterRegion(aFrame);

    if (aModType == nsISVGValue::mod_die) {
      aFrame->DeleteProperty(nsGkAtoms::filter);
      aFrame->RemoveStateBits(NS_STATE_SVG_FILTERED);
    }
  }
}

PRBool
nsSVGUtils::HitTestClip(nsIFrame *aFrame, float x, float y)
{
  PRBool clipHit = PR_TRUE;

  nsISVGChildFrame* SVGFrame;
  CallQueryInterface(aFrame, &SVGFrame);

  if (aFrame->GetStateBits() & NS_STATE_SVG_CLIPPED_MASK) {
    nsISVGClipPathFrame *clip;
    clip = NS_STATIC_CAST(nsISVGClipPathFrame *,
                          aFrame->GetProperty(nsGkAtoms::clipPath));
    nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM(aFrame);
    clip->ClipHitTest(SVGFrame, matrix, x, y, &clipHit);
  }

  return clipHit;
}

void
nsSVGUtils::HitTestChildren(nsIFrame *aFrame, float x, float y,
                            nsIFrame **aResult)
{
  *aResult = nsnull;
  for (nsIFrame* kid = aFrame->GetFirstChild(nsnull); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame) {
      nsIFrame* temp=nsnull;
      nsresult rv = SVGFrame->GetFrameForPointSVG(x, y, &temp);
      if (NS_SUCCEEDED(rv) && temp) {
        *aResult = temp;
        // return NS_OK; can't return. we need reverse order but only
        // have a singly linked list...
      }
    }
  }

  if (*aResult && !HitTestClip(aFrame, x, y))
    *aResult = nsnull;
}
