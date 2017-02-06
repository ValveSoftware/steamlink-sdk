/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#include "platform/network/ResourceRequest.h"

#include "platform/HTTPNames.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/NetworkUtils.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

double ResourceRequest::s_defaultTimeoutInterval = INT_MAX;

ResourceRequest::ResourceRequest(CrossThreadResourceRequestData* data)
    : ResourceRequest()
{
    setURL(data->m_url);
    setCachePolicy(data->m_cachePolicy);
    setTimeoutInterval(data->m_timeoutInterval);
    setFirstPartyForCookies(data->m_firstPartyForCookies);
    setRequestorOrigin(data->m_requestorOrigin);
    setHTTPMethod(AtomicString(data->m_httpMethod));
    setPriority(data->m_priority, data->m_intraPriorityValue);

    m_httpHeaderFields.adopt(std::move(data->m_httpHeaders));

    setHTTPBody(data->m_httpBody);
    setAttachedCredential(data->m_attachedCredential);
    setAllowStoredCredentials(data->m_allowStoredCredentials);
    setReportUploadProgress(data->m_reportUploadProgress);
    setHasUserGesture(data->m_hasUserGesture);
    setDownloadToFile(data->m_downloadToFile);
    setUseStreamOnResponse(data->m_useStreamOnResponse);
    setSkipServiceWorker(data->m_skipServiceWorker);
    setShouldResetAppCache(data->m_shouldResetAppCache);
    setRequestorID(data->m_requestorID);
    setRequestorProcessID(data->m_requestorProcessID);
    setAppCacheHostID(data->m_appCacheHostID);
    setRequestContext(data->m_requestContext);
    setFrameType(data->m_frameType);
    setFetchRequestMode(data->m_fetchRequestMode);
    setFetchCredentialsMode(data->m_fetchCredentialsMode);
    setFetchRedirectMode(data->m_fetchRedirectMode);
    setLoFiState(data->m_loFiState);
    m_referrerPolicy = data->m_referrerPolicy;
    m_didSetHTTPReferrer = data->m_didSetHTTPReferrer;
    m_checkForBrowserSideNavigation = data->m_checkForBrowserSideNavigation;
    m_uiStartTime = data->m_uiStartTime;
    m_isExternalRequest = data->m_isExternalRequest;
    m_inputPerfMetricReportPolicy = data->m_inputPerfMetricReportPolicy;
    m_redirectStatus = data->m_redirectStatus;
}

std::unique_ptr<CrossThreadResourceRequestData> ResourceRequest::copyData() const
{
    std::unique_ptr<CrossThreadResourceRequestData> data = wrapUnique(new CrossThreadResourceRequestData());
    data->m_url = url().copy();
    data->m_cachePolicy = getCachePolicy();
    data->m_timeoutInterval = timeoutInterval();
    data->m_firstPartyForCookies = firstPartyForCookies().copy();
    data->m_requestorOrigin = requestorOrigin() ? requestorOrigin()->isolatedCopy() : nullptr;
    data->m_httpMethod = httpMethod().getString().isolatedCopy();
    data->m_httpHeaders = httpHeaderFields().copyData();
    data->m_priority = priority();
    data->m_intraPriorityValue = m_intraPriorityValue;

    if (m_httpBody)
        data->m_httpBody = m_httpBody->deepCopy();
    if (m_attachedCredential)
        data->m_attachedCredential = m_attachedCredential->deepCopy();
    data->m_allowStoredCredentials = m_allowStoredCredentials;
    data->m_reportUploadProgress = m_reportUploadProgress;
    data->m_hasUserGesture = m_hasUserGesture;
    data->m_downloadToFile = m_downloadToFile;
    data->m_useStreamOnResponse = m_useStreamOnResponse;
    data->m_skipServiceWorker = m_skipServiceWorker;
    data->m_shouldResetAppCache = m_shouldResetAppCache;
    data->m_requestorID = m_requestorID;
    data->m_requestorProcessID = m_requestorProcessID;
    data->m_appCacheHostID = m_appCacheHostID;
    data->m_requestContext = m_requestContext;
    data->m_frameType = m_frameType;
    data->m_fetchRequestMode = m_fetchRequestMode;
    data->m_fetchCredentialsMode = m_fetchCredentialsMode;
    data->m_fetchRedirectMode = m_fetchRedirectMode;
    data->m_loFiState = m_loFiState;
    data->m_referrerPolicy = m_referrerPolicy;
    data->m_didSetHTTPReferrer = m_didSetHTTPReferrer;
    data->m_checkForBrowserSideNavigation = m_checkForBrowserSideNavigation;
    data->m_uiStartTime = m_uiStartTime;
    data->m_isExternalRequest = m_isExternalRequest;
    data->m_inputPerfMetricReportPolicy = m_inputPerfMetricReportPolicy;
    data->m_redirectStatus = m_redirectStatus;
    return data;
}

bool ResourceRequest::isEmpty() const
{
    return m_url.isEmpty();
}

bool ResourceRequest::isNull() const
{
    return m_url.isNull();
}

const KURL& ResourceRequest::url() const
{
    return m_url;
}

void ResourceRequest::setURL(const KURL& url)
{
    m_url = url;
}

void ResourceRequest::removeCredentials()
{
    if (m_url.user().isEmpty() && m_url.pass().isEmpty())
        return;

    m_url.setUser(String());
    m_url.setPass(String());
}

WebCachePolicy ResourceRequest::getCachePolicy() const
{
    return m_cachePolicy;
}

void ResourceRequest::setCachePolicy(WebCachePolicy cachePolicy)
{
    m_cachePolicy = cachePolicy;
}

double ResourceRequest::timeoutInterval() const
{
    return m_timeoutInterval;
}

void ResourceRequest::setTimeoutInterval(double timeoutInterval)
{
    m_timeoutInterval = timeoutInterval;
}

const KURL& ResourceRequest::firstPartyForCookies() const
{
    return m_firstPartyForCookies;
}

void ResourceRequest::setFirstPartyForCookies(const KURL& firstPartyForCookies)
{
    m_firstPartyForCookies = firstPartyForCookies;
}

PassRefPtr<SecurityOrigin> ResourceRequest::requestorOrigin() const
{
    return m_requestorOrigin;
}

void ResourceRequest::setRequestorOrigin(PassRefPtr<SecurityOrigin> requestorOrigin)
{
    m_requestorOrigin = requestorOrigin;
}

const AtomicString& ResourceRequest::httpMethod() const
{
    return m_httpMethod;
}

void ResourceRequest::setHTTPMethod(const AtomicString& httpMethod)
{
    m_httpMethod = httpMethod;
}

const HTTPHeaderMap& ResourceRequest::httpHeaderFields() const
{
    return m_httpHeaderFields;
}

const AtomicString& ResourceRequest::httpHeaderField(const AtomicString& name) const
{
    return m_httpHeaderFields.get(name);
}

void ResourceRequest::setHTTPHeaderField(const AtomicString& name, const AtomicString& value)
{
    m_httpHeaderFields.set(name, value);
}

void ResourceRequest::setHTTPReferrer(const Referrer& referrer)
{
    if (referrer.referrer.isEmpty())
        m_httpHeaderFields.remove(HTTPNames::Referer);
    else
        setHTTPHeaderField(HTTPNames::Referer, referrer.referrer);
    m_referrerPolicy = referrer.referrerPolicy;
    m_didSetHTTPReferrer = true;
}

void ResourceRequest::clearHTTPReferrer()
{
    m_httpHeaderFields.remove(HTTPNames::Referer);
    m_referrerPolicy = ReferrerPolicyDefault;
    m_didSetHTTPReferrer = false;
}

void ResourceRequest::setHTTPOrigin(PassRefPtr<SecurityOrigin> origin)
{
    setHTTPHeaderField(HTTPNames::Origin, origin->toAtomicString());
    if (origin->hasSuborigin())
        setHTTPHeaderField(HTTPNames::Suborigin, AtomicString(origin->suborigin()->name()));
}

void ResourceRequest::clearHTTPOrigin()
{
    m_httpHeaderFields.remove(HTTPNames::Origin);
    m_httpHeaderFields.remove(HTTPNames::Suborigin);
}

void ResourceRequest::addHTTPOriginIfNeeded(PassRefPtr<SecurityOrigin> origin)
{
    if (!httpOrigin().isEmpty())
        return; // Request already has an Origin header.

    // Don't send an Origin header for GET or HEAD to avoid privacy issues.
    // For example, if an intranet page has a hyperlink to an external web
    // site, we don't want to include the Origin of the request because it
    // will leak the internal host name. Similar privacy concerns have lead
    // to the widespread suppression of the Referer header at the network
    // layer.
    if (httpMethod() == HTTPNames::GET || httpMethod() == HTTPNames::HEAD)
        return;

    // For non-GET and non-HEAD methods, always send an Origin header so the
    // server knows we support this feature.

    AtomicString originString = origin->toAtomicString();
    if (originString.isEmpty()) {
        // If we don't know what origin header to attach, we attach the value
        // for an empty origin.
        setHTTPOrigin(SecurityOrigin::createUnique());
        return;
    }
    setHTTPOrigin(origin);
}

void ResourceRequest::clearHTTPUserAgent()
{
    m_httpHeaderFields.remove(HTTPNames::User_Agent);
}

EncodedFormData* ResourceRequest::httpBody() const
{
    return m_httpBody.get();
}

void ResourceRequest::setHTTPBody(PassRefPtr<EncodedFormData> httpBody)
{
    m_httpBody = httpBody;
}

EncodedFormData* ResourceRequest::attachedCredential() const
{
    return m_attachedCredential.get();
}

void ResourceRequest::setAttachedCredential(PassRefPtr<EncodedFormData> attachedCredential)
{
    m_attachedCredential = attachedCredential;
}

bool ResourceRequest::allowStoredCredentials() const
{
    return m_allowStoredCredentials;
}

void ResourceRequest::setAllowStoredCredentials(bool allowCredentials)
{
    m_allowStoredCredentials = allowCredentials;
}

ResourceLoadPriority ResourceRequest::priority() const
{
    return m_priority;
}

void ResourceRequest::setPriority(ResourceLoadPriority priority, int intraPriorityValue)
{
    m_priority = priority;
    m_intraPriorityValue = intraPriorityValue;
}

void ResourceRequest::addHTTPHeaderField(const AtomicString& name, const AtomicString& value)
{
    HTTPHeaderMap::AddResult result = m_httpHeaderFields.add(name, value);
    if (!result.isNewEntry)
        result.storedValue->value = result.storedValue->value + ',' + value;
}

void ResourceRequest::addHTTPHeaderFields(const HTTPHeaderMap& headerFields)
{
    HTTPHeaderMap::const_iterator end = headerFields.end();
    for (HTTPHeaderMap::const_iterator it = headerFields.begin(); it != end; ++it)
        addHTTPHeaderField(it->key, it->value);
}

void ResourceRequest::clearHTTPHeaderField(const AtomicString& name)
{
    m_httpHeaderFields.remove(name);
}

void ResourceRequest::setExternalRequestStateFromRequestorAddressSpace(WebAddressSpace requestorSpace)
{
    static_assert(WebAddressSpaceLocal < WebAddressSpacePrivate, "Local is inside Private");
    static_assert(WebAddressSpaceLocal < WebAddressSpacePublic, "Local is inside Public");
    static_assert(WebAddressSpacePrivate < WebAddressSpacePublic, "Private is inside Public");

    // TODO(mkwst): This only checks explicit IP addresses. We'll have to move all this up to //net and //content in
    // order to have any real impact on gateway attacks. That turns out to be a TON of work. https://crbug.com/378566
    if (!RuntimeEnabledFeatures::corsRFC1918Enabled()) {
        m_isExternalRequest = false;
        return;
    }

    WebAddressSpace targetSpace = WebAddressSpacePublic;
    if (NetworkUtils::isReservedIPAddress(m_url.host()))
        targetSpace = WebAddressSpacePrivate;
    if (SecurityOrigin::create(m_url)->isLocalhost())
        targetSpace = WebAddressSpaceLocal;

    m_isExternalRequest = requestorSpace > targetSpace;
}

bool ResourceRequest::isConditional() const
{
    return (m_httpHeaderFields.contains(HTTPNames::If_Match)
        || m_httpHeaderFields.contains(HTTPNames::If_Modified_Since)
        || m_httpHeaderFields.contains(HTTPNames::If_None_Match)
        || m_httpHeaderFields.contains(HTTPNames::If_Range)
        || m_httpHeaderFields.contains(HTTPNames::If_Unmodified_Since));
}

void ResourceRequest::setHasUserGesture(bool hasUserGesture)
{
    m_hasUserGesture |= hasUserGesture;
}

const CacheControlHeader& ResourceRequest::cacheControlHeader() const
{
    if (!m_cacheControlHeaderCache.parsed)
        m_cacheControlHeaderCache = parseCacheControlDirectives(m_httpHeaderFields.get(HTTPNames::Cache_Control), m_httpHeaderFields.get(HTTPNames::Pragma));
    return m_cacheControlHeaderCache;
}

bool ResourceRequest::cacheControlContainsNoCache() const
{
    return cacheControlHeader().containsNoCache;
}

bool ResourceRequest::cacheControlContainsNoStore() const
{
    return cacheControlHeader().containsNoStore;
}

bool ResourceRequest::hasCacheValidatorFields() const
{
    return !m_httpHeaderFields.get(HTTPNames::Last_Modified).isEmpty() || !m_httpHeaderFields.get(HTTPNames::ETag).isEmpty();
}

void ResourceRequest::initialize(const KURL& url)
{
    m_url = url;
    m_cachePolicy = WebCachePolicy::UseProtocolCachePolicy;
    m_timeoutInterval = s_defaultTimeoutInterval;
    m_httpMethod = HTTPNames::GET;
    m_allowStoredCredentials = true;
    m_reportUploadProgress = false;
    m_reportRawHeaders = false;
    m_hasUserGesture = false;
    m_downloadToFile = false;
    m_useStreamOnResponse = false;
    m_skipServiceWorker = WebURLRequest::SkipServiceWorker::None;
    m_shouldResetAppCache = false;
    m_priority = ResourceLoadPriorityLowest;
    m_intraPriorityValue = 0;
    m_requestorID = 0;
    m_requestorProcessID = 0;
    m_appCacheHostID = 0;
    m_requestContext = WebURLRequest::RequestContextUnspecified;
    m_frameType = WebURLRequest::FrameTypeNone;
    m_fetchRequestMode = WebURLRequest::FetchRequestModeNoCORS;
    m_fetchCredentialsMode = WebURLRequest::FetchCredentialsModeInclude;
    m_fetchRedirectMode = WebURLRequest::FetchRedirectModeFollow;
    m_referrerPolicy = ReferrerPolicyDefault;
    m_loFiState = WebURLRequest::LoFiUnspecified;
    m_didSetHTTPReferrer = false;
    m_checkForBrowserSideNavigation = true;
    m_uiStartTime = 0;
    m_isExternalRequest = false;
    m_inputPerfMetricReportPolicy = InputToLoadPerfMetricReportPolicy::NoReport;
    m_redirectStatus = RedirectStatus::NoRedirect;
    m_requestorOrigin = SecurityOrigin::createUnique();
}

} // namespace blink
