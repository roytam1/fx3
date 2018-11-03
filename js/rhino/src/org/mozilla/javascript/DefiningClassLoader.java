/*
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Roger Lawrence
 * Patrick Beard
 * Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript;

/**
 * Load generated classes.
 *
 * @author Norris Boyd
 */
public class DefiningClassLoader extends ClassLoader
    implements GeneratedClassLoader
{
    public DefiningClassLoader() {
        this.parentLoader = getClass().getClassLoader();
    }

    public DefiningClassLoader(ClassLoader parentLoader) {
        this.parentLoader = parentLoader;
    }

    public Class defineClass(String name, byte[] data) {
        return super.defineClass(name, data, 0, data.length);
    }

    public void linkClass(Class cl) {
        resolveClass(cl);
    }

    public Class loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        Class cl = findLoadedClass(name);
        if (cl == null) {
            if (parentLoader != null) {
                cl = parentLoader.loadClass(name);
            } else {
                cl = findSystemClass(name);
            }
        }
        if (resolve) {
            resolveClass(cl);
        }
        return cl;
    }

    private ClassLoader parentLoader;
}
