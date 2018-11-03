/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*******************************************************************************
 * Source date: 9 Apr 1997 21:45:13 GMT
 * netscape/fonts/cfp module C stub file
 * Generated by jmc version 1.8 -- DO NOT EDIT
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "xp_mem.h"

/* Include the implementation-specific header: */
#include "Pcfp.h"

/* Include other interface headers: */

/*******************************************************************************
 * cfp Methods
 ******************************************************************************/

#ifndef OVERRIDE_cfp_getInterface
JMC_PUBLIC_API(void*)
_cfp_getInterface(struct cfp* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &cfp_ID, sizeof(JMCInterfaceID)) == 0)
		return cfpImpl2cfp(cfp2cfpImpl(self));
	return _cfp_getBackwardCompatibleInterface(self, iid, exc);
}
#endif

#ifndef OVERRIDE_cfp_addRef
JMC_PUBLIC_API(void)
_cfp_addRef(struct cfp* self, jint op, JMCException* *exc)
{
	cfpImplHeader* impl = (cfpImplHeader*)cfp2cfpImpl(self);
	impl->refcount++;
}
#endif

#ifndef OVERRIDE_cfp_release
JMC_PUBLIC_API(void)
_cfp_release(struct cfp* self, jint op, JMCException* *exc)
{
	cfpImplHeader* impl = (cfpImplHeader*)cfp2cfpImpl(self);
	if (--impl->refcount == 0) {
		cfp_finalize(self, exc);
	}
}
#endif

#ifndef OVERRIDE_cfp_hashCode
JMC_PUBLIC_API(jint)
_cfp_hashCode(struct cfp* self, jint op, JMCException* *exc)
{
	return (jint)self;
}
#endif

#ifndef OVERRIDE_cfp_equals
JMC_PUBLIC_API(jbool)
_cfp_equals(struct cfp* self, jint op, void* obj, JMCException* *exc)
{
	return self == obj;
}
#endif

#ifndef OVERRIDE_cfp_clone
JMC_PUBLIC_API(void*)
_cfp_clone(struct cfp* self, jint op, JMCException* *exc)
{
	cfpImpl* impl = cfp2cfpImpl(self);
	cfpImpl* newImpl = (cfpImpl*)malloc(sizeof(cfpImpl));
	if (newImpl == NULL) return NULL;
	memcpy(newImpl, impl, sizeof(cfpImpl));
	((cfpImplHeader*)newImpl)->refcount = 1;
	return newImpl;
}
#endif

#ifndef OVERRIDE_cfp_toString
JMC_PUBLIC_API(const char*)
_cfp_toString(struct cfp* self, jint op, JMCException* *exc)
{
	return NULL;
}
#endif

#ifndef OVERRIDE_cfp_finalize
JMC_PUBLIC_API(void)
_cfp_finalize(struct cfp* self, jint op, JMCException* *exc)
{
	/* Override this method and add your own finalization here. */
	XP_FREEIF(self);
}
#endif

/*******************************************************************************
 * Jump Tables
 ******************************************************************************/

const struct cfpInterface cfpVtable = {
	_cfp_getInterface,
	_cfp_addRef,
	_cfp_release,
	_cfp_hashCode,
	_cfp_equals,
	_cfp_clone,
	_cfp_toString,
	_cfp_finalize,
	_cfp_LookupFont,
	_cfp_CreateFontFromFile,
	_cfp_CreateFontStreamHandler,
	_cfp_EnumerateSizes,
	_cfp_ReleaseFontHandle,
	_cfp_GetMatchInfo,
	_cfp_GetRenderableFont,
	_cfp_Name,
	_cfp_Description,
	_cfp_EnumerateMimeTypes,
	_cfp_ListFonts,
	_cfp_ListSizes,
	_cfp_CacheFontInfo
};

/*******************************************************************************
 * Factory Operations
 ******************************************************************************/

JMC_PUBLIC_API(cfp*)
cfpFactory_Create(JMCException* *exception, struct nffbp* a)
{
	cfpImplHeader* impl = (cfpImplHeader*)XP_NEW_ZAP(cfpImpl);
	cfp* self;
	if (impl == NULL) {
		JMC_EXCEPTION(exception, JMCEXCEPTION_OUT_OF_MEMORY);
		return NULL;
	}
	self = cfpImpl2cfp(impl);
	impl->vtablecfp = &cfpVtable;
	impl->refcount = 1;
	_cfp_init(self, exception, a);
	if (JMC_EXCEPTION_RETURNED(exception)) {
		XP_FREE(impl);
		return NULL;
	}
	return self;
}

