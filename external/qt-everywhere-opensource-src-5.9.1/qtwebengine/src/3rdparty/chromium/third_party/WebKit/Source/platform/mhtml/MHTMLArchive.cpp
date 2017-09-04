/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/mhtml/MHTMLArchive.h"

#include "platform/DateComponents.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/text/QuotedPrintable.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "wtf/Assertions.h"
#include "wtf/CryptographicallyRandomNumber.h"
#include "wtf/CurrentTime.h"
#include "wtf/DateMath.h"
#include "wtf/text/Base64.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

const char* const quotedPrintable = "quoted-printable";
const char* const base64 = "base64";
const char* const binary = "binary";

static String replaceNonPrintableCharacters(const String& text) {
  StringBuilder stringBuilder;
  for (size_t i = 0; i < text.length(); ++i) {
    if (isASCIIPrintable(text[i]))
      stringBuilder.append(text[i]);
    else
      stringBuilder.append('?');
  }
  return stringBuilder.toString();
}

MHTMLArchive::MHTMLArchive() {}

MHTMLArchive* MHTMLArchive::create(const KURL& url,
                                   PassRefPtr<const SharedBuffer> data) {
  // MHTML pages can only be loaded from local URLs, http/https URLs, and
  // content URLs(Android specific).  The latter is now allowed due to full
  // sandboxing enforcement on MHTML pages.
  if (!canLoadArchive(url))
    return nullptr;

  MHTMLParser parser(std::move(data));
  HeapVector<Member<ArchiveResource>> resources = parser.parseArchive();
  if (resources.isEmpty())
    return nullptr;  // Invalid MHTML file.

  MHTMLArchive* archive = new MHTMLArchive;

  size_t resourcesCount = resources.size();
  // The first document suitable resource is the main resource of the top frame.
  for (size_t i = 0; i < resourcesCount; ++i) {
    if (archive->mainResource()) {
      archive->addSubresource(resources[i].get());
      continue;
    }

    const AtomicString& mimeType = resources[i]->mimeType();
    bool isMimeTypeSuitableForMainResource =
        MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType);
    // Want to allow image-only MHTML archives, but retain behavior for other
    // documents that have already been created expecting the first HTML page to
    // be considered the main resource.
    if (resourcesCount == 1 &&
        MIMETypeRegistry::isSupportedImageResourceMIMEType(mimeType)) {
      isMimeTypeSuitableForMainResource = true;
    }
    // explicitly disallow JS and CSS as the main resource.
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType) ||
        MIMETypeRegistry::isSupportedStyleSheetMIMEType(mimeType))
      isMimeTypeSuitableForMainResource = false;

    if (isMimeTypeSuitableForMainResource)
      archive->setMainResource(resources[i].get());
    else
      archive->addSubresource(resources[i].get());
  }
  return archive;
}

bool MHTMLArchive::canLoadArchive(const KURL& url) {
  // MHTML pages can only be loaded from local URLs, http/https URLs, and
  // content URLs(Android specific).  The latter is now allowed due to full
  // sandboxing enforcement on MHTML pages.
  if (SchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol()))
    return true;
  if (url.protocolIsInHTTPFamily())
    return true;
#if OS(ANDROID)
  if (url.protocolIs("content"))
    return true;
#endif
  return false;
}

void MHTMLArchive::generateMHTMLHeader(const String& boundary,
                                       const String& title,
                                       const String& mimeType,
                                       Vector<char>& outputBuffer) {
  ASSERT(!boundary.isEmpty());
  ASSERT(!mimeType.isEmpty());

  DateComponents now;
  now.setMillisecondsSinceEpochForDateTime(currentTimeMS());
  // TODO(lukasza): Passing individual date/time components seems fragile.
  String dateString = makeRFC2822DateString(
      now.weekDay(), now.monthDay(), now.month(), now.fullYear(), now.hour(),
      now.minute(), now.second(), 0);

  StringBuilder stringBuilder;
  stringBuilder.append("From: <Saved by Blink>\r\n");
  stringBuilder.append("Subject: ");
  // We replace non ASCII characters with '?' characters to match IE's behavior.
  stringBuilder.append(replaceNonPrintableCharacters(title));
  stringBuilder.append("\r\nDate: ");
  stringBuilder.append(dateString);
  stringBuilder.append("\r\nMIME-Version: 1.0\r\n");
  stringBuilder.append("Content-Type: multipart/related;\r\n");
  stringBuilder.append("\ttype=\"");
  stringBuilder.append(mimeType);
  stringBuilder.append("\";\r\n");
  stringBuilder.append("\tboundary=\"");
  stringBuilder.append(boundary);
  stringBuilder.append("\"\r\n\r\n");

  // We use utf8() below instead of ascii() as ascii() replaces CRLFs with ??
  // (we still only have put ASCII characters in it).
  ASSERT(stringBuilder.toString().containsOnlyASCII());
  CString asciiString = stringBuilder.toString().utf8();

  outputBuffer.append(asciiString.data(), asciiString.length());
}

void MHTMLArchive::generateMHTMLPart(const String& boundary,
                                     const String& contentID,
                                     EncodingPolicy encodingPolicy,
                                     const SerializedResource& resource,
                                     Vector<char>& outputBuffer) {
  ASSERT(!boundary.isEmpty());
  ASSERT(contentID.isEmpty() || contentID[0] == '<');

  StringBuilder stringBuilder;
  stringBuilder.append("--");
  stringBuilder.append(boundary);
  stringBuilder.append("\r\n");

  stringBuilder.append("Content-Type: ");
  stringBuilder.append(resource.mimeType);
  stringBuilder.append("\r\n");

  if (!contentID.isEmpty()) {
    stringBuilder.append("Content-ID: ");
    stringBuilder.append(contentID);
    stringBuilder.append("\r\n");
  }

  const char* contentEncoding = 0;
  if (encodingPolicy == UseBinaryEncoding)
    contentEncoding = binary;
  else if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(resource.mimeType) ||
           MIMETypeRegistry::isSupportedNonImageMIMEType(resource.mimeType))
    contentEncoding = quotedPrintable;
  else
    contentEncoding = base64;

  stringBuilder.append("Content-Transfer-Encoding: ");
  stringBuilder.append(contentEncoding);
  stringBuilder.append("\r\n");

  if (!resource.url.protocolIsAbout()) {
    stringBuilder.append("Content-Location: ");
    stringBuilder.append(resource.url.getString());
    stringBuilder.append("\r\n");
  }

  stringBuilder.append("\r\n");

  CString asciiString = stringBuilder.toString().utf8();
  outputBuffer.append(asciiString.data(), asciiString.length());

  if (!strcmp(contentEncoding, binary)) {
    const char* data;
    size_t position = 0;
    while (size_t length = resource.data->getSomeData(data, position)) {
      outputBuffer.append(data, length);
      position += length;
    }
  } else {
    // FIXME: ideally we would encode the content as a stream without having to
    // fetch it all.
    const char* data = resource.data->data();
    size_t dataLength = resource.data->size();
    Vector<char> encodedData;
    if (!strcmp(contentEncoding, quotedPrintable)) {
      quotedPrintableEncode(data, dataLength, encodedData);
      outputBuffer.append(encodedData.data(), encodedData.size());
      outputBuffer.append("\r\n", 2u);
    } else {
      ASSERT(!strcmp(contentEncoding, base64));
      // We are not specifying insertLFs = true below as it would cut the lines
      // with LFs and MHTML requires CRLFs.
      base64Encode(data, dataLength, encodedData);
      const size_t maximumLineLength = 76;
      size_t index = 0;
      size_t encodedDataLength = encodedData.size();
      do {
        size_t lineLength =
            std::min(encodedDataLength - index, maximumLineLength);
        outputBuffer.append(encodedData.data() + index, lineLength);
        outputBuffer.append("\r\n", 2u);
        index += maximumLineLength;
      } while (index < encodedDataLength);
    }
  }
}

void MHTMLArchive::generateMHTMLFooter(const String& boundary,
                                       Vector<char>& outputBuffer) {
  ASSERT(!boundary.isEmpty());
  CString asciiString = String("--" + boundary + "--\r\n").utf8();
  outputBuffer.append(asciiString.data(), asciiString.length());
}

void MHTMLArchive::setMainResource(ArchiveResource* mainResource) {
  m_mainResource = mainResource;
}

void MHTMLArchive::addSubresource(ArchiveResource* resource) {
  const KURL& url = resource->url();
  m_subresources.set(url, resource);
  KURL cidURI = MHTMLParser::convertContentIDToURI(resource->contentID());
  if (cidURI.isValid())
    m_subresources.set(cidURI, resource);
}

ArchiveResource* MHTMLArchive::subresourceForURL(const KURL& url) const {
  return m_subresources.get(url.getString());
}

DEFINE_TRACE(MHTMLArchive) {
  visitor->trace(m_mainResource);
  visitor->trace(m_subresources);
}

}  // namespace blink
