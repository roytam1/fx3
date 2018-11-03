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
 * The Original Code is Minimo.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef MINIMO_PRIVATE_H
#define MINIMO_PRIVATE_H

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

// System header files

#ifdef MOZ_WIDGET_GTK2
#include <gtk/gtk.h>
#endif

// Mozilla header files
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppShellCID.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsEmbedAPI.h"
#include "nsEmbedCID.h"
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIAppStartupNotifier.h"
#include "nsIBadCertListener.h"
#include "nsIClipboardCommands.h"
#include "nsIComponentRegistrar.h"
#include "nsIDOMWindow.h"
#include "nsIEventQueueService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPromptService.h"
#include "nsIPromptService.h"
#include "nsIStringBundle.h"
#include "nsITimelineService.h"
#include "nsIURI.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWidget.h"
#include "nsIWindowCreator.h"
#include "nsIWindowCreator2.h"
#include "nsIWindowWatcher.h"
#include "nsIXULWindow.h"
#include "nsProfileDirServiceProvider.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "plstr.h"

// Local header files
#include "WindowCreator.h"

void CreateSplashScreen();
void KillSplashScreen();
void GetScreenSize(unsigned long* x, unsigned long* y);

void WriteConsoleLog();

#include "nsIBrowserInstance.h"

#include "nsBrowserStatusFilter.h"
#include "nsBrowserInstance.h"

#endif // MINIMO_PRIVATE_H
