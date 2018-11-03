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
 *   Simon Fraser <sfraser@netscape.com>
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

#import <Foundation/Foundation.h>

@class NetworkServices;

// protocol implemented by someone who wants to provide UI for network services

@protocol NetworkServicesClient

- (void)availableServicesChanged:(NSNotification *)note;
- (void)serviceResolved:(NSNotification *)note;
- (void)serviceResolutionFailed:(NSNotification *)note;

@end

@interface NetworkServices : NSObject
{
    // browser can only do one search at a time, so we have a browser for
    // each protocol that we care about.
    NSNetServiceBrowser*    mHttpBrowser;
    NSNetServiceBrowser*    mHttpsBrowser;
    NSNetServiceBrowser*    mFtpBrowser;

    int                     mCurServiceID;      // unique ID for each service
    NSMutableDictionary*    mNetworkServices;  // services keyed by ID
    
    NSMutableDictionary*    mClients;           // dictionary of cliend id's for a request    
}

+ (id)sharedNetworkServices;
+ (void)shutdownNetworkServices;
- (void)startServices;
- (void)stopServices;
- (void)attemptResolveService:(int)serviceID forSender:(id)aSender;

- (NSString*)serviceName:(int)serviceID;
- (NSString*)serviceProtocol:(int)serviceID;
- (NSEnumerator*)serviceEnumerator;

// Notifications
extern NSString* const NetworkServicesAvailableServicesChanged;
extern NSString* const NetworkServicesResolutionSuccess;
extern NSString* const NetworkServicesResolutionFailure;
extern NSString* const NetworkServicesClientKey;
extern NSString* const NetworkServicesResolvedURLKey;
extern NSString* const NetworkServicesServiceKey;

@end
