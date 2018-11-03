/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

package org.mozilla.dom.events;

import org.w3c.dom.Node;
import org.w3c.dom.events.Event;
import org.w3c.dom.events.EventTarget;

/**
 * The <code>Event</code> interface is used to provide contextual information 
 * about an event to the handler processing the event.  An object which 
 * implements the <code>Event</code> interface is generally passed as the 
 * first parameter to an event handler.  More specific  context information 
 * is passed to event handlers by deriving additional interfaces from  
 * <code>Event</code> which contain information directly relating to the type 
 * of event they accompany.  These derived interfaces are also implemented by 
 * the object passed to the event listener. 
 * @since DOM Level 2
 */
public class EventImpl implements Event {

    protected long p_nsIDOMEvent = 0;

    // instantiated from JNI only
    protected EventImpl() {}

    public String toString() {
	return "<c=org.mozilla.dom.events.Event type=" + getType() + 
	    " target=" + getTarget() +
            " phase=" + getEventPhase() +
            " p=" + Long.toHexString(p_nsIDOMEvent) + ">";
    }

    /**
     * The <code>type</code> property represents the event name as a string 
     * property. 
     */
    public native String getType();
    
    /**
     * The <code>target</code> property indicates the <code>EventTarget</code> 
     * to which the event  was originally dispatched. 
     */
    public native EventTarget getTarget();

    /**
     * The <code>currentNode</code> property indicates the <code>Node</code> 
     * whose <code>EventListener</code>s are currently being processed.  This 
     * is particularly  useful during capturing and bubbling. 
     */
    public native EventTarget getCurrentTarget();

    /**
     * The <code>eventPhase</code> property indicates which phase of event flow 
     * is currently  being evaluated. 
     */
    public native short getEventPhase();
    
    /**
     * The <code>bubbles</code> property indicates whether or not an event is a 
     * bubbling event.  If the event can bubble the value is true, else the 
     * value is false. 
     */
    public native boolean getBubbles();

    /**
     * The <code>cancelable</code> property indicates whether or not an event 
     * can have its default action prevented.  If the default action can be 
     * prevented the value is true, else the value is false. 
     */
    public native boolean getCancelable();

    /**
     * The <code>preventBubble</code> method is used to end the bubbling phase 
     * of  event flow. If this method is called by any 
     * <code>EventListener</code>s registered on the same 
     * <code>EventTarget</code> during bubbling, the bubbling phase will cease 
     * at that level and the event will not be propagated upward within the 
     * tree. 
     */
    public native void preventBubble();
    
    /**
     * The <code>preventCapture</code> method is used to end the capturing phase 
     * of  event flow. If this method is called by any 
     * <code>EventListener</code>s registered on the same 
     * <code>EventTarget</code> during capturing, the capturing phase will 
     * cease at that level and the event will not be propagated any further 
     * down. 
     */
    public native void preventCapture();

    /**
     * If an event is cancelable, the <code>preventCapture</code> method is used 
     * to signify that the event is to be canceled, meaning any default action 
     * normally taken by the implementation as a result of the event will not 
     * occur.  If, during any stage of event flow, the 
     * <code>preventDefault</code> method is called the event is canceled. Any 
     * default action associated with the event will not occur.  Calling this 
     * method for a non-cancelable event has no effect.  Once 
     * <code>preventDefault</code> has been called it will remain in effect 
     * throughout the remainder of the event's propagation. 
     */
    public native void preventDefault();
    
    /**
     * The <code>stopPropagation</code> method is used prevent further 
     * propagation of an event during event flow. If this method is called by 
     * any <code>EventListener</code> the event will cease propagating 
     * through the tree.  The event will complete dispatch to all listeners 
     * on the current <code>EventTarget</code> before event flow stops.  This 
     * method may be used during any stage of event flow.
     */
    public native void stopPropagation();
    
    /**
     * 
     * @param eventTypeArg Specifies the event type.  This type may be any event 
     *   type currently defined in this specification or a new event type.  Any 
     *   new event type must not begin with any upper, lower, or mixed case 
     *   version of the string  "DOM".  This prefix is reserved for future DOM 
     *   event sets.
     * @param canBubbleArg Specifies whether or not the event can bubble.
     * @param cancelableArg Specifies whether or not the event's default  action 
     *   can be prevented.
     */
    public native void initEvent(String eventTypeArg, 
				 boolean canBubbleArg, 
				 boolean cancelableArg);
    
    public native long getTimeStamp();
}
