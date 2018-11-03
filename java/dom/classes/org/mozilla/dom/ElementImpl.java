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

package org.mozilla.dom;

import org.w3c.dom.Attr;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.DOMException;

public class ElementImpl extends NodeImpl implements Element {

    // instantiated from JNI or Document.createElement()
    private ElementImpl() {}

    public native String getAttribute(String name);
    public native Attr getAttributeNode(String name);
    public native NodeList getElementsByTagName(String name);
    public native String getTagName();
    public native void normalize();
    public native void removeAttribute(String name);
    public native Attr removeAttributeNode(Attr oldAttr);
    public native void setAttribute(String name, String value);
    public native Attr setAttributeNode(Attr newAttr);

    //since DOM2
    public native String getAttributeNS(String namespaceURI, String localName);
    public native void setAttributeNS(String namespaceURI, String qualifiedName, String value);
    public native void removeAttributeNS(String namespacURI, String localName);   
    public native Attr getAttributeNodeNS(String namespaceURI, String localName);
    public native Attr setAttributeNodeNS(Attr newAttr);
    public native NodeList getElementsByTagNameNS(String namespaceURI, String localName);
    public native boolean hasAttribute(String name);
    public native boolean hasAttributeNS(String namespaceURI, String localName);
}
