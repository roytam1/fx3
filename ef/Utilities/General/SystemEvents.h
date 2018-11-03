/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// SystemEvents.h
//
// Scott M. Silver
//

// EventListener and EventBroadcaster which work together
// to distribute interesting events around a system.

class EventListener;
class EventBroadcaster
{
	Vector<EventListener*>	mListeners;
	void	addListener(EventListener&	inListener) { mListeners.append(&inListener); }
	void	removeListener(EventListener& inListener) { }
	void	broadcastEvent(BroadcastEventKind inKind, void* inArgument);

	static EventBroadcaster&	checkAllocated(EventBroadcaster*& ioBroadcaster)
	{
		if (!ioBroadcaster)
			ioBroadcaster = new EventBroadcaster();

		return *ioBroadcaster;
	}
	
public:
	static  void addListener(EventBroadcaster*& ioBroadcaster, EventListener& inListener)
	{
		checkAllocated(ioBroadcaster).addListener(inListener);
	}

	static  void broadcastEvent(EventBroadcaster*& ioBroadcaster, BroadcastEventKind inKind, void* inArgument)
	{
		checkAllocated(ioBroadcaster).broadcastEvent(inKind, inArgument);
	}

	static  void removeListener(EventBroadcaster*& ioBroadcaster, EventListener& inListener)
	{
		checkAllocated(ioBroadcaster).removeListener(inListener);
	}

};


class EventListener
{
#ifdef DEBUG
	Vector<EventBroadcaster*>	mBroadcasters;
#endif


public:
						EventListener(EventBroadcaster*& ioBroadcaster)
						{
							ioBroadcaster->addListener(ioBroadcaster, *this);
						}


						~EventListener()								{  }

	virtual void		listenToMessage(BroadcastEventKind inKind, void* inArgument) = 0;
};
