/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *  Daniel Glazman <daniel@glazman.org>, on behalf of Lindows.com
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

#include "nsXULAppAPI.h"
#include "nsISoftwareUpdate.h"
#ifdef XP_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
  nsXREAppData appData;
  appData.SetSplashEnabled(PR_FALSE);
  appData.SetProductName(NS_LITERAL_CSTRING("Composer"));
  appData.SetUseStartupPrefs(PR_TRUE);
  return xre_main(argc, argv, appData);
}

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)
char* splash_xpm[] = {0};
#endif

#if defined XP_WIN && !defined __GNUC__
// We need WinMain in order to not be a console app.  This function is
// unused if we are a console application.
//
// However, if using MingW on Win32, we can't use this as GCC complains
// about the use of main.
int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR args, int )
{
    // Do the real work.
    return main( __argc, __argv );
}
#endif
