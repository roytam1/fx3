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

#include "nsSVGPathGeometryFrame.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsIDOMSVGPointList.h"
#include "nsIDOMSVGPoint.h"
#include "nsLayoutAtoms.h"
#include "nsSVGUtils.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsSVGMarkerFrame.h"

class nsSVGPolygonFrame : public nsSVGPathGeometryFrame
{
protected:
  friend nsIFrame*
  NS_NewSVGPolygonFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

  NS_IMETHOD InitSVG();
  
public:
  nsSVGPolygonFrame(nsStyleContext* aContext) : nsSVGPathGeometryFrame(aContext) {}

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(cairo_t *aCtx);
  
  nsCOMPtr<nsIDOMSVGPointList> mPoints;

  // nsSVGPathGeometry methods
  virtual PRBool IsMarkable() { return PR_TRUE; }
  virtual void GetMarkPoints(nsVoidArray *aMarks);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgPolygonFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPolygon"), aResult);
  }
#endif
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGPolygonFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGAnimatedPoints> anim_points = do_QueryInterface(aContent);
  if (!anim_points) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGPolygonFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGPolygonFrame(aContext);
}

NS_IMETHODIMP
nsSVGPolygonFrame::InitSVG()
{
  nsresult rv = nsSVGPathGeometryFrame::InitSVG();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMSVGAnimatedPoints> anim_points = do_QueryInterface(mContent);
  NS_ASSERTION(anim_points,"wrong content element");
  anim_points->GetPoints(getter_AddRefs(mPoints));
  NS_ASSERTION(mPoints, "no points");
  if (!mPoints) return NS_ERROR_FAILURE;

  return NS_OK; 
}  

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGPolygonFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::points) {
    UpdateGraphic();
    return NS_OK;
  }

  return nsSVGPathGeometryFrame::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

NS_IMETHODIMP nsSVGPolygonFrame::ConstructPath(cairo_t *aCtx)
{
  if (!mPoints) return NS_OK;

  PRUint32 count;
  mPoints->GetNumberOfItems(&count);
  if (count == 0) return NS_OK;
  
  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMSVGPoint> point;
    mPoints->GetItem(i, getter_AddRefs(point));

    float x, y;
    point->GetX(&x);
    point->GetY(&y);
    if (i == 0)
      cairo_move_to(aCtx, x, y);
    else
      cairo_line_to(aCtx, x, y);
  }
  // the difference between a polyline and a polygon is that the
  // polygon is closed:
  cairo_close_path(aCtx);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGPathGeometry methods:

void
nsSVGPolygonFrame::GetMarkPoints(nsVoidArray *aMarks) {

  if (!mPoints)
    return;

  PRUint32 count;
  mPoints->GetNumberOfItems(&count);
  if (count == 0)
    return;
  
  float px = 0.0, py = 0.0, prevAngle, startAngle;

  nsCOMPtr<nsIDOMSVGPoint> point;
  for (PRUint32 i = 0; i < count; ++i) {
    mPoints->GetItem(i, getter_AddRefs(point));

    float x, y;
    point->GetX(&x);
    point->GetY(&y);

    float angle = atan2(y-py, x-px);
    if (i == 1)
      startAngle = angle;
    else if (i > 1)
      ((nsSVGMark *)aMarks->ElementAt(aMarks->Count()-1))->angle = 
        nsSVGUtils::AngleBisect(prevAngle, angle);

    nsSVGMark *mark;
    mark = new nsSVGMark;
    mark->x = x;
    mark->y = y;
    aMarks->AppendElement(mark);

    prevAngle = angle;
    px = x;
    py = y;
  }

  float nx, ny, angle;
  mPoints->GetItem(0, getter_AddRefs(point));
  point->GetX(&nx);
  point->GetY(&ny);
  angle = atan2(ny - py, nx - px);

  ((nsSVGMark *)aMarks->ElementAt(aMarks->Count()-1))->angle = 
    nsSVGUtils::AngleBisect(prevAngle, angle);
  ((nsSVGMark *)aMarks->ElementAt(0))->angle =
    nsSVGUtils::AngleBisect(angle, startAngle);
}

nsIAtom *
nsSVGPolygonFrame::GetType() const
{
  return nsLayoutAtoms::svgPolygonFrame;
}
