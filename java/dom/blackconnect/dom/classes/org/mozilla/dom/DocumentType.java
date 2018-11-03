/*
 * ************* DO NOT EDIT THIS FILE ***********
 *
 * This file was automatically generated from dom.idl.
 */


package org.mozilla.dom;

import org.mozilla.xpcom.*;


/**
 * Interface DocumentType
 *
 * IID: 0x0c07ada0-9ad5-11d4-a983-00105ae3801e
 */

public interface DocumentType extends Node
{
    public static final String IID =
        "0c07ada0-9ad5-11d4-a983-00105ae3801e";


    /* readonly attribute DOMString name; */
    public String getName();

    /* readonly attribute NamedNodeMap entities; */
    public NamedNodeMap getEntities();

    /* readonly attribute NamedNodeMap notations; */
    public NamedNodeMap getNotations();

    /* readonly attribute DOMString publicId; */
    public String getPublicId();

    /* readonly attribute DOMString systemId; */
    public String getSystemId();

    /* readonly attribute DOMString internalSubset; */
    public String getInternalSubset();

}

/*
 * end
 */
