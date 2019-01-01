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
 * The Initial Developer of the Original Code is Robert Sayre.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
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

#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsSAXAttributes.h"
#include "nsSAXXMLReader.h"

#define XMLNS_URI "http://www.w3.org/2000/xmlns/"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

NS_IMPL_ISUPPORTS4(nsSAXXMLReader, nsISAXXMLReader,
                   nsIExpatSink, nsIExtendedExpatSink,
                   nsIContentSink)

nsSAXXMLReader::nsSAXXMLReader() : mAsync(PR_TRUE)
{
}

// nsIContentSink
NS_IMETHODIMP
nsSAXXMLReader::WillBuildModel()
{
  if (mContentHandler) {
    return mContentHandler->StartDocument();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::DidBuildModel()
{
  if (mContentHandler) {
    return mContentHandler->EndDocument();
  }
  mParser = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetParser(nsIParser* aParser)
{
  mParser = aParser;
  return NS_OK;
}

// nsIExtendedExpatSink
NS_IMETHODIMP
nsSAXXMLReader::HandleStartElement(const PRUnichar *aName,
                                   const PRUnichar **aAtts,
                                   PRUint32 aAttsCount,
                                   PRInt32 aIndex,
                                   PRUint32 aLineNumber)
{
  if (!mContentHandler) {
    return NS_OK;;
  }

  nsCOMPtr<nsSAXAttributes> atts = new nsSAXAttributes();
  if (!atts)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString uri, localName, qName;
  for (; *aAtts; aAtts += 2) {
    SplitExpatName(aAtts[0], uri, localName, qName);
    // XXX don't have attr type information
    NS_NAMED_LITERAL_STRING(cdataType, "CDATA");
    // could support xmlns reporting, it's a standard SAX feature
    if (!uri.EqualsLiteral(XMLNS_URI)) {
      atts->AddAttribute(uri, localName, qName, cdataType,
                         nsDependentString(aAtts[1]));
    }
  }

  // Deal with the element name
  SplitExpatName(aName, uri, localName, qName);
  return mContentHandler->StartElement(uri, localName, qName, atts);
}

NS_IMETHODIMP
nsSAXXMLReader::HandleEndElement(const PRUnichar *aName)
{
  if (mContentHandler) {
    nsAutoString uri, localName, qName;
    SplitExpatName(aName, uri, localName, qName);
    return mContentHandler->EndElement(uri, localName, qName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleComment(const PRUnichar *aName)
{
  if (mLexicalHandler) {
    return mLexicalHandler->Comment(nsDependentString(aName));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleCDataSection(const PRUnichar *aData,
                                   PRUint32 aLength)
{
  nsresult rv;
  if (mLexicalHandler) {
    rv = mLexicalHandler->StartCDATA();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mContentHandler) {
    rv = mContentHandler->Characters(Substring(aData, aData+aLength));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mLexicalHandler) {
    rv = mLexicalHandler->EndCDATA();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleStartDTD(const PRUnichar *aName,
                               const PRUnichar *aSystemId,
                               const PRUnichar *aPublicId)
{
  if (mLexicalHandler) {
    return mLexicalHandler->StartDTD(nsDependentString(aName),
                                     nsDependentString(aSystemId),
                                     nsDependentString(aPublicId));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleDoctypeDecl(const nsAString & aSubset,
                                  const nsAString & aName,
                                  const nsAString & aSystemId,
                                  const nsAString & aPublicId,
                                  nsISupports* aCatalogData)
{
  if (mLexicalHandler) {
    return mLexicalHandler->EndDTD();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleCharacterData(const PRUnichar *aData,
                                    PRUint32 aLength)
{
  if (mContentHandler) {
    return mContentHandler->Characters(Substring(aData, aData+aLength));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleStartNamespaceDecl(const PRUnichar *aPrefix,
                                         const PRUnichar *aUri)
{
  if (!mContentHandler) {
    return NS_OK;
  }
  
  const nsDependentString& uri = nsDependentString(aUri);
  if (aPrefix) {
    return mContentHandler->StartPrefixMapping(nsDependentString(aPrefix),
                                               uri);
  }

  return mContentHandler->StartPrefixMapping(EmptyString(), uri);
}

NS_IMETHODIMP
nsSAXXMLReader::HandleEndNamespaceDecl(const PRUnichar *aPrefix)
{
  if (!mContentHandler) {
    return NS_OK;
  }
  
  if (aPrefix) {
    return mContentHandler->EndPrefixMapping(nsDependentString(aPrefix));
  }

  return mContentHandler->EndPrefixMapping(EmptyString());
}

NS_IMETHODIMP
nsSAXXMLReader::HandleProcessingInstruction(const PRUnichar *aTarget,
                                            const PRUnichar *aData)
{
  if (mContentHandler) {
    return mContentHandler->ProcessingInstruction(nsDependentString(aTarget),
                                                  nsDependentString(aData));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleNotationDecl(const PRUnichar *aNotationName,
                                   const PRUnichar *aSystemId,
                                   const PRUnichar *aPublicId)
{
  if (mDTDHandler) {
    PRUnichar nullChar = PRUnichar(0);
    if (!aSystemId) aSystemId = &nullChar;
    if (!aPublicId) aPublicId = &nullChar;

    return mDTDHandler->NotationDecl(nsDependentString(aNotationName),
                                     nsDependentString(aSystemId),
                                     nsDependentString(aPublicId));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleUnparsedEntityDecl(const PRUnichar *aEntityName,
                                         const PRUnichar *aSystemId,
                                         const PRUnichar *aPublicId,
                                         const PRUnichar *aNotationName)
{
  if (mDTDHandler) {
    PRUnichar nullChar = PRUnichar(0);
    if (!aSystemId) aSystemId = &nullChar;
    if (!aPublicId) aPublicId = &nullChar;

    return mDTDHandler->UnparsedEntityDecl(nsDependentString(aEntityName),
                                           nsDependentString(aSystemId),
                                           nsDependentString(aPublicId),
                                           nsDependentString(aNotationName));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleXMLDeclaration(const PRUnichar *aVersion,
                                     const PRUnichar *aEncoding,
                                     PRInt32 aStandalone)
{
  // XXX need to decide what to do with this. It's a separate
  // optional interface in SAX.
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::ReportError(const PRUnichar* aErrorText,
                            const PRUnichar* aSourceText,
                            PRInt32 aLineNumber,
                            PRInt32 aColumnNumber)
{
  /// XXX need to settle what to do about the input setup, so I have
  /// coherent values for the nsISAXLocator here. nsnull for now.
  if (mErrorHandler) {
    return mErrorHandler->FatalError(nsnull, nsDependentString(aErrorText));
  }
  return NS_OK;
}


NS_IMETHODIMP
nsSAXXMLReader::GetAsync(PRBool *aAsync)
{
  *aAsync = mAsync;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetAsync(PRBool aAsync)
{
  mAsync = aAsync;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::GetBaseURI(nsIURI **aBaseURI)
{
  *aBaseURI = mBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetBaseURI(nsIURI *aBaseURI)
{
  mBaseURI = aBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::GetContentHandler(nsISAXContentHandler **aContentHandler)
{
  *aContentHandler = mContentHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetContentHandler(nsISAXContentHandler *aContentHandler)
{
  mContentHandler = aContentHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::GetDtdHandler(nsISAXDTDHandler **aDtdHandler)
{
  *aDtdHandler = mDTDHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetDtdHandler(nsISAXDTDHandler *aDtdHandler)
{
  mDTDHandler = aDtdHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::GetErrorHandler(nsISAXErrorHandler **aErrorHandler)
{
  *aErrorHandler = mErrorHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetErrorHandler(nsISAXErrorHandler *aErrorHandler)
{
  mErrorHandler = aErrorHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetFeature(const nsAString &aName, PRBool aValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetFeature(const nsAString &aName, PRBool *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetLexicalHandler(nsISAXLexicalHandler **aLexicalHandler)
{
  *aLexicalHandler = mLexicalHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetLexicalHandler(nsISAXLexicalHandler *aLexicalHandler)
{
  mLexicalHandler = aLexicalHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetProperty(const nsAString &aName, nsISupports* aValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetProperty(const nsAString &aName, PRBool *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::ParseFromString(const nsAString &aStr,
                                const char *aContentType)
{
  NS_ConvertUTF16toUTF8 data(aStr);

  // The new stream holds a reference to the buffer
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      data.get(), data.Length(),
                                      NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  return ParseFromStream(stream, "UTF-8", aContentType);
}

NS_IMETHODIMP
nsSAXXMLReader::ParseFromStream(nsIInputStream *aStream,
                                const char *aCharset,
                                const char *aContentType)
{
  NS_ENSURE_ARG(aStream);
  NS_ENSURE_ARG(aContentType);

  // Put the nsCOMPtr out here so we hold a ref to the stream as needed
  nsresult rv;
  nsCOMPtr<nsIInputStream> bufferedStream;
  if (!NS_InputStreamIsBuffered(aStream)) {
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                   aStream, 4096);
    NS_ENSURE_SUCCESS(rv, rv);
    aStream = bufferedStream;
  }
 
  // setup the parser
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
  parser->SetContentSink(this);
  nsCOMPtr<nsIURI> baseURI;
  if (mBaseURI) {
    baseURI = mBaseURI;
  } else {
    rv = NS_NewURI(getter_AddRefs(baseURI), "about:blank");
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mAsync) {
    // XXX This doesn't call DidBuildModel
    rv = parser->Parse(aStream, nsDependentCString(aContentType));
  } else {
    nsCOMPtr<nsIChannel> parserChannel;
    NS_NewInputStreamChannel(getter_AddRefs(parserChannel), baseURI, aStream,
                             nsDependentCString(aContentType), nsnull);
    NS_ENSURE_STATE(parserChannel);

    if (aCharset)
      parserChannel->SetContentCharset(nsDependentCString(aCharset));
    
    nsCOMPtr<nsIInputStreamChannel> inputChannel =
      do_QueryInterface(parserChannel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = parser->Parse(baseURI, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = parserChannel->AsyncOpen(listener, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
nsSAXXMLReader::SplitExpatName(const PRUnichar *aExpatName,
                               nsString &aURI,
                               nsString &aLocalName,
                               nsString &aQName)
{
  /**
   * Adapted from RDFContentSinkImpl
   *
   * Expat can send the following:
   *    localName
   *    namespaceURI<separator>localName
   *    namespaceURI<separator>localName<separator>prefix
   *
   * and we use 0xFFFF for the <separator>.
   *
   */

  nsDependentString expatStr(aExpatName);
  PRInt32 break1, break2 = kNotFound;
  break1 = expatStr.FindChar(PRUnichar(0xFFFF));

  if (break1 == kNotFound) {
    aLocalName = expatStr; // no namespace
    aURI.Truncate();
    aQName = expatStr;
  } else {
    aURI = StringHead(expatStr, break1);
    break2 = expatStr.FindChar(PRUnichar(0xFFFF), break1 + 1);
    if (break2 == kNotFound) { // namespace, but no prefix
      aLocalName = Substring(expatStr, break1 + 1);
      aQName = aLocalName;
    } else { // namespace with prefix
      aLocalName = Substring(expatStr, break1 + 1, break2 - break1 - 1);
      aQName = Substring(expatStr, break2 + 1) +
        NS_LITERAL_STRING(":") + aLocalName;
    }
  }

  return NS_OK;
}