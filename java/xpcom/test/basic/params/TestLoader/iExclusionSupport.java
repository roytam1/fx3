/*
 * ************* DO NOT EDIT THIS FILE ***********
 *
 * This file was automatically generated from iExclusionSupport.idl.
 */


package org.mozilla.xpcom;


/**
 * Interface iExclusionSupport
 *
 * IID: 0xf271bdb9-44e8-4f34-a345-490300f1f141
 */

public interface iExclusionSupport extends nsISupports
{
    public static final IID IID =
       new IID("f271bdb9-44e8-4f34-a345-490300f1f141");


    /* void exclude (in unsigned long count, [array, size_is (count)] in string exclusionList); */
    public void exclude(int count, String[] exclusionList);

}

/*
 * end
 */
