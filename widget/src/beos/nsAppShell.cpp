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
 *   Duncan Wilcox <duncan@be.com>
 *   Yannick Koehler <ykoehler@mythrium.com>
 *   Makoto Hamanaka <VYA04230@nifty.com>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
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

#include "nsAppShell.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"
#include "nsSwitchToUIThread.h"
#include "plevent.h"
#include "prprf.h"
#include "nsGUIEvent.h"

#include <Application.h>
#include <stdlib.h>

static int gBAppCount = 0;

struct ThreadInterfaceData
{
  void	*data;
  thread_id waitingThread;
};

struct EventItem
{
  int32 code;
  ThreadInterfaceData ifdata;
};

static sem_id my_find_sem(const char *name)
{
  sem_id	ret = B_ERROR;

  /* Get the sem_info for every sempahore in this team. */
  sem_info info;
  int32 cookie = 0;

  while(get_next_sem_info(0, &cookie, &info) == B_OK)
  {
    if(strcmp(name, info.name) == 0)
    {
      ret = info.sem;
      break;
    }
  }
  return ret;
}


//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
NS_IMPL_THREADSAFE_ISUPPORTS1(nsAppShell, nsIAppShell)

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
	: is_port_error(false)
{ 
  gBAppCount++;
}


//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Create(int* argc, char ** argv)
{
  // system wide unique names
  // NOTE: this needs to be run from within the main application thread
  char		portname[64];
  char		semname[64];
  PR_snprintf(portname, sizeof(portname), "event%lx", 
              (long unsigned) PR_GetCurrentThread());
  PR_snprintf(semname, sizeof(semname), "sync%lx", 
              (long unsigned) PR_GetCurrentThread());
              
#ifdef DEBUG              
  printf("nsAppShell::Create portname: %s, semname: %s\n", portname, semname);
#endif
  /* 
   * Set up the port for communicating. As restarts thru execv may occur
   * and ports survive those (with faulty events as result). Combined with the fact
   * that plevent.c can setup the port ahead of us we need to take extra
   * care that the port is created for this launch, otherwise we need to reopen it
   * so that faulty messages gets lost.
   *
   * We do this by checking if the sem has been created. If it is we can reuse the port (if it exists).
   * Otherwise we need to create the sem and the port, deleting any open ports before.
   * TODO: The semaphore is no longer needed for syncing, so it's only use is for detecting if the
   * port needs to be reopened. This should be replaced, but I'm not sure how -tqh
   */
  syncsem = my_find_sem(semname);
  eventport = find_port(portname);
  if(B_ERROR != syncsem) 
  {
    if(eventport < 0)
    {
      eventport = create_port(200, portname);
  }
    return NS_OK;
  } 
  if(eventport >= 0)
  {
    delete_port(eventport);
  }
  eventport = create_port(200, portname);
  syncsem = create_sem(0, semname);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Run()
{
  int32               code;
  ThreadInterfaceData id;

  NS_ADDREF_THIS();

  set_thread_priority( find_thread(NULL), B_DISPLAY_PRIORITY);

  if (!mEventQueue)
    Spinup();

  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;

  while (!is_port_error)
  {
    RetrieveAllEvents(true);
    
    while (CountStoredEvents() > 0) {
      // get an event of the best priority
      EventItem *newitem = (EventItem *) GetNextEvent();
      if (!newitem) break;
      
      code = newitem->code;
      id = newitem->ifdata;
      
      switch(code)
      {
      case WM_CALLMETHOD :
        {
          MethodInfo *mInfo = (MethodInfo *)id.data;
          mInfo->Invoke();
          if(id.waitingThread != 0)
          {
            resume_thread(id.waitingThread);
          }
          delete mInfo;
        }
        break;

      case 'natv' :	// native queue PLEvent
        if (mEventQueue)
          mEventQueue->ProcessPendingEvents();
        break;

      default :
#ifdef DEBUG
        printf("nsAppShell::Run - UNKNOWN EVENT\n");
#endif
        break;
      }

      delete newitem;
      newitem = nsnull;
      
      RetrieveAllEvents(false); // get newer messages (non-block)
    }
  }

  Spindown();

  Release();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Exit()
{
#ifdef DEBUG
  fprintf(stderr, "nsAppShell::Exit() called\n");
#endif
  // interrupt message flow
  close_port(eventport);
  delete_sem(syncsem);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  if(--gBAppCount == 0)
  {
    if(be_app->Lock())
    {
      be_app->Quit();
    }
  }
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  // To be implemented.
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Spinup - do any preparation necessary for running a message loop
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spinup()
{
  nsresult   rv = NS_OK;

  // Get the event queue service
  nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID, &rv);

  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

  //Get the event queue for the thread.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
  
  // If we got an event queue, use it.
  if (mEventQueue)
    goto done;

  // otherwise create a new event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }

  // Ask again nicely for the event queue now that we have created one.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));

done:
  ListenToEventQueue(mEventQueue, PR_TRUE);
  return rv;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spindown()
{
  if (mEventQueue) {
    ListenToEventQueue(mEventQueue, PR_FALSE);
    mEventQueue->ProcessPendingEvents();
    mEventQueue = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  aRealEvent = PR_FALSE;
  aEvent = 0;

  return NS_OK;
}

NS_IMETHODIMP nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  // should we check for eventport initialization ?
  char  portname[64];
  PR_snprintf(portname, sizeof(portname), "event%lx", 
              (long unsigned) PR_GetCurrentThread());

  if((eventport = find_port(portname)) < 0) 
  {
    // not initialized
#ifdef DEBUG
    printf("nsAppShell::DispatchNativeEvent() was called before init\n");
#endif
    fflush(stdout);
    return NS_ERROR_FAILURE;
  }

  int32 code;
  ThreadInterfaceData id;
  id.data = 0;
  id.waitingThread = 0;
  bool gotMessage = false;

  do 
  {
      if (CountStoredEvents() == 0)
        RetrieveAllEvents(true); // queue is empty. block until new message comes.
      
      EventItem *newitem = (EventItem *) GetNextEvent();
      if (!newitem) continue;
      
      code = newitem->code;
      id = newitem->ifdata;

      switch(code) 
      {
        case WM_CALLMETHOD :
          {
            MethodInfo *mInfo = (MethodInfo *)id.data;
            mInfo->Invoke();
            if(id.waitingThread != 0)
            {
              resume_thread(id.waitingThread);
            }
            delete mInfo;
            gotMessage = PR_TRUE;
          }
          break;
        
        case 'natv' :	// native queue PLEvent
          {
            if (mEventQueue)
              mEventQueue->ProcessPendingEvents();
            gotMessage = PR_TRUE;
          }
          break;
          
        default :
#ifdef DEBUG
          printf("nsAppShell::Run - UNKNOWN EVENT\n");
#endif
          break;
      }

      delete newitem;
      newitem = nsnull;

  } while (!gotMessage);

  return NS_OK;
}

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue, PRBool aListen)
{
  // do nothing
  return NS_OK;
}

// count all stored events 
int nsAppShell::CountStoredEvents()
{
  int count = 0;
  for (int i=0 ; i < PRIORITY_LEVELS ; i++)
    count += events[i].CountItems();
  
  return count;
}

// get an event of the best priority
void *nsAppShell::GetNextEvent()
{
  void *newitem = nsnull;
  for (int i=0 ; i < PRIORITY_LEVELS ; i++) {
    if (!events[i].IsEmpty()) {
      newitem = events[i].RemoveItem((long int)0);
      break;
    }
  }
  return newitem;
}

// get all the messages on the port and dispatch them to 
// several queues by priority.
void nsAppShell::RetrieveAllEvents(bool blockable)
{
  if (is_port_error) return;
  
  bool is_first_loop = true;
  while(true)
  {
    EventItem *newitem = new EventItem;
    if ( !newitem ) break;

    newitem->code = 0;
    newitem->ifdata.data = nsnull;
    newitem->ifdata.waitingThread = 0;

    // only block on read_port when 
    //   blockable == true
    //   and
    //   this is the first loop
    // otherwise, return immediately.
    if ( (!is_first_loop || !blockable) && port_count(eventport) <= 0 ) {
      delete newitem;
      break;
    }
    is_first_loop = false;
    if ( read_port(eventport, &newitem->code, &newitem->ifdata, sizeof(newitem->ifdata)) < 0 ) {
      delete newitem;
      is_port_error = true;
      return;
    }
    // synchronous events should be processed quickly (?)
    if (newitem->ifdata.waitingThread != 0) {
      events[PRIORITY_TOP].AddItem(newitem);
    } else {
      switch(newitem->code)
      {
      case WM_CALLMETHOD :
        {
          MethodInfo *mInfo = (MethodInfo *)newitem->ifdata.data;
          switch( mInfo->methodId ) {
          case nsSwitchToUIThread::ONKEY :
            events[PRIORITY_SECOND].AddItem(newitem);
            break;
          case nsSwitchToUIThread::ONMOUSE:
            ConsumeRedundantMouseMoveEvent(mInfo);
            events[PRIORITY_THIRD].AddItem(newitem);
            break;
          case nsSwitchToUIThread::ONWHEEL :
          case nsSwitchToUIThread::BTNCLICK :
            events[PRIORITY_THIRD].AddItem(newitem);
            break;
          default:
            events[PRIORITY_NORMAL].AddItem(newitem);
            break;
          }
        }
        break;
            
      case 'natv' :	// native queue PLEvent
        events[PRIORITY_LOW].AddItem(newitem);
        break;
      }
    }
  }
  return;
}

// detect sequential NS_MOUSE_MOVE event and delete older one,
// for the purpose of performance
void nsAppShell::ConsumeRedundantMouseMoveEvent(MethodInfo *pNewEventMInfo)
{
  if (pNewEventMInfo->args[0] != NS_MOUSE_MOVE) return;

  nsISupports *widget0 = pNewEventMInfo->widget;
  nsSwitchToUIThread *target0 = pNewEventMInfo->target;
  
  int count = events[PRIORITY_THIRD].CountItems();
  for (int i=count-1 ; i >= 0 ; i --) {
    EventItem *previtem = (EventItem *)events[PRIORITY_THIRD].ItemAt(i);
    if (!previtem) continue;
    MethodInfo *mInfoPrev = (MethodInfo *)previtem->ifdata.data;
    if (!mInfoPrev
      || mInfoPrev->widget != widget0
      || mInfoPrev->target != target0) continue;
    // if other mouse event was found, then no sequential.
    if (mInfoPrev->args[0] != NS_MOUSE_MOVE) break;
    // check if other conditions are the same
    if (mInfoPrev->args[3] == pNewEventMInfo->args[3]
      && mInfoPrev->args[4] == pNewEventMInfo->args[4]) {
      // sequential mouse move found!
      events[PRIORITY_THIRD].RemoveItem(previtem);
      delete mInfoPrev;
      //if it's a synchronized call also wake up thread.
      if(previtem->ifdata.waitingThread != 0)
        resume_thread(previtem->ifdata.waitingThread);
      delete previtem;
      break;
    }
  }
  return;
}

