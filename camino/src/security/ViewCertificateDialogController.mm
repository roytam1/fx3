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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <smfr@smfr.org>
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
#import "NSDate+Utils.h"

#import "nsCOMPtr.h"
#import "nsString.h"
#import "nsArray.h"
#import "nsIArray.h"

#import "nsIX509Cert.h"
#import "nsIX509CertValidity.h"
#import "nsIX509CertDB.h"

#import "nsServiceManagerUtils.h"

#import "CertificateItem.h"
#import "CertificateView.h"
#import "ViewCertificateDialogController.h"


@interface ViewCertificateDialogController(Private)

@end

#pragma mark -

@implementation ViewCertificateDialogController

+ (ViewCertificateDialogController*)showCertificateWindowWithCertificateItem:(CertificateItem*)inCertItem certTypeForTrustSettings:(unsigned int)inCertType
{
  // look for existing window with this cert
  NSEnumerator* windowEnum = [[NSApp windows] objectEnumerator];
  NSWindow* thisWindow;
  while ((thisWindow = [windowEnum nextObject]))
  {
    if ([[thisWindow delegate] isKindOfClass:[ViewCertificateDialogController class]])
    {
      ViewCertificateDialogController* dialogController = (ViewCertificateDialogController*)[thisWindow delegate];
      if ([[dialogController certificateItem] isEqualTo:inCertItem])
      {
        [dialogController showWindow:nil];
        return dialogController;
      }
    }
  }

  // init balanced by the autorelease in windowWillClose
  ViewCertificateDialogController* viewCertDialogController = [[ViewCertificateDialogController alloc] initWithWindowNibName:@"ViewCertificateDialog"];
  [viewCertDialogController setCertTypeForTrustSettings:inCertType];
  [viewCertDialogController setCertificateItem:inCertItem];
  [viewCertDialogController showWindow:nil];
  return viewCertDialogController;
}


+ (void)runModalCertificateWindowWithCertificateItem:(CertificateItem*)inCertItem certTypeForTrustSettings:(unsigned int)inCertType
{
  // init balanced by the autorelease in windowWillClose
  ViewCertificateDialogController* viewCertDialogController = [[ViewCertificateDialogController alloc] initWithWindowNibName:@"ViewCertificateDialog"];
  [viewCertDialogController setCertTypeForTrustSettings:inCertType];
  [viewCertDialogController setCertificateItem:inCertItem];
  [viewCertDialogController runModally];
  [viewCertDialogController release];
}

- (void)dealloc
{
  [super dealloc];
}

- (void)awakeFromNib
{
}

- (void)windowDidLoad
{
  [[self window] setFrameAutosaveName:@"ViewCertificateDialog"];
  mAllowTrustSaving = YES;
  [mCertView setDelegate:self];
}

- (void)windowWillClose:(NSNotification *)aNotification
{
  if (mAllowTrustSaving)
    [mCertView saveTrustSettings:nil];
  
  if (!mRunningModally)
    [self autorelease];

  [mCertView setCertificateItem:nil];
}

- (int)runModally
{
  mRunningModally = YES;
  return [NSApp runModalForWindow:[self window]];
}

- (void)allowTrustSaving:(BOOL)inAllow
{
  mAllowTrustSaving = inAllow;
}

- (IBAction)defaultButtonHit:(id)sender
{
  if (mRunningModally)
  {
    if (mAllowTrustSaving)
      [mCertView saveTrustSettings:nil];

    [NSApp stopModalWithCode:NSAlertDefaultReturn];
    [[self window] orderOut:nil];
  }
  else
    [self close];
}

- (IBAction)alternateButtonHit:(id)sender
{
  if (mRunningModally)
  {
    [NSApp stopModalWithCode:NSAlertAlternateReturn];
    [[self window] orderOut:nil];
  }
  else
    [self close];
}

// CertificateViewDelegate method
- (void)certificateView:(CertificateView*)certView showIssuerCertificate:(CertificateItem*)issuerCert
{
  // if we are modal, then this must also be modal
  if (mRunningModally)
    [ViewCertificateDialogController runModalCertificateWindowWithCertificateItem:issuerCert
                                                         certTypeForTrustSettings:nsIX509Cert::CA_CERT];
  else
    [ViewCertificateDialogController showCertificateWindowWithCertificateItem:issuerCert
                                                     certTypeForTrustSettings:nsIX509Cert::CA_CERT];
}

- (void)setCertificateItem:(CertificateItem*)inCert
{
  [[self window] setTitle:[inCert displayName]];  // also makes sure that the window is loaded
  [mCertView setCertificateItem:inCert];
}

- (CertificateItem*)certificateItem
{
  return [mCertView certificateItem];
}

- (void)setCertTypeForTrustSettings:(unsigned int)inCertType
{
  [self window];    // make sure view is hooked up
  [mCertView setCertTypeForTrustSettings:inCertType];
}

@end
