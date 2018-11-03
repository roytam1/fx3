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
 * The Original Code is Camino code.
 *
 * The Initial Developer of the Original Code is
 * Bruce Davidson <Bruce.Davidson@iplbath.com>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bruce Davidson
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

#import <AddressBook/AddressBook.h>

@interface ABAddressBook (CaminoExtensions)

// Returns the record containing the specified e-mail, or nil if
// there is no such record. (If more than one record does!) we
// simply return the first.
- (ABPerson*)recordFromEmail:(NSString*)emailAddress;

// Determine if a record containing the given e-mail address as an e-mail address
// property exists in the address book.
- (BOOL)emailAddressExistsInAddressBook:(NSString*)emailAddress;

// Return the real name of the person in the address book with the
// specified e-mail address. Returns nil if the e-mail address does not
// occur in the address book.
- (NSString*)getRealNameForEmailAddress:(NSString*)emailAddress;

// Add a new ABPerson record to the address book with the given e-mail address
// Then open the new record for edit so the user can fill in the rest of
// the details.
- (void)addNewPersonFromEmail:(NSString*)emailAddress;

// Opens the Address Book application at the record containing the given email address
- (void)openAddressBookForRecordWithEmail:(NSString*)emailAddress;

@end
