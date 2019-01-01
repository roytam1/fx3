/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include "nsMetricsService.h"
#include "nsMetricsEventItem.h"
#include "nsIMetricsCollector.h"
#include "nsStringUtils.h"
#include "nsXPCOM.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNetCID.h"
#include "nsIObserverService.h"
#include "nsIUpdateService.h"
#include "nsIUploadChannel.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIObserver.h"
#include "nsILocalFile.h"
#include "nsIPropertyBag.h"
#include "nsIProperty.h"
#include "nsIVariant.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSerializer.h"
#include "nsIVariant.h"
#include "nsICryptoHash.h"
#include "nsISimpleEnumerator.h"
#include "nsIInputStreamChannel.h"
#include "nsIFileStreams.h"
#include "nsIBufferedStreams.h"
#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsMultiplexInputStream.h"
#include "prtime.h"
#include "prmem.h"
#include "prprf.h"
#include "prrng.h"
#include "bzlib.h"
#ifndef MOZILLA_1_8_BRANCH
#include "nsIClassInfoImpl.h"
#endif
#include "nsDocShellCID.h"
#include "nsMemory.h"
#include "nsIBadCertListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIX509Cert.h"
#include "nsAutoPtr.h"

// We need to suppress inclusion of nsString.h
#define nsString_h___
#include "nsIStringStream.h"
#undef nsString_h___

// Make our MIME type inform the server of possible compression.
#ifdef NS_METRICS_SEND_UNCOMPRESSED_DATA
#define NS_METRICS_MIME_TYPE "application/vnd.mozilla.metrics"
#else
#define NS_METRICS_MIME_TYPE "application/vnd.mozilla.metrics.bz2"
#endif

// Flush the event log whenever its size exceeds this number of events.
#define NS_EVENTLOG_FLUSH_POINT 64

#define NS_SECONDS_PER_DAY (60 * 60 * 24)

nsMetricsService* nsMetricsService::sMetricsService = nsnull;
#ifdef PR_LOGGING
PRLogModuleInfo *gMetricsLog;
#endif

static const char kQuitApplicationTopic[] = "quit-application";

//-----------------------------------------------------------------------------

#ifndef NS_METRICS_SEND_UNCOMPRESSED_DATA

// Compress data read from |src|, and write to |outFd|.
static nsresult
CompressBZ2(nsIInputStream *src, PRFileDesc *outFd)
{
  // compress the data chunk-by-chunk

  char inbuf[4096], outbuf[4096];
  bz_stream strm;
  int ret = BZ_OK;

  memset(&strm, 0, sizeof(strm));

  if (BZ2_bzCompressInit(&strm, 9 /*max blocksize*/, 0, 0) != BZ_OK)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = NS_OK;
  int action = BZ_RUN;
  for (;;) {
    PRUint32 bytesRead = 0;
    if (action == BZ_RUN && strm.avail_in == 0) {
      // fill inbuf
      rv = src->Read(inbuf, sizeof(inbuf), &bytesRead);
      if (NS_FAILED(rv))
        break;
      strm.next_in = inbuf;
      strm.avail_in = (int) bytesRead;
    }

    strm.next_out = outbuf;
    strm.avail_out = sizeof(outbuf);

    ret = BZ2_bzCompress(&strm, action);
    if (action == BZ_RUN) {
      if (ret != BZ_RUN_OK) {
        MS_LOG(("BZ2_bzCompress/RUN failed: %d", ret));
        rv = NS_ERROR_UNEXPECTED;
        break;
      }

      if (bytesRead < sizeof(inbuf)) {
        // We're done now, tell libbz2 to finish
        action = BZ_FINISH;
      }
    } else if (ret != BZ_FINISH_OK && ret != BZ_STREAM_END) {
      MS_LOG(("BZ2_bzCompress/FINISH failed: %d", ret));
      rv = NS_ERROR_UNEXPECTED;
      break;
    }

    if (strm.avail_out < sizeof(outbuf)) {
      PRInt32 n = sizeof(outbuf) - strm.avail_out;
      if (PR_Write(outFd, outbuf, n) != n) {
        MS_LOG(("Failed to write compressed file"));
        rv = NS_ERROR_UNEXPECTED;
        break;
      }
    }

    if (ret == BZ_STREAM_END)
      break;
  }

  BZ2_bzCompressEnd(&strm);
  return rv;
}

#endif // !defined(NS_METRICS_SEND_UNCOMPRESSED_DATA)

//-----------------------------------------------------------------------------

class nsMetricsService::BadCertListener : public nsIBadCertListener,
                                          public nsIInterfaceRequestor
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBADCERTLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

  BadCertListener() { }

 private:
  ~BadCertListener() { }
};

// This object has to implement threadsafe addref and release, but this is
// only because the GetInterface call happens on the socket transport thread.
// The actual notifications are proxied to the main thread.
NS_IMPL_THREADSAFE_ISUPPORTS2(nsMetricsService::BadCertListener,
                              nsIBadCertListener, nsIInterfaceRequestor)

NS_IMETHODIMP
nsMetricsService::BadCertListener::ConfirmUnknownIssuer(
    nsIInterfaceRequestor *socketInfo, nsIX509Cert *cert,
    PRInt16 *certAddType, PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::BadCertListener::ConfirmMismatchDomain(
    nsIInterfaceRequestor *socketInfo, const nsACString &targetURL,
    nsIX509Cert *cert, PRBool *result)
{
  *result = PR_FALSE;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_STATE(prefs);

  nsCString certHostOverride;
  prefs->GetCharPref("metrics.upload.cert-host-override",
                     getter_Copies(certHostOverride));

  if (!certHostOverride.IsEmpty()) {
    // Accept the given alternate hostname (CN) for the certificate
    nsString certHost;
    cert->GetCommonName(certHost);
    if (certHostOverride.Equals(NS_ConvertUTF16toUTF8(certHost))) {
      *result = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::BadCertListener::ConfirmCertExpired(
    nsIInterfaceRequestor *socketInfo, nsIX509Cert *cert, PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::BadCertListener::NotifyCrlNextupdate(
    nsIInterfaceRequestor *socketInfo,
    const nsACString &targetURL, nsIX509Cert *cert)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::BadCertListener::GetInterface(const nsIID &uuid,
                                                void **result)
{
  NS_ENSURE_ARG_POINTER(result);

  if (uuid.Equals(NS_GET_IID(nsIBadCertListener))) {
    *result = NS_STATIC_CAST(nsIBadCertListener *, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  *result = nsnull;
  return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------

nsMetricsService::nsMetricsService()  
    : mEventCount(0),
      mSuspendCount(0),
      mUploading(PR_FALSE),
      mNextWindowID(0)
{
  NS_ASSERTION(!sMetricsService, ">1 MetricsService object created");
  sMetricsService = this;
}

/* static */ PLDHashOperator PR_CALLBACK
nsMetricsService::DetachCollector(const nsAString &key,
                                  nsIMetricsCollector *value, void *userData)
{
  value->OnDetach();
  return PL_DHASH_NEXT;
}

nsMetricsService::~nsMetricsService()
{
  NS_ASSERTION(sMetricsService == this, ">1 MetricsService object created");

  mCollectorMap.EnumerateRead(DetachCollector, nsnull);
  sMetricsService = nsnull;
}

NS_IMPL_ISUPPORTS6_CI(nsMetricsService, nsIMetricsService, nsIAboutModule,
                      nsIStreamListener, nsIRequestObserver, nsIObserver,
                      nsITimerCallback)

NS_IMETHODIMP
nsMetricsService::CreateEventItem(const nsAString &itemNamespace,
                                  const nsAString &itemName,
                                  nsIMetricsEventItem **result)
{
  *result = nsnull;

  nsMetricsEventItem *item = new nsMetricsEventItem(itemNamespace, itemName);
  NS_ENSURE_TRUE(item, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*result = item);
  return NS_OK;
}

nsresult
nsMetricsService::BuildEventItem(nsIMetricsEventItem *item,
                                 nsIDOMElement **itemElement)
{
  *itemElement = nsnull;

  nsString itemNS, itemName;
  item->GetItemNamespace(itemNS);
  item->GetItemName(itemName);

  nsCOMPtr<nsIDOMElement> element;
  nsresult rv = mDocument->CreateElementNS(itemNS, itemName,
                                           getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);

  // Attach the given properties as attributes.
  nsCOMPtr<nsIPropertyBag> properties;
  item->GetProperties(getter_AddRefs(properties));
  if (properties) {
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = properties->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> propertySupports;
    while (NS_SUCCEEDED(
               enumerator->GetNext(getter_AddRefs(propertySupports)))) {
      nsCOMPtr<nsIProperty> property = do_QueryInterface(propertySupports);
      if (!property) {
        NS_WARNING("PropertyBag enumerator has non-nsIProperty elements");
        continue;
      }

      nsString name;
      rv = property->GetName(name);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to get property name");
        continue;
      }

      nsCOMPtr<nsIVariant> value;
      rv = property->GetValue(getter_AddRefs(value));
      if (NS_FAILED(rv) || !value) {
        NS_WARNING("Failed to get property value");
        continue;
      }

      // If the type is boolean, we want to use the strings "true" and "false",
      // rather than "1" and "0" which is what nsVariant generates on its own.
      PRUint16 dataType;
      value->GetDataType(&dataType);

      nsString valueString;
      if (dataType == nsIDataType::VTYPE_BOOL) {
        PRBool valueBool;
        rv = value->GetAsBool(&valueBool);
        if (NS_FAILED(rv)) {
          NS_WARNING("Variant has bool type but couldn't get bool value");
          continue;
        }
        valueString = valueBool ? NS_LITERAL_STRING("true")
                      : NS_LITERAL_STRING("false");
      } else {
        rv = value->GetAsDOMString(valueString);
        if (NS_FAILED(rv)) {
          NS_WARNING("Failed to convert property value to string");
          continue;
        }
      }

      rv = element->SetAttribute(name, valueString);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to set attribute value");
      }
      continue;
    }
  }

  // Now recursively build the child event items
  PRInt32 childCount = 0;
  item->GetChildCount(&childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIMetricsEventItem> childItem;
    item->ChildAt(i, getter_AddRefs(childItem));
    NS_ASSERTION(childItem, "The child list cannot contain null items");

    nsCOMPtr<nsIDOMElement> childElement;
    rv = BuildEventItem(childItem, getter_AddRefs(childElement));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> nodeReturn;
    rv = element->AppendChild(childElement, getter_AddRefs(nodeReturn));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  element.swap(*itemElement);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::LogEvent(nsIMetricsEventItem *item)
{
  NS_ENSURE_ARG_POINTER(item);

  if (mSuspendCount != 0)  // Ignore events while suspended
    return NS_OK;

  // Restrict the number of events logged
  if (mEventCount >= mConfig.EventLimit())
    return NS_OK;

  // Restrict the types of events logged
  nsString eventNS, eventName;
  item->GetItemNamespace(eventNS);
  item->GetItemName(eventName);

  if (!mConfig.IsEventEnabled(eventNS, eventName))
    return NS_OK;

  // Create a DOM element for the event and append it to our document.
  nsCOMPtr<nsIDOMElement> eventElement;
  nsresult rv = BuildEventItem(item, getter_AddRefs(eventElement));
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the event timestamp
  nsString timeString;
  AppendInt(timeString, PR_Now() / PR_USEC_PER_SEC);
  rv = eventElement->SetAttribute(NS_LITERAL_STRING("time"), timeString);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the session id
  rv = eventElement->SetAttribute(NS_LITERAL_STRING("session"), mSessionID);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMNode> outChild;
  rv = mRoot->AppendChild(eventElement, getter_AddRefs(outChild));
  NS_ENSURE_SUCCESS(rv, rv);

  // Flush event log to disk if it has grown too large
  if ((++mEventCount % NS_EVENTLOG_FLUSH_POINT) == 0)
    Flush();
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::LogSimpleEvent(const nsAString &eventNS,
                                 const nsAString &eventName,
                                 nsIPropertyBag *eventProperties)
{
  NS_ENSURE_ARG_POINTER(eventProperties);

  nsCOMPtr<nsIMetricsEventItem> item;
  nsresult rv = CreateEventItem(eventNS, eventName, getter_AddRefs(item));
  NS_ENSURE_SUCCESS(rv, rv);

  item->SetProperties(eventProperties);
  return LogEvent(item);
}

NS_IMETHODIMP
nsMetricsService::Flush()
{
  nsresult rv;

  PRFileDesc *fd;
  rv = OpenDataFile(PR_WRONLY | PR_APPEND | PR_CREATE_FILE, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  // Serialize our document, then strip off the root start and end tags,
  // and write it out.

  nsCOMPtr<nsIDOMSerializer> ds =
    do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID);
  NS_ENSURE_TRUE(ds, NS_ERROR_UNEXPECTED);

  nsString docText;
  rv = ds->SerializeToString(mRoot, docText);
  NS_ENSURE_SUCCESS(rv, rv);

  // The first '>' will be the end of the root start tag.
  docText.Cut(0, FindChar(docText, '>') + 1);

  // The last '<' will be the beginning of the root end tag.
  PRInt32 start = RFindChar(docText, '<');
  docText.Cut(start, docText.Length() - start);

  NS_ConvertUTF16toUTF8 utf8Doc(docText);
  PRInt32 num = utf8Doc.Length();
  PRBool succeeded = ( PR_Write(fd, utf8Doc.get(), num) == num );

  PR_Close(fd);
  NS_ENSURE_STATE(succeeded);

  // Write current event count to prefs
  NS_ENSURE_STATE(PersistEventCount());

  // Create a new mRoot
  rv = CreateRoot();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Upload()
{
  if (mUploading)  // Ignore new uploads issued while uploading.
    return NS_OK;

  // XXX Download filtering rules and apply them.

  nsresult rv = Flush();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = UploadData();
  if (NS_SUCCEEDED(rv))
    mUploading = PR_TRUE;

  // Since UploadData is uploading a copy of the data, we can delete the
  // original data file, and allow new events to be logged to a new file.
  nsCOMPtr<nsILocalFile> dataFile;
  GetDataFile(&dataFile);
  if (dataFile) {
    if (NS_FAILED(dataFile->Remove(PR_FALSE)))
      NS_WARNING("failed to remove data file");
  }

  // Reset event count and persist.
  mEventCount = 0;
  NS_ENSURE_STATE(PersistEventCount());

  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Suspend()
{
  mSuspendCount++;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Resume()
{
  if (mSuspendCount > 0)
    mSuspendCount--;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::NewChannel(nsIURI *uri, nsIChannel **result)
{
  nsresult rv = Flush();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> dataFile;
  GetDataFile(&dataFile);
  NS_ENSURE_STATE(dataFile);

  nsCOMPtr<nsIInputStreamChannel> streamChannel =
      do_CreateInstance(NS_INPUTSTREAMCHANNEL_CONTRACTID);
  NS_ENSURE_STATE(streamChannel);

  rv = streamChannel->SetURI(uri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(streamChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool val;
  if (NS_SUCCEEDED(dataFile->Exists(&val)) && val) {
    nsCOMPtr<nsIInputStream> stream;
    OpenCompleteXMLStream(dataFile, getter_AddRefs(stream));
    NS_ENSURE_STATE(stream);

    rv  = streamChannel->SetContentStream(stream);
    rv |= channel->SetContentType(NS_LITERAL_CSTRING("text/xml"));
  } else {
    nsCOMPtr<nsIStringInputStream> errorStream =
        do_CreateInstance("@mozilla.org/io/string-input-stream;1");
    NS_ENSURE_STATE(errorStream);

    rv = errorStream->SetData("no metrics data", -1);
    NS_ENSURE_SUCCESS(rv, rv);

    rv  = streamChannel->SetContentStream(errorStream);
    rv |= channel->SetContentType(NS_LITERAL_CSTRING("text/plain"));
  }

  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  NS_ADDREF(*result = channel);
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::OnStartRequest(nsIRequest *request, nsISupports *context)
{
  NS_ENSURE_STATE(!mConfigOutputStream);

  nsCOMPtr<nsIFile> file;
  GetConfigFile(getter_AddRefs(file));

  nsCOMPtr<nsIFileOutputStream> out =
      do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID);
  NS_ENSURE_STATE(out);

  nsresult rv = out->Init(file, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, -1,
                          0);
  NS_ENSURE_SUCCESS(rv, rv);

  mConfigOutputStream = out;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::OnStopRequest(nsIRequest *request, nsISupports *context,
                                nsresult status)
{
  if (mConfigOutputStream) {
    mConfigOutputStream->Close();
    mConfigOutputStream = 0;
  }

  // Load configuration file:

  nsCOMPtr<nsIFile> file;
  GetConfigFile(getter_AddRefs(file));

  if (NS_SUCCEEDED(status))
    status = mConfig.Load(file);

  if (NS_FAILED(status)) {
    // Upon failure, dial back the upload interval
    PRInt32 interval = mConfig.UploadInterval();
    mConfig.Reset();

    interval <<= 2;
    if (interval > NS_SECONDS_PER_DAY)
      interval = NS_SECONDS_PER_DAY;
    mConfig.SetUploadInterval(interval);

    if (NS_FAILED(file->Remove(PR_FALSE)))
      NS_WARNING("failed to remove config file");
  }

  // Apply possibly new upload interval:
  RegisterUploadTimer();

  EnableCollectors();

  mUploading = PR_FALSE;
  return NS_OK;
}

struct DisabledCollectorsClosure
{
  DisabledCollectorsClosure(const nsTArray<nsString> &enabled)
      : enabledCollectors(enabled) { }

  // Collectors which are enabled in the new config
  const nsTArray<nsString> &enabledCollectors;

  // Collector instances which should no longer be enabled
  nsTArray< nsCOMPtr<nsIMetricsCollector> > disabledCollectors;
};

/* static */ PLDHashOperator PR_CALLBACK
nsMetricsService::PruneDisabledCollectors(const nsAString &key,
                                          nsCOMPtr<nsIMetricsCollector> &value,
                                          void *userData)
{
  DisabledCollectorsClosure *dc =
    NS_STATIC_CAST(DisabledCollectorsClosure *, userData);

  // The frozen string API doesn't expose operator==, so we can't use
  // IndexOf() here.
  for (PRUint32 i = 0; i < dc->enabledCollectors.Length(); ++i) {
    if (dc->enabledCollectors[i].Equals(key)) {
      // The collector is enabled, continue
      return PL_DHASH_NEXT;
    }
  }

  // We didn't find the collector |key| in the list of enabled collectors,
  // so move it from the hash table to the disabledCollectors list.
  MS_LOG(("Disabling collector %s", NS_ConvertUTF16toUTF8(key).get()));
  dc->disabledCollectors.AppendElement(value);
  return PL_DHASH_REMOVE;
}

/* static */ PLDHashOperator PR_CALLBACK
nsMetricsService::NotifyNewLog(const nsAString &key,
                               nsIMetricsCollector *value, void *userData)
{
  value->OnNewLog();
  return PL_DHASH_NEXT;
}

nsresult
nsMetricsService::EnableCollectors()
{
  // Start and stop collectors based on the current config.
  nsTArray<nsString> enabledCollectors;
  mConfig.GetEvents(enabledCollectors);

  // We need to find two sets of collectors:
  //  (1) collectors which are running but not in |collectors|.
  //      We'll call onDetach() on them and let them be released.
  //  (2) collectors which are in |collectors| but not running.
  //      We need to instantiate these collectors.

  DisabledCollectorsClosure dc(enabledCollectors);
  mCollectorMap.Enumerate(PruneDisabledCollectors, &dc);

  // Notify this set of collectors that they're going away, and release them.
  PRUint32 i;
  for (i = 0; i < dc.disabledCollectors.Length(); ++i) {
    dc.disabledCollectors[i]->OnDetach();
  }
  dc.disabledCollectors.Clear();

  // Now instantiate any newly-enabled collectors.
  for (i = 0; i < enabledCollectors.Length(); ++i) {
    const nsString &name = enabledCollectors[i];
    if (!mCollectorMap.GetWeak(name)) {
      nsCString contractID("@mozilla.org/metrics/collector;1?name=");
      contractID.Append(NS_ConvertUTF16toUTF8(name));

      nsCOMPtr<nsIMetricsCollector> coll = do_GetService(contractID.get());
      if (coll) {
        MS_LOG(("Created collector %s", contractID.get()));
        mCollectorMap.Put(name, coll);
        coll->OnAttach();
      } else {
        MS_LOG(("Couldn't instantiate collector %s", contractID.get()));
      }
    }
  }

  // Finally, notify all collectors that we've restarted the log.
  mCollectorMap.EnumerateRead(NotifyNewLog, nsnull);

  return NS_OK;
}

// Copied from nsStreamUtils.cpp:
static NS_METHOD
CopySegmentToStream(nsIInputStream *inStr,
                    void *closure,
                    const char *buffer,
                    PRUint32 offset,
                    PRUint32 count,
                    PRUint32 *countWritten)
{
  nsIOutputStream *outStr = NS_STATIC_CAST(nsIOutputStream *, closure);
  *countWritten = 0;
  while (count) {
    PRUint32 n;
    nsresult rv = outStr->Write(buffer, count, &n);
    if (NS_FAILED(rv))
      return rv;
    buffer += n;
    count -= n;
    *countWritten += n;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::OnDataAvailable(nsIRequest *request, nsISupports *context,
                                  nsIInputStream *stream, PRUint32 offset,
                                  PRUint32 count)
{
  PRUint32 n;
  return stream->ReadSegments(CopySegmentToStream, mConfigOutputStream,
                              count, &n);
}

NS_IMETHODIMP
nsMetricsService::Observe(nsISupports *subject, const char *topic,
                          const PRUnichar *data)
{
  if (strcmp(topic, kQuitApplicationTopic) == 0) {
    Flush();

    // We don't detach the collectors here, to allow them to log events
    // as we're shutting down.  The collectors will be detached and released
    // when the MetricsService goes away.
  } else if (strcmp(topic, "profile-after-change") == 0) {
    nsresult rv = ProfileStartup();
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (strcmp(topic, NS_WEBNAVIGATION_DESTROY) == 0 ||
             strcmp(topic, NS_CHROME_WEBNAVIGATION_DESTROY) == 0) {

    // Dispatch our notification before removing the window from the map.
    nsCOMPtr<nsIObserverService> obsSvc =
      do_GetService("@mozilla.org/observer-service;1");
    NS_ENSURE_STATE(obsSvc);

    const char *newTopic;
    if (strcmp(topic, NS_WEBNAVIGATION_DESTROY) == 0) {
      newTopic = NS_METRICS_WEBNAVIGATION_DESTROY;
    } else {
      newTopic = NS_METRICS_CHROME_WEBNAVIGATION_DESTROY;
    }

    obsSvc->NotifyObservers(subject, newTopic, data);

    // Remove the window from our map.
    mWindowMap.Remove(subject);
  }
  
  return NS_OK;
}

nsresult
nsMetricsService::ProfileStartup()
{
  // Initialize configuration by reading our old config file if one exists.
  NS_ENSURE_STATE(mConfig.Init());
  nsCOMPtr<nsIFile> file;
  GetConfigFile(getter_AddRefs(file));

  PRBool loaded = PR_FALSE;
  if (file) {
    PRBool exists;
    if (NS_SUCCEEDED(file->Exists(&exists)) && exists) {
      loaded = NS_SUCCEEDED(mConfig.Load(file));
    }
  }
  
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_STATE(prefs);
  nsresult rv = prefs->GetIntPref("metrics.event-count", &mEventCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update the session id pref for the new session
  static const char kSessionIDPref[] = "metrics.last-session-id";
  PRInt32 sessionID = -1;
  prefs->GetIntPref(kSessionIDPref, &sessionID);
  mSessionID.Cut(0, PR_UINT32_MAX);
  AppendInt(mSessionID, ++sessionID);
  rv = prefs->SetIntPref(kSessionIDPref, sessionID);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mCryptoHash = do_CreateInstance("@mozilla.org/security/hash;1");
  NS_ENSURE_TRUE(mCryptoHash, NS_ERROR_FAILURE);

  // Set up our hashtables
  NS_ENSURE_TRUE(mWindowMap.Init(32), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mCollectorMap.Init(16), NS_ERROR_OUT_OF_MEMORY);

  // Create an XML document to serve as the owner document for elements.
  mDocument = do_CreateInstance("@mozilla.org/xml/xml-document;1");
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  // Create a root log element.
  rv = CreateRoot();
  NS_ENSURE_SUCCESS(rv, rv);

  // Start up the collectors
  rv = EnableCollectors();
  NS_ENSURE_SUCCESS(rv, rv);

  RegisterUploadTimer();

  // If we didn't load a config, immediately upload our empty log.
  // This will allow us to receive a config file from the server.
  // If we fail to get a config, we'll try again later, see OnStopRequest().
  if (!loaded) {
    Upload();
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Notify(nsITimer *timer)
{
  // OK, we are ready to upload!
  Upload();
  return NS_OK;
}

/*static*/ nsMetricsService *
nsMetricsService::get()
{
  if (!sMetricsService) {
    nsCOMPtr<nsIMetricsService> ms =
      do_GetService(NS_METRICSSERVICE_CONTRACTID);
    if (!sMetricsService)
      NS_WARNING("failed to initialize metrics service");
  }
  return sMetricsService;
}

/*static*/ NS_METHOD
nsMetricsService::Create(nsISupports *outer, const nsIID &iid, void **result)
{
  NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);

  nsRefPtr<nsMetricsService> ms;
  if (!sMetricsService) {
    ms = new nsMetricsService();
    if (!ms)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ASSERTION(sMetricsService, "should be non-null");

    nsresult rv = ms->Init();
    if (NS_FAILED(rv))
      return rv;
  }
  return sMetricsService->QueryInterface(iid, result);
}

nsresult
nsMetricsService::Init()
{
  // We defer most of our initialization until the profile-after-change
  // notification, because profile prefs aren't available until then.
  // Register for notifications here though, since the observer service
  // is set up.
  
#ifdef PR_LOGGING
  gMetricsLog = PR_NewLogModule("nsMetricsService");
#endif

  MS_LOG(("nsMetricsService::Init"));

  nsresult rv;

  nsCOMPtr<nsIObserverService> obsSvc =
      do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // The rest of startup will happen on profile-after-change
  rv = obsSvc->AddObserver(this, "profile-after-change", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Listen for quit-application so we can properly flush our data to disk
  rv = obsSvc->AddObserver(this, kQuitApplicationTopic, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Listen for window destruction so that we can remove the windows
  // from our window id map.
  rv = obsSvc->AddObserver(this, NS_WEBNAVIGATION_DESTROY, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, NS_CHROME_WEBNAVIGATION_DESTROY, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsMetricsService::CreateRoot()
{
  nsresult rv;
  nsCOMPtr<nsIDOMElement> root;
  rv = mDocument->CreateElementNS(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                                  NS_LITERAL_STRING("log"),
                                  getter_AddRefs(root));
  NS_ENSURE_SUCCESS(rv, rv);

  mRoot = root;
  return NS_OK;
}

nsresult
nsMetricsService::GetDataFile(nsCOMPtr<nsILocalFile> *result)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING("metrics.xml"));
  NS_ENSURE_SUCCESS(rv, rv);

  *result = do_QueryInterface(file, &rv);
  return rv;
}

nsresult
nsMetricsService::OpenDataFile(PRUint32 flags, PRFileDesc **fd)
{
  nsCOMPtr<nsILocalFile> dataFile;
  nsresult rv = GetDataFile(&dataFile);
  NS_ENSURE_SUCCESS(rv, rv);

  return dataFile->OpenNSPRFileDesc(flags, 0600, fd);
}

nsresult
nsMetricsService::UploadData()
{
  // TODO: Prepare a data stream for upload that is prefixed with a PROFILE
  //       event.
 
  PRBool enable = PR_FALSE;
  nsCString spec;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->GetBoolPref("metrics.upload.enable", &enable);
    prefs->GetCharPref("metrics.upload.uri", getter_Copies(spec));
  }
  if (!enable || spec.IsEmpty())
    return NS_ERROR_ABORT;

  nsCOMPtr<nsILocalFile> file;
  nsresult rv = GetDataFileForUpload(&file);
  NS_ENSURE_SUCCESS(rv, rv);

  // NOTE: nsIUploadChannel requires a buffered stream to upload...

  nsCOMPtr<nsIFileInputStream> fileStream =
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID);
  NS_ENSURE_STATE(fileStream);

  rv = fileStream->Init(file, -1, -1, nsIFileInputStream::DELETE_ON_CLOSE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 streamLen;
  rv = fileStream->Available(&streamLen);
  NS_ENSURE_SUCCESS(rv, rv);

  if (streamLen == 0)
    return NS_ERROR_ABORT;

  nsCOMPtr<nsIBufferedInputStream> uploadStream =
      do_CreateInstance(NS_BUFFEREDINPUTSTREAM_CONTRACTID);
  NS_ENSURE_STATE(uploadStream);

  rv = uploadStream->Init(fileStream, 4096);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID);
  NS_ENSURE_STATE(ios);

  nsCOMPtr<nsIChannel> channel;
  ios->NewChannel(spec, nsnull, nsnull, getter_AddRefs(channel));
  NS_ENSURE_STATE(channel); 

  nsCOMPtr<nsIInterfaceRequestor> certListener = new BadCertListener();
  NS_ENSURE_TRUE(certListener, NS_ERROR_OUT_OF_MEMORY);

  channel->SetNotificationCallbacks(certListener);

  nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(channel);
  NS_ENSURE_STATE(uploadChannel); 

  NS_NAMED_LITERAL_CSTRING(binaryType, NS_METRICS_MIME_TYPE);
  rv = uploadChannel->SetUploadStream(uploadStream, binaryType, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  NS_ENSURE_STATE(httpChannel);
  rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->AsyncOpen(this, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsMetricsService::GetDataFileForUpload(nsCOMPtr<nsILocalFile> *result)
{
  nsCOMPtr<nsILocalFile> input;
  nsresult rv = GetDataFile(&input);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> src;
  rv = OpenCompleteXMLStream(input, getter_AddRefs(src));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> temp;
  rv = input->Clone(getter_AddRefs(temp));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString leafName;
  rv = temp->GetNativeLeafName(leafName);
  NS_ENSURE_SUCCESS(rv, rv);

  leafName.Append(".bz2");
  rv = temp->SetNativeLeafName(leafName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> ltemp = do_QueryInterface(temp, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRFileDesc *destFd = NULL;
  rv = ltemp->OpenNSPRFileDesc(PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE, 0600,
                               &destFd);

  // Copy file using bzip2 compression:

  if (NS_SUCCEEDED(rv)) {
#ifdef NS_METRICS_SEND_UNCOMPRESSED_DATA
    char buf[4096];
    PRUint32 n;

    while (NS_SUCCEEDED(rv = src->Read(buf, sizeof(buf), &n)) && n) {
      if (PR_Write(destFd, buf, n) != n) {
        NS_WARNING("failed to write data");
        rv = NS_ERROR_UNEXPECTED;
        break;
      }
    }
#else
    rv = CompressBZ2(src, destFd);
#endif
  }

  if (destFd)
    PR_Close(destFd);

  if (NS_SUCCEEDED(rv)) {
    *result = nsnull;
    ltemp.swap(*result);
  }

  return rv;
}

nsresult
nsMetricsService::OpenCompleteXMLStream(nsILocalFile *dataFile,
                                       nsIInputStream **result)
{
  // Construct a full XML document using the header, file contents, and
  // footer.  We need to generate a client id now if one doesn't exist.
  static const char kClientIDPref[] = "metrics.client-id";

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_STATE(prefs);
  
  nsCString clientID;
  nsresult rv = prefs->GetCharPref(kClientIDPref, getter_Copies(clientID));
  if (NS_FAILED(rv) || clientID.IsEmpty()) {
    rv = GenerateClientID(clientID);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = prefs->SetCharPref(kClientIDPref, clientID.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  static const char METRICS_XML_HEAD[] =
      "<?xml version=\"1.0\"?>\n"
      "<log xmlns=\"" NS_METRICS_NAMESPACE "\" clientid=\"%s\">\n";
  static const char METRICS_XML_TAIL[] = "</log>";

  nsCOMPtr<nsIFileInputStream> fileStream =
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID);
  NS_ENSURE_STATE(fileStream);

  rv = fileStream->Init(dataFile, -1, -1, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMultiplexInputStream> miStream =
    do_CreateInstance(NS_MULTIPLEXINPUTSTREAM_CONTRACTID);
  NS_ENSURE_STATE(miStream);

  nsCOMPtr<nsIStringInputStream> stringStream =
      do_CreateInstance("@mozilla.org/io/string-input-stream;1");
  NS_ENSURE_STATE(stringStream);

  char *head = PR_smprintf(METRICS_XML_HEAD, clientID.get());
  rv = stringStream->SetData(head, -1);
  PR_smprintf_free(head);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = miStream->AppendStream(stringStream);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = miStream->AppendStream(fileStream);
  NS_ENSURE_SUCCESS(rv, rv);

  stringStream = do_CreateInstance("@mozilla.org/io/string-input-stream;1");
  NS_ENSURE_STATE(stringStream);

  rv = stringStream->SetData(METRICS_XML_TAIL, sizeof(METRICS_XML_TAIL)-1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = miStream->AppendStream(stringStream);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*result = miStream);
  return NS_OK;
}

void
nsMetricsService::RegisterUploadTimer()
{
  nsCOMPtr<nsIUpdateTimerManager> mgr =
      do_GetService("@mozilla.org/updates/timer-manager;1");
  if (mgr)
    mgr->RegisterTimer(NS_LITERAL_STRING("metrics-upload"), this,
                       mConfig.UploadInterval() * PR_MSEC_PER_SEC);
}

void
nsMetricsService::GetConfigFile(nsIFile **result)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  if (file)
    file->AppendNative(NS_LITERAL_CSTRING("metrics-config.xml"));

  *result = nsnull;
  file.swap(*result);
}

nsresult
nsMetricsService::GenerateClientID(nsCString &clientID)
{
  // Feed some data into the hasher...

  struct {
    PRTime  a;
    PRUint8 b[32];
  } input;

  input.a = PR_Now();
  PR_GetRandomNoise(input.b, sizeof(input.b));

  return HashBytes(
      NS_REINTERPRET_CAST(const PRUint8 *, &input), sizeof(input), clientID);
}

nsresult
nsMetricsService::HashBytes(const PRUint8 *bytes, PRUint32 length,
                            nsACString &result)
{
  nsresult rv = mCryptoHash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mCryptoHash->Update(bytes, length);
  NS_ENSURE_SUCCESS(rv, rv);

  return mCryptoHash->Finish(PR_TRUE, result);
}

PRBool
nsMetricsService::PersistEventCount()
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, PR_FALSE);

  return NS_SUCCEEDED(prefs->SetIntPref("metrics.event-count", mEventCount));
}

/* static */ PRUint32
nsMetricsService::GetWindowID(nsIDOMWindow *window)
{
  if (!sMetricsService) {
    NS_NOTREACHED("metrics service not created");
    return PR_UINT32_MAX;
  }

  return sMetricsService->GetWindowIDInternal(window);
}

NS_IMETHODIMP
nsMetricsService::GetWindowID(nsIDOMWindow *window, PRUint32 *id)
{
  *id = GetWindowIDInternal(window);
  return NS_OK;
}

PRUint32
nsMetricsService::GetWindowIDInternal(nsIDOMWindow *window)
{
  PRUint32 id;
  if (!mWindowMap.Get(window, &id)) {
    id = mNextWindowID++;
    mWindowMap.Put(window, id);
  }

  return id;
}

nsresult
nsMetricsService::Hash(const nsAString &str, nsCString &hashed)
{
  if (str.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ConvertUTF16toUTF8 utf8(str);
  return HashBytes(
      NS_REINTERPRET_CAST(const PRUint8 *, utf8.get()), utf8.Length(), hashed);
}

/* static */ nsresult
nsMetricsUtils::NewPropertyBag(nsIWritablePropertyBag2 **result)
{
  return CallCreateInstance("@mozilla.org/hash-property-bag;1", result);
}

/* static */ nsresult
nsMetricsUtils::AddChildItem(nsIMetricsEventItem *parent,
                             const nsAString &childName,
                             nsIPropertyBag *childProperties)
{
  nsCOMPtr<nsIMetricsEventItem> item;
  nsMetricsService::get()->CreateEventItem(childName, getter_AddRefs(item));
  NS_ENSURE_STATE(item);

  item->SetProperties(childProperties);

  nsresult rv = parent->AppendChild(item);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
