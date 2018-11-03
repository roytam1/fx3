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

#ifndef nsFormControlHelper_h___
#define nsFormControlHelper_h___

#include "nsIFormControlFrame.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"
#include "nsCoord.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"

class nsIView;
class nsPresContext;
class nsStyleCoord;
class nsStyleContext;

#define CSS_NOTSET -1
#define ATTR_NOTSET -1

#define NS_STRING_TRUE   NS_LITERAL_STRING("1")
#define NS_STRING_FALSE  NS_LITERAL_STRING("0")

/**
  * Enumeration of possible mouse states used to detect mouse clicks
  */
enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseExit,
  eMouseDown,
  eMouseUp
};


/** 
  * nsFormControlHelper is the base class for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsFormControlHelper 
{

public:
  
  // returns the an addref'ed FontMetrics for the default font for the frame
  static nsresult GetFrameFontFM(nsIFrame* aFrame,
                                 nsIFontMetrics** aFontMet);

  // Map platform line endings (CR, CRLF, LF) to DOM line endings (LF)
  static void PlatformToDOMLineBreaks(nsString &aString);

  /**
   * Get whether a form control is disabled
   * @param aContent the content of the form control in question
   * @return whether the form control is disabled
   */
  static PRBool GetDisabled(nsIContent* aContent) {
    return aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::disabled);
  };

  /**
   * Cause the form control to reset its value
   * @param aFrame the frame who owns the form control
   * @param aPresContext the pres context
   */
  static nsresult Reset(nsIFrame* aFrame, nsPresContext* aPresContext);

  /** 
   * Utility to convert a string to a PRBool
   * @param aValue string to convert to a PRBool
   * @returns PR_TRUE if aValue = "1", PR_FALSE otherwise
   */
  static PRBool GetBool(const nsAString& aValue);

  /** 
   * Utility to convert a PRBool to a string
   * @param aValue Boolean value to convert to string.
   * @param aResult string to hold the boolean value. It is set to "1" 
   *        if aValue equals PR_TRUE, "0" if aValue equals PR_FALSE.
   */
  static void  GetBoolString(const PRBool aValue, nsAString& aResult);
  

  static void GetRepChars(char& char1, char& char2) {
    char1 = 'W';
    char2 = 'w';
  }

  // wrap can be one of these three values.  
  typedef enum {
    eHTMLTextWrap_Off     = 1,    // "off"
    eHTMLTextWrap_Hard    = 2,    // "hard"
    eHTMLTextWrap_Soft    = 3     // the default
  } nsHTMLTextWrap;

  static PRBool GetWrapPropertyEnum(nsIContent * aContent, nsHTMLTextWrap& aWrapProp);

//
//-------------------------------------------------------------------------------------
// Utility methods for rendering Form Elements using GFX
//-------------------------------------------------------------------------------------
//
// XXX: The following location for the paint code is TEMPORARY. 
// It is being used to get printing working
// under windows. Later it will be used to GFX-render the controls to the display. 
// Expect this code to repackaged and moved to a new location in the future.

   /**
    * Enumeration of possible mouse states used to detect mouse clicks
    */
   enum nsArrowDirection {
    eArrowDirection_Left,
    eArrowDirection_Right,
    eArrowDirection_Up,
    eArrowDirection_Down
   };

   /**
    * Scale, translate and convert an arrow of poitns from nscoord's to nsPoints's.
    *
    * @param aNumberOfPoints number of (x,y) pairs
    * @param aPoints arrow of points to convert
    * @param aScaleFactor scale factor to apply to each points translation.
    * @param aX x coordinate to add to each point after scaling
    * @param aY y coordinate to add to each point after scaling
    * @param aCenterX x coordinate of the center point in the original array of points.
    * @param aCenterY y coordiante of the center point in the original array of points.
    */

   static void SetupPoints(PRUint32 aNumberOfPoints, nscoord* aPoints, 
     nsPoint* aPolygon, nscoord aScaleFactor, nscoord aX, nscoord aY,
     nscoord aCenterX, nscoord aCenterY);

   /**
    * Paint a fat line. The line is drawn as a polygon with a specified width.
	  *  
    * @param aRenderingContext the rendering context
    * @param aSX starting x in pixels
	  * @param aSY starting y in pixels
	  * @param aEX ending x in pixels
	  * @param aEY ending y in pixels
    * @param aHorz PR_TRUE if aWidth is added to x coordinates to form polygon. If 
	  *              PR_FALSE  then aWidth as added to the y coordinates.
    * @param aOnePixel number of twips in a single pixel.
    */

  static void PaintLine(nsIRenderingContext& aRenderingContext, 
                 nscoord aSX, nscoord aSY, nscoord aEX, nscoord aEY, 
                 PRBool aHorz, nscoord aWidth, nscoord aOnePixel);

   /**
    * Paint a fixed size checkmark
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    */

  static void PaintFixedSizeCheckMark(nsIRenderingContext& aRenderingContext, 
                                     float aPixelsToTwips);
                       
  /**
    * Paint a checkmark
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    * @param aWidth width in twips
    * @param aHeight height in twips
    */

  static void PaintCheckMark(nsIRenderingContext& aRenderingContext,
                             float aPixelsToTwips, const nsRect & aRect);

   /**
    * Paint a fixed size checkmark border
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    * @param aBackgroundColor color for background of the checkbox 
    */

  static void PaintFixedSizeCheckMarkBorder(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips, const nsStyleColor& aBackgroundColor);

   /**
    * Paint a rectangular button. Includes background, string, and focus indicator
	  * 
    * @param aPresContext the presentation context
    * @param aRenderingContext the rendering context
    * @param aDirtyRect rectangle requiring update
    * @param aRect  x,y, width, and height of the button in TWIPS
    * @param aShift if PR_TRUE offset button as if it were pressed
    * @param aShowFocus if PR_TRUE draw focus rectangle over button
    * @param aStyleContext style context used for drawing button background 
    * @param aLabel label for button
    * @param aForFrame the frame that the scrollbar will be rendered in to
    */

  static void PaintRectangularButton(nsPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect, const nsRect& aRect, 
                            PRBool aShift, PRBool aShowFocus, PRBool aDisabled,
							              PRBool aDrawOutline,
							              nsStyleContext* aOutlineStyle,
							              nsStyleContext* aFocusStyle,
                            nsStyleContext* aStyleContext, nsString& aLabel, 
                            nsIFrame* aForFrame);

  static void StyleChangeReflow(nsPresContext* aPresContext,
                                nsIFrame* aFrame);

protected:
  nsFormControlHelper();
  virtual ~nsFormControlHelper();

};

#endif

