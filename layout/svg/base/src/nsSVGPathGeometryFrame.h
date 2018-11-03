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

#ifndef __NS_SVGPATHGEOMETRYFRAME_H__
#define __NS_SVGPATHGEOMETRYFRAME_H__

#include "nsFrame.h"
#include "nsISVGChildFrame.h"
#include "nsISVGPathGeometrySource.h"
#include "nsISVGRendererPathGeometry.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsSVGGradient.h"
#include "nsSVGPattern.h"
#include "nsLayoutAtoms.h"

class nsPresContext;
class nsIDOMSVGMatrix;
class nsISVGRendererRegion;
class nsISVGMarkerFrame;
class nsISVGFilterFrame;

typedef nsFrame nsSVGPathGeometryFrameBase;

class nsSVGPathGeometryFrame : public nsSVGPathGeometryFrameBase,
                               public nsISVGValueObserver,
                               public nsSupportsWeakReference,
                               public nsISVGPathGeometrySource,
                               public nsISVGChildFrame
{
protected:
  nsSVGPathGeometryFrame();
  virtual ~nsSVGPathGeometryFrame();
  
public:
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsIFrame interface:
  NS_IMETHOD
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsStyleContext*  aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  NS_IMETHOD DidSetStyleContext();

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgPathGeometryFrame
   */
  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPathGeometry"), aResult);
  }
#endif

protected:
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
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate);
  NS_IMETHOD SetOverrideCTM(nsIDOMSVGMatrix *aCTM);
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGGeometrySource interface: 
  NS_DECL_NSISVGGEOMETRYSOURCE
  
  // nsISVGPathGeometrySource interface:
  NS_IMETHOD GetHittestMask(PRUint16 *aHittestMask);
  NS_IMETHOD GetShapeRendering(PRUint16 *aShapeRendering);
  // to be implemented by subclass:
  //NS_IMETHOD ConstructPath(nsISVGRendererPathBuilder *pathBuilder) = 0;
  
protected:
  NS_IMETHOD InitSVG();
  void UpdateGraphic(PRUint32 flags, PRBool suppressInvalidation = PR_FALSE);
  nsISVGRendererPathGeometry *GetGeometry();

  nsCOMPtr<nsISVGRendererRegion> mMarkerRegion;

private:
  void GetMarkerFrames(nsISVGMarkerFrame **markerStart,
                       nsISVGMarkerFrame **markerMid,
                       nsISVGMarkerFrame **markerEnd);

  nsCOMPtr<nsISVGRendererPathGeometry> mGeometry;
  PRUint32 mUpdateFlags;
  nsISVGGradient*     mFillGradient;
  nsISVGGradient*     mStrokeGradient;
  nsCOMPtr<nsIDOMSVGMatrix>    mOverrideCTM;
  nsISVGPattern*      mFillPattern;
  nsISVGPattern*      mStrokePattern;

  PRPackedBool mPropagateTransform;
};

#endif // __NS_SVGPATHGEOMETRYFRAME_H__
