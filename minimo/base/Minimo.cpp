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

#include "MinimoPrivate.h"

#include "nsIFullScreen.h"
#include "nsIGenericFactory.h"

#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponents.h"
#endif

#include "nsISecurityWarningDialogs.h"

#define MINIMO_PROPERTIES_URL "chrome://minimo/locale/minimo.properties"

// Global variables

const static char* start_url = "chrome://minimo/content/minimo.xul";

//const static char* start_url = "http://www.mozilla.org";
//const static char* start_url = "http://www.meer.net/~dougt/test.html";
//const static char* start_url = "resource://gre/res/start.html";
//const static char* start_url = "resource://gre/res/1.html";

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

class ApplicationObserver: public nsIObserver 
{
public:  
  ApplicationObserver(nsIAppShell* aAppShell);
  ~ApplicationObserver();  
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsCOMPtr<nsIAppShell> mAppShell;
  int mWindowCount;
  int mSeenLoadLibraryFailure;
};


ApplicationObserver::ApplicationObserver(nsIAppShell* aAppShell)
{
    mAppShell = aAppShell;
    mWindowCount = 0;
    mSeenLoadLibraryFailure = 0;

    nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");


    os->AddObserver(this, "nsIEventQueueActivated", PR_FALSE);
    os->AddObserver(this, "nsIEventQueueDestroyed", PR_FALSE);

    os->AddObserver(this, "xul-window-registered", PR_FALSE);
    os->AddObserver(this, "xul-window-destroyed", PR_FALSE);
    os->AddObserver(this, "xul-window-visible", PR_FALSE);

    os->AddObserver(this, "xpcom-loader", PR_FALSE);
}

ApplicationObserver::~ApplicationObserver()
{
}

NS_IMPL_ISUPPORTS1(ApplicationObserver, nsIObserver)

NS_IMETHODIMP
ApplicationObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
    if (!strcmp(aTopic, "nsIEventQueueActivated")) 
    {
        nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
        if (eq)
        {
            PRBool isNative = PR_TRUE;
            // we only add native event queues to the appshell
            eq->IsQueueNative(&isNative);
            if (isNative)
                mAppShell->ListenToEventQueue(eq, PR_TRUE);
        }
    } 
    else if (!strcmp(aTopic, "nsIEventQueueDestroyed")) 
    {
        nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
        if (eq) 
        {
            PRBool isNative = PR_TRUE;
            // we only remove native event queues from the appshell
            eq->IsQueueNative(&isNative);
            if (isNative)
                mAppShell->ListenToEventQueue(eq, PR_FALSE);
        }
    } 
    else if (!strcmp(aTopic, "xul-window-visible"))
    {
        KillSplashScreen();
    }
 
    else if (!strcmp(aTopic, "xul-window-registered"))
    {
        mWindowCount++;
    }
    else if (!strcmp(aTopic, "xul-window-destroyed"))
    {
        mWindowCount--;
        if (mWindowCount == 0)
            mAppShell->Exit();
    }
    else if (!strcmp(aTopic, "xpcom-loader"))
    {
      if (mWindowCount == 0 || mSeenLoadLibraryFailure != 0)
        return NS_OK;

      mSeenLoadLibraryFailure++;

      nsIFile* file = (nsIFile*) aSubject;
      nsAutoString filePath;
      file->GetPath(filePath);

      nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
      if (!bundleService)
        return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIStringBundle> bundle;
      bundleService->CreateBundle(MINIMO_PROPERTIES_URL, getter_AddRefs(bundle));
      
      if (!bundle)
        return NS_ERROR_FAILURE;
      
      const PRUnichar *formatStrings[] = { filePath.get() };
      nsXPIDLString message;
      bundle->FormatStringFromName(NS_LITERAL_STRING("loadFailed").get(),
                                   formatStrings,
                                   NS_ARRAY_LENGTH(formatStrings),
                                   getter_Copies(message));
      
      nsXPIDLString title;
      bundle->GetStringFromName(NS_LITERAL_STRING("loadFailedTitle").get(), getter_Copies(title));

      nsCOMPtr<nsIPromptService> promptService = do_GetService("@mozilla.org/embedcomp/prompt-service;1");
      promptService->Alert(nsnull, title.get(), message.get());
    }
    return NS_OK;
}


class nsFullScreen : public nsIFullScreen
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFULLSCREEN
  
  nsFullScreen();
  ~nsFullScreen();

#ifdef WINCE
  HWND hTaskBarWnd;
#endif
};

NS_IMPL_ISUPPORTS1(nsFullScreen, nsIFullScreen)

nsFullScreen::nsFullScreen()
{
#ifdef WINCE
  hTaskBarWnd = FindWindow("HHTaskBar", NULL); 
#endif
}

nsFullScreen::~nsFullScreen()
{
}

NS_IMETHODIMP
nsFullScreen::HideAllOSChrome()
{
#ifndef WINCE
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  if (!hTaskBarWnd)
    return NS_ERROR_NOT_INITIALIZED;

  SetWindowPos(hTaskBarWnd, 
               HWND_NOTOPMOST,
               0, 0, 0, 0, 
               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

  ShowWindow(hTaskBarWnd, SW_HIDE);

  return NS_OK;
#endif
}

NS_IMETHODIMP 
nsFullScreen::ShowAllOSChrome()
{
#ifndef WINCE
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  if (!hTaskBarWnd)
    return NS_ERROR_NOT_INITIALIZED;

  SetWindowPos(hTaskBarWnd, 
               HWND_TOPMOST,
               0, 0, 0, 0, 
               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

  ShowWindow(hTaskBarWnd, SW_SHOW);

  return NS_OK;
#endif
}

NS_IMETHODIMP
nsFullScreen::GetChromeItems(nsISimpleEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#ifndef WINCE

class nsBadCertListener : public nsIBadCertListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBADCERTLISTENER

  nsBadCertListener();
  ~nsBadCertListener();
};


nsBadCertListener::nsBadCertListener()
{
}

nsBadCertListener::~nsBadCertListener()
{
}

NS_IMPL_ISUPPORTS1(nsBadCertListener, nsIBadCertListener)



NS_IMETHODIMP 
nsBadCertListener::ConfirmUnknownIssuer(nsIInterfaceRequestor *socketInfo, nsIX509Cert *cert, PRInt16 *certAddType, PRBool *_retval)
{
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!bundleService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(MINIMO_PROPERTIES_URL, getter_AddRefs(bundle));

  if (!bundle)
    return NS_ERROR_FAILURE;

  nsXPIDLString message;
  nsXPIDLString title;
  bundle->GetStringFromName(NS_LITERAL_STRING("confirmUnknownIssuer").get(), getter_Copies(message));
  bundle->GetStringFromName(NS_LITERAL_STRING("securityWarningTitle").get(), getter_Copies(title));

  nsCOMPtr<nsIWindowWatcher> wwatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  if (!wwatcher)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMWindow> parent;
  if (!parent)
    wwatcher->GetActiveWindow(getter_AddRefs(parent));

  PRBool result;
  nsCOMPtr<nsIPromptService> dlgService(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
  dlgService->Confirm(parent, title, message, &result);

  *_retval = result;

  if (result)
    *certAddType = ADD_TRUSTED_FOR_SESSION;

  return NS_OK;
}

NS_IMETHODIMP 
nsBadCertListener::ConfirmMismatchDomain(nsIInterfaceRequestor *socketInfo, const nsACString & targetURL, nsIX509Cert *cert, PRBool *_retval)
{
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!bundleService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(MINIMO_PROPERTIES_URL, getter_AddRefs(bundle));

  if (!bundle)
    return NS_ERROR_FAILURE;

  nsXPIDLString message;
  nsXPIDLString title;
  bundle->GetStringFromName(NS_LITERAL_STRING("confirmMismatch").get(), getter_Copies(message));
  bundle->GetStringFromName(NS_LITERAL_STRING("securityWarningTitle").get(), getter_Copies(title));

  nsCOMPtr<nsIWindowWatcher> wwatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  if (!wwatcher)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMWindow> parent;
  if (!parent)
    wwatcher->GetActiveWindow(getter_AddRefs(parent));

  PRBool result;
  nsCOMPtr<nsIPromptService> dlgService(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
  dlgService->Confirm(parent, title, message, &result);

  *_retval = result;

  return NS_OK;
}

NS_IMETHODIMP 
nsBadCertListener::ConfirmCertExpired(nsIInterfaceRequestor *socketInfo, nsIX509Cert *cert, PRBool *_retval)
{
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!bundleService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(MINIMO_PROPERTIES_URL, getter_AddRefs(bundle));

  if (!bundle)
    return NS_ERROR_FAILURE;

  nsXPIDLString message;
  nsXPIDLString title;
  bundle->GetStringFromName(NS_LITERAL_STRING("confirmCertExpired").get(), getter_Copies(message));
  bundle->GetStringFromName(NS_LITERAL_STRING("securityWarningTitle").get(), getter_Copies(title));

  nsCOMPtr<nsIWindowWatcher> wwatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  if (!wwatcher)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMWindow> parent;
  if (!parent)
    wwatcher->GetActiveWindow(getter_AddRefs(parent));

  PRBool result;
  nsCOMPtr<nsIPromptService> dlgService(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
  dlgService->Confirm(parent, title, message, &result);

  *_retval = result;

  return NS_OK;
}

NS_IMETHODIMP
nsBadCertListener::NotifyCrlNextupdate(nsIInterfaceRequestor *socketInfo, const nsACString & targetURL, nsIX509Cert *cert)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif


#ifdef WINCE

class nsSecurityWarningDialogs : public nsISecurityWarningDialogs
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECURITYWARNINGDIALOGS

  nsSecurityWarningDialogs();

private:
  ~nsSecurityWarningDialogs();
};

NS_IMPL_ISUPPORTS1(nsSecurityWarningDialogs, nsISecurityWarningDialogs)

nsSecurityWarningDialogs::nsSecurityWarningDialogs()
{
}

nsSecurityWarningDialogs::~nsSecurityWarningDialogs()
{
}

NS_IMETHODIMP
nsSecurityWarningDialogs::ConfirmEnteringSecure(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityWarningDialogs::ConfirmEnteringWeak(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityWarningDialogs::ConfirmLeavingSecure(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityWarningDialogs::ConfirmMixedMode(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityWarningDialogs::ConfirmPostToInsecure(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityWarningDialogs::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}
#endif


nsresult StartupProfile()
{    
    NS_TIMELINE_MARK_FUNCTION("Profile Startup");

	nsCOMPtr<nsIFile> appDataDir;
	nsresult rv = NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_DIR, getter_AddRefs(appDataDir));
	if (NS_FAILED(rv))
        return rv;
    
	rv = appDataDir->Append(NS_LITERAL_STRING("minimo"));
	if (NS_FAILED(rv))
        return rv;

	nsCOMPtr<nsILocalFile> localAppDataDir(do_QueryInterface(appDataDir));
    
	nsCOMPtr<nsProfileDirServiceProvider> locProvider;
    NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(locProvider));
    if (!locProvider)
        return NS_ERROR_FAILURE;
    
	rv = locProvider->Register();
    if (NS_FAILED(rv))
        return rv;
    
	return locProvider->SetProfileDir(localAppDataDir);   
}

void DoPreferences()
{
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefBranch)
        return;

    prefBranch->SetIntPref("snav.keyCode.modifier", 0);
    prefBranch->SetBoolPref("snav.enabled", true);
}

#define NS_FULLSCREEN_CID                          \
{ /* aca93a4e-53f8-40e2-a59b-9363a0bf9a87 */       \
  0xaca93a4e,                                      \
  0x53f8,                                          \
  0x40e2,                                          \
  {0xa5, 0x9b, 0x93, 0x63, 0xa0, 0xbf, 0x9a, 0x87} \
}

#define NS_BADCERTLISTENER_CID                     \
{ /* a4bdf79a-ed05-4256-bf12-4581a03f966e */       \
  0xa4bdf79a,                                      \
  0xed05,                                          \
  0x4256,                                          \
  {0xbf, 0x12, 0x45, 0x81, 0xa0, 0x3f, 0x96, 0x6e} \
}

#define NS_SECURITYWARNINGDIALOGS_CID              \
{ /* 8d995d4f-adcc-4159-b7f1-e94af72eeb88 */       \
  0x8d995d4f,                                      \
  0xadcc,                                          \
  0x4159,                                          \
  {0xb7, 0xf1, 0xe9, 0x4a, 0xf7, 0x2e, 0xeb, 0x88} \
}
 

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserStatusFilter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFullScreen)

#ifndef WINCE
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBadCertListener)
#endif

#ifdef WINCE
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecurityWarningDialogs)
#endif



static const nsModuleComponentInfo defaultAppComps[] = {
  {
     NS_BROWSERSTATUSFILTER_CLASSNAME,
     NS_BROWSERSTATUSFILTER_CID,
     NS_BROWSERSTATUSFILTER_CONTRACTID,
     nsBrowserStatusFilterConstructor
  },

  { "nsBrowserInstance",
    NS_BROWSERINSTANCE_CID,
    NS_BROWSERINSTANCE_CONTRACTID,
    nsBrowserInstanceConstructor
  },
  
  {
     "FullScreen",
     NS_FULLSCREEN_CID,
     "@mozilla.org/browser/fullscreen;1",
     nsFullScreenConstructor
  },

#ifndef WINCE
  {
     "Bad Cert Dialogs",
     NS_BADCERTLISTENER_CID,
     NS_BADCERTLISTENER_CONTRACTID,
     nsBadCertListenerConstructor
  },
#endif

#ifdef WINCE
   {
     "PSM Security Warnings",
     NS_SECURITYWARNINGDIALOGS_CID,
     NS_SECURITYWARNINGDIALOGS_CONTRACTID,
     nsSecurityWarningDialogsConstructor
   },
#endif
};

void OverrideComponents()
{
  int count = sizeof(defaultAppComps) / sizeof(nsModuleComponentInfo);

  nsCOMPtr<nsIComponentRegistrar> cr;
  NS_GetComponentRegistrar(getter_AddRefs(cr));
  
  nsCOMPtr<nsIComponentManager> cm;
  NS_GetComponentManager (getter_AddRefs (cm));
  
  for (int i = 0; i < count; ++i) 
  {
    nsCOMPtr<nsIGenericFactory> componentFactory;
    nsresult rv = NS_NewGenericFactory(getter_AddRefs(componentFactory), &(defaultAppComps[i]));
    
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to create factory for component");
      continue;  // don't abort registering other components
    }
    
    rv = cr->RegisterFactory(defaultAppComps[i].mCID,
                             defaultAppComps[i].mDescription,
                             defaultAppComps[i].mContractID,
                             componentFactory);
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *   Complete hack below.  We to ensure that the layout of all
 *   of the shared libaries are at tightly packed as possible.
 *   We do this by explictly loading all of the modules we
 *   know about at compile time.  This seams to work for our
 *   purposes.  Ultimately, we should statically link all of
 *   these libraries.
 *
 *   If you are building this to put on a ROM where XIP exists.
 *
 *   For more information:
 *   1) http://msdn.microsoft.com/library/default.asp?url=/ \
        library/en-us/dncenet/html/advmemmgmt.asp
 *
 * * * * * * * * * * * * * * *  * * * * * * * * * * * * * * * * * * * */
#ifdef WINCE
#define HACKY_PRE_LOAD_LIBRARY 1
#endif

#ifdef HACKY_PRE_LOAD_LIBRARY
typedef struct _library
{
	const unsigned short* name;
	HMODULE module;
} _library;

_library Libraries[] =
{
  {  L"schannel.dll",    NULL },
  {  NULL, NULL },
};

void LoadKnownLibs()
{
  for (int i=0; Libraries[i].name; i++) 
  {
    Libraries[i].module = LoadLibraryW(Libraries[i].name);
    if (!Libraries[i].module)
    {
      MessageBox(0, 
                 "Preload library failed to load.", 
                 "Lib Load Failed", 
                 MB_APPLMODAL);
    }
  }
}

void UnloadKnownLibs()
{
  for (int i=0; Libraries[i].name; i++)
    if (Libraries[i].module)
      FreeLibrary(Libraries[i].module);
}
#endif // HACKY_PRE_LOAD_LIBRARY

int main(int argc, char *argv[])
{
#ifdef MOZ_WIDGET_GTK2
  gtk_set_locale();
  gtk_init(&argc, &argv);
#endif

#ifdef HACKY_PRE_LOAD_LIBRARY
  LoadKnownLibs();
#endif
  
  CreateSplashScreen();
  
#ifdef _BUILD_STATIC_BIN
  NS_InitEmbedding(nsnull, nsnull, kPStaticModules, kStaticModuleCount);
#else
  NS_InitEmbedding(nsnull, nsnull);
#endif

    // Choose the new profile
  if (NS_FAILED(StartupProfile()))
    return 1;
  
  DoPreferences();
  OverrideComponents();
  
  NS_TIMELINE_ENTER("appStartup");
  nsCOMPtr<nsIAppShell> appShell = do_CreateInstance(kAppShellCID);
  if (!appShell)
  {
    // if we can't get the nsIAppShell, then we should auto reg.
    nsCOMPtr<nsIComponentRegistrar> registrar;
    NS_GetComponentRegistrar(getter_AddRefs(registrar));
    if (!registrar)
      return -1;

    registrar->AutoRegister(nsnull);

	appShell = do_CreateInstance(kAppShellCID);

	if (!appShell)
		return 1;
  }

  appShell->Create(nsnull, nsnull);
  
  ApplicationObserver *appObserver = new ApplicationObserver(appShell);
  if (!appObserver)
    return 1;
  NS_ADDREF(appObserver);
  
  WindowCreator *creatorCallback = new WindowCreator(appShell);
  if (!creatorCallback)
    return 1;
  
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  wwatch->SetWindowCreator(creatorCallback);
  
  nsCOMPtr<nsIDOMWindow> newWindow;
  wwatch->OpenWindow(nsnull, start_url, "_blank", "chrome,dialog=no,all", nsnull, getter_AddRefs(newWindow));
  
  appShell->Run();
  
  appShell = nsnull;
  wwatch = nsnull;
  newWindow = nsnull;
  
  delete appObserver;
  delete creatorCallback;
  
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefBranch)
    return -1;
  
  PRBool dumpJSConsole = PR_FALSE;
  
  prefBranch->GetBoolPref("config.wince.dumpJSConsole", &dumpJSConsole);
  prefBranch = 0;

  if (dumpJSConsole)
    WriteConsoleLog();

  // Close down Embedding APIs
  NS_TermEmbedding();
  
#ifdef HACKY_PRE_LOAD_LIBRARY
  UnloadKnownLibs();
#endif
  return NS_OK;
}


