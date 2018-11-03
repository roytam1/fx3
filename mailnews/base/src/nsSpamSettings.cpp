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
 * Portions created by the Initial Developer are Copyright (C) 2001-2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Dan Mosedale <dmose@netscape.com>
 *   David Bienvenu <bienvenu@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSpamSettings.h"
#include "nsISupportsObsolete.h"
#include "nsILocalFile.h"
#include "plstr.h"
#include "prmem.h"
#include "nsIMsgHdr.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIMsgFolder.h"
#include "nsMsgUtils.h"
#include "nsMsgFolderFlags.h"
#include "nsImapCore.h"
#include "nsIImapIncomingServer.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"

nsSpamSettings::nsSpamSettings()
{
  mLevel = 0;
  mMoveOnSpam = PR_FALSE;
  mMarkAsReadOnSpam = PR_FALSE;
  mMoveTargetMode = nsISpamSettings::MOVE_TARGET_MODE_ACCOUNT;
  mPurge = PR_FALSE;
  mPurgeInterval = 14; // 14 days

  mServerFilterTrustFlags = 0;

  mUseWhiteList = PR_FALSE;
  mLoggingEnabled = PR_FALSE;
  mManualMark = PR_FALSE;
  mUseServerFilter = PR_FALSE;
  mManualMarkMode = nsISpamSettings::MANUAL_MARK_MODE_MOVE;
}

nsSpamSettings::~nsSpamSettings()
{
}

NS_IMPL_ISUPPORTS2(nsSpamSettings, nsISpamSettings, nsIUrlListener)

NS_IMETHODIMP 
nsSpamSettings::GetLevel(PRInt32 *aLevel)
{
  NS_ENSURE_ARG_POINTER(aLevel);
  *aLevel = mLevel;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetLevel(PRInt32 aLevel)
{
  NS_ASSERTION((aLevel >= 0 && aLevel <= 100), "bad level");
  mLevel = aLevel;
  return NS_OK;
}

NS_IMETHODIMP 
nsSpamSettings::GetMoveTargetMode(PRInt32 *aMoveTargetMode)
{
  NS_ENSURE_ARG_POINTER(aMoveTargetMode);
  *aMoveTargetMode = mMoveTargetMode;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetMoveTargetMode(PRInt32 aMoveTargetMode)
{
  NS_ASSERTION((aMoveTargetMode == nsISpamSettings::MOVE_TARGET_MODE_FOLDER || aMoveTargetMode == nsISpamSettings::MOVE_TARGET_MODE_ACCOUNT), "bad move target mode");
  mMoveTargetMode = aMoveTargetMode;
  return NS_OK;
}

NS_IMETHODIMP 
nsSpamSettings::GetManualMarkMode(PRInt32 *aManualMarkMode)
{
  NS_ENSURE_ARG_POINTER(aManualMarkMode);
  *aManualMarkMode = mManualMarkMode;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetManualMarkMode(PRInt32 aManualMarkMode)
{ 
  NS_ASSERTION((aManualMarkMode == nsISpamSettings::MANUAL_MARK_MODE_MOVE || aManualMarkMode == nsISpamSettings::MANUAL_MARK_MODE_DELETE), "bad manual mark mode");
  mManualMarkMode = aManualMarkMode;
  return NS_OK;
}

NS_IMPL_GETSET(nsSpamSettings, LoggingEnabled, PRBool, mLoggingEnabled)
NS_IMPL_GETSET(nsSpamSettings, MoveOnSpam, PRBool, mMoveOnSpam)
NS_IMPL_GETSET(nsSpamSettings, MarkAsReadOnSpam, PRBool, mMarkAsReadOnSpam)
NS_IMPL_GETSET(nsSpamSettings, Purge, PRBool, mPurge)
NS_IMPL_GETSET(nsSpamSettings, UseWhiteList, PRBool, mUseWhiteList)
NS_IMPL_GETSET(nsSpamSettings, UseServerFilter, PRBool, mUseServerFilter)
NS_IMPL_GETSET(nsSpamSettings, ManualMark, PRBool, mManualMark)

NS_IMETHODIMP nsSpamSettings::GetWhiteListAbURI(char * *aWhiteListAbURI)
{
  NS_ENSURE_ARG_POINTER(aWhiteListAbURI);
  *aWhiteListAbURI = ToNewCString(mWhiteListAbURI);
  return NS_OK;
}
NS_IMETHODIMP nsSpamSettings::SetWhiteListAbURI(const char * aWhiteListAbURI)
{
  mWhiteListAbURI = aWhiteListAbURI;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetActionTargetAccount(char * *aActionTargetAccount)
{
  NS_ENSURE_ARG_POINTER(aActionTargetAccount);
  *aActionTargetAccount = ToNewCString(mActionTargetAccount);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetActionTargetAccount(const char * aActionTargetAccount)
{
  mActionTargetAccount = aActionTargetAccount;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetActionTargetFolder(char * *aActionTargetFolder)
{
  NS_ENSURE_ARG_POINTER(aActionTargetFolder);
  *aActionTargetFolder = ToNewCString(mActionTargetFolder);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetActionTargetFolder(const char * aActionTargetFolder)
{
  mActionTargetFolder = aActionTargetFolder;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetPurgeInterval(PRInt32 *aPurgeInterval)
{
  NS_ENSURE_ARG_POINTER(aPurgeInterval);
  *aPurgeInterval = mPurgeInterval;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetPurgeInterval(PRInt32 aPurgeInterval)
{
  NS_ASSERTION(aPurgeInterval >= 0, "bad purge interval");
  mPurgeInterval = aPurgeInterval;
  return NS_OK;
}

NS_IMETHODIMP
nsSpamSettings::SetLogStream(nsIOutputStream *aLogStream)
{
  // if there is a log stream already, close it
  if (mLogStream) {
    // will flush
    nsresult rv = mLogStream->Close();
    NS_ENSURE_SUCCESS(rv,rv);
  }

  mLogStream = aLogStream;
  return NS_OK;
}

NS_IMETHODIMP
nsSpamSettings::GetLogStream(nsIOutputStream **aLogStream)
{
  NS_ENSURE_ARG_POINTER(aLogStream);

  nsresult rv;

  if (!mLogStream) {
    nsCOMPtr <nsIFileSpec> file;
    rv = GetLogFileSpec(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString nativePath;
    rv = file->GetNativePath(getter_Copies(nativePath));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr <nsILocalFile> logFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = logFile->InitWithNativePath(nsDependentCString(nativePath));
    NS_ENSURE_SUCCESS(rv,rv);

    // append to the end of the log file
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(mLogStream),
                                   logFile,
                                   PR_CREATE_FILE | PR_WRONLY | PR_APPEND,
                                   0600);
    NS_ENSURE_SUCCESS(rv,rv);

    if (!mLogStream)
      return NS_ERROR_FAILURE;
  }
 
  NS_ADDREF(*aLogStream = mLogStream);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetServer(nsIMsgIncomingServer **aServer)
{
  NS_ENSURE_ARG_POINTER(aServer);
  NS_IF_ADDREF(*aServer = mServer);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetServer(nsIMsgIncomingServer *aServer)
{
  mServer = aServer;
  return NS_OK;
}

nsresult
nsSpamSettings::GetLogFileSpec(nsIFileSpec **aFileSpec)
{
  NS_ENSURE_ARG_POINTER(aFileSpec);

  nsCOMPtr <nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  NS_ASSERTION(server, "are you trying to use a cloned spam settings?");
  if (!server)
    return NS_ERROR_FAILURE;

  rv = server->GetLocalPath(aFileSpec);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = (*aFileSpec)->AppendRelativeUnixPath("junklog.html");
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetLogURL(char * *aLogURL)
{
  NS_ENSURE_ARG_POINTER(aLogURL);

  nsCOMPtr <nsIFileSpec> file;
  nsresult rv = GetLogFileSpec(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = file->GetURLString(aLogURL);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult nsSpamSettings::TruncateLog()
{
  // this will flush and close the steam
  nsresult rv = SetLogStream(nsnull);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIFileSpec> file;
  rv = GetLogFileSpec(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = file->Truncate(0);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsSpamSettings::ClearLog()
{
  PRBool loggingEnabled = mLoggingEnabled;
  
  // disable logging while clearing
  mLoggingEnabled = PR_FALSE;

  nsresult rv;
  rv = TruncateLog();
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to truncate filter log");

  mLoggingEnabled = loggingEnabled;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::EnsureLogFile()
{
  nsCOMPtr <nsIFileSpec> file;
  nsresult rv = GetLogFileSpec(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv,rv);

  PRBool exists;
  rv = file->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists) {
    rv = file->Touch();
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::FlushLogIfNecessary()
{
  // only flush the log if we are logging
  PRBool loggingEnabled = PR_FALSE;
  nsresult rv = GetLoggingEnabled(&loggingEnabled);
  NS_ENSURE_SUCCESS(rv,rv);

  if (loggingEnabled) 
  {
    nsCOMPtr <nsIOutputStream> logStream;
    rv = GetLogStream(getter_AddRefs(logStream));    
    if (NS_SUCCEEDED(rv) && logStream) {
      rv = logStream->Flush();
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  return rv;
}

NS_IMETHODIMP nsSpamSettings::Clone(nsISpamSettings *aSpamSettings)
{
  NS_ENSURE_ARG_POINTER(aSpamSettings);

  nsresult rv = aSpamSettings->GetUseWhiteList(&mUseWhiteList); 
  NS_ENSURE_SUCCESS(rv,rv);

  (void)aSpamSettings->GetMoveOnSpam(&mMoveOnSpam);
  (void)aSpamSettings->GetMarkAsReadOnSpam(&mMarkAsReadOnSpam);
  (void)aSpamSettings->GetManualMark(&mManualMark); 
  (void)aSpamSettings->GetManualMarkMode(&mManualMarkMode); 
  (void)aSpamSettings->GetPurge(&mPurge); 
  (void)aSpamSettings->GetUseServerFilter(&mUseServerFilter);

  rv = aSpamSettings->GetPurgeInterval(&mPurgeInterval); 
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aSpamSettings->GetLevel(&mLevel); 
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aSpamSettings->GetMoveTargetMode(&mMoveTargetMode); 
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString actionTargetAccount;
  rv = aSpamSettings->GetActionTargetAccount(getter_Copies(actionTargetAccount)); 
  NS_ENSURE_SUCCESS(rv,rv);
  mActionTargetAccount = actionTargetAccount;

  nsXPIDLCString actionTargetFolder;
  rv = aSpamSettings->GetActionTargetFolder(getter_Copies(actionTargetFolder)); 
  NS_ENSURE_SUCCESS(rv,rv);
  mActionTargetFolder = actionTargetFolder;

  nsXPIDLCString whiteListAbURI;
  rv = aSpamSettings->GetWhiteListAbURI(getter_Copies(whiteListAbURI)); 
  NS_ENSURE_SUCCESS(rv,rv);
  mWhiteListAbURI = whiteListAbURI;
  
  rv = aSpamSettings->GetLoggingEnabled(&mLoggingEnabled);

  aSpamSettings->GetServerFilterName(mServerFilterName);
  aSpamSettings->GetServerFilterTrustFlags(&mServerFilterTrustFlags);

  return rv;
}

NS_IMETHODIMP nsSpamSettings::GetSpamFolderURI(char **aSpamFolderURI)
{
  NS_ENSURE_ARG_POINTER(aSpamFolderURI);

  if (mMoveTargetMode == nsISpamSettings::MOVE_TARGET_MODE_FOLDER)
    return GetActionTargetFolder(aSpamFolderURI);

  // if the mode is nsISpamSettings::MOVE_TARGET_MODE_ACCOUNT
  // the spam folder URI = account uri + "/Junk"
  nsXPIDLCString folderURI;
  nsresult rv = GetActionTargetAccount(getter_Copies(folderURI));
  NS_ENSURE_SUCCESS(rv,rv);

  // we might be trying to get the old spam folder uri
  // in order to clear the flag
  // if we didn't have one, bail out.
  if (folderURI.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIRDFResource> folderResource;
  rv = rdf->GetResource(folderURI, getter_AddRefs(folderResource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIMsgFolder> folder = do_QueryInterface(folderResource);
  if (!folder)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  // see nsMsgFolder::SetPrettyName() for where the pretty name is set.
  folderURI.Append("/Junk");
  
  // XXX todo
  // better not to make base depend in imap
  // but doing it here, like in nsMsgCopy.cpp
  // one day, we'll fix this (and nsMsgCopy.cpp) to use GetMsgFolderFromURI()
  nsCOMPtr<nsIImapIncomingServer> imapServer = do_QueryInterface(server);
  if (imapServer) {
    // Make sure an specific IMAP folder has correct personal namespace
    // see bug #197043
    nsXPIDLCString folderUriWithNamespace;
    (void)imapServer->GetUriWithNamespacePrefixIfNecessary(kPersonalNamespace, folderURI.get(), getter_Copies(folderUriWithNamespace));
    if (!folderUriWithNamespace.IsEmpty())
      folderURI = folderUriWithNamespace;
  }

  *aSpamFolderURI = ToNewCString(folderURI);
  if (!*aSpamFolderURI)
    return NS_ERROR_OUT_OF_MEMORY;
  else 
    return rv;
}

NS_IMETHODIMP nsSpamSettings::GetServerFilterName(nsACString &aFilterName)
{
  aFilterName = mServerFilterName;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetServerFilterName(const nsACString &aFilterName)
{
  mServerFilterName = aFilterName;
  return NS_OK;
}

NS_IMPL_GETSET(nsSpamSettings, ServerFilterTrustFlags, PRBool, mServerFilterTrustFlags)

#define LOG_ENTRY_START_TAG "<p>\n"
#define LOG_ENTRY_START_TAG_LEN (strlen(LOG_ENTRY_START_TAG))
#define LOG_ENTRY_END_TAG "</p>\n"
#define LOG_ENTRY_END_TAG_LEN (strlen(LOG_ENTRY_END_TAG))

NS_IMETHODIMP nsSpamSettings::LogJunkHit(nsIMsgDBHdr *aMsgHdr, PRBool aMoveMessage)
{
  PRBool loggingEnabled;
  nsresult rv = GetLoggingEnabled(&loggingEnabled);
  NS_ENSURE_SUCCESS(rv,rv);

  if (!loggingEnabled)
    return NS_OK;

  PRTime date;
  char dateStr[40];	/* 30 probably not enough */
  
  nsXPIDLCString author;
  nsXPIDLCString subject;
  
  rv = aMsgHdr->GetDate(&date);
  PRExplodedTime exploded;
  PR_ExplodeTime(date, PR_LocalTimeParameters, &exploded);
  // XXX Temporary until logging is fully localized
  PR_FormatTimeUSEnglish(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &exploded);
  
  aMsgHdr->GetAuthor(getter_Copies(author));
  aMsgHdr->GetSubject(getter_Copies(subject));
  
  nsCString buffer;
  // this is big enough to hold a log entry.  
  // do this so we avoid growing and copying as we append to the log.
  buffer.SetCapacity(512);  
  
  buffer = "Detected junk message from ";
  buffer +=  (const char*)author;
  buffer +=  " - ";
  buffer +=  (const char *)subject;
  buffer +=  " at ";
  buffer +=  dateStr;
  buffer +=  "\n";
  
  if (aMoveMessage) {
    nsXPIDLCString msgId;
    aMsgHdr->GetMessageId(getter_Copies(msgId));
    
    nsXPIDLCString junkFolderURI;
    rv = GetSpamFolderURI(getter_Copies(junkFolderURI));
    NS_ENSURE_SUCCESS(rv,rv);

    buffer += "Move message id = ";
    buffer += msgId.get();
    buffer += " to ";
    buffer += junkFolderURI.get();
    buffer += "\n";
  }

  return LogJunkString(buffer.get());
}

NS_IMETHODIMP nsSpamSettings::LogJunkString(const char *string)
{
  PRBool loggingEnabled;
  nsresult rv = GetLoggingEnabled(&loggingEnabled);
  NS_ENSURE_SUCCESS(rv,rv);

  if (!loggingEnabled)
    return NS_OK;

  nsCOMPtr <nsIOutputStream> logStream;
  rv = GetLogStream(getter_AddRefs(logStream));
  NS_ENSURE_SUCCESS(rv,rv);
  
  PRUint32 writeCount;
  
  rv = logStream->Write(LOG_ENTRY_START_TAG, LOG_ENTRY_START_TAG_LEN, &writeCount);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ASSERTION(writeCount == LOG_ENTRY_START_TAG_LEN, "failed to write out start log tag");
  
  // html escape the log for security reasons.
  // we don't want some to send us a message with a subject with
  // html tags, especially <script>
  char *escapedBuffer = nsEscapeHTML(string);
  if (!escapedBuffer)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRUint32 escapedBufferLen = strlen(escapedBuffer);
  rv = logStream->Write(escapedBuffer, escapedBufferLen, &writeCount);
  PR_Free(escapedBuffer);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ASSERTION(writeCount == escapedBufferLen, "failed to write out log hit");
  
  rv = logStream->Write(LOG_ENTRY_END_TAG, LOG_ENTRY_END_TAG_LEN, &writeCount);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ASSERTION(writeCount == LOG_ENTRY_END_TAG_LEN, "failed to write out end log tag");
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::OnStartRunningUrl(nsIURI* aURL)
{
  // do nothing
  // all the action happens in OnStopRunningUrl()
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::OnStopRunningUrl(nsIURI* aURL, nsresult exitCode)
{
  nsXPIDLCString junkFolderURI;
  nsresult rv = GetSpamFolderURI(getter_Copies(junkFolderURI));
  NS_ENSURE_SUCCESS(rv,rv);

  if (junkFolderURI.IsEmpty())
    return NS_ERROR_UNEXPECTED;

  // when we get here, the folder should exist.
  nsCOMPtr <nsIMsgFolder> junkFolder;
  rv = GetExistingFolder(junkFolderURI.get(), getter_AddRefs(junkFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!junkFolder)
    return NS_ERROR_UNEXPECTED;

  rv = junkFolder->SetFlag(MSG_FOLDER_FLAG_JUNK);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}
