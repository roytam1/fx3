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

#import "NSString+Utils.h"

#import "CHBrowserView.h"
#import "CHBrowserService.h"
#import "CocoaPromptService.h"

#include "nsCRT.h"
#include "nsString.h"
#include "nsServiceManagerUtils.h"

CocoaPromptService::CocoaPromptService()
{
}

CocoaPromptService::~CocoaPromptService()
{
}

NS_IMPL_ISUPPORTS3(CocoaPromptService, nsIPromptService, nsINonBlockingAlertService, nsICookiePromptService)

/* void alert (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text); */
NS_IMETHODIMP
CocoaPromptService::Alert(nsIDOMWindow *parent,
                          const PRUnichar *dialogTitle,
                          const PRUnichar *text)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];
  
  nsresult rv = NS_OK;
  
  NS_DURING
    [controller alert:[browserView getNativeWindow] title:titleStr text:textStr];
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  return rv;
}

// nsINonBlockingService implementation
/* void showNonBlockingAlert (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText); */
NS_IMETHODIMP
CocoaPromptService::ShowNonBlockingAlert(nsIDOMWindow *aParent,
                                         const PRUnichar *aDialogTitle,
                                         const PRUnichar *aText)

{
  NSString* titleStr = [NSString stringWithPRUnichars:aDialogTitle];
  NSString* msgStr = [NSString stringWithPRUnichars:aText];
  NSWindow* parentWindow =
    [[CHBrowserView browserViewFromDOMWindow:aParent] getNativeWindow];
  if (!parentWindow) {
    NS_WARNING("ShowNonBlockingAlert: failed to get the parent window.");
    return NS_ERROR_FAILURE;
  }
  NSBeginInformationalAlertSheet(titleStr,
                                 @"OK", nil, nil, // only one button
                                 parentWindow,
                                 nil, // no delegate
                                 NULL, NULL, nil, // no delegate selectors
                                 msgStr);
  return NS_OK;
}

/* void alertCheck (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP
CocoaPromptService::AlertCheck(nsIDOMWindow *parent,
                               const PRUnichar *dialogTitle,
                               const PRUnichar *text,
                               const PRUnichar *checkMsg,
                               PRBool *checkValue)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];

  nsresult rv = NS_OK;
  NS_DURING
    // only show the checkbox if we have an out param and string for it
    if (checkValue && checkMsg && *checkMsg) {
      NSString* msgStr = [NSString stringWithPRUnichars:checkMsg];
      BOOL valueBool = *checkValue ? YES : NO;
      [controller alertCheck:[browserView getNativeWindow]
                       title:titleStr
                        text:textStr
                    checkMsg:msgStr
                  checkValue:&valueBool];
      *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
    }
    else {
      [controller alert:[browserView getNativeWindow] title:titleStr text:textStr];
    }
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  return rv;
}

/* boolean confirm (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text); */
NS_IMETHODIMP
CocoaPromptService::Confirm(nsIDOMWindow *parent,
                            const PRUnichar *dialogTitle,
                            const PRUnichar *text,
                            PRBool *_retval)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];

  nsresult rv = NS_OK;
  NS_DURING
    *_retval = (PRBool)[controller confirm:[browserView getNativeWindow]
                                     title:titleStr
                                      text:textStr];
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  return rv;
}

/* boolean confirmCheck (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP
CocoaPromptService::ConfirmCheck(nsIDOMWindow *parent,
                                 const PRUnichar *dialogTitle,
                                 const PRUnichar *text,
                                 const PRUnichar *checkMsg,
                                 PRBool *checkValue, PRBool *_retval)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];

  nsresult rv = NS_OK;
  NS_DURING
    // only show the checkbox if we have an out param and string for it
    if (checkValue && checkMsg && *checkMsg) {
      NSString* msgStr = [NSString stringWithPRUnichars:checkMsg];
      BOOL valueBool = *checkValue ? YES : NO;
      *_retval = (PRBool)[controller confirmCheck:[browserView getNativeWindow]
                                            title:titleStr
                                             text:textStr
                                         checkMsg:msgStr
                                       checkValue:&valueBool];
      *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
    }
    else {
      *_retval = (PRBool)[controller confirm:[browserView getNativeWindow]
                                       title:titleStr
                                        text:textStr];
    }
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  return rv;
}

// these constants are used for identifying the buttons and are intentionally overloaded to
// correspond to the number of bits needed for shifting to obtain the flags for a particular
// button (should be defined in nsIPrompt*.idl instead of here)
const PRUint32 kButton0 = 0;
const PRUint32 kButton1 = 8;
const PRUint32 kButton2 = 16;

/* void confirmEx (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in unsigned long buttonFlags, in wstring button0Title, in wstring button1Title, in wstring button2Title, in wstring checkMsg, inout boolean checkValue, out PRInt32 buttonPressed); */
NS_IMETHODIMP
CocoaPromptService::ConfirmEx(nsIDOMWindow *parent,
                              const PRUnichar *dialogTitle,
                              const PRUnichar *text,
                              PRUint32 buttonFlags,
                              const PRUnichar *button0Title,
                              const PRUnichar *button1Title,
                              const PRUnichar *button2Title,
                              const PRUnichar *checkMsg,
                              PRBool *checkValue, PRInt32 *buttonPressed)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];

  NSString* btn1Str = GetButtonStringFromFlags(buttonFlags, kButton0, button0Title);
  NSString* btn2Str = GetButtonStringFromFlags(buttonFlags, kButton1, button1Title);
  NSString* btn3Str = GetButtonStringFromFlags(buttonFlags, kButton2, button2Title);

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];

  nsresult rv = NS_OK;
  NS_DURING
    int result;
    // only show the checkbox if we have an out param and string for it
    if (checkValue && checkMsg && *checkMsg) {
      NSString* msgStr = [NSString stringWithPRUnichars:checkMsg];
      BOOL valueBool = *checkValue ? YES : NO;

      result = [controller confirmCheckEx:[browserView getNativeWindow]
                                    title:titleStr
                                     text:textStr
                                  button1:btn1Str
                                  button2:btn2Str
                                  button3:btn3Str
                                 checkMsg:msgStr
                               checkValue:&valueBool];
      *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
    }
    else {
      result = [controller confirmEx:[browserView getNativeWindow]
                               title:titleStr
                                text:textStr
                             button1:btn1Str
                             button2:btn2Str
                             button3:btn3Str];
    }

    switch (result)
    {
      case NSAlertDefaultReturn:    *buttonPressed = 0;    break;
      default:
      case NSAlertAlternateReturn:  *buttonPressed = 1;    break;
      case NSAlertOtherReturn:      *buttonPressed = 2;    break;
    }
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  return rv;
}


/* boolean prompt (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, inout wstring value, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP
CocoaPromptService::Prompt(nsIDOMWindow *parent,
                           const PRUnichar *dialogTitle,
                           const PRUnichar *text,
                           PRUnichar **value,
                           const PRUnichar *checkMsg,
                           PRBool *checkValue,
                           PRBool *_retval)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];
  NSString* msgStr = [NSString stringWithPRUnichars:checkMsg];

  NSMutableString* valueStr = [NSMutableString stringWithPRUnichars:*value];

  BOOL valueBool;
  if (checkValue) {
    valueBool = *checkValue ? YES : NO;
  }

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];

  nsresult rv = NS_OK;
  NS_DURING
    // only show the checkbox if we have a string for it
    *_retval = (PRBool)[controller prompt:[browserView getNativeWindow]
                                    title:titleStr
                                     text:textStr
                               promptText:valueStr
                                 checkMsg:msgStr
                               checkValue:&valueBool
                                  doCheck:(checkMsg && *checkMsg)];
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  if (NS_SUCCEEDED(rv)) {
    // the caller only cares about |value| and |checkValue| if |_retval| 
    // is something other than cancel. If it is, we'd leak any string we allocated
    // to fill in |value|. 
    if (*_retval) {
      if (checkValue) {
        *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
      }

      *value = [valueStr createNewUnicodeBuffer];
    }
    else
      *value = nsnull;
  }

  return rv;
}

/* boolean promptUsernameAndPassword (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, inout wstring username, inout wstring password, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP
CocoaPromptService::PromptUsernameAndPassword(nsIDOMWindow *parent,
                                              const PRUnichar *dialogTitle,
                                              const PRUnichar *text,
                                              PRUnichar **username,
                                              PRUnichar **password,
                                              const PRUnichar *checkMsg,
                                              PRBool *checkValue,
                                              PRBool *_retval)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];
  NSString* msgStr = [NSString stringWithPRUnichars:checkMsg];
  NSMutableString* userNameStr = [NSMutableString stringWithPRUnichars:*username];
  NSMutableString* passwordStr = [NSMutableString stringWithPRUnichars:*password];

  BOOL valueBool = NO;
  if (checkValue) {
    valueBool = *checkValue ? YES : NO;
  }

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];

  nsresult rv = NS_OK;
  NS_DURING
    // only show the checkbox if we have a string for it
    *_retval = (PRBool)[controller promptUserNameAndPassword:[browserView getNativeWindow]
                                                       title:titleStr
                                                        text:textStr
                                                userNameText:userNameStr
                                                passwordText:passwordStr
                                                    checkMsg:msgStr
                                                  checkValue:&valueBool
                                                     doCheck:(checkMsg && *checkMsg)];
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  if (NS_SUCCEEDED(rv)) {
    // the caller only cares about |username|, |password|, and |checkValue| if |_retval|
    // is something other than cancel. If it is, we'd leak any string we allocated
    // to fill in |value|. 
    if (*_retval) {
      if (checkValue)
        *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;

      *username = [userNameStr createNewUnicodeBuffer];
      *password = [passwordStr createNewUnicodeBuffer];
    } else {
      *username = nsnull;
      *password = nsnull;
    }
  }

  return rv;
}

/* boolean promptPassword (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, inout wstring password, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP
CocoaPromptService::PromptPassword(nsIDOMWindow *parent,
                                   const PRUnichar *dialogTitle,
                                   const PRUnichar *text,
                                   PRUnichar **password,
                                   const PRUnichar *checkMsg,
                                   PRBool *checkValue,
                                   PRBool *_retval)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithPRUnichars:dialogTitle];
  NSString* textStr = [NSString stringWithPRUnichars:text];
  NSString* msgStr = [NSString stringWithPRUnichars:checkMsg];
  NSMutableString* passwordStr = [NSMutableString stringWithPRUnichars:*password];

  BOOL valueBool = NO;
  if (checkValue) {
    valueBool = *checkValue ? YES : NO;
  }

  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:parent];
  [browserView doBeforePromptDisplay];

  nsresult rv = NS_OK;
  NS_DURING
    // only show the checkbox if we have a string for it
    *_retval = (PRBool)[controller promptPassword:[browserView getNativeWindow]
                                            title:titleStr
                                             text:textStr
                                     passwordText:passwordStr
                                         checkMsg:msgStr
                                       checkValue:&valueBool
                                          doCheck:(checkMsg && *checkMsg)];
  NS_HANDLER
    rv = NS_ERROR_FAILURE;
  NS_ENDHANDLER

  [browserView doAfterPromptDismissal];

  if (NS_SUCCEEDED(rv)) {
    // the caller only cares about |password| and |checkValue| if |_retval|
    // is something other than cancel. If it is, we'd leak any string we allocated
    // to fill in |value|. 
    if (*_retval) {
      if (checkValue)
        *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;

      *password = [passwordStr createNewUnicodeBuffer];
    }
    else
      *password = nsnull;
  }
  return rv;
}

/* boolean select (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in PRUint32 count, [array, size_is (count)] in wstring selectList, out long outSelection); */
NS_IMETHODIMP
CocoaPromptService::Select(nsIDOMWindow *parent,
                           const PRUnichar *dialogTitle,
                           const PRUnichar *text,
                           PRUint32 count,
                           const PRUnichar **selectList,
                           PRInt32 *outSelection,
                           PRBool *_retval)
{
#if DEBUG
  NSLog(@"Uh-oh. Select has not been implemented.");
#endif
  return NS_ERROR_NOT_IMPLEMENTED;
}

#pragma mark -

NSString *
CocoaPromptService::GetCommonDialogLocaleString(const char *key)
{
  NSString *returnValue = @"";

  nsresult rv;
  if (!mCommonDialogStringBundle) {
#define kCommonDialogsStrings "chrome://global/locale/commonDialogs.properties"
    nsCOMPtr<nsIStringBundleService> service = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if ( service )
      rv = service->CreateBundle(kCommonDialogsStrings, getter_AddRefs(mCommonDialogStringBundle));
    else
      rv = NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) return returnValue;
  }

  nsXPIDLString string;
  rv = mCommonDialogStringBundle->GetStringFromName(NS_ConvertASCIItoUTF16(key).get(), getter_Copies(string));
  if (NS_FAILED(rv)) return returnValue;

  returnValue = [NSString stringWithPRUnichars:string];
  // take care of the fact that it could have windows shortcut specified by ampersand
  returnValue = [returnValue stringByRemovingWindowsShortcutAmpersand];
  return returnValue;
}

NSString *
CocoaPromptService::GetButtonStringFromFlags(PRUint32 btnFlags,
                                             PRUint32 btnIDAndShift,
                                             const PRUnichar *btnTitle)
{
  NSString *btnStr = nsnull;
  switch ((btnFlags >> btnIDAndShift) & 0xff) {
    case BUTTON_TITLE_OK:
      btnStr = GetCommonDialogLocaleString("OK");
      break;
    case BUTTON_TITLE_CANCEL:
      btnStr = GetCommonDialogLocaleString("Cancel");
      break;
    case BUTTON_TITLE_YES:
      btnStr = GetCommonDialogLocaleString("Yes");
      break;
    case BUTTON_TITLE_NO:
      btnStr = GetCommonDialogLocaleString("No");
      break;
    case BUTTON_TITLE_SAVE:
      btnStr = GetCommonDialogLocaleString("Save");
      break;
    case BUTTON_TITLE_DONT_SAVE:
      btnStr = GetCommonDialogLocaleString("DontSave");
      break;
    case BUTTON_TITLE_REVERT:
      btnStr = GetCommonDialogLocaleString("Revert");
      break;
    case BUTTON_TITLE_IS_STRING:
      btnStr = [NSString stringWithPRUnichars:btnTitle];
  }

  return btnStr;
}


//
// CookieDialog
//
// Implements the interface on nsICookiePromptService to pose the cookie dialog. For
// starters, just use a rather simple text string.
//
NS_IMETHODIMP
CocoaPromptService::CookieDialog(nsIDOMWindow *parent, nsICookie *cookie, const nsACString & hostname, PRInt32 cookiesFromHost, PRBool changingCookie, PRBool *rememberDecision, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(rememberDecision && _retval);

  NSString* dialogText = [NSString stringWithFormat:NSLocalizedString(@"CookieText",@"CookieText"),
                           PromiseFlatCString(hostname).get()];
  PRUnichar* textStr = [dialogText createNewUnicodeBuffer];
  NSString* checkboxText = NSLocalizedString(@"CookieCheckbox", @"CookieCheckbox");
  PRUnichar* checkboxStr = [checkboxText createNewUnicodeBuffer];
  NSString* titleText = NSLocalizedString(@"CookieTitle", @"CookieTitle");
  PRUnichar* titleStr = [titleText createNewUnicodeBuffer];
  NSString* allowText = NSLocalizedString(@"Allow", @"AllowCookie");
  PRUnichar* allowStr = [allowText createNewUnicodeBuffer];
  NSString* denyText = NSLocalizedString(@"Deny", @"DenyCookie");
  PRUnichar* denyStr = [denyText createNewUnicodeBuffer];
  NSString* allowForSessionText = NSLocalizedString(@"Allow for Session", @"AllowCookieForSession");
  PRUnichar* allowForSessionStr = [allowForSessionText createNewUnicodeBuffer];

  long buttonFlags = (BUTTON_TITLE_IS_STRING << kButton0) | (BUTTON_TITLE_IS_STRING << kButton1) | (BUTTON_TITLE_IS_STRING << kButton2);
  PRInt32 buttonPressed = 0;
  *rememberDecision = PR_TRUE;          // "remember this decision" should be checked when we show the dialog
  ConfirmEx(parent, titleStr, textStr, buttonFlags, allowStr, denyStr, allowForSessionStr, checkboxStr, rememberDecision, &buttonPressed);
 
  // map return values for nsICookiePromptService
  switch (buttonPressed)
  {
    case 0: // allow button
      *_retval = nsICookiePromptService::ACCEPT_COOKIE;
      break;
    case 1: // deny button
      *_retval = nsICookiePromptService::DENY_COOKIE;
      break;
    case 2: // allow for session button
      *_retval = nsICookiePromptService::ACCEPT_SESSION_COOKIE;
      break;
  }
  
  nsMemory::Free(textStr);
  nsMemory::Free(checkboxStr);
  nsMemory::Free(titleStr);
  nsMemory::Free(allowStr);
  nsMemory::Free(allowForSessionStr);
  nsMemory::Free(denyStr);
  return NS_OK;
}


