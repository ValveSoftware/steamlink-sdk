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

#include "public/platform/WebURLRequest.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebHTTPBody.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebURL.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

class ExtraDataContainer : public ResourceRequest::ExtraData {
 public:
  static PassRefPtr<ExtraDataContainer> create(
      WebURLRequest::ExtraData* extraData) {
    return adoptRef(new ExtraDataContainer(extraData));
  }

  ~ExtraDataContainer() override {}

  WebURLRequest::ExtraData* getExtraData() const { return m_extraData.get(); }

 private:
  explicit ExtraDataContainer(WebURLRequest::ExtraData* extraData)
      : m_extraData(wrapUnique(extraData)) {}

  std::unique_ptr<WebURLRequest::ExtraData> m_extraData;
};

}  // namespace

// The purpose of this struct is to permit allocating a ResourceRequest on the
// heap, which is otherwise disallowed by DISALLOW_NEW_EXCEPT_PLACEMENT_NEW
// annotation on ResourceRequest.
struct WebURLRequest::ResourceRequestContainer {
  ResourceRequestContainer() {}
  explicit ResourceRequestContainer(const ResourceRequest& r)
      : resourceRequest(r) {}

  ResourceRequest resourceRequest;
};

WebURLRequest::~WebURLRequest() {}

WebURLRequest::WebURLRequest()
    : m_ownedResourceRequest(new ResourceRequestContainer()),
      m_resourceRequest(&m_ownedResourceRequest->resourceRequest) {}

WebURLRequest::WebURLRequest(const WebURLRequest& r)
    : m_ownedResourceRequest(
          new ResourceRequestContainer(*r.m_resourceRequest)),
      m_resourceRequest(&m_ownedResourceRequest->resourceRequest) {}

WebURLRequest::WebURLRequest(const WebURL& url) : WebURLRequest() {
  setURL(url);
}

WebURLRequest& WebURLRequest::operator=(const WebURLRequest& r) {
  // Copying subclasses that have different m_resourceRequest ownership
  // semantics via this operator is just not supported.
  DCHECK(m_ownedResourceRequest);
  DCHECK(m_resourceRequest);
  if (&r != this)
    *m_resourceRequest = *r.m_resourceRequest;
  return *this;
}

bool WebURLRequest::isNull() const {
  return m_resourceRequest->isNull();
}

WebURL WebURLRequest::url() const {
  return m_resourceRequest->url();
}

void WebURLRequest::setURL(const WebURL& url) {
  m_resourceRequest->setURL(url);
}

WebURL WebURLRequest::firstPartyForCookies() const {
  return m_resourceRequest->firstPartyForCookies();
}

void WebURLRequest::setFirstPartyForCookies(
    const WebURL& firstPartyForCookies) {
  m_resourceRequest->setFirstPartyForCookies(firstPartyForCookies);
}

WebSecurityOrigin WebURLRequest::requestorOrigin() const {
  return m_resourceRequest->requestorOrigin();
}

void WebURLRequest::setRequestorOrigin(
    const WebSecurityOrigin& requestorOrigin) {
  m_resourceRequest->setRequestorOrigin(requestorOrigin);
}

bool WebURLRequest::allowStoredCredentials() const {
  return m_resourceRequest->allowStoredCredentials();
}

void WebURLRequest::setAllowStoredCredentials(bool allowStoredCredentials) {
  m_resourceRequest->setAllowStoredCredentials(allowStoredCredentials);
}

WebCachePolicy WebURLRequest::getCachePolicy() const {
  return m_resourceRequest->getCachePolicy();
}

void WebURLRequest::setCachePolicy(WebCachePolicy cachePolicy) {
  m_resourceRequest->setCachePolicy(cachePolicy);
}

WebString WebURLRequest::httpMethod() const {
  return m_resourceRequest->httpMethod();
}

void WebURLRequest::setHTTPMethod(const WebString& httpMethod) {
  m_resourceRequest->setHTTPMethod(httpMethod);
}

WebString WebURLRequest::httpHeaderField(const WebString& name) const {
  return m_resourceRequest->httpHeaderField(name);
}

void WebURLRequest::setHTTPHeaderField(const WebString& name,
                                       const WebString& value) {
  RELEASE_ASSERT(!equalIgnoringCase(name, "referer"));
  m_resourceRequest->setHTTPHeaderField(name, value);
}

void WebURLRequest::setHTTPReferrer(const WebString& webReferrer,
                                    WebReferrerPolicy referrerPolicy) {
  // WebString doesn't have the distinction between empty and null. We use
  // the null WTFString for referrer.
  ASSERT(Referrer::noReferrer() == String());
  String referrer =
      webReferrer.isEmpty() ? Referrer::noReferrer() : String(webReferrer);
  m_resourceRequest->setHTTPReferrer(
      Referrer(referrer, static_cast<ReferrerPolicy>(referrerPolicy)));
}

void WebURLRequest::addHTTPHeaderField(const WebString& name,
                                       const WebString& value) {
  m_resourceRequest->addHTTPHeaderField(name, value);
}

void WebURLRequest::clearHTTPHeaderField(const WebString& name) {
  m_resourceRequest->clearHTTPHeaderField(name);
}

void WebURLRequest::visitHTTPHeaderFields(WebHTTPHeaderVisitor* visitor) const {
  const HTTPHeaderMap& map = m_resourceRequest->httpHeaderFields();
  for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
    visitor->visitHeader(it->key, it->value);
}

WebHTTPBody WebURLRequest::httpBody() const {
  // TODO(mkwst): This is wrong, as it means that we're producing the body
  // before any ServiceWorker has a chance to operate, which means we're
  // revealing data to the SW that we ought to be hiding. Baby steps.
  // https://crbug.com/599597
  if (m_resourceRequest->attachedCredential())
    return WebHTTPBody(m_resourceRequest->attachedCredential());
  return WebHTTPBody(m_resourceRequest->httpBody());
}

void WebURLRequest::setHTTPBody(const WebHTTPBody& httpBody) {
  m_resourceRequest->setHTTPBody(httpBody);
}

WebHTTPBody WebURLRequest::attachedCredential() const {
  return WebHTTPBody(m_resourceRequest->attachedCredential());
}

void WebURLRequest::setAttachedCredential(
    const WebHTTPBody& attachedCredential) {
  m_resourceRequest->setAttachedCredential(attachedCredential);
}

bool WebURLRequest::reportUploadProgress() const {
  return m_resourceRequest->reportUploadProgress();
}

void WebURLRequest::setReportUploadProgress(bool reportUploadProgress) {
  m_resourceRequest->setReportUploadProgress(reportUploadProgress);
}

void WebURLRequest::setReportRawHeaders(bool reportRawHeaders) {
  m_resourceRequest->setReportRawHeaders(reportRawHeaders);
}

bool WebURLRequest::reportRawHeaders() const {
  return m_resourceRequest->reportRawHeaders();
}

WebURLRequest::RequestContext WebURLRequest::getRequestContext() const {
  return m_resourceRequest->requestContext();
}

WebURLRequest::FrameType WebURLRequest::getFrameType() const {
  return m_resourceRequest->frameType();
}

WebReferrerPolicy WebURLRequest::referrerPolicy() const {
  return static_cast<WebReferrerPolicy>(m_resourceRequest->getReferrerPolicy());
}

void WebURLRequest::addHTTPOriginIfNeeded(const WebSecurityOrigin& origin) {
  m_resourceRequest->addHTTPOriginIfNeeded(origin.get());
}

bool WebURLRequest::hasUserGesture() const {
  return m_resourceRequest->hasUserGesture();
}

void WebURLRequest::setHasUserGesture(bool hasUserGesture) {
  m_resourceRequest->setHasUserGesture(hasUserGesture);
}

void WebURLRequest::setRequestContext(RequestContext requestContext) {
  m_resourceRequest->setRequestContext(requestContext);
}

void WebURLRequest::setFrameType(FrameType frameType) {
  m_resourceRequest->setFrameType(frameType);
}

int WebURLRequest::requestorID() const {
  return m_resourceRequest->requestorID();
}

void WebURLRequest::setRequestorID(int requestorID) {
  m_resourceRequest->setRequestorID(requestorID);
}

int WebURLRequest::requestorProcessID() const {
  return m_resourceRequest->requestorProcessID();
}

void WebURLRequest::setRequestorProcessID(int requestorProcessID) {
  m_resourceRequest->setRequestorProcessID(requestorProcessID);
}

int WebURLRequest::appCacheHostID() const {
  return m_resourceRequest->appCacheHostID();
}

void WebURLRequest::setAppCacheHostID(int appCacheHostID) {
  m_resourceRequest->setAppCacheHostID(appCacheHostID);
}

bool WebURLRequest::downloadToFile() const {
  return m_resourceRequest->downloadToFile();
}

void WebURLRequest::setDownloadToFile(bool downloadToFile) {
  m_resourceRequest->setDownloadToFile(downloadToFile);
}

bool WebURLRequest::useStreamOnResponse() const {
  return m_resourceRequest->useStreamOnResponse();
}

void WebURLRequest::setUseStreamOnResponse(bool useStreamOnResponse) {
  m_resourceRequest->setUseStreamOnResponse(useStreamOnResponse);
}

WebURLRequest::SkipServiceWorker WebURLRequest::skipServiceWorker() const {
  return m_resourceRequest->skipServiceWorker();
}

void WebURLRequest::setSkipServiceWorker(
    WebURLRequest::SkipServiceWorker skipServiceWorker) {
  m_resourceRequest->setSkipServiceWorker(skipServiceWorker);
}

bool WebURLRequest::shouldResetAppCache() const {
  return m_resourceRequest->shouldResetAppCache();
}

void WebURLRequest::setShouldResetAppCache(bool setShouldResetAppCache) {
  m_resourceRequest->setShouldResetAppCache(setShouldResetAppCache);
}

WebURLRequest::FetchRequestMode WebURLRequest::getFetchRequestMode() const {
  return m_resourceRequest->fetchRequestMode();
}

void WebURLRequest::setFetchRequestMode(WebURLRequest::FetchRequestMode mode) {
  return m_resourceRequest->setFetchRequestMode(mode);
}

WebURLRequest::FetchCredentialsMode WebURLRequest::getFetchCredentialsMode()
    const {
  return m_resourceRequest->fetchCredentialsMode();
}

void WebURLRequest::setFetchCredentialsMode(
    WebURLRequest::FetchCredentialsMode mode) {
  return m_resourceRequest->setFetchCredentialsMode(mode);
}

WebURLRequest::FetchRedirectMode WebURLRequest::getFetchRedirectMode() const {
  return m_resourceRequest->fetchRedirectMode();
}

void WebURLRequest::setFetchRedirectMode(
    WebURLRequest::FetchRedirectMode redirect) {
  return m_resourceRequest->setFetchRedirectMode(redirect);
}

WebURLRequest::LoFiState WebURLRequest::getLoFiState() const {
  return m_resourceRequest->loFiState();
}

void WebURLRequest::setLoFiState(WebURLRequest::LoFiState loFiState) {
  return m_resourceRequest->setLoFiState(loFiState);
}

WebURLRequest::ExtraData* WebURLRequest::getExtraData() const {
  RefPtr<ResourceRequest::ExtraData> data = m_resourceRequest->getExtraData();
  if (!data)
    return 0;
  return static_cast<ExtraDataContainer*>(data.get())->getExtraData();
}

void WebURLRequest::setExtraData(WebURLRequest::ExtraData* extraData) {
  m_resourceRequest->setExtraData(ExtraDataContainer::create(extraData));
}

ResourceRequest& WebURLRequest::toMutableResourceRequest() {
  DCHECK(m_resourceRequest);
  return *m_resourceRequest;
}

WebURLRequest::Priority WebURLRequest::getPriority() const {
  return static_cast<WebURLRequest::Priority>(m_resourceRequest->priority());
}

void WebURLRequest::setPriority(WebURLRequest::Priority priority) {
  m_resourceRequest->setPriority(static_cast<ResourceLoadPriority>(priority));
}

bool WebURLRequest::checkForBrowserSideNavigation() const {
  return m_resourceRequest->checkForBrowserSideNavigation();
}

void WebURLRequest::setCheckForBrowserSideNavigation(bool check) {
  m_resourceRequest->setCheckForBrowserSideNavigation(check);
}

double WebURLRequest::uiStartTime() const {
  return m_resourceRequest->uiStartTime();
}

void WebURLRequest::setUiStartTime(double timeSeconds) {
  m_resourceRequest->setUIStartTime(timeSeconds);
}

bool WebURLRequest::isExternalRequest() const {
  return m_resourceRequest->isExternalRequest();
}

WebURLRequest::LoadingIPCType WebURLRequest::getLoadingIPCType() const {
  if (RuntimeEnabledFeatures::loadingWithMojoEnabled())
    return WebURLRequest::LoadingIPCType::Mojo;
  return WebURLRequest::LoadingIPCType::ChromeIPC;
}

void WebURLRequest::setNavigationStartTime(double navigationStartSeconds) {
  m_resourceRequest->setNavigationStartTime(navigationStartSeconds);
}

WebURLRequest::InputToLoadPerfMetricReportPolicy
WebURLRequest::inputPerfMetricReportPolicy() const {
  return static_cast<WebURLRequest::InputToLoadPerfMetricReportPolicy>(
      m_resourceRequest->inputPerfMetricReportPolicy());
}

void WebURLRequest::setInputPerfMetricReportPolicy(
    WebURLRequest::InputToLoadPerfMetricReportPolicy policy) {
  m_resourceRequest->setInputPerfMetricReportPolicy(
      static_cast<blink::InputToLoadPerfMetricReportPolicy>(policy));
}

const ResourceRequest& WebURLRequest::toResourceRequest() const {
  DCHECK(m_resourceRequest);
  return *m_resourceRequest;
}

WebURLRequest::WebURLRequest(ResourceRequest& r) : m_resourceRequest(&r) {}

}  // namespace blink
