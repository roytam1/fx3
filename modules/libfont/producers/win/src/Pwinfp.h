/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
 * Private implementation of the FontDisplayer.
 */

#ifndef _Pwinfp_H_
#define _Pwinfp_H_

#ifndef XP_WIN
#define XP_WIN
#endif

#include "Mnffmi.h"
#include "Mnfrc.h"
#include "Mnfdoer.h"
#include "Mwinfp.h"			/* Generated header */

// structure to hold each font broker asked by lookupFont()
typedef struct NetscapePrimeFont_s {
    LOGFONT						logFontInPrimeFont;
	int							csIDInPrimeFont;
	int							encordingInPrimeFont;
	int							YPixelPerInch;
	struct NetscapePrimeFont_s	*nextFont;       // link for list.
}	* pPrimeFont_t ;


struct winfpImpl {
  winfpImplHeader	header;

  /*************************************************************************
   * FONTDISPLAYER Implementors:
   *	Add your private data here. If you are implementing in C++,
   *	then hang off a pointer to your actual object here.
   *************************************************************************/

    struct nffbp    *m_pBrokerObj;

	/*** The following list need not be maintained at all. - dp
    // header of font link list.
	pPrimeFont_t	m_pPrimeFontList;
	***/
};

/* The generated getInterface used the wrong object IDS. So we
 * override them with ours.
 */
#define OVERRIDE_winfp_getInterface

/* The generated finalize doesn't have provision to free the
 * private data that we create inside the object. So we
 * override the finalize method and implement destruction
 * of our private data.
 */
#define OVERRIDE_winfp_finalize

#endif /* _Pwinfp_H_ */
