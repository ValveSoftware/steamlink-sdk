/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/platform/WebURLResponse.h"

#include "platform/network/ResourceLoadTiming.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "public/platform/WebHTTPLoadInfo.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoadTiming.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

namespace {

class ExtraDataContainer : public ResourceResponse::ExtraData {
 public:
  static PassRefPtr<ExtraDataContainer> create(
      WebURLResponse::ExtraData* extraData) {
    return adoptRef(new ExtraDataContainer(extraData));
  }

  ~ExtraDataContainer() override {}

  WebURLResponse::ExtraData* getExtraData() const { return m_extraData.get(); }

 private:
  explicit ExtraDataContainer(WebURLResponse::ExtraData* extraData)
      : m_extraData(wrapUnique(extraData)) {}

  std::unique_ptr<WebURLResponse::ExtraData> m_extraData;
};

}  // namespace

// The purpose of this struct is to permit allocating a ResourceResponse on the
// heap, which is otherwise disallowed by the DISALLOW_NEW_EXCEPT_PLACEMENT_NEW
// annotation on ResourceResponse.
struct WebURLResponse::ResourceResponseContainer {
  ResourceResponseContainer() {}

  explicit ResourceResponseContainer(const ResourceResponse& r)
      : resourceResponse(r) {}

  ResourceResponse resourceResponse;
};

WebURLResponse::~WebURLResponse() {}

WebURLResponse::WebURLResponse()
    : m_ownedResourceResponse(new ResourceResponseContainer()),
      m_resourceResponse(&m_ownedResourceResponse->resourceResponse) {}

WebURLResponse::WebURLResponse(const WebURLResponse& r)
    : m_ownedResourceResponse(
          new ResourceResponseContainer(*r.m_resourceResponse)),
      m_resourceResponse(&m_ownedResourceResponse->resourceResponse) {}

WebURLResponse::WebURLResponse(const WebURL& url) : WebURLResponse() {
  setURL(url);
}

WebURLResponse& WebURLResponse::operator=(const WebURLResponse& r) {
  // Copying subclasses that have different m_resourceResponse ownership
  // semantics via this operator is just not supported.
  DCHECK(m_ownedResourceResponse);
  DCHECK(m_resourceResponse);
  if (&r != this)
    *m_resourceResponse = *r.m_resourceResponse;
  return *this;
}

bool WebURLResponse::isNull() const {
  return m_resourceResponse->isNull();
}

WebURL WebURLResponse::url() const {
  return m_resourceResponse->url();
}

void WebURLResponse::setURL(const WebURL& url) {
  m_resourceResponse->setURL(url);
}

unsigned WebURLResponse::connectionID() const {
  return m_resourceResponse->connectionID();
}

void WebURLResponse::setConnectionID(unsigned connectionID) {
  m_resourceResponse->setConnectionID(connectionID);
}

bool WebURLResponse::connectionReused() const {
  return m_resourceResponse->connectionReused();
}

void WebURLResponse::setConnectionReused(bool connectionReused) {
  m_resourceResponse->setConnectionReused(connectionReused);
}

WebURLLoadTiming WebURLResponse::loadTiming() {
  return WebURLLoadTiming(m_resourceResponse->resourceLoadTiming());
}

void WebURLResponse::setLoadTiming(const WebURLLoadTiming& timing) {
  RefPtr<ResourceLoadTiming> loadTiming =
      PassRefPtr<ResourceLoadTiming>(timing);
  m_resourceResponse->setResourceLoadTiming(loadTiming.release());
}

WebHTTPLoadInfo WebURLResponse::httpLoadInfo() {
  return WebHTTPLoadInfo(m_resourceResponse->resourceLoadInfo());
}

void WebURLResponse::setHTTPLoadInfo(const WebHTTPLoadInfo& value) {
  m_resourceResponse->setResourceLoadInfo(value);
}

void WebURLResponse::setResponseTime(long long responseTime) {
  m_resourceResponse->setResponseTime(static_cast<int64_t>(responseTime));
}

WebString WebURLResponse::mimeType() const {
  return m_resourceResponse->mimeType();
}

void WebURLResponse::setMIMEType(const WebString& mimeType) {
  m_resourceResponse->setMimeType(mimeType);
}

long long WebURLResponse::expectedContentLength() const {
  return m_resourceResponse->expectedContentLength();
}

void WebURLResponse::setExpectedContentLength(long long expectedContentLength) {
  m_resourceResponse->setExpectedContentLength(expectedContentLength);
}

WebString WebURLResponse::textEncodingName() const {
  return m_resourceResponse->textEncodingName();
}

void WebURLResponse::setTextEncodingName(const WebString& textEncodingName) {
  m_resourceResponse->setTextEncodingName(textEncodingName);
}

WebString WebURLResponse::suggestedFileName() const {
  return m_resourceResponse->suggestedFilename();
}

void WebURLResponse::setSuggestedFileName(const WebString& suggestedFileName) {
  m_resourceResponse->setSuggestedFilename(suggestedFileName);
}

WebURLResponse::HTTPVersion WebURLResponse::httpVersion() const {
  return static_cast<HTTPVersion>(m_resourceResponse->httpVersion());
}

void WebURLResponse::setHTTPVersion(HTTPVersion version) {
  m_resourceResponse->setHTTPVersion(
      static_cast<ResourceResponse::HTTPVersion>(version));
}

int WebURLResponse::httpStatusCode() const {
  return m_resourceResponse->httpStatusCode();
}

void WebURLResponse::setHTTPStatusCode(int httpStatusCode) {
  m_resourceResponse->setHTTPStatusCode(httpStatusCode);
}

WebString WebURLResponse::httpStatusText() const {
  return m_resourceResponse->httpStatusText();
}

void WebURLResponse::setHTTPStatusText(const WebString& httpStatusText) {
  m_resourceResponse->setHTTPStatusText(httpStatusText);
}

WebString WebURLResponse::httpHeaderField(const WebString& name) const {
  return m_resourceResponse->httpHeaderField(name);
}

void WebURLResponse::setHTTPHeaderField(const WebString& name,
                                        const WebString& value) {
  m_resourceResponse->setHTTPHeaderField(name, value);
}

void WebURLResponse::addHTTPHeaderField(const WebString& name,
                                        const WebString& value) {
  if (name.isNull() || value.isNull())
    return;

  m_resourceResponse->addHTTPHeaderField(name, value);
}

void WebURLResponse::clearHTTPHeaderField(const WebString& name) {
  m_resourceResponse->clearHTTPHeaderField(name);
}

void WebURLResponse::visitHTTPHeaderFields(
    WebHTTPHeaderVisitor* visitor) const {
  const HTTPHeaderMap& map = m_resourceResponse->httpHeaderFields();
  for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
    visitor->visitHeader(it->key, it->value);
}

double WebURLResponse::lastModifiedDate() const {
  return static_cast<double>(m_resourceResponse->lastModifiedDate());
}

void WebURLResponse::setLastModifiedDate(double lastModifiedDate) {
  m_resourceResponse->setLastModifiedDate(
      static_cast<time_t>(lastModifiedDate));
}

long long WebURLResponse::appCacheID() const {
  return m_resourceResponse->appCacheID();
}

void WebURLResponse::setAppCacheID(long long appCacheID) {
  m_resourceResponse->setAppCacheID(appCacheID);
}

WebURL WebURLResponse::appCacheManifestURL() const {
  return m_resourceResponse->appCacheManifestURL();
}

void WebURLResponse::setAppCacheManifestURL(const WebURL& url) {
  m_resourceResponse->setAppCacheManifestURL(url);
}

void WebURLResponse::setHasMajorCertificateErrors(bool value) {
  m_resourceResponse->setHasMajorCertificateErrors(value);
}

WebSecurityStyle WebURLResponse::getSecurityStyle() const {
  return static_cast<WebSecurityStyle>(m_resourceResponse->getSecurityStyle());
}

void WebURLResponse::setSecurityStyle(WebSecurityStyle securityStyle) {
  m_resourceResponse->setSecurityStyle(
      static_cast<ResourceResponse::SecurityStyle>(securityStyle));
}

void WebURLResponse::setSecurityDetails(
    const WebSecurityDetails& webSecurityDetails) {
  blink::ResourceResponse::SignedCertificateTimestampList sctList;
  for (const auto& iter : webSecurityDetails.sctList)
    sctList.append(
        static_cast<blink::ResourceResponse::SignedCertificateTimestamp>(iter));
  Vector<String> sanList;
  sanList.append(webSecurityDetails.sanList.data(),
                 webSecurityDetails.sanList.size());
  Vector<AtomicString> certificate;
  for (const auto& iter : webSecurityDetails.certificate) {
    AtomicString cert = iter;
    certificate.append(cert);
  }
  m_resourceResponse->setSecurityDetails(
      webSecurityDetails.protocol, webSecurityDetails.keyExchange,
      webSecurityDetails.keyExchangeGroup, webSecurityDetails.cipher,
      webSecurityDetails.mac, webSecurityDetails.subjectName, sanList,
      webSecurityDetails.issuer,
      static_cast<time_t>(webSecurityDetails.validFrom),
      static_cast<time_t>(webSecurityDetails.validTo), certificate, sctList);
}

ResourceResponse& WebURLResponse::toMutableResourceResponse() {
  return *m_resourceResponse;
}

const ResourceResponse& WebURLResponse::toResourceResponse() const {
  return *m_resourceResponse;
}

bool WebURLResponse::wasCached() const {
  return m_resourceResponse->wasCached();
}

void WebURLResponse::setWasCached(bool value) {
  m_resourceResponse->setWasCached(value);
}

bool WebURLResponse::wasFetchedViaSPDY() const {
  return m_resourceResponse->wasFetchedViaSPDY();
}

void WebURLResponse::setWasFetchedViaSPDY(bool value) {
  m_resourceResponse->setWasFetchedViaSPDY(value);
}

bool WebURLResponse::wasNpnNegotiated() const {
  return m_resourceResponse->wasNpnNegotiated();
}

void WebURLResponse::setWasNpnNegotiated(bool value) {
  m_resourceResponse->setWasNpnNegotiated(value);
}

bool WebURLResponse::wasAlternateProtocolAvailable() const {
  return m_resourceResponse->wasAlternateProtocolAvailable();
}

void WebURLResponse::setWasAlternateProtocolAvailable(bool value) {
  m_resourceResponse->setWasAlternateProtocolAvailable(value);
}

bool WebURLResponse::wasFetchedViaServiceWorker() const {
  return m_resourceResponse->wasFetchedViaServiceWorker();
}

void WebURLResponse::setWasFetchedViaServiceWorker(bool value) {
  m_resourceResponse->setWasFetchedViaServiceWorker(value);
}

bool WebURLResponse::wasFetchedViaForeignFetch() const {
  return m_resourceResponse->wasFetchedViaForeignFetch();
}

void WebURLResponse::setWasFetchedViaForeignFetch(bool value) {
  m_resourceResponse->setWasFetchedViaForeignFetch(value);
}

bool WebURLResponse::wasFallbackRequiredByServiceWorker() const {
  return m_resourceResponse->wasFallbackRequiredByServiceWorker();
}

void WebURLResponse::setWasFallbackRequiredByServiceWorker(bool value) {
  m_resourceResponse->setWasFallbackRequiredByServiceWorker(value);
}

WebServiceWorkerResponseType WebURLResponse::serviceWorkerResponseType() const {
  return m_resourceResponse->serviceWorkerResponseType();
}

void WebURLResponse::setServiceWorkerResponseType(
    WebServiceWorkerResponseType value) {
  m_resourceResponse->setServiceWorkerResponseType(value);
}

WebURL WebURLResponse::originalURLViaServiceWorker() const {
  return m_resourceResponse->originalURLViaServiceWorker();
}

void WebURLResponse::setOriginalURLViaServiceWorker(const WebURL& url) {
  m_resourceResponse->setOriginalURLViaServiceWorker(url);
}

void WebURLResponse::setMultipartBoundary(const char* bytes, size_t size) {
  m_resourceResponse->setMultipartBoundary(bytes, size);
}

WebString WebURLResponse::cacheStorageCacheName() const {
  return m_resourceResponse->cacheStorageCacheName();
}

void WebURLResponse::setCacheStorageCacheName(
    const WebString& cacheStorageCacheName) {
  m_resourceResponse->setCacheStorageCacheName(cacheStorageCacheName);
}

void WebURLResponse::setCorsExposedHeaderNames(
    const WebVector<WebString>& headerNames) {
  Vector<String> exposedHeaderNames;
  exposedHeaderNames.append(headerNames.data(), headerNames.size());
  m_resourceResponse->setCorsExposedHeaderNames(exposedHeaderNames);
}

WebString WebURLResponse::downloadFilePath() const {
  return m_resourceResponse->downloadedFilePath();
}

void WebURLResponse::setDownloadFilePath(const WebString& downloadFilePath) {
  m_resourceResponse->setDownloadedFilePath(downloadFilePath);
}

WebString WebURLResponse::remoteIPAddress() const {
  return m_resourceResponse->remoteIPAddress();
}

void WebURLResponse::setRemoteIPAddress(const WebString& remoteIPAddress) {
  m_resourceResponse->setRemoteIPAddress(remoteIPAddress);
}

unsigned short WebURLResponse::remotePort() const {
  return m_resourceResponse->remotePort();
}

void WebURLResponse::setRemotePort(unsigned short remotePort) {
  m_resourceResponse->setRemotePort(remotePort);
}

long long WebURLResponse::encodedDataLengthForTesting() const {
  return m_resourceResponse->encodedDataLength();
}

void WebURLResponse::addToEncodedDataLength(long long length) {
  m_resourceResponse->addToEncodedDataLength(length);
}

long long WebURLResponse::encodedBodyLengthForTesting() const {
  return m_resourceResponse->encodedBodyLength();
}

void WebURLResponse::addToEncodedBodyLength(long long length) {
  m_resourceResponse->addToEncodedBodyLength(length);
}

long long WebURLResponse::decodedBodyLengthForTesting() const {
  return m_resourceResponse->decodedBodyLength();
}

void WebURLResponse::addToDecodedBodyLength(long long bytes) {
  m_resourceResponse->addToDecodedBodyLength(bytes);
}

WebURLResponse::ExtraData* WebURLResponse::getExtraData() const {
  RefPtr<ResourceResponse::ExtraData> data = m_resourceResponse->getExtraData();
  if (!data)
    return 0;
  return static_cast<ExtraDataContainer*>(data.get())->getExtraData();
}

void WebURLResponse::setExtraData(WebURLResponse::ExtraData* extraData) {
  m_resourceResponse->setExtraData(ExtraDataContainer::create(extraData));
}

void WebURLResponse::appendRedirectResponse(const WebURLResponse& response) {
  m_resourceResponse->appendRedirectResponse(response.toResourceResponse());
}

WebURLResponse::WebURLResponse(ResourceResponse& r) : m_resourceResponse(&r) {}

}  // namespace blink
