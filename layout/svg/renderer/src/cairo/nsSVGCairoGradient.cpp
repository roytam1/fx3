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
 * The Original Code is the Mozilla SVG Cairo Renderer project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Parts of this file contain code derived from the following files(s)
 * of the Mozilla SVG project (these parts are Copyright (C) by their
 * respective copyright-holders):
 *    layout/svg/renderer/src/libart/nsSVGCairoGradient.cpp
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

#include <math.h>

#include "nsCOMPtr.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsISVGPathGeometrySource.h"
#include "nsSVGCairoGradient.h"


static cairo_matrix_t SVGToMatrix(nsIDOMSVGMatrix *ctm)
{
  float A, B, C, D, E, F;
  ctm->GetA(&A);
  ctm->GetB(&B);
  ctm->GetC(&C);
  ctm->GetD(&D);
  ctm->GetE(&E);
  ctm->GetF(&F);
  cairo_matrix_t matrix = { A, B, C, D, E, F };
  return matrix;
}


static void
CairoSetStops(cairo_pattern_t *aPattern, nsISVGGradient *aGrad, float aOpacity)
{
  PRUint32 nStops;
  float lastOffset = 0.0f;

  aGrad->GetStopCount(&nStops);
  for (PRUint32 i = 0; i < nStops; i++) {
    nscolor rgba;
    float offset;
    float opacity;

    aGrad->GetStopOffset(i, &offset);
    aGrad->GetStopColor(i, &rgba);
    aGrad->GetStopOpacity(i, &opacity);

    if (offset < lastOffset)
      offset = lastOffset;
    else
      lastOffset = offset;

    cairo_pattern_add_color_stop_rgba(aPattern, offset,
                                      NS_GET_R(rgba)/255.0,
                                      NS_GET_G(rgba)/255.0,
                                      NS_GET_B(rgba)/255.0,
                                      opacity * aOpacity);
  }
}


static cairo_pattern_t *
CairoLinearGradient(cairo_t *ctx, nsISVGGradient *aGrad)
{
  float fX1, fY1, fX2, fY2;
    
  nsISVGLinearGradient *aLgrad;
  CallQueryInterface(aGrad, &aLgrad);
  NS_ASSERTION(aLgrad, "error gradient did not provide a Linear Gradient interface");
  
  aLgrad->GetX1(&fX1);
  aLgrad->GetX2(&fX2);
  aLgrad->GetY1(&fY1);
  aLgrad->GetY2(&fY2);
  
  return cairo_pattern_create_linear(fX1, fY1, fX2, fY2);
}


static cairo_pattern_t *
CairoRadialGradient(cairo_t *ctx, nsISVGGradient *aGrad)
{
  float fCx, fCy, fR, fFx, fFy;

  // Get the Radial Gradient interface
  nsISVGRadialGradient *aRgrad;
  CallQueryInterface(aGrad, &aRgrad);
  NS_ASSERTION(aRgrad, "error gradient did not provide a Linear Gradient interface");

  aRgrad->GetCx(&fCx);
  aRgrad->GetCy(&fCy);
  aRgrad->GetR(&fR);
  aRgrad->GetFx(&fFx);
  aRgrad->GetFy(&fFy);

  if (fFx != fCx || fFy != fCy) {
    // The focal point (fFx and fFy) must be clamped to be *inside* - not on -
    // the circumference of the gradient or we'll get rendering anomalies. We
    // calculate the distance from the focal point to the gradient center and
    // make sure it is *less* than the gradient radius. 0.999 is used as the
    // factor of the radius because it's close enough to 1 that we won't get a
    // fringe at the edge of the gradient if we clamp, but not so close to 1
    // that rounding error will give us the same results as using fR itself.
    double dMax = 0.999 * fR;
    float dx = fFx - fCx;
    float dy = fFy - fCy;
    double d = sqrt((dx * dx) + (dy * dy));
    if (d > dMax) {
      double angle = atan2(dy, dx);
      fFx = (float)(dMax * cos(angle)) + fCx;
      fFy = (float)(dMax * sin(angle)) + fCy;
    }
  }

  return cairo_pattern_create_radial(fFx, fFy, 0, fCx, fCy, fR);
}


cairo_pattern_t *
CairoGradient(cairo_t *ctx, nsISVGGradient *aGrad,
              nsISVGGeometrySource *aSource)
{
  NS_ASSERTION(aGrad, "Called CairoGradient without a gradient!");
  if (!aGrad)
    return nsnull;

  // Get the transform list (if there is one)
  nsCOMPtr<nsIDOMSVGMatrix> svgMatrix;
  aGrad->GetGradientTransform(getter_AddRefs(svgMatrix), aSource);
  NS_ASSERTION(svgMatrix, "CairoGradient: GetGradientTransform returns null");

  cairo_matrix_t patternMatrix = SVGToMatrix(svgMatrix);
  if (cairo_matrix_invert(&patternMatrix)) {
    return nsnull;
  }

  cairo_pattern_t *gradient;

  // Linear or Radial?
  PRUint32 type;
  aGrad->GetGradientType(&type);
  if (type == nsISVGGradient::SVG_LINEAR_GRADIENT)
    gradient = CairoLinearGradient(ctx, aGrad);
  else if (type == nsISVGGradient::SVG_RADIAL_GRADIENT)
    gradient = CairoRadialGradient(ctx, aGrad);
  else 
    return nsnull; // Shouldn't happen

  PRUint16 aSpread;
  aGrad->GetSpreadMethod(&aSpread);
  if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD)
    cairo_pattern_set_extend(gradient, CAIRO_EXTEND_PAD);
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT)
    cairo_pattern_set_extend(gradient, CAIRO_EXTEND_REFLECT);
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT)
    cairo_pattern_set_extend(gradient, CAIRO_EXTEND_REPEAT);
  
  cairo_pattern_set_matrix(gradient, &patternMatrix);

  float opacity;
  aSource->GetFillOpacity(&opacity);

  CairoSetStops(gradient, aGrad, opacity);

  return gradient;
}
