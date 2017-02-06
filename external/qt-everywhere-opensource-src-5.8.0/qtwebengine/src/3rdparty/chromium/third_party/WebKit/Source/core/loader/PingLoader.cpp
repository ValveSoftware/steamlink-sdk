/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#include "core/loader/PingLoader.h"

#include "core/dom/Document.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/MixedContentChecker.h"
#include "core/page/Page.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/PtrUtil.h"

namespace blink {

static void finishPingRequestInitialization(ResourceRequest& request, LocalFrame* frame)
{
    request.setRequestContext(WebURLRequest::RequestContextPing);
    frame->document()->fetcher()->context().addAdditionalRequestHeaders(request, FetchSubresource);
    frame->document()->fetcher()->context().setFirstPartyForCookies(request);
}

void PingLoader::loadImage(LocalFrame* frame, const KURL& url)
{
    if (!frame->document()->getSecurityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame, url.getString());
        return;
    }

    ResourceRequest request(url);
    request.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
    finishPingRequestInitialization(request, frame);

    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = FetchInitiatorTypeNames::ping;
    PingLoader::start(frame, request, initiatorInfo);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::sendLinkAuditPing(LocalFrame* frame, const KURL& pingURL, const KURL& destinationURL)
{
    ResourceRequest request(pingURL);
    request.setHTTPMethod(HTTPNames::POST);
    request.setHTTPContentType("text/ping");
    request.setHTTPBody(EncodedFormData::create("PING"));
    request.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
    finishPingRequestInitialization(request, frame);

    RefPtr<SecurityOrigin> pingOrigin = SecurityOrigin::create(pingURL);
    // addAdditionalRequestHeaders() will have added a referrer for same origin requests,
    // but the spec omits the referrer.
    request.clearHTTPReferrer();

    request.setHTTPHeaderField(HTTPNames::Ping_To, AtomicString(destinationURL.getString()));

    // Ping-From follows the same rules as the default referrer beahavior for subresource requests.
    if (!SecurityPolicy::shouldHideReferrer(pingURL, frame->document()->url().getString()))
        request.setHTTPHeaderField(HTTPNames::Ping_From, AtomicString(frame->document()->url().getString()));

    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = FetchInitiatorTypeNames::ping;
    PingLoader::start(frame, request, initiatorInfo);
}

void PingLoader::sendViolationReport(LocalFrame* frame, const KURL& reportURL, PassRefPtr<EncodedFormData> report, ViolationReportType type)
{
    ResourceRequest request(reportURL);
    request.setHTTPMethod(HTTPNames::POST);
    request.setHTTPContentType(type == ContentSecurityPolicyViolationReport ? "application/csp-report" : "application/json");
    request.setHTTPBody(report);
    finishPingRequestInitialization(request, frame);

    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = FetchInitiatorTypeNames::violationreport;
    PingLoader::start(frame, request, initiatorInfo, SecurityOrigin::create(reportURL)->isSameSchemeHostPort(frame->document()->getSecurityOrigin()) ? AllowStoredCredentials : DoNotAllowStoredCredentials);
}

void PingLoader::start(LocalFrame* frame, ResourceRequest& request, const FetchInitiatorInfo& initiatorInfo, StoredCredentials credentialsAllowed)
{
    if (MixedContentChecker::shouldBlockFetch(frame, request, request.url()))
        return;

    // The loader keeps itself alive until it receives a response and disposes itself.
    PingLoader* loader = new PingLoader(frame, request, initiatorInfo, credentialsAllowed);
    ASSERT_UNUSED(loader, loader);
}

PingLoader::PingLoader(LocalFrame* frame, ResourceRequest& request, const FetchInitiatorInfo& initiatorInfo, StoredCredentials credentialsAllowed)
    : LocalFrameLifecycleObserver(frame)
    , m_timeout(this, &PingLoader::timeout)
    , m_url(request.url())
    , m_identifier(createUniqueIdentifier())
    , m_keepAlive(this)
{
    frame->loader().client()->didDispatchPingLoader(request.url());
    frame->document()->fetcher()->context().willStartLoadingResource(m_identifier, request, Resource::Image);
    frame->document()->fetcher()->context().dispatchWillSendRequest(m_identifier, request, ResourceResponse(), initiatorInfo);

    m_loader = wrapUnique(Platform::current()->createURLLoader());
    ASSERT(m_loader);
    WrappedResourceRequest wrappedRequest(request);
    wrappedRequest.setAllowStoredCredentials(credentialsAllowed == AllowStoredCredentials);
    m_loader->loadAsynchronously(wrappedRequest, this);

    // If the server never responds, FrameLoader won't be able to cancel this load and
    // we'll sit here waiting forever. Set a very generous timeout, just in case.
    m_timeout.startOneShot(60000, BLINK_FROM_HERE);
}

PingLoader::~PingLoader()
{
    if (m_loader)
        m_loader->cancel();
}

void PingLoader::dispose()
{
    if (m_loader) {
        m_loader->cancel();
        m_loader = nullptr;
    }
    m_timeout.stop();
    m_keepAlive.clear();
}

void PingLoader::didReceiveResponse(WebURLLoader*, const WebURLResponse& response)
{
    if (LocalFrame* frame = this->frame()) {
        TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(m_identifier, 0, true));
        const ResourceResponse& resourceResponse = response.toResourceResponse();
        InspectorInstrumentation::didReceiveResourceResponse(frame, m_identifier, 0, resourceResponse, 0);
        didFailLoading(frame);
    }
    dispose();
}

void PingLoader::didReceiveData(WebURLLoader*, const char*, int, int)
{
    if (LocalFrame* frame = this->frame()) {
        TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(m_identifier, 0, true));
        didFailLoading(frame);
    }
    dispose();
}

void PingLoader::didFinishLoading(WebURLLoader*, double, int64_t)
{
    if (LocalFrame* frame = this->frame()) {
        TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(m_identifier, 0, true));
        didFailLoading(frame);
    }
    dispose();
}

void PingLoader::didFail(WebURLLoader*, const WebURLError& resourceError)
{
    if (LocalFrame* frame = this->frame()) {
        TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(m_identifier, 0, true));
        didFailLoading(frame);
    }
    dispose();
}

void PingLoader::timeout(Timer<PingLoader>*)
{
    if (LocalFrame* frame = this->frame()) {
        TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(m_identifier, 0, true));
        didFailLoading(frame);
    }
    dispose();
}

void PingLoader::didFailLoading(LocalFrame* frame)
{
    InspectorInstrumentation::didFailLoading(frame, m_identifier, ResourceError::cancelledError(m_url));
    frame->console().didFailLoading(m_identifier, ResourceError::cancelledError(m_url));
}

DEFINE_TRACE(PingLoader)
{
    LocalFrameLifecycleObserver::trace(visitor);
}

} // namespace blink
