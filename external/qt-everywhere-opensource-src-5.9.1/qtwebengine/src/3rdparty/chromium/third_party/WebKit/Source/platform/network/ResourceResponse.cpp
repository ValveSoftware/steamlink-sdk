/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/network/ResourceResponse.h"

#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

namespace {

template <typename Interface>
Vector<Interface> isolatedCopy(const Vector<Interface>& src) {
  Vector<Interface> result;
  result.reserveCapacity(src.size());
  for (const auto& timestamp : src) {
    result.append(timestamp.isolatedCopy());
  }
  return result;
}

}  // namespace

ResourceResponse::SignedCertificateTimestamp::SignedCertificateTimestamp(
    const blink::WebURLResponse::SignedCertificateTimestamp& sct)
    : m_status(sct.status),
      m_origin(sct.origin),
      m_logDescription(sct.logDescription),
      m_logId(sct.logId),
      m_timestamp(sct.timestamp),
      m_hashAlgorithm(sct.hashAlgorithm),
      m_signatureAlgorithm(sct.signatureAlgorithm),
      m_signatureData(sct.signatureData) {}

ResourceResponse::SignedCertificateTimestamp
ResourceResponse::SignedCertificateTimestamp::isolatedCopy() const {
  return SignedCertificateTimestamp(
      m_status.isolatedCopy(), m_origin.isolatedCopy(),
      m_logDescription.isolatedCopy(), m_logId.isolatedCopy(), m_timestamp,
      m_hashAlgorithm.isolatedCopy(), m_signatureAlgorithm.isolatedCopy(),
      m_signatureData.isolatedCopy());
}

ResourceResponse::ResourceResponse()
    : m_expectedContentLength(0),
      m_httpStatusCode(0),
      m_lastModifiedDate(0),
      m_wasCached(false),
      m_connectionID(0),
      m_connectionReused(false),
      m_isNull(true),
      m_haveParsedAgeHeader(false),
      m_haveParsedDateHeader(false),
      m_haveParsedExpiresHeader(false),
      m_haveParsedLastModifiedHeader(false),
      m_age(0.0),
      m_date(0.0),
      m_expires(0.0),
      m_lastModified(0.0),
      m_hasMajorCertificateErrors(false),
      m_securityStyle(SecurityStyleUnknown),
      m_httpVersion(HTTPVersionUnknown),
      m_appCacheID(0),
      m_wasFetchedViaSPDY(false),
      m_wasNpnNegotiated(false),
      m_wasAlternateProtocolAvailable(false),
      m_wasFetchedViaProxy(false),
      m_wasFetchedViaServiceWorker(false),
      m_wasFetchedViaForeignFetch(false),
      m_wasFallbackRequiredByServiceWorker(false),
      m_serviceWorkerResponseType(WebServiceWorkerResponseTypeDefault),
      m_responseTime(0),
      m_remotePort(0),
      m_encodedDataLength(0),
      m_encodedBodyLength(0),
      m_decodedBodyLength(0) {}

ResourceResponse::ResourceResponse(const KURL& url,
                                   const AtomicString& mimeType,
                                   long long expectedLength,
                                   const AtomicString& textEncodingName,
                                   const String& filename)
    : m_url(url),
      m_mimeType(mimeType),
      m_expectedContentLength(expectedLength),
      m_textEncodingName(textEncodingName),
      m_suggestedFilename(filename),
      m_httpStatusCode(0),
      m_lastModifiedDate(0),
      m_wasCached(false),
      m_connectionID(0),
      m_connectionReused(false),
      m_isNull(false),
      m_haveParsedAgeHeader(false),
      m_haveParsedDateHeader(false),
      m_haveParsedExpiresHeader(false),
      m_haveParsedLastModifiedHeader(false),
      m_age(0.0),
      m_date(0.0),
      m_expires(0.0),
      m_lastModified(0.0),
      m_hasMajorCertificateErrors(false),
      m_securityStyle(SecurityStyleUnknown),
      m_httpVersion(HTTPVersionUnknown),
      m_appCacheID(0),
      m_wasFetchedViaSPDY(false),
      m_wasNpnNegotiated(false),
      m_wasAlternateProtocolAvailable(false),
      m_wasFetchedViaProxy(false),
      m_wasFetchedViaServiceWorker(false),
      m_wasFetchedViaForeignFetch(false),
      m_wasFallbackRequiredByServiceWorker(false),
      m_serviceWorkerResponseType(WebServiceWorkerResponseTypeDefault),
      m_responseTime(0),
      m_remotePort(0),
      m_encodedDataLength(0),
      m_encodedBodyLength(0),
      m_decodedBodyLength(0) {}

ResourceResponse::ResourceResponse(CrossThreadResourceResponseData* data)
    : ResourceResponse() {
  setURL(data->m_url);
  setMimeType(AtomicString(data->m_mimeType));
  setExpectedContentLength(data->m_expectedContentLength);
  setTextEncodingName(AtomicString(data->m_textEncodingName));
  setSuggestedFilename(data->m_suggestedFilename);

  setHTTPStatusCode(data->m_httpStatusCode);
  setHTTPStatusText(AtomicString(data->m_httpStatusText));

  m_httpHeaderFields.adopt(std::move(data->m_httpHeaders));
  setLastModifiedDate(data->m_lastModifiedDate);
  setResourceLoadTiming(data->m_resourceLoadTiming.release());
  m_hasMajorCertificateErrors = data->m_hasMajorCertificateErrors;
  m_securityStyle = data->m_securityStyle;
  m_securityDetails.protocol = data->m_securityDetails.protocol;
  m_securityDetails.cipher = data->m_securityDetails.cipher;
  m_securityDetails.keyExchange = data->m_securityDetails.keyExchange;
  m_securityDetails.keyExchangeGroup = data->m_securityDetails.keyExchangeGroup;
  m_securityDetails.mac = data->m_securityDetails.mac;
  m_securityDetails.subjectName = data->m_securityDetails.subjectName;
  m_securityDetails.sanList = data->m_securityDetails.sanList;
  m_securityDetails.issuer = data->m_securityDetails.issuer;
  m_securityDetails.validFrom = data->m_securityDetails.validFrom;
  m_securityDetails.validTo = data->m_securityDetails.validTo;
  for (auto& cert : data->m_certificate)
    m_securityDetails.certificate.append(AtomicString(cert));
  m_securityDetails.sctList = data->m_securityDetails.sctList;
  m_httpVersion = data->m_httpVersion;
  m_appCacheID = data->m_appCacheID;
  m_appCacheManifestURL = data->m_appCacheManifestURL.copy();
  m_multipartBoundary = data->m_multipartBoundary;
  m_wasFetchedViaSPDY = data->m_wasFetchedViaSPDY;
  m_wasNpnNegotiated = data->m_wasNpnNegotiated;
  m_wasAlternateProtocolAvailable = data->m_wasAlternateProtocolAvailable;
  m_wasFetchedViaProxy = data->m_wasFetchedViaProxy;
  m_wasFetchedViaServiceWorker = data->m_wasFetchedViaServiceWorker;
  m_wasFetchedViaForeignFetch = data->m_wasFetchedViaForeignFetch;
  m_wasFallbackRequiredByServiceWorker =
      data->m_wasFallbackRequiredByServiceWorker;
  m_serviceWorkerResponseType = data->m_serviceWorkerResponseType;
  m_originalURLViaServiceWorker = data->m_originalURLViaServiceWorker;
  m_cacheStorageCacheName = data->m_cacheStorageCacheName;
  m_responseTime = data->m_responseTime;
  m_remoteIPAddress = AtomicString(data->m_remoteIPAddress);
  m_remotePort = data->m_remotePort;
  m_encodedDataLength = data->m_encodedDataLength;
  m_encodedBodyLength = data->m_encodedBodyLength;
  m_decodedBodyLength = data->m_decodedBodyLength;
  m_downloadedFilePath = data->m_downloadedFilePath;
  m_downloadedFileHandle = data->m_downloadedFileHandle;

  // Bug https://bugs.webkit.org/show_bug.cgi?id=60397 this doesn't support
  // whatever values may be present in the opaque m_extraData structure.
}

ResourceResponse::ResourceResponse(const ResourceResponse&) = default;
ResourceResponse& ResourceResponse::operator=(const ResourceResponse&) =
    default;

std::unique_ptr<CrossThreadResourceResponseData> ResourceResponse::copyData()
    const {
  std::unique_ptr<CrossThreadResourceResponseData> data =
      wrapUnique(new CrossThreadResourceResponseData);
  data->m_url = url().copy();
  data->m_mimeType = mimeType().getString().isolatedCopy();
  data->m_expectedContentLength = expectedContentLength();
  data->m_textEncodingName = textEncodingName().getString().isolatedCopy();
  data->m_suggestedFilename = suggestedFilename().isolatedCopy();
  data->m_httpStatusCode = httpStatusCode();
  data->m_httpStatusText = httpStatusText().getString().isolatedCopy();
  data->m_httpHeaders = httpHeaderFields().copyData();
  data->m_lastModifiedDate = lastModifiedDate();
  if (m_resourceLoadTiming)
    data->m_resourceLoadTiming = m_resourceLoadTiming->deepCopy();
  data->m_hasMajorCertificateErrors = m_hasMajorCertificateErrors;
  data->m_securityStyle = m_securityStyle;
  data->m_securityDetails.protocol = m_securityDetails.protocol.isolatedCopy();
  data->m_securityDetails.cipher = m_securityDetails.cipher.isolatedCopy();
  data->m_securityDetails.keyExchange =
      m_securityDetails.keyExchange.isolatedCopy();
  data->m_securityDetails.keyExchangeGroup =
      m_securityDetails.keyExchangeGroup.isolatedCopy();
  data->m_securityDetails.mac = m_securityDetails.mac.isolatedCopy();
  data->m_securityDetails.subjectName =
      m_securityDetails.subjectName.isolatedCopy();
  data->m_securityDetails.sanList = isolatedCopy(m_securityDetails.sanList);
  data->m_securityDetails.issuer = m_securityDetails.issuer.isolatedCopy();
  data->m_securityDetails.validFrom = m_securityDetails.validFrom;
  data->m_securityDetails.validTo = m_securityDetails.validTo;
  for (auto& cert : m_securityDetails.certificate)
    data->m_certificate.append(cert.getString().isolatedCopy());
  data->m_securityDetails.sctList = isolatedCopy(m_securityDetails.sctList);
  data->m_httpVersion = m_httpVersion;
  data->m_appCacheID = m_appCacheID;
  data->m_appCacheManifestURL = m_appCacheManifestURL.copy();
  data->m_multipartBoundary = m_multipartBoundary;
  data->m_wasFetchedViaSPDY = m_wasFetchedViaSPDY;
  data->m_wasNpnNegotiated = m_wasNpnNegotiated;
  data->m_wasAlternateProtocolAvailable = m_wasAlternateProtocolAvailable;
  data->m_wasFetchedViaProxy = m_wasFetchedViaProxy;
  data->m_wasFetchedViaServiceWorker = m_wasFetchedViaServiceWorker;
  data->m_wasFetchedViaForeignFetch = m_wasFetchedViaForeignFetch;
  data->m_wasFallbackRequiredByServiceWorker =
      m_wasFallbackRequiredByServiceWorker;
  data->m_serviceWorkerResponseType = m_serviceWorkerResponseType;
  data->m_originalURLViaServiceWorker = m_originalURLViaServiceWorker.copy();
  data->m_cacheStorageCacheName = cacheStorageCacheName().isolatedCopy();
  data->m_responseTime = m_responseTime;
  data->m_remoteIPAddress = m_remoteIPAddress.getString().isolatedCopy();
  data->m_remotePort = m_remotePort;
  data->m_encodedDataLength = m_encodedDataLength;
  data->m_encodedBodyLength = m_encodedBodyLength;
  data->m_decodedBodyLength = m_decodedBodyLength;
  data->m_downloadedFilePath = m_downloadedFilePath.isolatedCopy();
  data->m_downloadedFileHandle = m_downloadedFileHandle;

  // Bug https://bugs.webkit.org/show_bug.cgi?id=60397 this doesn't support
  // whatever values may be present in the opaque m_extraData structure.

  return data;
}

bool ResourceResponse::isHTTP() const {
  return m_url.protocolIsInHTTPFamily();
}

const KURL& ResourceResponse::url() const {
  return m_url;
}

void ResourceResponse::setURL(const KURL& url) {
  m_isNull = false;

  m_url = url;
}

const AtomicString& ResourceResponse::mimeType() const {
  return m_mimeType;
}

void ResourceResponse::setMimeType(const AtomicString& mimeType) {
  m_isNull = false;

  // FIXME: MIME type is determined by HTTP Content-Type header. We should
  // update the header, so that it doesn't disagree with m_mimeType.
  m_mimeType = mimeType;
}

long long ResourceResponse::expectedContentLength() const {
  return m_expectedContentLength;
}

void ResourceResponse::setExpectedContentLength(
    long long expectedContentLength) {
  m_isNull = false;

  // FIXME: Content length is determined by HTTP Content-Length header. We
  // should update the header, so that it doesn't disagree with
  // m_expectedContentLength.
  m_expectedContentLength = expectedContentLength;
}

const AtomicString& ResourceResponse::textEncodingName() const {
  return m_textEncodingName;
}

void ResourceResponse::setTextEncodingName(const AtomicString& encodingName) {
  m_isNull = false;

  // FIXME: Text encoding is determined by HTTP Content-Type header. We should
  // update the header, so that it doesn't disagree with m_textEncodingName.
  m_textEncodingName = encodingName;
}

// FIXME should compute this on the fly
const String& ResourceResponse::suggestedFilename() const {
  return m_suggestedFilename;
}

void ResourceResponse::setSuggestedFilename(const String& suggestedName) {
  m_isNull = false;

  // FIXME: Suggested file name is calculated based on other headers. There
  // should not be a setter for it.
  m_suggestedFilename = suggestedName;
}

int ResourceResponse::httpStatusCode() const {
  return m_httpStatusCode;
}

void ResourceResponse::setHTTPStatusCode(int statusCode) {
  m_httpStatusCode = statusCode;
}

const AtomicString& ResourceResponse::httpStatusText() const {
  return m_httpStatusText;
}

void ResourceResponse::setHTTPStatusText(const AtomicString& statusText) {
  m_httpStatusText = statusText;
}

const AtomicString& ResourceResponse::httpHeaderField(
    const AtomicString& name) const {
  return m_httpHeaderFields.get(name);
}

static const AtomicString& cacheControlHeaderString() {
  DEFINE_STATIC_LOCAL(const AtomicString, cacheControlHeader,
                      ("cache-control"));
  return cacheControlHeader;
}

static const AtomicString& pragmaHeaderString() {
  DEFINE_STATIC_LOCAL(const AtomicString, pragmaHeader, ("pragma"));
  return pragmaHeader;
}

void ResourceResponse::updateHeaderParsedState(const AtomicString& name) {
  DEFINE_STATIC_LOCAL(const AtomicString, ageHeader, ("age"));
  DEFINE_STATIC_LOCAL(const AtomicString, dateHeader, ("date"));
  DEFINE_STATIC_LOCAL(const AtomicString, expiresHeader, ("expires"));
  DEFINE_STATIC_LOCAL(const AtomicString, lastModifiedHeader,
                      ("last-modified"));

  if (equalIgnoringCase(name, ageHeader))
    m_haveParsedAgeHeader = false;
  else if (equalIgnoringCase(name, cacheControlHeaderString()) ||
           equalIgnoringCase(name, pragmaHeaderString()))
    m_cacheControlHeader = CacheControlHeader();
  else if (equalIgnoringCase(name, dateHeader))
    m_haveParsedDateHeader = false;
  else if (equalIgnoringCase(name, expiresHeader))
    m_haveParsedExpiresHeader = false;
  else if (equalIgnoringCase(name, lastModifiedHeader))
    m_haveParsedLastModifiedHeader = false;
}

void ResourceResponse::setSecurityDetails(
    const String& protocol,
    const String& keyExchange,
    const String& keyExchangeGroup,
    const String& cipher,
    const String& mac,
    const String& subjectName,
    const Vector<String>& sanList,
    const String& issuer,
    time_t validFrom,
    time_t validTo,
    const Vector<AtomicString>& certificate,
    const SignedCertificateTimestampList& sctList) {
  m_securityDetails.protocol = protocol;
  m_securityDetails.keyExchange = keyExchange;
  m_securityDetails.keyExchangeGroup = keyExchangeGroup;
  m_securityDetails.cipher = cipher;
  m_securityDetails.mac = mac;
  m_securityDetails.subjectName = subjectName;
  m_securityDetails.sanList = sanList;
  m_securityDetails.issuer = issuer;
  m_securityDetails.validFrom = validFrom;
  m_securityDetails.validTo = validTo;
  m_securityDetails.certificate = certificate;
  m_securityDetails.sctList = sctList;
}

void ResourceResponse::setHTTPHeaderField(const AtomicString& name,
                                          const AtomicString& value) {
  updateHeaderParsedState(name);

  m_httpHeaderFields.set(name, value);
}

void ResourceResponse::addHTTPHeaderField(const AtomicString& name,
                                          const AtomicString& value) {
  updateHeaderParsedState(name);

  HTTPHeaderMap::AddResult result = m_httpHeaderFields.add(name, value);
  if (!result.isNewEntry)
    result.storedValue->value = result.storedValue->value + ", " + value;
}

void ResourceResponse::clearHTTPHeaderField(const AtomicString& name) {
  m_httpHeaderFields.remove(name);
}

const HTTPHeaderMap& ResourceResponse::httpHeaderFields() const {
  return m_httpHeaderFields;
}

bool ResourceResponse::cacheControlContainsNoCache() const {
  if (!m_cacheControlHeader.parsed)
    m_cacheControlHeader = parseCacheControlDirectives(
        m_httpHeaderFields.get(cacheControlHeaderString()),
        m_httpHeaderFields.get(pragmaHeaderString()));
  return m_cacheControlHeader.containsNoCache;
}

bool ResourceResponse::cacheControlContainsNoStore() const {
  if (!m_cacheControlHeader.parsed)
    m_cacheControlHeader = parseCacheControlDirectives(
        m_httpHeaderFields.get(cacheControlHeaderString()),
        m_httpHeaderFields.get(pragmaHeaderString()));
  return m_cacheControlHeader.containsNoStore;
}

bool ResourceResponse::cacheControlContainsMustRevalidate() const {
  if (!m_cacheControlHeader.parsed)
    m_cacheControlHeader = parseCacheControlDirectives(
        m_httpHeaderFields.get(cacheControlHeaderString()),
        m_httpHeaderFields.get(pragmaHeaderString()));
  return m_cacheControlHeader.containsMustRevalidate;
}

bool ResourceResponse::hasCacheValidatorFields() const {
  DEFINE_STATIC_LOCAL(const AtomicString, lastModifiedHeader,
                      ("last-modified"));
  DEFINE_STATIC_LOCAL(const AtomicString, eTagHeader, ("etag"));
  return !m_httpHeaderFields.get(lastModifiedHeader).isEmpty() ||
         !m_httpHeaderFields.get(eTagHeader).isEmpty();
}

double ResourceResponse::cacheControlMaxAge() const {
  if (!m_cacheControlHeader.parsed)
    m_cacheControlHeader = parseCacheControlDirectives(
        m_httpHeaderFields.get(cacheControlHeaderString()),
        m_httpHeaderFields.get(pragmaHeaderString()));
  return m_cacheControlHeader.maxAge;
}

double ResourceResponse::cacheControlStaleWhileRevalidate() const {
  if (!m_cacheControlHeader.parsed)
    m_cacheControlHeader = parseCacheControlDirectives(
        m_httpHeaderFields.get(cacheControlHeaderString()),
        m_httpHeaderFields.get(pragmaHeaderString()));
  return m_cacheControlHeader.staleWhileRevalidate;
}

static double parseDateValueInHeader(const HTTPHeaderMap& headers,
                                     const AtomicString& headerName) {
  const AtomicString& headerValue = headers.get(headerName);
  if (headerValue.isEmpty())
    return std::numeric_limits<double>::quiet_NaN();
  // This handles all date formats required by RFC2616:
  // Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
  // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
  // Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
  double dateInMilliseconds = parseDate(headerValue);
  if (!std::isfinite(dateInMilliseconds))
    return std::numeric_limits<double>::quiet_NaN();
  return dateInMilliseconds / 1000;
}

double ResourceResponse::date() const {
  if (!m_haveParsedDateHeader) {
    DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("date"));
    m_date = parseDateValueInHeader(m_httpHeaderFields, headerName);
    m_haveParsedDateHeader = true;
  }
  return m_date;
}

double ResourceResponse::age() const {
  if (!m_haveParsedAgeHeader) {
    DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("age"));
    const AtomicString& headerValue = m_httpHeaderFields.get(headerName);
    bool ok;
    m_age = headerValue.toDouble(&ok);
    if (!ok)
      m_age = std::numeric_limits<double>::quiet_NaN();
    m_haveParsedAgeHeader = true;
  }
  return m_age;
}

double ResourceResponse::expires() const {
  if (!m_haveParsedExpiresHeader) {
    DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("expires"));
    m_expires = parseDateValueInHeader(m_httpHeaderFields, headerName);
    m_haveParsedExpiresHeader = true;
  }
  return m_expires;
}

double ResourceResponse::lastModified() const {
  if (!m_haveParsedLastModifiedHeader) {
    DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("last-modified"));
    m_lastModified = parseDateValueInHeader(m_httpHeaderFields, headerName);
    m_haveParsedLastModifiedHeader = true;
  }
  return m_lastModified;
}

bool ResourceResponse::isAttachment() const {
  DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("content-disposition"));
  String value = m_httpHeaderFields.get(headerName);
  size_t loc = value.find(';');
  if (loc != kNotFound)
    value = value.left(loc);
  value = value.stripWhiteSpace();
  DEFINE_STATIC_LOCAL(const AtomicString, attachmentString, ("attachment"));
  return equalIgnoringCase(value, attachmentString);
}

void ResourceResponse::setLastModifiedDate(time_t lastModifiedDate) {
  m_lastModifiedDate = lastModifiedDate;
}

time_t ResourceResponse::lastModifiedDate() const {
  return m_lastModifiedDate;
}

bool ResourceResponse::wasCached() const {
  return m_wasCached;
}

void ResourceResponse::setWasCached(bool value) {
  m_wasCached = value;
}

bool ResourceResponse::connectionReused() const {
  return m_connectionReused;
}

void ResourceResponse::setConnectionReused(bool connectionReused) {
  m_connectionReused = connectionReused;
}

unsigned ResourceResponse::connectionID() const {
  return m_connectionID;
}

void ResourceResponse::setConnectionID(unsigned connectionID) {
  m_connectionID = connectionID;
}

ResourceLoadTiming* ResourceResponse::resourceLoadTiming() const {
  return m_resourceLoadTiming.get();
}

void ResourceResponse::setResourceLoadTiming(
    PassRefPtr<ResourceLoadTiming> resourceLoadTiming) {
  m_resourceLoadTiming = resourceLoadTiming;
}

PassRefPtr<ResourceLoadInfo> ResourceResponse::resourceLoadInfo() const {
  return m_resourceLoadInfo.get();
}

void ResourceResponse::setResourceLoadInfo(
    PassRefPtr<ResourceLoadInfo> loadInfo) {
  m_resourceLoadInfo = loadInfo;
}

void ResourceResponse::addToEncodedDataLength(long long value) {
  m_encodedDataLength += value;
}

void ResourceResponse::addToEncodedBodyLength(long long value) {
  m_encodedBodyLength += value;
}

void ResourceResponse::addToDecodedBodyLength(long long value) {
  m_decodedBodyLength += value;
}

void ResourceResponse::setDownloadedFilePath(const String& downloadedFilePath) {
  m_downloadedFilePath = downloadedFilePath;
  if (m_downloadedFilePath.isEmpty()) {
    m_downloadedFileHandle.clear();
    return;
  }
  // TODO(dmurph): Investigate whether we need the mimeType on this blob.
  std::unique_ptr<BlobData> blobData =
      BlobData::createForFileWithUnknownSize(m_downloadedFilePath);
  blobData->detachFromCurrentThread();
  m_downloadedFileHandle = BlobDataHandle::create(std::move(blobData), -1);
}

void ResourceResponse::appendRedirectResponse(
    const ResourceResponse& response) {
  m_redirectResponses.append(response);
}

bool ResourceResponse::compare(const ResourceResponse& a,
                               const ResourceResponse& b) {
  if (a.isNull() != b.isNull())
    return false;
  if (a.url() != b.url())
    return false;
  if (a.mimeType() != b.mimeType())
    return false;
  if (a.expectedContentLength() != b.expectedContentLength())
    return false;
  if (a.textEncodingName() != b.textEncodingName())
    return false;
  if (a.suggestedFilename() != b.suggestedFilename())
    return false;
  if (a.httpStatusCode() != b.httpStatusCode())
    return false;
  if (a.httpStatusText() != b.httpStatusText())
    return false;
  if (a.httpHeaderFields() != b.httpHeaderFields())
    return false;
  if (a.resourceLoadTiming() && b.resourceLoadTiming() &&
      *a.resourceLoadTiming() == *b.resourceLoadTiming())
    return true;
  if (a.resourceLoadTiming() != b.resourceLoadTiming())
    return false;
  if (a.encodedBodyLength() != b.encodedBodyLength())
    return false;
  if (a.decodedBodyLength() != b.decodedBodyLength())
    return false;
  return true;
}

}  // namespace blink
