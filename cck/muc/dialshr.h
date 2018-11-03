/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* dialshr.h
	common header file for integrated dialer 

	** WINVER values in this file:
	**      WINVER < 0x400 = Windows NT 3.5, Windows NT 3.51
	**      WINVER = 0x400 = Windows 95, Windows NT SUR (default)
	**      WINVER > 0x400 = Windows NT SUR enhancements

		NB: Certain usages of WINVER would imply WIN32, our header
		file assumes that WINVER >= 0x400 implies WIN32
*/

#include <ras.h>
#include "shlobj.h"
#include "winerror.h"

extern HINSTANCE	gDLL;   		// dll instance    
extern int			gPlatformOS;          // platform OS  (95 or NT40)

// account parameter block
typedef struct ACCOUNTPARAMS 
{
	char		ISPName[ 32 ];
	char		FileName[ 16 ];
	char		DNS[ 16 ];
	char		DNS2[ 16 ];
	char		DomainName[ 255 ];
	char		LoginName[ 64 ];
	char		Password[ 64 ];
	char		ScriptFileName[ 255 ];
	BOOL		ScriptEnabled;
	BOOL		NeedsTTYWindow;
	char		ISPPhoneNum[ 64 ];
	char		ISDNPhoneNum[ 64 ];
	BOOL		VJCompressionEnabled;
	BOOL		IntlMode;
	BOOL		DialOnDemand;
} ACCOUNTPARAMS;
    
    
// location parameter block
typedef struct LOCATIONPARAMS 
{
	char		ModemName[ 255 ];
	char		ModemType[ 80 ];
	BOOL		DialType;
	char		OutsideLineAccess[ 6 ];
	BOOL		DisableCallWaiting;
	char		DisableCallWaitingCode[ 6 ];
	char		UserAreaCode[ 6 ];
	short		UserCountryCode;
	BOOL		DialAsLongDistance;
	char		LongDistanceAccess[ 6 ];
	BOOL		DialAreaCode;
	char		DialPrefix[ 32 ];
	char		DialSuffix[ 32 ];
	BOOL		UseBothISDNLines;
	BOOL		b56kISDN;
	DWORD		DisconnectTime;
} LOCATIONPARAMS;
    
// connection parameter block
typedef struct CONNECTIONPARAMS
{
	char			szEntryName[ MAX_PATH ];
   	LPITEMIDLIST	pidl;
} CONNECTIONPARAMS;

// following API entry-points are included for all builds
typedef DWORD (WINAPI* RASDIAL)(LPRASDIALEXTENSIONS,LPTSTR,LPRASDIALPARAMS,DWORD,LPVOID,LPHRASCONN);
typedef DWORD (WINAPI* RASHANGUP)(HRASCONN);
typedef DWORD (WINAPI* RASGETERRORSTRING)(UINT, LPSTR, DWORD);
typedef DWORD (WINAPI* RASSETENTRYPROPERTIES)(LPSTR, LPSTR, LPBYTE, DWORD, LPBYTE, DWORD);
typedef DWORD (WINAPI* RASSETENTRYDIALPARAMS)(LPTSTR,LPRASDIALPARAMS,BOOL);
typedef DWORD (WINAPI* RASGETCOUNTRYINFO)(LPRASCTRYINFO, LPDWORD);
typedef DWORD (WINAPI* RASENUMCONNECTIONS)(LPRASCONN,LPDWORD,LPDWORD);
typedef DWORD (WINAPI* RASENUMENTRIES)(LPTSTR,LPTSTR,LPRASENTRYNAME,LPDWORD,LPDWORD);
typedef DWORD (WINAPI* RASENUMDEVICES)(LPRASDEVINFO, LPDWORD, LPDWORD);
typedef DWORD (WINAPI* RASGETENTRYPROPERTIES)(LPSTR, LPSTR, LPRASENTRY, LPDWORD, LPBYTE, LPDWORD);
typedef DWORD (WINAPI* RASVALIDATEENTRYNAME)(LPSTR, LPSTR);
typedef DWORD (WINAPI* RASDELETEENTRY)(LPSTR, LPSTR);

extern RASDIAL					m_lpfnRasDial;
extern RASHANGUP				m_lpfnRasHangUp;
extern RASGETERRORSTRING		m_lpfnRasGetErrorString;
extern RASSETENTRYPROPERTIES	m_lpfnRasSetEntryProperties;
extern RASSETENTRYDIALPARAMS	m_lpfnRasSetEntryDialParams;
extern RASGETCOUNTRYINFO		m_lpfnRasGetCountryInfo;
extern RASENUMCONNECTIONS		m_lpfnRasEnumConnections;
extern RASENUMENTRIES			m_lpfnRasEnumEntries;
extern RASENUMDEVICES			m_lpfnRasEnumDevices;
extern RASGETENTRYPROPERTIES	m_lpfnRasGetEntryProperties;
extern RASVALIDATEENTRYNAME		m_lpfnRasValidateEntryName;
extern RASDELETEENTRY			m_lpfnRasDeleteEntry;

extern HINSTANCE				m_hRasInst;


// for NT40 only
#if ( WINVER >= 0x401 )
typedef DWORD (WINAPI* RASSETAUTODIALADDRESS)(LPSTR, DWORD, LPRASAUTODIALENTRYA, DWORD, DWORD);
typedef DWORD (WINAPI* RASGETAUTODIALADDRESS)(LPSTR, DWORD, LPRASAUTODIALENTRYA, LPDWORD, LPDWORD);
typedef DWORD (WINAPI* RASSETAUTODIALENABLE)(DWORD, BOOL);
typedef DWORD (WINAPI* RASSETAUTODIALPARAM)(DWORD, LPVOID, DWORD);
typedef DWORD (WINAPI* RASENUMAUTODIALADDRESSES)(LPTSTR *,LPDWORD,LPDWORD);
typedef DWORD (WINAPI* RASSETCREDENTIALS)(LPTSTR, LPTSTR, LPRASCREDENTIALS, BOOL);

extern RASSETAUTODIALENABLE		m_lpfnRasSetAutodialEnable;
extern RASSETAUTODIALADDRESS	m_lpfnRasSetAutodialAddress;
extern RASGETAUTODIALADDRESS	m_lpfnRasGetAutodialAddress;
extern RASSETAUTODIALPARAM		m_lpfnRasSetAutodialParam;
extern RASENUMAUTODIALADDRESSES	m_lpfnRasEnumAutodialAddresses;
extern RASSETCREDENTIALS        m_lpfnRasSetCredentials;
#endif

void SizeofRAS95();
void SizeofRASNT40();

BOOL LoadRasFunctionsNT( LPCSTR lpszLibrary );
BOOL GetDialUpConnection95( CONNECTIONPARAMS** connectionNames, int* numNames );
BOOL GetDialUpConnectionNT( CONNECTIONPARAMS** connectionNames, int* numNames );

void EnableDialOnDemandNT(LPSTR lpProfileName, BOOL flag);
void EnableDialOnDemand95(LPSTR lpProfileName, BOOL flag);


BOOL	LoadRasFunctions( LPCSTR lpszLibrary );
void	FreeRasFunctions();

BOOL	GetModemList( char ***, int* );
BOOL	GetModemType( char* strModemName, char* strModemType );

BOOL	IsDialerConnected();
int	 	DialerConfig( char** dialParams );
BOOL	DialerConnect();
void	DialerHangup();
