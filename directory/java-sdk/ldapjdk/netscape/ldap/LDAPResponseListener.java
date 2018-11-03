/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package netscape.ldap;


/**
 * Represents the message queue associated with a particular LDAP
 * operation or operations.
 * 
 */
public class LDAPResponseListener extends LDAPMessageQueue{

    static final long serialVersionUID = 901897097111294329L;

    /**
     * Constructor
     * @param asynchOp a boolean flag that is true if the object is used for 
     * asynchronous LDAP operations
     * @see netscape.ldap.LDAPAsynchronousConnection
     */
    LDAPResponseListener(boolean asynchOp) {
        super(asynchOp);
    }
    
    /**
     * Blocks until a response is available, or until all operations
     * associated with the object have completed or been canceled, and
     * returns the response.
     *
     * @return a response for an LDAP operation or null if there are no
     * more outstanding requests.
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */
    public LDAPResponse getResponse() throws LDAPException {
        return (LDAPResponse)nextMessage();
    }

    /**
     * Merge two response listeners.
     * Move/append the content from another response listener to this one.
     * <P>
     * To be used for synchronization of asynchronous LDAP operations where
     * requests are sent by one thread but processed by another one.
     * <P>
     * A client may be implemented in such a way that one thread makes LDAP
     * requests and calls l.getMessageIDs(), while another thread
     * is responsible for processing of responses (call l.getResponse()).
     * Both threads are using
     * the same listener objects. In such a case, a race
     * condition may occur, where a LDAP response message is retrieved and
     * the request terminated (request ID removed) before the first thread
     * has a chance to execute l.getMessageIDs().
     * The proper way to handle this scenario is to create a separate listener
     * for each new request, and after l.getMessageIDs() has been invoked, merge the
     * new request with the existing one.
     * @param listener2 the listener with which to merge
     */
    public void merge(LDAPResponseListener listener2) {
        super.merge(listener2);
    }
    
    /**
     * Reports true if a response has been received from the server.
     *
     * @return a flag indicating whether the response message queue is empty.
     */
    public boolean isResponseReceived() {
        return super.isMessageReceived();
    }

    /**
     * Returns message IDs for all outstanding requests
     * @return message ID array.
     */
    public int[] getMessageIDs() {
        return super.getMessageIDs();
    }
}
