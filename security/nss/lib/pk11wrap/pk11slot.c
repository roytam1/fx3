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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
 * Deal with PKCS #11 Slots.
 */
#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11t.h"
#include "pk11func.h"
#include "secitem.h"
#include "secerr.h"

#include "dev.h" 
#include "dev3hack.h" 
#include "pkim.h"


/*************************************************************
 * local static and global data
 *************************************************************/

/*
 * This array helps parsing between names, mechanisms, and flags.
 * to make the config files understand more entries, add them
 * to this table. (NOTE: we need function to export this table and it's size)
 */
PK11DefaultArrayEntry PK11_DefaultArray[] = {
	{ "RSA", SECMOD_RSA_FLAG, CKM_RSA_PKCS },
	{ "DSA", SECMOD_DSA_FLAG, CKM_DSA },
	{ "DH", SECMOD_DH_FLAG, CKM_DH_PKCS_DERIVE },
	{ "RC2", SECMOD_RC2_FLAG, CKM_RC2_CBC },
	{ "RC4", SECMOD_RC4_FLAG, CKM_RC4 },
	{ "DES", SECMOD_DES_FLAG, CKM_DES_CBC },
	{ "AES", SECMOD_AES_FLAG, CKM_AES_CBC },
	{ "RC5", SECMOD_RC5_FLAG, CKM_RC5_CBC },
	{ "SHA-1", SECMOD_SHA1_FLAG, CKM_SHA_1 },
	{ "SHA256", SECMOD_SHA256_FLAG, CKM_SHA256 },
/*	{ "SHA384", SECMOD_SHA512_FLAG, CKM_SHA384 }, */
	{ "SHA512", SECMOD_SHA512_FLAG, CKM_SHA512 },
	{ "MD5", SECMOD_MD5_FLAG, CKM_MD5 },
	{ "MD2", SECMOD_MD2_FLAG, CKM_MD2 },
	{ "SSL", SECMOD_SSL_FLAG, CKM_SSL3_PRE_MASTER_KEY_GEN },
	{ "TLS", SECMOD_TLS_FLAG, CKM_TLS_MASTER_KEY_DERIVE },
	{ "SKIPJACK", SECMOD_FORTEZZA_FLAG, CKM_SKIPJACK_CBC64 },
	{ "Publicly-readable certs", SECMOD_FRIENDLY_FLAG, CKM_INVALID_MECHANISM },
	{ "Random Num Generator", SECMOD_RANDOM_FLAG, CKM_FAKE_RANDOM },
};
const int num_pk11_default_mechanisms = 
                sizeof(PK11_DefaultArray) / sizeof(PK11_DefaultArray[0]);

PK11DefaultArrayEntry *
PK11_GetDefaultArray(int *size)
{
    if (size) {
	*size = num_pk11_default_mechanisms;
    }
    return PK11_DefaultArray;
}

/*
 * These  slotlists are lists of modules which provide default support for
 *  a given algorithm or mechanism.
 */
static PK11SlotList pk11_aesSlotList,
    pk11_desSlotList,
    pk11_rc4SlotList,
    pk11_rc2SlotList,
    pk11_rc5SlotList,
    pk11_sha1SlotList,
    pk11_md5SlotList,
    pk11_md2SlotList,
    pk11_rsaSlotList,
    pk11_dsaSlotList,
    pk11_dhSlotList,
    pk11_ecSlotList,
    pk11_ideaSlotList,
    pk11_sslSlotList,
    pk11_tlsSlotList,
    pk11_randomSlotList,
    pk11_sha256SlotList,
    pk11_sha512SlotList;	/* slots do SHA512 and SHA384 */

/************************************************************
 * Generic Slot List and Slot List element manipulations
 ************************************************************/

/*
 * allocate a new list 
 */
PK11SlotList *
PK11_NewSlotList(void)
{
    PK11SlotList *list;
 
    list = (PK11SlotList *)PORT_Alloc(sizeof(PK11SlotList));
    if (list == NULL) return NULL;
    list->head = NULL;
    list->tail = NULL;
    list->lock = PZ_NewLock(nssILockList);
    if (list->lock == NULL) {
	PORT_Free(list);
	return NULL;
    }

    return list;
}

/*
 * free a list element when all the references go away.
 */
SECStatus
PK11_FreeSlotListElement(PK11SlotList *list, PK11SlotListElement *le)
{
    PRBool freeit = PR_FALSE;

    if (list == NULL || le == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    PZ_Lock(list->lock);
    if (le->refCount-- == 1) {
	freeit = PR_TRUE;
    }
    PZ_Unlock(list->lock);
    if (freeit) {
    	PK11_FreeSlot(le->slot);
	PORT_Free(le);
    }
    return SECSuccess;
}

static void
pk11_FreeSlotListStatic(PK11SlotList *list)
{
    PK11SlotListElement *le, *next ;
    if (list == NULL) return;

    for (le = list->head ; le; le = next) {
	next = le->next;
	PK11_FreeSlotListElement(list,le);
    }
    if (list->lock) {
    	PZ_DestroyLock(list->lock);
    }
    list->lock = NULL;
    list->head = NULL;
}

/*
 * if we are freeing the list, we must be the only ones with a pointer
 * to the list.
 */
void
PK11_FreeSlotList(PK11SlotList *list)
{
    pk11_FreeSlotListStatic(list);
    PORT_Free(list);
}

/*
 * add a slot to a list
 */
SECStatus
PK11_AddSlotToList(PK11SlotList *list,PK11SlotInfo *slot)
{
    PK11SlotListElement *le;

    le = (PK11SlotListElement *) PORT_Alloc(sizeof(PK11SlotListElement));
    if (le == NULL) return SECFailure;

    le->slot = PK11_ReferenceSlot(slot);
    le->prev = NULL;
    le->refCount = 1;
    PZ_Lock(list->lock);
    if (list->head) list->head->prev = le; else list->tail = le;
    le->next = list->head;
    list->head = le;
    PZ_Unlock(list->lock);

    return SECSuccess;
}

/*
 * remove a slot entry from the list
 */
SECStatus
PK11_DeleteSlotFromList(PK11SlotList *list,PK11SlotListElement *le)
{
    PZ_Lock(list->lock);
    if (le->prev) le->prev->next = le->next; else list->head = le->next;
    if (le->next) le->next->prev = le->prev; else list->tail = le->prev;
    le->next = le->prev = NULL;
    PZ_Unlock(list->lock);
    PK11_FreeSlotListElement(list,le);
    return SECSuccess;
}

/*
 * Move a list to the end of the target list. NOTE: There is no locking
 * here... This assumes BOTH lists are private copy lists.
 */
SECStatus
PK11_MoveListToList(PK11SlotList *target,PK11SlotList *src)
{
    if (src->head == NULL) return SECSuccess;

    if (target->tail == NULL) {
	target->head = src->head;
    } else {
	target->tail->next = src->head;
    }
    src->head->prev = target->tail;
    target->tail = src->tail;
    src->head = src->tail = NULL;
    return SECSuccess;
}

/*
 * get an element from the list with a reference. You must own the list.
 */
PK11SlotListElement *
PK11_GetFirstRef(PK11SlotList *list)
{
    PK11SlotListElement *le;

    le = list->head;
    if (le != NULL) (le)->refCount++;
    return le;
}

/*
 * get the next element from the list with a reference. You must own the list.
 */
PK11SlotListElement *
PK11_GetNextRef(PK11SlotList *list, PK11SlotListElement *le, PRBool restart)
{
    PK11SlotListElement *new_le;
    new_le = le->next;
    if (new_le) new_le->refCount++;
    PK11_FreeSlotListElement(list,le);
    return new_le;
}

/*
 * get an element safely from the list. This just makes sure that if
 * this element is not deleted while we deal with it.
 */
PK11SlotListElement *
PK11_GetFirstSafe(PK11SlotList *list)
{
    PK11SlotListElement *le;

    PZ_Lock(list->lock);
    le = list->head;
    if (le != NULL) (le)->refCount++;
    PZ_Unlock(list->lock);
    return le;
}

/*
 * NOTE: if this element gets deleted, we can no longer safely traverse using
 * it's pointers. We can either terminate the loop, or restart from the
 * beginning. This is controlled by the restart option.
 */
PK11SlotListElement *
PK11_GetNextSafe(PK11SlotList *list, PK11SlotListElement *le, PRBool restart)
{
    PK11SlotListElement *new_le;
    PZ_Lock(list->lock);
    new_le = le->next;
    if (le->next == NULL) {
	/* if the prev and next fields are NULL then either this element
	 * has been removed and we need to walk the list again (if restart
	 * is true) or this was the only element on the list */
	if ((le->prev == NULL) && restart &&  (list->head != le)) {
	    new_le = list->head;
	}
    }
    if (new_le) new_le->refCount++;
    PZ_Unlock(list->lock);
    PK11_FreeSlotListElement(list,le);
    return new_le;
}


/*
 * Find the element that holds this slot
 */
PK11SlotListElement *
PK11_FindSlotElement(PK11SlotList *list,PK11SlotInfo *slot)
{
    PK11SlotListElement *le;

    for (le = PK11_GetFirstSafe(list); le;
			 	le = PK11_GetNextSafe(list,le,PR_TRUE)) {
	if (le->slot == slot) return le;
    }
    return NULL;
}

/************************************************************
 * Generic Slot Utilities
 ************************************************************/
/*
 * Create a new slot structure
 */
PK11SlotInfo *
PK11_NewSlotInfo(SECMODModule *mod)
{
    PK11SlotInfo *slot;

    slot = (PK11SlotInfo *)PORT_Alloc(sizeof(PK11SlotInfo));
    if (slot == NULL) return slot;

    slot->sessionLock = mod->isThreadSafe ?
	PZ_NewLock(nssILockSession) : mod->refLock;
    if (slot->sessionLock == NULL) {
	PORT_Free(slot);
	return NULL;
    }
    slot->freeListLock = PZ_NewLock(nssILockFreelist);
    if (slot->freeListLock == NULL) {
	if (mod->isThreadSafe) {
	    PZ_DestroyLock(slot->sessionLock);
	}
	PORT_Free(slot);
	return NULL;
    }
    slot->freeSymKeysWithSessionHead = NULL;
    slot->freeSymKeysHead = NULL;
    slot->keyCount = 0;
    slot->maxKeyCount = 0;
    slot->functionList = NULL;
    slot->needTest = PR_TRUE;
    slot->isPerm = PR_FALSE;
    slot->isHW = PR_FALSE;
    slot->isInternal = PR_FALSE;
    slot->isThreadSafe = PR_FALSE;
    slot->disabled = PR_FALSE;
    slot->series = 1;
    slot->wrapKey = 0;
    slot->wrapMechanism = CKM_INVALID_MECHANISM;
    slot->refKeys[0] = CK_INVALID_HANDLE;
    slot->reason = PK11_DIS_NONE;
    slot->readOnly = PR_TRUE;
    slot->needLogin = PR_FALSE;
    slot->hasRandom = PR_FALSE;
    slot->defRWSession = PR_FALSE;
    slot->protectedAuthPath = PR_FALSE;
    slot->flags = 0;
    slot->session = CK_INVALID_SESSION;
    slot->slotID = 0;
    slot->defaultFlags = 0;
    slot->refCount = 1;
    slot->askpw = 0;
    slot->timeout = 0;
    slot->mechanismList = NULL;
    slot->mechanismCount = 0;
    slot->cert_array = NULL;
    slot->cert_count = 0;
    slot->slot_name[0] = 0;
    slot->token_name[0] = 0;
    PORT_Memset(slot->serial,' ',sizeof(slot->serial));
    slot->module = NULL;
    slot->authTransact = 0;
    slot->authTime = LL_ZERO;
    slot->minPassword = 0;
    slot->maxPassword = 0;
    slot->hasRootCerts = PR_FALSE;
    slot->nssToken = NULL;
    return slot;
}
    
/* create a new reference to a slot so it doesn't go away */
PK11SlotInfo *
PK11_ReferenceSlot(PK11SlotInfo *slot)
{
    PR_AtomicIncrement(&slot->refCount);
    return slot;
}

/* Destroy all info on a slot we have built up */
void
PK11_DestroySlot(PK11SlotInfo *slot)
{
   /* free up the cached keys and sessions */
   PK11_CleanKeyList(slot);
   
   /* free up all the sessions on this slot */
   if (slot->functionList) {
	PK11_GETTAB(slot)->C_CloseAllSessions(slot->slotID);
   }

   if (slot->mechanismList) {
	PORT_Free(slot->mechanismList);
   }
   if (slot->isThreadSafe && slot->sessionLock) {
	PZ_DestroyLock(slot->sessionLock);
   }
   slot->sessionLock = NULL;
   if (slot->freeListLock) {
	PZ_DestroyLock(slot->freeListLock);
	slot->freeListLock = NULL;
   }

   /* finally Tell our parent module that we've gone away so it can unload */
   if (slot->module) {
	SECMOD_SlotDestroyModule(slot->module,PR_TRUE);
   }

   /* ok, well not quit finally... now we free the memory */
   PORT_Free(slot);
}


/* We're all done with the slot, free it */
void
PK11_FreeSlot(PK11SlotInfo *slot)
{
    if (PR_AtomicDecrement(&slot->refCount) == 0) {
	PK11_DestroySlot(slot);
    }
}

void
PK11_EnterSlotMonitor(PK11SlotInfo *slot) {
    PZ_Lock(slot->sessionLock);
}

void
PK11_ExitSlotMonitor(PK11SlotInfo *slot) {
    PZ_Unlock(slot->sessionLock);
}

/***********************************************************
 * Functions to find specific slots.
 ***********************************************************/
PRBool
SECMOD_HasRootCerts(void)
{
   SECMODModuleList *mlp;
   SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
   SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
   int i;
   PRBool found = PR_FALSE;

   /* work through all the slots */
   SECMOD_GetReadLock(moduleLock);
   for(mlp = modules; mlp != NULL; mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    PK11SlotInfo *tmpSlot = mlp->module->slots[i];
	    if (PK11_IsPresent(tmpSlot)) {
		if (tmpSlot->hasRootCerts) {
		    found = PR_TRUE;
		    break;
		}
	    }
	}
	if (found) break;
    }
    SECMOD_ReleaseReadLock(moduleLock);

    return found;
}

/***********************************************************
 * Functions to find specific slots.
 ***********************************************************/
PK11SlotList *
PK11_FindSlotsByNames(const char *dllName, const char* slotName,
                        const char* tokenName, PRBool presentOnly)
{
    SECMODModuleList *mlp;
    SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
    SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
    int i;
    PK11SlotList* slotList = NULL;
    PRUint32 slotcount = 0;
    SECStatus rv = SECSuccess;

    slotList = PK11_NewSlotList();
    if (!slotList) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    if ( ((NULL == dllName) || (0 == *dllName)) &&
        ((NULL == slotName) || (0 == *slotName)) &&
        ((NULL == tokenName) || (0 == *tokenName)) ) {
        /* default to softoken */
        PK11_AddSlotToList(slotList, PK11_GetInternalKeySlot());
        return slotList;
    }

    /* work through all the slots */
    SECMOD_GetReadLock(moduleLock);
    for (mlp = modules; mlp != NULL; mlp = mlp->next) {
        PORT_Assert(mlp->module);
        if (!mlp->module) {
            rv = SECFailure;
            break;
        }
        if ((!dllName) || (mlp->module->dllName &&
            (0 == PORT_Strcmp(mlp->module->dllName, dllName)))) {
            for (i=0; i < mlp->module->slotCount; i++) {
                PK11SlotInfo *tmpSlot = (mlp->module->slots?mlp->module->slots[i]:NULL);
                PORT_Assert(tmpSlot);
                if (!tmpSlot) {
                    rv = SECFailure;
                    break;
                }
                if ((PR_FALSE == presentOnly || PK11_IsPresent(tmpSlot)) &&
                    ( (!tokenName) || (tmpSlot->token_name &&
                    (0==PORT_Strcmp(tmpSlot->token_name, tokenName)))) &&
                    ( (!slotName) || (tmpSlot->slot_name &&
                    (0==PORT_Strcmp(tmpSlot->slot_name, slotName)))) ) {
                    PK11SlotInfo* slot = PK11_ReferenceSlot(tmpSlot);
                    if (slot) {
                        PK11_AddSlotToList(slotList, slot);
                        slotcount++;
                    }
                }
            }
        }
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if ( (0 == slotcount) || (SECFailure == rv) ) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        PK11_FreeSlotList(slotList);
        slotList = NULL;
    }

    if (SECFailure == rv) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    }

    return slotList;
}

PK11SlotInfo *
PK11_FindSlotByName(const char *name)
{
   SECMODModuleList *mlp;
   SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
   SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
   int i;
   PK11SlotInfo *slot = NULL;

   if ((name == NULL) || (*name == 0)) {
	return PK11_GetInternalKeySlot();
   }

   /* work through all the slots */
   SECMOD_GetReadLock(moduleLock);
   for(mlp = modules; mlp != NULL; mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    PK11SlotInfo *tmpSlot = mlp->module->slots[i];
	    if (PK11_IsPresent(tmpSlot)) {
		if (PORT_Strcmp(tmpSlot->token_name,name) == 0) {
		    slot = PK11_ReferenceSlot(tmpSlot);
		    break;
		}
	    }
	}
	if (slot != NULL) break;
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if (slot == NULL) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
    }

    return slot;
}


PK11SlotInfo *
PK11_FindSlotBySerial(char *serial)
{
   SECMODModuleList *mlp;
   SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
   SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
   int i;
   PK11SlotInfo *slot = NULL;

   /* work through all the slots */
   SECMOD_GetReadLock(moduleLock);
   for(mlp = modules; mlp != NULL; mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    PK11SlotInfo *tmpSlot = mlp->module->slots[i];
	    if (PK11_IsPresent(tmpSlot)) {
		if (PORT_Memcmp(tmpSlot->serial,serial,
					sizeof(tmpSlot->serial)) == 0) {
		    slot = PK11_ReferenceSlot(tmpSlot);
		    break;
		}
	    }
	}
	if (slot != NULL) break;
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if (slot == NULL) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
    }

    return slot;
}

/*
 * notification stub. If we ever get interested in any events that
 * the pkcs11 functions may pass back to use, we can catch them here...
 * currently pdata is a slotinfo structure.
 */
CK_RV pk11_notify(CK_SESSION_HANDLE session, CK_NOTIFICATION event,
							 CK_VOID_PTR pdata)
{
    return CKR_OK;
}

/*
 * grab a new RW session
 * !!! has a side effect of grabbing the Monitor if either the slot's default
 * session is RW or the slot is not thread safe. Monitor is release in function
 * below
 */
CK_SESSION_HANDLE PK11_GetRWSession(PK11SlotInfo *slot)
{
    CK_SESSION_HANDLE rwsession;
    CK_RV crv;
    PRBool haveMonitor = PR_FALSE;

    if (!slot->isThreadSafe || slot->defRWSession) {
    	PK11_EnterSlotMonitor(slot);
	haveMonitor = PR_TRUE;
    }
    if (slot->defRWSession) {
	PORT_Assert(slot->session != CK_INVALID_SESSION);
	if (slot->session != CK_INVALID_SESSION) 
	    return slot->session;
    }

    crv = PK11_GETTAB(slot)->C_OpenSession(slot->slotID,
				CKF_RW_SESSION|CKF_SERIAL_SESSION,
				  	  slot, pk11_notify,&rwsession);
    PORT_Assert(rwsession != CK_INVALID_SESSION || crv != CKR_OK);
    if (crv != CKR_OK || rwsession == CK_INVALID_SESSION) {
	if (crv == CKR_OK) 
	    crv = CKR_DEVICE_ERROR;
	if (haveMonitor)
	    PK11_ExitSlotMonitor(slot);
	PORT_SetError(PK11_MapError(crv));
	return CK_INVALID_SESSION;
    }
    if (slot->defRWSession) { /* we have the monitor */
    	slot->session = rwsession;
    }
    return rwsession;
}

PRBool
PK11_RWSessionHasLock(PK11SlotInfo *slot,CK_SESSION_HANDLE session_handle) 
{
    PRBool hasLock;
    hasLock = (PRBool)(!slot->isThreadSafe || 
    	      (slot->defRWSession && slot->session != CK_INVALID_SESSION));
    return hasLock;
}

static PRBool
pk11_RWSessionIsDefault(PK11SlotInfo *slot,CK_SESSION_HANDLE rwsession)
{
    PRBool isDefault;
    isDefault = (PRBool)(slot->session == rwsession &&
    	                 slot->defRWSession && 
			 slot->session != CK_INVALID_SESSION);
    return isDefault;
}

/*
 * close the rwsession and restore our readonly session
 * !!! has a side effect of releasing the Monitor if either the slot's default
 * session is RW or the slot is not thread safe.
 */
void
PK11_RestoreROSession(PK11SlotInfo *slot,CK_SESSION_HANDLE rwsession)
{
    PORT_Assert(rwsession != CK_INVALID_SESSION);
    if (rwsession != CK_INVALID_SESSION) {
    	PRBool doExit = PK11_RWSessionHasLock(slot, rwsession);
	if (!pk11_RWSessionIsDefault(slot, rwsession))
	    PK11_GETTAB(slot)->C_CloseSession(rwsession);
	if (doExit)
	    PK11_ExitSlotMonitor(slot);
    }
}

/************************************************************
 * Manage the built-In Slot Lists
 ************************************************************/

/* Init the static built int slot list (should actually integrate
 * with PK11_NewSlotList */
static void
pk11_InitSlotListStatic(PK11SlotList *list)
{
    list->lock = PZ_NewLock(nssILockList);
    list->head = NULL;
}


/* initialize the system slotlists */
SECStatus
PK11_InitSlotLists(void)
{
    pk11_InitSlotListStatic(&pk11_aesSlotList);
    pk11_InitSlotListStatic(&pk11_desSlotList);
    pk11_InitSlotListStatic(&pk11_rc4SlotList);
    pk11_InitSlotListStatic(&pk11_rc2SlotList);
    pk11_InitSlotListStatic(&pk11_rc5SlotList);
    pk11_InitSlotListStatic(&pk11_md5SlotList);
    pk11_InitSlotListStatic(&pk11_md2SlotList);
    pk11_InitSlotListStatic(&pk11_sha1SlotList);
    pk11_InitSlotListStatic(&pk11_rsaSlotList);
    pk11_InitSlotListStatic(&pk11_dsaSlotList);
    pk11_InitSlotListStatic(&pk11_dhSlotList);
    pk11_InitSlotListStatic(&pk11_ecSlotList);
    pk11_InitSlotListStatic(&pk11_ideaSlotList);
    pk11_InitSlotListStatic(&pk11_sslSlotList);
    pk11_InitSlotListStatic(&pk11_tlsSlotList);
    pk11_InitSlotListStatic(&pk11_randomSlotList);
    pk11_InitSlotListStatic(&pk11_sha256SlotList);
    pk11_InitSlotListStatic(&pk11_sha512SlotList);
    return SECSuccess;
}

void
PK11_DestroySlotLists(void)
{
    pk11_FreeSlotListStatic(&pk11_aesSlotList);
    pk11_FreeSlotListStatic(&pk11_desSlotList);
    pk11_FreeSlotListStatic(&pk11_rc4SlotList);
    pk11_FreeSlotListStatic(&pk11_rc2SlotList);
    pk11_FreeSlotListStatic(&pk11_rc5SlotList);
    pk11_FreeSlotListStatic(&pk11_md5SlotList);
    pk11_FreeSlotListStatic(&pk11_md2SlotList);
    pk11_FreeSlotListStatic(&pk11_sha1SlotList);
    pk11_FreeSlotListStatic(&pk11_rsaSlotList);
    pk11_FreeSlotListStatic(&pk11_dsaSlotList);
    pk11_FreeSlotListStatic(&pk11_dhSlotList);
    pk11_FreeSlotListStatic(&pk11_ecSlotList);
    pk11_FreeSlotListStatic(&pk11_ideaSlotList);
    pk11_FreeSlotListStatic(&pk11_sslSlotList);
    pk11_FreeSlotListStatic(&pk11_tlsSlotList);
    pk11_FreeSlotListStatic(&pk11_randomSlotList);
    pk11_FreeSlotListStatic(&pk11_sha256SlotList);
    pk11_FreeSlotListStatic(&pk11_sha512SlotList);
    return;
}

/* return a system slot list based on mechanism */
PK11SlotList *
PK11_GetSlotList(CK_MECHANISM_TYPE type)
{
/* XXX a workaround for Bugzilla bug #55267 */
#if defined(HPUX) && defined(__LP64__)
    if (CKM_INVALID_MECHANISM == type)
        return NULL;
#endif
    switch (type) {
    case CKM_AES_CBC:
    case CKM_AES_ECB:
	return &pk11_aesSlotList;
    case CKM_DES_CBC:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_DES3_CBC:
	return &pk11_desSlotList;
    case CKM_RC4:
	return &pk11_rc4SlotList;
    case CKM_RC5_CBC:
	return &pk11_rc5SlotList;
    case CKM_SHA_1:
	return &pk11_sha1SlotList;
    case CKM_SHA256:
	return &pk11_sha256SlotList;
    case CKM_SHA384:
    case CKM_SHA512:
	return &pk11_sha512SlotList;
    case CKM_MD5:
	return &pk11_md5SlotList;
    case CKM_MD2:
	return &pk11_md2SlotList;
    case CKM_RC2_ECB:
    case CKM_RC2_CBC:
	return &pk11_rc2SlotList;
    case CKM_RSA_PKCS:
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
    case CKM_RSA_X_509:
	return &pk11_rsaSlotList;
    case CKM_DSA:
	return &pk11_dsaSlotList;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
    case CKM_DH_PKCS_DERIVE:
	return &pk11_dhSlotList;
    case CKM_ECDSA:
    case CKM_ECDSA_SHA1:
    case CKM_EC_KEY_PAIR_GEN: /* aka CKM_ECDSA_KEY_PAIR_GEN */
    case CKM_ECDH1_DERIVE:
	return &pk11_ecSlotList;
    case CKM_SSL3_PRE_MASTER_KEY_GEN:
    case CKM_SSL3_MASTER_KEY_DERIVE:
    case CKM_SSL3_SHA1_MAC:
    case CKM_SSL3_MD5_MAC:
	return &pk11_sslSlotList;
    case CKM_TLS_MASTER_KEY_DERIVE:
    case CKM_TLS_KEY_AND_MAC_DERIVE:
	return &pk11_tlsSlotList;
    case CKM_IDEA_CBC:
    case CKM_IDEA_ECB:
	return &pk11_ideaSlotList;
    case CKM_FAKE_RANDOM:
	return &pk11_randomSlotList;
    }
    return NULL;
}

/*
 * load the static SlotInfo structures used to select a PKCS11 slot.
 * preSlotInfo has a list of all the default flags for the slots on this
 * module.
 */
void
PK11_LoadSlotList(PK11SlotInfo *slot, PK11PreSlotInfo *psi, int count)
{
    int i;

    for (i=0; i < count; i++) {
	if (psi[i].slotID == slot->slotID)
	    break;
    }

    if (i == count) return;

    slot->defaultFlags = psi[i].defaultFlags;
    slot->askpw = psi[i].askpw;
    slot->timeout = psi[i].timeout;
    slot->hasRootCerts = psi[i].hasRootCerts;

    /* if the slot is already disabled, don't load them into the
     * default slot lists. We get here so we can save the default
     * list value. */
    if (slot->disabled) return;

    /* if the user has disabled us, don't load us in */
    if (slot->defaultFlags & PK11_DISABLE_FLAG) {
	slot->disabled = PR_TRUE;
	slot->reason = PK11_DIS_USER_SELECTED;
	/* free up sessions and things?? */
	return;
    }

    for (i=0; i < num_pk11_default_mechanisms; i++) {
	if (slot->defaultFlags & PK11_DefaultArray[i].flag) {
	    CK_MECHANISM_TYPE mechanism = PK11_DefaultArray[i].mechanism;
	    PK11SlotList *slotList = PK11_GetSlotList(mechanism);

	    if (slotList) PK11_AddSlotToList(slotList,slot);
	}
    }

    return;
}


/*
 * update a slot to its new attribute according to the slot list
 * returns: SECSuccess if nothing to do or add/delete is successful
 */
SECStatus
PK11_UpdateSlotAttribute(PK11SlotInfo *slot, PK11DefaultArrayEntry *entry,
                        PRBool add)  
                        /* add: PR_TRUE if want to turn on */
{
    SECStatus result = SECSuccess;
    PK11SlotList *slotList = PK11_GetSlotList(entry->mechanism);

    if (add) { /* trying to turn on a mechanism */
                 
        /* turn on the default flag in the slot */
        slot->defaultFlags |= entry->flag;
        
        /* add this slot to the list */
        if (slotList!=NULL)
            result = PK11_AddSlotToList(slotList, slot);
        
    } else { /* trying to turn off */
            
        /* turn OFF the flag in the slot */ 
        slot->defaultFlags &= ~entry->flag;
        
        if (slotList) {
            /* find the element in the list & delete it */
            PK11SlotListElement *le = PK11_FindSlotElement(slotList, slot);

            /* remove the slot from the list */
            if (le)
                result = PK11_DeleteSlotFromList(slotList, le);
        }
    }
    return result;
}

/*
 * clear a slot off of all of it's default list
 */
void
PK11_ClearSlotList(PK11SlotInfo *slot)
{
    int i;

    if (slot->disabled) return;
    if (slot->defaultFlags == 0) return;

    for (i=0; i < num_pk11_default_mechanisms; i++) {
	if (slot->defaultFlags & PK11_DefaultArray[i].flag) {
	    CK_MECHANISM_TYPE mechanism = PK11_DefaultArray[i].mechanism;
	    PK11SlotList *slotList = PK11_GetSlotList(mechanism);
	    PK11SlotListElement *le = NULL;

	    if (slotList) le = PK11_FindSlotElement(slotList,slot);

	    if (le) {
		PK11_DeleteSlotFromList(slotList,le);
		PK11_FreeSlotListElement(slotList,le);
	    }
	}
    }
}


/******************************************************************
 *           Slot initialization
 ******************************************************************/
/*
 * turn a PKCS11 Static Label into a string
 */
char *
PK11_MakeString(PRArenaPool *arena,char *space,
					char *staticString,int stringLen)
{
	int i;
	char *newString;
	for(i=(stringLen-1); i >= 0; i--) {
	  if (staticString[i] != ' ') break;
	}
	/* move i to point to the last space */
	i++;
	if (arena) {
	    newString = (char*)PORT_ArenaAlloc(arena,i+1 /* space for NULL */);
	} else if (space) {
	    newString = space;
	} else {
	    newString = (char*)PORT_Alloc(i+1 /* space for NULL */);
	}
	if (newString == NULL) return NULL;

	if (i) PORT_Memcpy(newString,staticString, i);
	newString[i] = 0;

	return newString;
}

/*
 * verify that slot implements Mechanism mech properly by checking against
 * our internal implementation
 */
PRBool
PK11_VerifyMechanism(PK11SlotInfo *slot,PK11SlotInfo *intern,
  CK_MECHANISM_TYPE mech, SECItem *data, SECItem *iv)
{
    PK11Context *test = NULL, *reference = NULL;
    PK11SymKey *symKey = NULL, *testKey = NULL;
    SECItem *param = NULL;
    unsigned char encTest[8];
    unsigned char encRef[8];
    int outLenTest,outLenRef;
    int key_size = 0;
    PRBool verify = PR_FALSE;
    SECStatus rv;

    if ((mech == CKM_RC2_CBC) || (mech == CKM_RC2_ECB) || (mech == CKM_RC4)) {
	key_size = 16;
    }

    /* initialize the mechanism parameter */
    param = PK11_ParamFromIV(mech,iv);
    if (param == NULL) goto loser;

    /* load the keys and contexts */
    symKey = PK11_KeyGen(intern,mech,NULL, key_size, NULL);
    if (symKey == NULL) goto loser;

    reference = PK11_CreateContextBySymKey(mech, CKA_ENCRYPT, symKey, param);
    if (reference == NULL) goto loser;

    testKey = pk11_CopyToSlot(slot, mech, CKA_ENCRYPT, symKey);
    if (testKey == NULL) goto loser;

    test = PK11_CreateContextBySymKey(mech, CKA_ENCRYPT, testKey, param);
    if (test == NULL) goto loser;
    SECITEM_FreeItem(param,PR_TRUE); param = NULL;

    /* encrypt the test data */
    rv = PK11_CipherOp(test,encTest,&outLenTest,sizeof(encTest),
							data->data,data->len);
    if (rv != SECSuccess) goto loser;
    rv = PK11_CipherOp(reference,encRef,&outLenRef,sizeof(encRef),
							data->data,data->len);
    if (rv != SECSuccess) goto loser;

    PK11_DestroyContext(reference,PR_TRUE); reference = NULL;
    PK11_DestroyContext(test,PR_TRUE); test = NULL;

    if (outLenTest != outLenRef) goto loser;
    if (PORT_Memcmp(encTest, encRef, outLenTest) != 0) goto loser;

    verify = PR_TRUE;

loser:
    if (test) PK11_DestroyContext(test,PR_TRUE);
    if (symKey) PK11_FreeSymKey(symKey);
    if (testKey) PK11_FreeSymKey(testKey);
    if (reference) PK11_DestroyContext(reference,PR_TRUE);
    if (param) SECITEM_FreeItem(param,PR_TRUE);

    return verify;
}

/*
 * this code verifies that the advertised mechanisms are what they
 * seem to be.
 */
#define MAX_MECH_LIST_SIZE 30	/* we only know of about 30 odd mechanisms */
PRBool
PK11_VerifySlotMechanisms(PK11SlotInfo *slot)
{
    CK_MECHANISM_TYPE mechListArray[MAX_MECH_LIST_SIZE];
    CK_MECHANISM_TYPE *mechList = mechListArray;
    static SECItem data;
    static SECItem iv;
    static unsigned char dataV[8];
    static unsigned char ivV[8];
    static PRBool generated = PR_FALSE;
    CK_ULONG count;
    int i;
    CK_RV crv;

    PRBool alloced = PR_FALSE;
    PK11SlotInfo *intern = PK11_GetInternalSlot();

    /* if we couldn't initialize an internal module, 
     * we can't check external ones */
    if (intern == NULL) return PR_FALSE;

    /* first get the count of mechanisms */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID,NULL,&count);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PK11_FreeSlot(intern);
	return PR_FALSE;
    }


    /* don't blow up just because the card supports more mechanisms than
     * we know about, just alloc space for them */
    if (count > MAX_MECH_LIST_SIZE) {
    	mechList = (CK_MECHANISM_TYPE *)
			    PORT_Alloc(count *sizeof(CK_MECHANISM_TYPE));
	alloced = PR_TRUE;
	if (mechList == NULL) {
	    PK11_FreeSlot(intern);
	    return PR_FALSE;
	}
    }
    /* get the list */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv =PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID, mechList, &count);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	if (alloced) PORT_Free(mechList);
	PK11_FreeSlot(intern);
	return PR_FALSE;
    }

    if (!generated) {
	data.data = dataV;
	data.len = sizeof(dataV);
	iv.data = ivV;
	iv.len = sizeof(ivV);
	/* ok, this is a cheat, we know our internal random number generater
	 * is thread safe */
	PK11_GETTAB(intern)->C_GenerateRandom(intern->session,
							data.data, data.len);
	PK11_GETTAB(intern)->C_GenerateRandom(intern->session,
							iv.data, iv.len);
    }
    for (i=0; i < (int) count; i++) {
	switch (mechList[i]) {
	case CKM_DES_CBC:
	case CKM_DES_ECB:
	case CKM_RC4:
	case CKM_RC2_CBC:
	case CKM_RC2_ECB:
	    if (!PK11_VerifyMechanism(slot,intern,mechList[i],&data,&iv)){
		if (alloced) PORT_Free(mechList);
    		PK11_FreeSlot(intern);
		return PR_FALSE;
	    }
	}
    }
    if (alloced) PORT_Free(mechList);
    PK11_FreeSlot(intern);
    return PR_TRUE;
}

/*
 * See if we need to run the verify test, do so if necessary. If we fail,
 * disable the slot.
 */    
SECStatus
pk11_CheckVerifyTest(PK11SlotInfo *slot)
{
    PK11_EnterSlotMonitor(slot);
    if (slot->needTest) {
	slot->needTest = PR_FALSE; 
    	PK11_ExitSlotMonitor(slot);
	if (!PK11_VerifySlotMechanisms(slot)) {
	    (void)PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    slot->session = CK_INVALID_SESSION;
	    PK11_ClearSlotList(slot);
	    slot->disabled = PR_TRUE;
	    slot->reason = PK11_DIS_TOKEN_VERIFY_FAILED;
	    slot->needTest = PR_TRUE;
	    PORT_SetError(SEC_ERROR_IO);
	    return SECFailure;
	}
    } else {
    	PK11_ExitSlotMonitor(slot);
    }
    return SECSuccess;
}

/*
 * Reads in the slots mechanism list for later use
 */
SECStatus
PK11_ReadMechanismList(PK11SlotInfo *slot)
{
    CK_ULONG count;
    CK_RV crv;
    PRUint32 i;

    if (slot->mechanismList) {
	PORT_Free(slot->mechanismList);
	slot->mechanismList = NULL;
    }
    slot->mechanismCount = 0;

    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID,NULL,&count);
    if (crv != CKR_OK) {
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }

    slot->mechanismList = (CK_MECHANISM_TYPE *)
			    PORT_Alloc(count *sizeof(CK_MECHANISM_TYPE));
    if (slot->mechanismList == NULL) {
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID,
						slot->mechanismList, &count);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_Free(slot->mechanismList);
	slot->mechanismList = NULL;
	PORT_SetError(PK11_MapError(crv));
	return SECSuccess;
    }
    slot->mechanismCount = count;
    PORT_Memset(slot->mechanismBits, 0, sizeof(slot->mechanismBits));

    for (i=0; i < count; i++) {
	CK_MECHANISM_TYPE mech = slot->mechanismList[i];
	if (mech < 0x7ff) {
	    slot->mechanismBits[mech & 0xff] |= 1 << (mech >> 8);
	}
    }
    return SECSuccess;
}

/*
 * initialize a new token
 * unlike initialize slot, this can be called multiple times in the lifetime
 * of NSS. It reads the information associated with a card or token,
 * that is not going to change unless the card or token changes.
 */
SECStatus
PK11_InitToken(PK11SlotInfo *slot, PRBool loadCerts)
{
    CK_TOKEN_INFO tokenInfo;
    CK_RV crv;
    char *tmp;
    SECStatus rv;

    /* set the slot flags to the current token values */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetTokenInfo(slot->slotID,&tokenInfo);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }

    /* set the slot flags to the current token values */
    slot->series++; /* allow other objects to detect that the 
		      * slot is different */
    slot->flags = tokenInfo.flags;
    slot->needLogin = ((tokenInfo.flags & CKF_LOGIN_REQUIRED) ? 
							PR_TRUE : PR_FALSE);
    slot->readOnly = ((tokenInfo.flags & CKF_WRITE_PROTECTED) ? 
							PR_TRUE : PR_FALSE);
    slot->hasRandom = ((tokenInfo.flags & CKF_RNG) ? PR_TRUE : PR_FALSE);
    slot->protectedAuthPath =
    		((tokenInfo.flags & CKF_PROTECTED_AUTHENTICATION_PATH) 
	 						? PR_TRUE : PR_FALSE);
    slot->lastLoginCheck = 0;
    slot->lastState = 0;
    /* on some platforms Active Card incorrectly sets the 
     * CKF_PROTECTED_AUTHENTICATION_PATH bit when it doesn't mean to. */
    if (slot->isActiveCard) {
	slot->protectedAuthPath = PR_FALSE;
    }
    tmp = PK11_MakeString(NULL,slot->token_name,
			(char *)tokenInfo.label, sizeof(tokenInfo.label));
    slot->minPassword = tokenInfo.ulMinPinLen;
    slot->maxPassword = tokenInfo.ulMaxPinLen;
    PORT_Memcpy(slot->serial,tokenInfo.serialNumber,sizeof(slot->serial));

    nssToken_UpdateName(slot->nssToken);

    slot->defRWSession = (PRBool)((!slot->readOnly) && 
					(tokenInfo.ulMaxSessionCount == 1));
    rv = PK11_ReadMechanismList(slot);
    if (rv != SECSuccess) return rv;

    slot->hasRSAInfo = PR_FALSE;
    slot->RSAInfoFlags = 0;

    /* initialize the maxKeyCount value */
    if (tokenInfo.ulMaxSessionCount == 0) {
	slot->maxKeyCount = 800; /* should be #define or a config param */
    } else if (tokenInfo.ulMaxSessionCount < 20) {
	/* don't have enough sessions to keep that many keys around */
	slot->maxKeyCount = 0;
    } else {
	slot->maxKeyCount = tokenInfo.ulMaxSessionCount/2;
    }

    /* Make sure our session handle is valid */
    if (slot->session == CK_INVALID_SESSION) {
	/* we know we don't have a valid session, go get one */
	CK_SESSION_HANDLE session;

	/* session should be Readonly, serial */
	if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB(slot)->C_OpenSession(slot->slotID,
	      (slot->defRWSession ? CKF_RW_SESSION : 0) | CKF_SERIAL_SESSION,
				  slot,pk11_notify,&session);
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	if (crv != CKR_OK) {
	    PORT_SetError(PK11_MapError(crv));
	    return SECFailure;
	}
	slot->session = session;
    } else {
	/* The session we have may be defunct (the token associated with it)
	 * has been removed   */
	CK_SESSION_INFO sessionInfo;

	if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB(slot)->C_GetSessionInfo(slot->session,&sessionInfo);
        if (crv == CKR_DEVICE_ERROR) {
	    PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    crv = CKR_SESSION_CLOSED;
	}
	if ((crv==CKR_SESSION_CLOSED) || (crv==CKR_SESSION_HANDLE_INVALID)) {
	    crv =PK11_GETTAB(slot)->C_OpenSession(slot->slotID,
	      (slot->defRWSession ? CKF_RW_SESSION : 0) | CKF_SERIAL_SESSION,
					slot,pk11_notify,&slot->session);
	    if (crv != CKR_OK) {
	        PORT_SetError(PK11_MapError(crv));
		slot->session = CK_INVALID_SESSION;
		if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
		return SECFailure;
	    }
	}
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    }

    nssToken_Refresh(slot->nssToken);

    if (!(slot->needLogin)) {
	return pk11_CheckVerifyTest(slot);
    }


    if (!(slot->isInternal) && (slot->hasRandom)) {
	/* if this slot has a random number generater, use it to add entropy
	 * to the internal slot. */
	PK11SlotInfo *int_slot = PK11_GetInternalSlot();

	if (int_slot) {
	    unsigned char random_bytes[32];

	    /* if this slot can issue random numbers, get some entropy from
	     * that random number generater and give it to our internal token.
	     */
	    PK11_EnterSlotMonitor(slot);
	    crv = PK11_GETTAB(slot)->C_GenerateRandom
			(slot->session,random_bytes, sizeof(random_bytes));
	    PK11_ExitSlotMonitor(slot);
	    if (crv == CKR_OK) {
	        PK11_EnterSlotMonitor(int_slot);
		PK11_GETTAB(int_slot)->C_SeedRandom(int_slot->session,
					random_bytes, sizeof(random_bytes));
	        PK11_ExitSlotMonitor(int_slot);
	    }

	    /* Now return the favor and send entropy to the token's random 
	     * number generater */
	    PK11_EnterSlotMonitor(int_slot);
	    crv = PK11_GETTAB(int_slot)->C_GenerateRandom(int_slot->session,
					random_bytes, sizeof(random_bytes));
	    PK11_ExitSlotMonitor(int_slot);
	    if (crv == CKR_OK) {
	        PK11_EnterSlotMonitor(slot);
		PK11_GETTAB(slot)->C_SeedRandom(slot->session,
					random_bytes, sizeof(random_bytes));
	        PK11_ExitSlotMonitor(slot);
	    }
	    PK11_FreeSlot(int_slot);
	}
    }

	
    return SECSuccess;
}

/*
 * initialize a new token
 * unlike initialize slot, this can be called multiple times in the lifetime
 * of NSS. It reads the information associated with a card or token,
 * that is not going to change unless the card or token changes.
 */
SECStatus
PK11_TokenRefresh(PK11SlotInfo *slot)
{
    CK_TOKEN_INFO tokenInfo;
    CK_RV crv;

    /* set the slot flags to the current token values */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetTokenInfo(slot->slotID,&tokenInfo);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }

    slot->flags = tokenInfo.flags;
    slot->needLogin = ((tokenInfo.flags & CKF_LOGIN_REQUIRED) ? 
							PR_TRUE : PR_FALSE);
    slot->readOnly = ((tokenInfo.flags & CKF_WRITE_PROTECTED) ? 
							PR_TRUE : PR_FALSE);
    slot->hasRandom = ((tokenInfo.flags & CKF_RNG) ? PR_TRUE : PR_FALSE);
    slot->protectedAuthPath =
    		((tokenInfo.flags & CKF_PROTECTED_AUTHENTICATION_PATH) 
	 						? PR_TRUE : PR_FALSE);
    /* on some platforms Active Card incorrectly sets the 
     * CKF_PROTECTED_AUTHENTICATION_PATH bit when it doesn't mean to. */
    if (slot->isActiveCard) {
	slot->protectedAuthPath = PR_FALSE;
    }
    return SECSuccess;
}

static PRBool
pk11_isRootSlot(PK11SlotInfo *slot) 
{
    CK_ATTRIBUTE findTemp[1];
    CK_ATTRIBUTE *attrs;
    CK_OBJECT_CLASS oclass = CKO_NETSCAPE_BUILTIN_ROOT_LIST;
    int tsize;
    CK_OBJECT_HANDLE handle;

    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &oclass, sizeof(oclass)); attrs++;
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    handle = pk11_FindObjectByTemplate(slot,findTemp,tsize);
    if (handle == CK_INVALID_HANDLE) {
	return PR_FALSE;
    }
    return PR_TRUE;
}

/*
 * Initialize the slot :
 * This initialization code is called on each slot a module supports when
 * it is loaded. It does the bringup initialization. The difference between
 * this and InitToken is Init slot does those one time initialization stuff,
 * usually associated with the reader, while InitToken may get called multiple
 * times as tokens are removed and re-inserted.
 */
void
PK11_InitSlot(SECMODModule *mod,CK_SLOT_ID slotID,PK11SlotInfo *slot)
{
    SECStatus rv;
    char *tmp;
    CK_SLOT_INFO slotInfo;

    slot->functionList = mod->functionList;
    slot->isInternal = mod->internal;
    slot->slotID = slotID;
    slot->isThreadSafe = mod->isThreadSafe;
    slot->hasRSAInfo = PR_FALSE;
    
    if (PK11_GETTAB(slot)->C_GetSlotInfo(slotID,&slotInfo) != CKR_OK) {
	slot->disabled = PR_TRUE;
	slot->reason = PK11_DIS_COULD_NOT_INIT_TOKEN;
	return;
    }

    /* test to make sure claimed mechanism work */
    slot->needTest = mod->internal ? PR_FALSE : PR_TRUE;
    slot->module = mod; /* NOTE: we don't make a reference here because
			 * modules have references to their slots. This
			 * works because modules keep implicit references
			 * from their slots, and won't unload and disappear
			 * until all their slots have been freed */
    tmp = PK11_MakeString(NULL,slot->slot_name,
	 (char *)slotInfo.slotDescription, sizeof(slotInfo.slotDescription));
    slot->isHW = (PRBool)((slotInfo.flags & CKF_HW_SLOT) == CKF_HW_SLOT);
#define ACTIVE_CARD "ActivCard SA"
    slot->isActiveCard = (PRBool)(PORT_Strncmp((char *)slotInfo.manufacturerID,
				ACTIVE_CARD, sizeof(ACTIVE_CARD)-1) == 0);
    if ((slotInfo.flags & CKF_REMOVABLE_DEVICE) == 0) {
	slot->isPerm = PR_TRUE;
	/* permanment slots must have the token present always */
	if ((slotInfo.flags & CKF_TOKEN_PRESENT) == 0) {
	    slot->disabled = PR_TRUE;
	    slot->reason = PK11_DIS_TOKEN_NOT_PRESENT;
	    return; /* nothing else to do */
	}
    }
    /* if the token is present, initialize it */
    if ((slotInfo.flags & CKF_TOKEN_PRESENT) != 0) {
	rv = PK11_InitToken(slot,PR_TRUE);
	/* the only hard failures are on permanent devices, or function
	 * verify failures... function verify failures are already handled
	 * by tokenInit */
	if ((rv != SECSuccess) && (slot->isPerm) && (!slot->disabled)) {
	    slot->disabled = PR_TRUE;
	    slot->reason = PK11_DIS_COULD_NOT_INIT_TOKEN;
	}
    }
    if (pk11_isRootSlot(slot)) {
	if (!slot->hasRootCerts) {
	    slot->module->trustOrder = 100;
	}
	slot->hasRootCerts= PR_TRUE;
    }
}

	

/*********************************************************************
 *            Slot mapping utility functions.
 *********************************************************************/

/*
 * determine if the token is present. If the token is present, make sure
 * we have a valid session handle. Also set the value of needLogin 
 * appropriately.
 */
static PRBool
pk11_IsPresentCertLoad(PK11SlotInfo *slot, PRBool loadCerts)
{
    CK_SLOT_INFO slotInfo;
    CK_SESSION_INFO sessionInfo;
    CK_RV crv;

    /* disabled slots are never present */
    if (slot->disabled) {
	return PR_FALSE;
    }

    /* permanent slots are always present */
    if (slot->isPerm && (slot->session != CK_INVALID_SESSION)) {
	return PR_TRUE;
    }

    if (slot->nssToken) {
	return nssToken_IsPresent(slot->nssToken);
    }

    /* removable slots have a flag that says they are present */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    if (PK11_GETTAB(slot)->C_GetSlotInfo(slot->slotID,&slotInfo) != CKR_OK) {
        if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	return PR_FALSE;
    }
    if ((slotInfo.flags & CKF_TOKEN_PRESENT) == 0) {
	/* if the slot is no longer present, close the session */
	if (slot->session != CK_INVALID_SESSION) {
	    PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    slot->session = CK_INVALID_SESSION;
	}
        if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	return PR_FALSE;
    }

    /* use the session Info to determine if the card has been removed and then
     * re-inserted */
    if (slot->session != CK_INVALID_SESSION) {
	if (slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB(slot)->C_GetSessionInfo(slot->session, &sessionInfo);
	if (crv != CKR_OK) {
	    PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    slot->session = CK_INVALID_SESSION;
	}
        if (slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    }
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);

    /* card has not been removed, current token info is correct */
    if (slot->session != CK_INVALID_SESSION) return PR_TRUE;

    /* initialize the token info state */
    if (PK11_InitToken(slot,loadCerts) != SECSuccess) {
	return PR_FALSE;
    }

    return PR_TRUE;
}

/*
 * old version of the routine
 */
PRBool
PK11_IsPresent(PK11SlotInfo *slot) {
   return pk11_IsPresentCertLoad(slot,PR_TRUE);
}

/* is the slot disabled? */
PRBool
PK11_IsDisabled(PK11SlotInfo *slot)
{
    return slot->disabled;
}

/* and why? */
PK11DisableReasons
PK11_GetDisabledReason(PK11SlotInfo *slot)
{
    return slot->reason;
}

/* returns PR_TRUE if successfully disable the slot */
/* returns PR_FALSE otherwise */
PRBool PK11_UserDisableSlot(PK11SlotInfo *slot) {

    slot->defaultFlags |= PK11_DISABLE_FLAG;
    slot->disabled = PR_TRUE;
    slot->reason = PK11_DIS_USER_SELECTED;
    
    return PR_TRUE;
}

PRBool PK11_UserEnableSlot(PK11SlotInfo *slot) {

    slot->defaultFlags &= ~PK11_DISABLE_FLAG;
    slot->disabled = PR_FALSE;
    slot->reason = PK11_DIS_NONE;
    return PR_TRUE;
}

PRBool PK11_HasRootCerts(PK11SlotInfo *slot) {
    return slot->hasRootCerts;
}

/* Get the module this slot is attached to */
SECMODModule *
PK11_GetModule(PK11SlotInfo *slot)
{
	return slot->module;
}

/* return the default flags of a slot */
unsigned long
PK11_GetDefaultFlags(PK11SlotInfo *slot)
{
	return slot->defaultFlags;
}

/*
 * The following wrapper functions allow us to export an opaque slot
 * function to the rest of libsec and the world... */
PRBool
PK11_IsReadOnly(PK11SlotInfo *slot)
{
    return slot->readOnly;
}

PRBool
PK11_IsHW(PK11SlotInfo *slot)
{
    return slot->isHW;
}

PRBool
PK11_IsInternal(PK11SlotInfo *slot)
{
    return slot->isInternal;
}

PRBool
PK11_NeedLogin(PK11SlotInfo *slot)
{
    return slot->needLogin;
}

PRBool
PK11_IsFriendly(PK11SlotInfo *slot)
{
    /* internal slot always has public readable certs */
    return (PRBool)(slot->isInternal || 
		    ((slot->defaultFlags & SECMOD_FRIENDLY_FLAG) == 
		     SECMOD_FRIENDLY_FLAG));
}

char *
PK11_GetTokenName(PK11SlotInfo *slot)
{
     return slot->token_name;
}

char *
PK11_GetSlotName(PK11SlotInfo *slot)
{
     return slot->slot_name;
}

int
PK11_GetSlotSeries(PK11SlotInfo *slot)
{
    return slot->series;
}

int
PK11_GetCurrentWrapIndex(PK11SlotInfo *slot)
{
    return slot->wrapKey;
}

CK_SLOT_ID
PK11_GetSlotID(PK11SlotInfo *slot)
{
    return slot->slotID;
}

SECMODModuleID
PK11_GetModuleID(PK11SlotInfo *slot)
{
    return slot->module->moduleID;
}

static void
pk11_zeroTerminatedToBlankPadded(CK_CHAR *buffer, size_t buffer_size)
{
    CK_CHAR *walk = buffer;
    CK_CHAR *end = buffer + buffer_size;

    /* find the NULL */
    while (walk < end && *walk != '\0') {
	walk++;
    }

    /* clear out the buffer */
    while (walk < end) {
	*walk++ = ' ';
    }
}

/* return the slot info structure */
SECStatus
PK11_GetSlotInfo(PK11SlotInfo *slot, CK_SLOT_INFO *info)
{
    CK_RV crv;

    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    /*
     * some buggy drivers do not fill the buffer completely, 
     * erase the buffer first
     */
    PORT_Memset(info->slotDescription,' ',sizeof(info->slotDescription));
    PORT_Memset(info->manufacturerID,' ',sizeof(info->manufacturerID));
    crv = PK11_GETTAB(slot)->C_GetSlotInfo(slot->slotID,info);
    pk11_zeroTerminatedToBlankPadded(info->slotDescription,
					sizeof(info->slotDescription));
    pk11_zeroTerminatedToBlankPadded(info->manufacturerID,
					sizeof(info->manufacturerID));
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}

/*  return the token info structure */
SECStatus
PK11_GetTokenInfo(PK11SlotInfo *slot, CK_TOKEN_INFO *info)
{
    CK_RV crv;
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    /*
     * some buggy drivers do not fill the buffer completely, 
     * erase the buffer first
     */
    PORT_Memset(info->label,' ',sizeof(info->label));
    PORT_Memset(info->manufacturerID,' ',sizeof(info->manufacturerID));
    PORT_Memset(info->model,' ',sizeof(info->model));
    PORT_Memset(info->serialNumber,' ',sizeof(info->serialNumber));
    crv = PK11_GETTAB(slot)->C_GetTokenInfo(slot->slotID,info);
    pk11_zeroTerminatedToBlankPadded(info->label,sizeof(info->label));
    pk11_zeroTerminatedToBlankPadded(info->manufacturerID,
					sizeof(info->manufacturerID));
    pk11_zeroTerminatedToBlankPadded(info->model,sizeof(info->model));
    pk11_zeroTerminatedToBlankPadded(info->serialNumber,
					sizeof(info->serialNumber));
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}

/* Find out if we need to initialize the user's pin */
PRBool
PK11_NeedUserInit(PK11SlotInfo *slot)
{
    PRBool needUserInit = (PRBool) ((slot->flags & CKF_USER_PIN_INITIALIZED) 
					== 0);

    if (needUserInit) {
	CK_TOKEN_INFO info;
	SECStatus rv;

	/* see if token has been initialized off line */
	rv = PK11_GetTokenInfo(slot, &info);
	if (rv == SECSuccess) {
	    slot->flags = info.flags;
	}
    }
    return (PRBool)((slot->flags & CKF_USER_PIN_INITIALIZED) == 0);
}

/* get the internal key slot. FIPS has only one slot for both key slots and
 * default slots */
PK11SlotInfo *
PK11_GetInternalKeySlot(void)
{
    SECMODModule *mod = SECMOD_GetInternalModule();
    PORT_Assert(mod != NULL);
    if (!mod) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return NULL;
    }
    return PK11_ReferenceSlot(mod->isFIPS ? mod->slots[0] : mod->slots[1]);
}

/* get the internal default slot */
PK11SlotInfo *
PK11_GetInternalSlot(void) 
{
    SECMODModule * mod = SECMOD_GetInternalModule();
    PORT_Assert(mod != NULL);
    if (!mod) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return NULL;
    }
    return PK11_ReferenceSlot(mod->slots[0]);
}

/*
 * check if a given slot supports the requested mechanism
 */
PRBool
PK11_DoesMechanism(PK11SlotInfo *slot, CK_MECHANISM_TYPE type)
{
    int i;

    /* CKM_FAKE_RANDOM is not a real PKCS mechanism. It's a marker to
     * tell us we're looking form someone that has implemented get
     * random bits */
    if (type == CKM_FAKE_RANDOM) {
	return slot->hasRandom;
    }

    /* for most mechanism, bypass the linear lookup */
    if (type < 0x7ff) {
	return (slot->mechanismBits[type & 0xff] & (1 << (type >> 8)))  ?
		PR_TRUE : PR_FALSE;
    }
	   
    for (i=0; i < (int) slot->mechanismCount; i++) {
	if (slot->mechanismList[i] == type) return PR_TRUE;
    }
    return PR_FALSE;
}

/*
 * Return true if a token that can do the desired mechanism exists.
 * This allows us to have hardware tokens that can do function XYZ magically
 * allow SSL Ciphers to appear if they are plugged in.
 */
PRBool
PK11_TokenExists(CK_MECHANISM_TYPE type)
{
    SECMODModuleList *mlp;
    SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
    SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
    PK11SlotInfo *slot;
    PRBool found = PR_FALSE;
    int i;

    /* we only need to know if there is a token that does this mechanism.
     * check the internal module first because it's fast, and supports 
     * almost everything. */
    slot = PK11_GetInternalSlot();
    if (slot) {
    	found = PK11_DoesMechanism(slot,type);
	PK11_FreeSlot(slot);
    }
    if (found) return PR_TRUE; /* bypass getting module locks */

    SECMOD_GetReadLock(moduleLock);
    for(mlp = modules; mlp != NULL && (!found); mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    slot = mlp->module->slots[i];
	    if (PK11_IsPresent(slot)) {
		if (PK11_DoesMechanism(slot,type)) {
		    found = PR_TRUE;
		    break;
		}
	    }
	}
    }
    SECMOD_ReleaseReadLock(moduleLock);
    return found;
}

/*
 * get all the currently available tokens in a list.
 * that can perform the given mechanism. If mechanism is CKM_INVALID_MECHANISM,
 * get all the tokens. Make sure tokens that need authentication are put at
 * the end of this list.
 */
PK11SlotList *
PK11_GetAllTokens(CK_MECHANISM_TYPE type, PRBool needRW, PRBool loadCerts, 
                  void *wincx)
{
    PK11SlotList *     list         = PK11_NewSlotList();
    PK11SlotList *     loginList    = PK11_NewSlotList();
    PK11SlotList *     friendlyList = PK11_NewSlotList();
    SECMODModuleList * mlp;
    SECMODModuleList * modules      = SECMOD_GetDefaultModuleList();
    SECMODListLock *   moduleLock   = SECMOD_GetDefaultModuleListLock();
    int                i;
#if defined( XP_WIN32 ) 
    int                j            = 0;
    PRInt32            waste[16];
#endif

    if ((list == NULL)  || (loginList == NULL) || (friendlyList == NULL)) {
	if (list) PK11_FreeSlotList(list);
	if (loginList) PK11_FreeSlotList(loginList);
	if (friendlyList) PK11_FreeSlotList(friendlyList);
	return NULL;
    }

    SECMOD_GetReadLock(moduleLock);
    for(mlp = modules; mlp != NULL; mlp = mlp->next) {

#if defined( XP_WIN32 ) 
	/* This is works around some horrible cache/page thrashing problems 
	** on Win32.  Without this, this loop can take up to 6 seconds at 
	** 100% CPU on a Pentium-Pro 200.  The thing this changes is to 
	** increase the size of the stack frame and modify it.  
	** Moving the loop code itself seems to have no effect.
	** Dunno why this combination makes a difference, but it does.
	*/
	waste[ j & 0xf] = j++; 
#endif

	for (i = 0; i < mlp->module->slotCount; i++) {
	    PK11SlotInfo *slot = mlp->module->slots[i];

	    if (pk11_IsPresentCertLoad(slot, loadCerts)) {
		if (needRW &&  slot->readOnly) continue;
		if ((type == CKM_INVALID_MECHANISM) 
					|| PK11_DoesMechanism(slot, type)) {
		    if (pk11_LoginStillRequired(slot,wincx)) {
			if (PK11_IsFriendly(slot)) {
			    PK11_AddSlotToList(friendlyList, slot);
			} else {
			    PK11_AddSlotToList(loginList, slot);
			}
		    } else {
			PK11_AddSlotToList(list, slot);
		    }
		}
	    }
	}
    }
    SECMOD_ReleaseReadLock(moduleLock);

    PK11_MoveListToList(list,friendlyList);
    PK11_FreeSlotList(friendlyList);
    PK11_MoveListToList(list,loginList);
    PK11_FreeSlotList(loginList);

    return list;
}

/*
 * NOTE: This routine is working from a private List generated by 
 * PK11_GetAllTokens. That is why it does not need to lock.
 */
PK11SlotList *
PK11_GetPrivateKeyTokens(CK_MECHANISM_TYPE type,PRBool needRW,void *wincx)
{
    PK11SlotList *list = PK11_GetAllTokens(type,needRW,PR_TRUE,wincx);
    PK11SlotListElement *le, *next ;
    SECStatus rv;

    if (list == NULL) return list;

    for (le = list->head ; le; le = next) {
	next = le->next; /* save the pointer here in case we have to 
			  * free the element later */
        rv = PK11_Authenticate(le->slot,PR_TRUE,wincx);
	if (rv != SECSuccess) {
	    PK11_DeleteSlotFromList(list,le);
	    continue;
	}
    }
    return list;
}


/*
 * find the best slot which supports the given
 * Mechanism. In normal cases this should grab the first slot on the list
 * with no fuss.
 */
PK11SlotInfo *
PK11_GetBestSlotMultiple(CK_MECHANISM_TYPE *type, int mech_count, void *wincx)
{
    PK11SlotList *list = NULL;
    PK11SlotListElement *le ;
    PK11SlotInfo *slot = NULL;
    PRBool freeit = PR_FALSE;
    PRBool listNeedLogin = PR_FALSE;
    int i;
    SECStatus rv;

    list = PK11_GetSlotList(type[0]);

    if ((list == NULL) || (list->head == NULL)) {
	/* We need to look up all the tokens for the mechanism */
	list = PK11_GetAllTokens(type[0],PR_FALSE,PR_TRUE,wincx);
	freeit = PR_TRUE;
    }

    /* no one can do it! */
    if (list == NULL) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
	return NULL;
    }

    PORT_SetError(0);


    listNeedLogin = PR_FALSE;
    for (i=0; i < mech_count; i++) {
	if ((type[i] != CKM_FAKE_RANDOM) && 
	    (type[i] != CKM_SHA_1) &&
	    (type[i] != CKM_SHA256) &&
	    (type[i] != CKM_SHA384) &&
	    (type[i] != CKM_SHA512) &&
	    (type[i] != CKM_MD5) && 
	    (type[i] != CKM_MD2)) {
	    listNeedLogin = PR_TRUE;
	    break;
	}
    }

    for (le = PK11_GetFirstSafe(list); le;
			 	le = PK11_GetNextSafe(list,le,PR_TRUE)) {
	if (PK11_IsPresent(le->slot)) {
	    PRBool doExit = PR_FALSE;
	    for (i=0; i < mech_count; i++) {
	    	if (!PK11_DoesMechanism(le->slot,type[i])) {
		    doExit = PR_TRUE;
		    break;
		}
	    }
	    if (doExit) continue;
	      
	    if (listNeedLogin && le->slot->needLogin) {
		rv = PK11_Authenticate(le->slot,PR_TRUE,wincx);
		if (rv != SECSuccess) continue;
	    }
	    slot = le->slot;
	    PK11_ReferenceSlot(slot);
	    PK11_FreeSlotListElement(list,le);
	    if (freeit) { PK11_FreeSlotList(list); }
	    return slot;
	}
    }
    if (freeit) { PK11_FreeSlotList(list); }
    if (PORT_GetError() == 0) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
    }
    return NULL;
}

/* original get best slot now calls the multiple version with only one type */
PK11SlotInfo *
PK11_GetBestSlot(CK_MECHANISM_TYPE type, void *wincx)
{
    return PK11_GetBestSlotMultiple(&type, 1, wincx);
}

int
PK11_GetBestKeyLength(PK11SlotInfo *slot,CK_MECHANISM_TYPE mechanism)
{
    CK_MECHANISM_INFO mechanism_info;
    CK_RV crv;

    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismInfo(slot->slotID,
                               mechanism,&mechanism_info);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) return 0;

    if (mechanism_info.ulMinKeySize == mechanism_info.ulMaxKeySize) 
		return 0;
    return mechanism_info.ulMaxKeySize;
}

SECStatus
PK11_SeedRandom(PK11SlotInfo *slot, unsigned char *data, int len) {
    CK_RV crv;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_SeedRandom(slot->session, data, (CK_ULONG)len);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}


SECStatus
PK11_GenerateRandomOnSlot(PK11SlotInfo *slot, unsigned char *data, int len) {
    CK_RV crv;

    if (!slot->isInternal) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GenerateRandom(slot->session,data, 
							(CK_ULONG)len);
    if (!slot->isInternal) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return  SECFailure;
    }
    return SECSuccess;
}

/* Attempts to update the Best Slot for "FAKE RANDOM" generation.
** If that's not the internal slot, then it also attempts to update the
** internal slot.
** The return value indicates if the INTERNAL slot was updated OK.
*/
SECStatus
PK11_RandomUpdate(void *data, size_t bytes)
{
    PK11SlotInfo *slot;
    PRBool        bestIsInternal;
    SECStatus     status;

    slot = PK11_GetBestSlot(CKM_FAKE_RANDOM, NULL);
    if (slot == NULL) {
	slot = PK11_GetInternalSlot();
	if (!slot)
	    return SECFailure;
    }

    bestIsInternal = PK11_IsInternal(slot);
    status = PK11_SeedRandom(slot, data, bytes);
    PK11_FreeSlot(slot);

    if (!bestIsInternal) {
    	/* do internal slot, too. */
    	slot = PK11_GetInternalSlot();	/* can't fail */
	status = PK11_SeedRandom(slot, data, bytes);
	PK11_FreeSlot(slot);
    }
    return status;
}


SECStatus
PK11_GenerateRandom(unsigned char *data,int len) {
    PK11SlotInfo *slot;
    SECStatus rv;

    slot = PK11_GetBestSlot(CKM_FAKE_RANDOM,NULL);
    if (slot == NULL) return SECFailure;

    rv = PK11_GenerateRandomOnSlot(slot, data, len);
    PK11_FreeSlot(slot);
    return rv;
}

/*
 * Reset the token to it's initial state. For the internal module, this will
 * Purge your keydb, and reset your cert db certs to USER_INIT.
 */
SECStatus 
PK11_ResetToken(PK11SlotInfo *slot, char *sso_pwd)
{
    unsigned char tokenName[32];
    int tokenNameLen;
    CK_RV crv;

    /* reconstruct the token name */
    tokenNameLen = PORT_Strlen(slot->token_name);
    if (tokenNameLen > sizeof(tokenName)) {
	tokenNameLen = sizeof(tokenName);
    }

    PORT_Memcpy(tokenName,slot->token_name,tokenNameLen);
    if (tokenNameLen < sizeof(tokenName)) {
	PORT_Memset(&tokenName[tokenNameLen],' ',
					 sizeof(tokenName)-tokenNameLen);
    }

    /* initialize the token */    
    PK11_EnterSlotMonitor(slot);

    /* first shutdown the token. Existing sessions will get closed here */
    PK11_GETTAB(slot)->C_CloseAllSessions(slot->slotID);
    slot->session = CK_INVALID_SESSION;

    /* now re-init the token */ 
    crv = PK11_GETTAB(slot)->C_InitToken(slot->slotID,
	(unsigned char *)sso_pwd, sso_pwd ? PORT_Strlen(sso_pwd): 0, tokenName);

    /* finally bring the token back up */
    PK11_InitToken(slot,PR_TRUE);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    nssTrustDomain_UpdateCachedTokenCerts(slot->nssToken->trustDomain,
	                                      slot->nssToken);
    return SECSuccess;
}
void
PK11Slot_SetNSSToken(PK11SlotInfo *sl, NSSToken *nsst) 
{
    sl->nssToken = nsst;
}

NSSToken *
PK11Slot_GetNSSToken(PK11SlotInfo *sl) 
{
    return sl->nssToken;
}

/*
 * wait for a token to change it's state. The application passes in the expected
 * new state in event. 
 */
PK11TokenStatus
PK11_WaitForTokenEvent(PK11SlotInfo *slot, PK11TokenEvent event, 
	PRIntervalTime timeout, PRIntervalTime latency, int series)
{
   PRIntervalTime first_time = 0;
   PRBool first_time_set = PR_FALSE;
   PRBool waitForRemoval;

   if (slot->isPerm) {
	return PK11TokenNotRemovable;
   }
   if (latency == 0) {
	latency = PR_SecondsToInterval(5);
   }
   waitForRemoval = (PRBool) (event == PK11TokenRemovedOrChangedEvent);

   if (series == 0) {
	series = PK11_GetSlotSeries(slot);
   }
   while (PK11_IsPresent(slot) == waitForRemoval ) {
	PRIntervalTime interval;

	if (waitForRemoval && series != PK11_GetSlotSeries(slot)) {
	    return PK11TokenChanged;
	}
	if (timeout == PR_INTERVAL_NO_WAIT) {
	    return waitForRemoval ? PK11TokenPresent : PK11TokenRemoved;
	}
	if (timeout != PR_INTERVAL_NO_TIMEOUT ) {
	    interval = PR_IntervalNow();
	    if (!first_time_set) {
		first_time = interval;
		first_time_set = PR_TRUE;
	    }
	    if ((interval-first_time) > timeout) {
		return waitForRemoval ? PK11TokenPresent : PK11TokenRemoved;
	    }
	}
	PR_Sleep(latency);
   }
   return waitForRemoval ? PK11TokenRemoved : PK11TokenPresent;
}
