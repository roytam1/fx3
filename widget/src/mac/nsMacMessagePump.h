/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// 
// nsMacMessagePump
//
// This file contains the default implementation for the mac event loop. Events that
// pertain to the layout engine are routed there via a MessageSink that is passed in
// at creation time. Events not destined for layout are handled here (such as window
// moved).
//
// Clients may either use this implementation or write their own. Embedding applications
// will almost certainly write their own because they will want control of the event
// loop to do other processing. There is nothing in the architecture which forces the
// embedding app to use anything called a "message pump" so the event loop can actually
// live anywhere the app wants.
//

#ifndef nsMacMessagePump_h__
#define nsMacMessagePump_h__


#include <Events.h>
#include "prtypes.h"
#include "nsIEventQueueService.h"

class nsToolkit;
class nsMacTSMMessagePump;
class nsIEventSink;
class nsIWidget;


//================================================

// Macintosh Message Pump Class
class nsMacMessagePump
{
	// CLASS MEMBERS
private:
	PRBool					mRunning;
	Point					mMousePoint;	// keep track of where the mouse is at all times
	RgnHandle				mMouseRgn;
	nsToolkit*				mToolkit;
	nsMacTSMMessagePump*	mTSMMessagePump;

	// CLASS METHODS
		    	    
public:
						nsMacMessagePump(nsToolkit *aToolKit);
	virtual 	~nsMacMessagePump();
  
	void			DoMessagePump();
	PRBool		GetEvent(EventRecord &theEvent);
	// returns true if handled
	PRBool		DispatchEvent(PRBool aRealEvent, EventRecord *anEvent);
	void			StartRunning() {mRunning = PR_TRUE;}
	void			StopRunning() {mRunning = PR_FALSE;}

private:  

  // these all return PR_TRUE if the event was handled
	PRBool 			DoMouseDown(EventRecord &anEvent);
	PRBool			DoMouseUp(EventRecord &anEvent);
	PRBool			DoMouseMove(EventRecord &anEvent);
	PRBool			DoUpdate(EventRecord &anEvent);
	PRBool 			DoKey(EventRecord &anEvent);
#if USE_MENUSELECT
	PRBool 			DoMenu(EventRecord &anEvent, long menuResult);
#endif
	PRBool 			DoDisk(const EventRecord &anEvent);
	PRBool			DoActivate(EventRecord &anEvent);
	void			DoIdle(EventRecord &anEvent);

	PRBool		DispatchOSEventToRaptor(EventRecord &anEvent, WindowPtr aWindow);
#if USE_MENUSELECT
	PRBool		DispatchMenuCommandToRaptor(EventRecord &anEvent, long menuResult);
#endif

	PRBool		BrowserIsBusy();

	WindowPtr GetFrontApplicationWindow();

  static pascal OSStatus	CarbonMouseHandler(EventHandlerCallRef nextHandler,
                                            EventRef theEvent, void *userData);
};



#endif // nsMacMessagePump_h__

