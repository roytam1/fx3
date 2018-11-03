/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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

package com.netscape.jsdebugging.api;

/**
 * When a hook is hit, the debugger records the state of the
 * thread before the hook in a ThreadState object.  This object
 * is then passed to any hook methods that are called, and can
 * be used to change the state of the thread when it resumes from the
 * hook.
 */
public interface ThreadStateBase
{
    /**
     * Return true if the Thread hasn't been resumed since this ThreadState
     * was made.
     */
    public boolean isValid();

    /**
     * partial list of thread states from sun.debug.ThreadInfo.
     * XXX some of these don't apply.
     */
    public int THR_STATUS_UNKNOWN       = 0x01;
    public int THR_STATUS_ZOMBIE        = 0x02;
    public int THR_STATUS_RUNNING       = 0x03;
    public int THR_STATUS_SLEEPING      = 0x04;
    public int THR_STATUS_MONWAIT       = 0x05;
    public int THR_STATUS_CONDWAIT      = 0x06;
    public int THR_STATUS_SUSPENDED     = 0x07;
    public int THR_STATUS_BREAK         = 0x08;

    /** 
     * Get the state of the thread at the time it entered debug mode.
     * This can't be modified directly.
     */
    public int getStatus();

    public int countStackFrames()
        throws InvalidInfoException;

    public StackFrameInfo getCurrentFrame()
        throws InvalidInfoException;

    /**
     * Get the thread's stack as an array.  stack[stack.length-1] is the
     * current frame, and stack[0] is the beginning of the stack.
     */
    public StackFrameInfo[] getStack()
        throws InvalidInfoException;

    /**
     * Return true if the thread is currently running a hook
     * for this ThreadState
     */
    public boolean isRunningHook();

    /**
     * Return true if the hook on this thread has already completed
     * and we are waiting for a call to resume()
     */
    public boolean isWaitingForResume();


    /**
     * Leave the thread in a suspended state when the hook method(s)
     * finish.  This can be undone by calling resume().
     */
    public void leaveSuspended();

    /**
    * Resume the thread. 
    * Two cases:
    *   1) Hook is still running, this will undo the leaveSuspended 
    *      and the thread will still be waiting for the hook to 
    *      return.
    *   2) Hook has already returned and we are in an event loop 
    *      waiting to contine the suspended thread, this will
    *      force completion of events pending on the suspended 
    *      thread and resume it.
    */
    public void resume();

    /**
     * if the continueState is DEAD, the thread cannot
     * be restarted.
     */
    public int DEBUG_STATE_DEAD     = 0x01;

    /**
     * if the continueState is RUN, the thread will
     * proceed to the next program counter value when it resumes.
     */
    public int DEBUG_STATE_RUN      = 0x02;

    /**
     * if the continueState is RETURN, the thread will
     * return from the current method with the value in getReturnValue()
     * when it resumes.
     */
    public int DEBUG_STATE_RETURN       = 0x03;

    /**
     * if the continueState is THROW, the thread will
     * throw an exception (accessible with getException()) when it
     * resumes.
     */
    public int DEBUG_STATE_THROW        = 0x04;

    /**
     * This gets the current continue state of the debug frame, which
     * will be one of the DEBUG_STATE_* values above.
     */
    public int getContinueState();

    public int setContinueState(int state);
}

