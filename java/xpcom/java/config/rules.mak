#!gmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
# Igor Kushnirskiy <idk@eng.sun.com>
#

XPIDL_PROG=$(DIST)\bin\xpidl
.SUFFIXES: .java .idl
XPIDL_JAVA=$(JAVAXPIDLSRCS:.idl=.java)

idl2java: $(XPIDL_JAVA)
.idl.java:
	$(XPIDL_PROG) -m java -w $(XPIDL_INCLUDES) -I$(DEPTH)\dist\idl\ $<
