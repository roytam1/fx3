/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Serge Pikalev <sep@sparc.spb.su>
 */

import org.mozilla.xpcom.*;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.io.*;

public class nsXPIDLInputStream extends InputStream {

    public nsIXPIDLInputStream inputStream;

    public nsXPIDLInputStream( nsIXPIDLInputStream inputStream ) {
        this.inputStream = inputStream;
    }

    public void close() {
        inputStream.close();
    }

    public int available() {
        return inputStream.available();
    }

    public int read(byte[] b, int off, int len) throws IOException {
        return inputStream.read( len, b );
    }

    public int read() throws IOException {
        byte buf[] = new byte[1];
        if( read( buf, 0, 1 ) < 0 ) {
            return -1;
        } else {
            return buf[0] & 0xff;
        }
    }
}
