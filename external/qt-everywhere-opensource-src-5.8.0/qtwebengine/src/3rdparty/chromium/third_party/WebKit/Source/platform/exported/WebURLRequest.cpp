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

#include "platform/exported/WebURLRequestPrivate.h"
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
    static PassRefPtr<ExtraDataContainer> create(WebURLRequest::ExtraData* extraData) { return adoptRef(new ExtraDataContainer(extraData)); }

    ~ExtraDataContainer() override {}

    WebURLRequest::ExtraData* getExtraData() const { return m_extraData.get(); }

private:
    explicit ExtraDataContainer(WebURLRequest::ExtraData* extraData)
        : m_extraData(wrapUnique(extraData))
    {
    }

    std::unique_ptr<WebURLRequest::ExtraData> m_extraData;
};

} // namespace

// The standard implementation of WebURLRequestPrivate, which maintains
// ownership of a ResourceRequest instance.
class WebURLRequestPrivateImpl final : public WebURLRequestPrivate {
    USING_FAST_MALLOC(WebURLRequestPrivate);
public:
    WebURLRequestPrivateImpl()
    {
        m_resourceRequest = &m_resourceRequestAllocation;
    }

    WebURLRequestPrivateImpl(const WebURLRequestPrivate* p)
        : m_resourceRequestAllocation(*p->m_resourceRequest)
    {
        m_resourceRequest = &m_resourceRequestAllocation;
    }

    virtual void dispose() { delete this; }

private:
    virtual ~WebURLRequestPrivateImpl() { }

    ResourceRequest m_resourceRequestAllocation;
};

void WebURLRequest::initialize()
{
    assign(new WebURLRequestPrivateImpl());
}

void WebURLRequest::reset()
{
    assign(0);
}

void WebURLRequest::assign(const WebURLRequest& r)
{
    if (&r != this)
        assign(r.m_private ? new WebURLRequestPrivateImpl(r.m_private) : 0);
}

bool WebURLRequest::isNull() const
{
    return !m_private || m_private->m_resourceRequest->isNull();
}

WebURL WebURLRequest::url() const
{
    return m_private->m_resourceRequest->url();
}

void WebURLRequest::setURL(const WebURL& url)
{
    m_private->m_resourceRequest->setURL(url);
}

WebURL WebURLRequest::firstPartyForCookies() const
{
    return m_private->m_resourceRequest->firstPartyForCookies();
}

void WebURLRequest::setFirstPartyForCookies(const WebURL& firstPartyForCookies)
{
    m_private->m_resourceRequest->setFirstPartyForCookies(firstPartyForCookies);
}

WebSecurityOrigin WebURLRequest::requestorOrigin() const
{
    return m_private->m_resourceRequest->requestorOrigin();
}

void WebURLRequest::setRequestorOrigin(const WebSecurityOrigin& requestorOrigin)
{
    m_private->m_resourceRequest->setRequestorOrigin(requestorOrigin);
}

bool WebURLRequest::allowStoredCredentials() const
{
    return m_private->m_resourceRequest->allowStoredCredentials();
}

void WebURLRequest::setAllowStoredCredentials(bool allowStoredCredentials)
{
    m_private->m_resourceRequest->setAllowStoredCredentials(allowStoredCredentials);
}

WebCachePolicy WebURLRequest::getCachePolicy() const
{
    return m_private->m_resourceRequest->getCachePolicy();
}

void WebURLRequest::setCachePolicy(WebCachePolicy cachePolicy)
{
    m_private->m_resourceRequest->setCachePolicy(cachePolicy);
}

WebString WebURLRequest::httpMethod() const
{
    return m_private->m_resourceRequest->httpMethod();
}

void WebURLRequest::setHTTPMethod(const WebString& httpMethod)
{
    m_private->m_resourceRequest->setHTTPMethod(httpMethod);
}

WebString WebURLRequest::httpHeaderField(const WebString& name) const
{
    return m_private->m_resourceRequest->httpHeaderField(name);
}

void WebURLRequest::setHTTPHeaderField(const WebString& name, const WebString& value)
{
    RELEASE_ASSERT(!equalIgnoringCase(name, "referer"));
    m_private->m_resourceRequest->setHTTPHeaderField(name, value);
}

void WebURLRequest::setHTTPReferrer(const WebString& webReferrer, WebReferrerPolicy referrerPolicy)
{
    // WebString doesn't have the distinction between empty and null. We use
    // the null WTFString for referrer.
    ASSERT(Referrer::noReferrer() == String());
    String referrer = webReferrer.isEmpty() ? Referrer::noReferrer() : String(webReferrer);
    m_private->m_resourceRequest->setHTTPReferrer(Referrer(referrer, static_cast<ReferrerPolicy>(referrerPolicy)));
}

void WebURLRequest::addHTTPHeaderField(const WebString& name, const WebString& value)
{
    m_private->m_resourceRequest->addHTTPHeaderField(name, value);
}

void WebURLRequest::clearHTTPHeaderField(const WebString& name)
{
    m_private->m_resourceRequest->clearHTTPHeaderField(name);
}

void WebURLRequest::visitHTTPHeaderFields(WebHTTPHeaderVisitor* visitor) const
{
    const HTTPHeaderMap& map = m_private->m_resourceRequest->httpHeaderFields();
    for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
        visitor->visitHeader(it->key, it->value);
}

WebHTTPBody WebURLRequest::httpBody() const
{
    // TODO(mkwst): This is wrong, as it means that we're producing the body
    // before any ServiceWorker has a chance to operate, which means we're
    // revealing data to the SW that we ought to be hiding. Baby steps.
    // https://crbug.com/599597
    if (m_private->m_resourceRequest->attachedCredential())
        return WebHTTPBody(m_private->m_resourceRequest->attachedCredential());
    return WebHTTPBody(m_private->m_resourceRequest->httpBody());
}

void WebURLRequest::setHTTPBody(const WebHTTPBody& httpBody)
{
    m_private->m_resourceRequest->setHTTPBody(httpBody);
}

WebHTTPBody WebURLRequest::attachedCredential() const
{
    return WebHTTPBody(m_private->m_resourceRequest->attachedCredential());
}

void WebURLRequest::setAttachedCredential(const WebHTTPBody& attachedCredential)
{
    m_private->m_resourceRequest->setAttachedCredential(attachedCredential);
}

bool WebURLRequest::reportUploadProgress() const
{
    return m_private->m_resourceRequest->reportUploadProgress();
}

void WebURLRequest::setReportUploadProgress(bool reportUploadProgress)
{
    m_private->m_resourceRequest->setReportUploadProgress(reportUploadProgress);
}

void WebURLRequest::setReportRawHeaders(bool reportRawHeaders)
{
    m_private->m_resourceRequest->setReportRawHeaders(reportRawHeaders);
}

bool WebURLRequest::reportRawHeaders() const
{
    return m_private->m_resourceRequest->reportRawHeaders();
}

WebURLRequest::RequestContext WebURLRequest::getRequestContext() const
{
    return m_private->m_resourceRequest->requestContext();
}

WebURLRequest::FrameType WebURLRequest::getFrameType() const
{
    return m_private->m_resourceRequest->frameType();
}

WebReferrerPolicy WebURLRequest::referrerPolicy() const
{
    return static_cast<WebReferrerPolicy>(m_private->m_resourceRequest->getReferrerPolicy());
}

void WebURLRequest::addHTTPOriginIfNeeded(const WebString& origin)
{
    m_private->m_resourceRequest->addHTTPOriginIfNeeded(WebSecurityOrigin::createFromString(origin));
}

bool WebURLRequest::hasUserGesture() const
{
    return m_private->m_resourceRequest->hasUserGesture();
}

void WebURLRequest::setHasUserGesture(bool hasUserGesture)
{
    m_private->m_resourceRequest->setHasUserGesture(hasUserGesture);
}

void WebURLRequest::setRequestContext(RequestContext requestContext)
{
    m_private->m_resourceRequest->setRequestContext(requestContext);
}

void WebURLRequest::setFrameType(FrameType frameType)
{
    m_private->m_resourceRequest->setFrameType(frameType);
}

int WebURLRequest::requestorID() const
{
    return m_private->m_resourceRequest->requestorID();
}

void WebURLRequest::setRequestorID(int requestorID)
{
    m_private->m_resourceRequest->setRequestorID(requestorID);
}

int WebURLRequest::requestorProcessID() const
{
    return m_private->m_resourceRequest->requestorProcessID();
}

void WebURLRequest::setRequestorProcessID(int requestorProcessID)
{
    m_private->m_resourceRequest->setRequestorProcessID(requestorProcessID);
}

int WebURLRequest::appCacheHostID() const
{
    return m_private->m_resourceRequest->appCacheHostID();
}

void WebURLRequest::setAppCacheHostID(int appCacheHostID)
{
    m_private->m_resourceRequest->setAppCacheHostID(appCacheHostID);
}

bool WebURLRequest::downloadToFile() const
{
    return m_private->m_resourceRequest->downloadToFile();
}

void WebURLRequest::setDownloadToFile(bool downloadToFile)
{
    m_private->m_resourceRequest->setDownloadToFile(downloadToFile);
}

bool WebURLRequest::useStreamOnResponse() const
{
    return m_private->m_resourceRequest->useStreamOnResponse();
}

void WebURLRequest::setUseStreamOnResponse(bool useStreamOnResponse)
{
    m_private->m_resourceRequest->setUseStreamOnResponse(useStreamOnResponse);
}

WebURLRequest::SkipServiceWorker WebURLRequest::skipServiceWorker() const
{
    return m_private->m_resourceRequest->skipServiceWorker();
}

void WebURLRequest::setSkipServiceWorker(WebURLRequest::SkipServiceWorker skipServiceWorker)
{
    m_private->m_resourceRequest->setSkipServiceWorker(skipServiceWorker);
}

bool WebURLRequest::shouldResetAppCache() const
{
    return m_private->m_resourceRequest->shouldResetAppCache();
}

void WebURLRequest::setShouldResetAppCache(bool setShouldResetAppCache)
{
    m_private->m_resourceRequest->setShouldResetAppCache(setShouldResetAppCache);
}

WebURLRequest::FetchRequestMode WebURLRequest::getFetchRequestMode() const
{
    return m_private->m_resourceRequest->fetchRequestMode();
}

void WebURLRequest::setFetchRequestMode(WebURLRequest::FetchRequestMode mode)
{
    return m_private->m_resourceRequest->setFetchRequestMode(mode);
}

WebURLRequest::FetchCredentialsMode WebURLRequest::getFetchCredentialsMode() const
{
    return m_private->m_resourceRequest->fetchCredentialsMode();
}

void WebURLRequest::setFetchCredentialsMode(WebURLRequest::FetchCredentialsMode mode)
{
    return m_private->m_resourceRequest->setFetchCredentialsMode(mode);
}

WebURLRequest::FetchRedirectMode WebURLRequest::getFetchRedirectMode() const
{
    return m_private->m_resourceRequest->fetchRedirectMode();
}

void WebURLRequest::setFetchRedirectMode(WebURLRequest::FetchRedirectMode redirect)
{
    return m_private->m_resourceRequest->setFetchRedirectMode(redirect);
}

WebURLRequest::LoFiState WebURLRequest::getLoFiState() const
{
    return m_private->m_resourceRequest->loFiState();
}

void WebURLRequest::setLoFiState(WebURLRequest::LoFiState loFiState)
{
    return m_private->m_resourceRequest->setLoFiState(loFiState);
}

WebURLRequest::ExtraData* WebURLRequest::getExtraData() const
{
    RefPtr<ResourceRequest::ExtraData> data = m_private->m_resourceRequest->getExtraData();
    if (!data)
        return 0;
    return static_cast<ExtraDataContainer*>(data.get())->getExtraData();
}

void WebURLRequest::setExtraData(WebURLRequest::ExtraData* extraData)
{
    m_private->m_resourceRequest->setExtraData(ExtraDataContainer::create(extraData));
}

ResourceRequest& WebURLRequest::toMutableResourceRequest()
{
    ASSERT(m_private);
    ASSERT(m_private->m_resourceRequest);

    return *m_private->m_resourceRequest;
}

WebURLRequest::Priority WebURLRequest::getPriority() const
{
    return static_cast<WebURLRequest::Priority>(
        m_private->m_resourceRequest->priority());
}

void WebURLRequest::setPriority(WebURLRequest::Priority priority)
{
    m_private->m_resourceRequest->setPriority(
        static_cast<ResourceLoadPriority>(priority));
}

bool WebURLRequest::checkForBrowserSideNavigation() const
{
    return m_private->m_resourceRequest->checkForBrowserSideNavigation();
}

void WebURLRequest::setCheckForBrowserSideNavigation(bool check)
{
    m_private->m_resourceRequest->setCheckForBrowserSideNavigation(check);
}

double WebURLRequest::uiStartTime() const
{
    return m_private->m_resourceRequest->uiStartTime();
}

void WebURLRequest::setUiStartTime(double time)
{
    m_private->m_resourceRequest->setUIStartTime(time);
}

bool WebURLRequest::isExternalRequest() const
{
    return m_private->m_resourceRequest->isExternalRequest();
}

WebURLRequest::InputToLoadPerfMetricReportPolicy WebURLRequest::inputPerfMetricReportPolicy() const
{
    return static_cast<WebURLRequest::InputToLoadPerfMetricReportPolicy>(
        m_private->m_resourceRequest->inputPerfMetricReportPolicy());
}

void WebURLRequest::setInputPerfMetricReportPolicy(
    WebURLRequest::InputToLoadPerfMetricReportPolicy policy)
{
    m_private->m_resourceRequest->setInputPerfMetricReportPolicy(
        static_cast<blink::InputToLoadPerfMetricReportPolicy>(policy));
}

const ResourceRequest& WebURLRequest::toResourceRequest() const
{
    ASSERT(m_private);
    ASSERT(m_private->m_resourceRequest);

    return *m_private->m_resourceRequest;
}

void WebURLRequest::assign(WebURLRequestPrivate* p)
{
    // Subclasses may call this directly so a self-assignment check is needed
    // here as well as in the public assign method.
    if (m_private == p)
        return;
    if (m_private)
        m_private->dispose();
    m_private = p;
}

} // namespace blink
