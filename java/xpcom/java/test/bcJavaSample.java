/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */
import org.mozilla.xpcom.*;
import java.lang.reflect.*;

public class bcJavaSample implements bcIJavaSample {
    bcIJavaSample object;
    public bcJavaSample() {
        System.out.println("--[java]bcJavaSample constructor");
    }
    public Object queryInterface(IID iid) {
        System.out.println("--[java]bcJavaSample::queryInterface iid="+iid);
        Object result;
        if ( iid.equals(nsISupports.IID)
             || iid.equals(bcIJavaSample.IID)) {
            result = this;
        } else {
            result = null;
        }
        System.out.println("--[java]bcJavaSample::queryInterface result=null "+(result==null));
        return result;
    }
    public void test0() {
        System.out.println("--[java]bcJavaSample.test0");
        nsIXPIDLServiceManager sm = Components.getServiceManager();
        nsISupports service = sm.getService(new CID("f0032af2-1dd1-11b2-bb75-c242dcb4f47a"), new IID("1f29f516-1dd2-11b2-9751-f129d72134d0"));
        System.out.println("--[java]bcJavaSample.test0 current thread "+Thread.currentThread()+"\n");
        Thread.dumpStack();
    }
    public void test1(int l) {
        System.out.println("--[java]bcJavaSample.test1 "+l+"\n");
        try {
            Thread.currentThread().sleep(1000);
        } catch (java.lang.InterruptedException e) {
        };
    }  
    public void test2(bcIJavaSample o) {
        System.out.println("--[java]bcJavaSample.test2");
        System.out.println("--[java]bcJavaSample.test2 :)))) Hi there");
        if (o != null) {
            System.out.println("--[java]bcJavaSample.test2 o!= null");
            o.test0();
            o.test1(1000);
            o.test2(this);
            int[] array={3,2,1};
            o.test3(3,array);
            { 
                String[] strings = {"4","3","2","1"};
                o.test6(4, strings);
            }
            System.out.println("--[java]bcJavaSample.test2 doing threads test\n");

            System.out.println("--[java]bcJavaSample.test2 current thread "+Thread.currentThread()+"\n");
            Thread.dumpStack();
            o.test2(this);
            object = o;
            new Thread( new Runnable() {
                    public void run() {
                        System.out.println("--[java]bcJavaSample.test2 current thread "+Thread.currentThread()+"\n");
                        object.test2(bcJavaSample.this);
                    }
                }).start();
            
        } else {
            System.out.println("--[java]bcJavaSample.test2 o = null");
        }

    }
    public void test3(int count, int[] valueArray) {
        System.out.println("--[java]bcJavaSample.test3");
        System.out.println(valueArray.length);
        for (int i = 0; i < valueArray.length; i++) {
            System.out.println("--[java]callMethodByIndex args["+i+"] = "+valueArray[i]);
        }

    }
    public void test4(int count, String[][] valueArray) {
        System.out.println("--[java]bcJavaSample.test4");
        String[] array = valueArray[0];
        for (int i = 0; i < array.length; i++) {
            System.out.println("--[java]bcJavaSample.test4 valueArray["+i+"] = "+array[i]);
        }
        String[] returnArray = {"4","3","2",null};
        valueArray[0] = returnArray;
    }
    /* void test5 (in nsIComponentManager cm); */
    public void test5(nsIComponentManager cm) {
        System.out.println("--[java]bcJavaSample.test5");
        try {
            nsIEnumerator retval;
            nsIEnumerator enumerator = cm.enumerateContractIDs();
            System.out.println("--[java] before calling enumerator.firts() "+
                               "enumerator==null "+(enumerator==null));

            enumerator.first();
            int counter = 0;
            nsISupports obj;
            String str;
            nsISupportsString strObj;
            while (true) {
                obj = enumerator.currentItem();
                //                if (obj == null 
                //  || counter > 300) {
                //  break;
                //}
                strObj = (nsISupportsString) obj.queryInterface(nsISupportsString.IID);
                str = strObj.getData();
                System.out.println("--[java] bcJavaSample.Test5 string "+str);
                enumerator.next(); counter++;
            }
        } catch (Exception e) {
            System.out.println(e);
        }

    }
    public void test6(int count, String[]  valueArray) {
        System.out.println("--[java]bcJavaSample.test6");
        String[] array = valueArray;
        for (int i = 0; i < array.length; i++) {
            System.out.println("--[java]bcJavaSample.test6 valueArray["+i+"] = "+array[i]);
        }
    }

    /* void test7 (out PRUint32 count, [array, size_is (count)] out char valueArray); */
    public void test7(int[] count, char[][] valueArray) {
        System.out.println("--[java]bcJavaSample.test7");
        char [] retValue = {'1','b','c','d'};
        count[0] = retValue.length;
        valueArray[0] = retValue;
    }
    /* void test8 (in nsCIDRef cid); */
    public void test8(CID cid) {
        System.out.println("--[java]bcJavaSample.test8 "+cid);
    }

    /* void test9 (out nsIIDPtr po); */
    public void test9(IID[] po) {
        System.out.println("--[java]bcJavaSample::Test9 "+po[0]);
    }

    static {
      try {
          Class nsIComponentManagerClass = 
              Class.forName("org.mozilla.xpcom.nsIComponentManager");
          Class nsIEnumeratorClass = 
              Class.forName("org.mozilla.xpcom.nsIEnumerator");
          Class nsISupportsStringClass = 
              Class.forName("org.mozilla.xpcom.nsISupportsString");
          InterfaceRegistry.register(nsIComponentManagerClass);
          InterfaceRegistry.register(nsIEnumeratorClass);
          InterfaceRegistry.register(nsISupportsStringClass);
      } catch (Exception e) {
          System.out.println(e);
      }
    }

};






