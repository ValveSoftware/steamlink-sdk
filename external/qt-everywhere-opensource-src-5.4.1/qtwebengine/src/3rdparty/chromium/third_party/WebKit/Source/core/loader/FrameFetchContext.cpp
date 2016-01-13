/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/loader/FrameFetchContext.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace WebCore {

FrameFetchContext::FrameFetchContext(LocalFrame* frame)
    : m_frame(frame)
{
}

void FrameFetchContext::reportLocalLoadFailed(const KURL& url)
{
    FrameLoader::reportLocalLoadFailed(m_frame, url.elidedString());
}

void FrameFetchContext::addAdditionalRequestHeaders(Document* document, ResourceRequest& request, FetchResourceType type)
{
    bool isMainResource = type == FetchMainResource;
    if (!isMainResource) {
        String outgoingReferrer;
        String outgoingOrigin;
        if (request.httpReferrer().isNull()) {
            outgoingReferrer = document->outgoingReferrer();
            outgoingOrigin = document->outgoingOrigin();
        } else {
            outgoingReferrer = request.httpReferrer();
            outgoingOrigin = SecurityOrigin::createFromString(outgoingReferrer)->toString();
        }

        outgoingReferrer = SecurityPolicy::generateReferrerHeader(document->referrerPolicy(), request.url(), outgoingReferrer);
        if (outgoingReferrer.isEmpty())
            request.clearHTTPReferrer();
        else if (!request.httpReferrer())
            request.setHTTPReferrer(Referrer(outgoingReferrer, document->referrerPolicy()));

        FrameLoader::addHTTPOriginIfNeeded(request, AtomicString(outgoingOrigin));
    }

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    m_frame->loader().applyUserAgent(request);

    // Default to sending an empty Origin header if one hasn't been set yet.
    FrameLoader::addHTTPOriginIfNeeded(request, nullAtom);
}

void FrameFetchContext::setFirstPartyForCookies(ResourceRequest& request)
{
    if (m_frame->tree().top()->isLocalFrame())
        request.setFirstPartyForCookies(toLocalFrame(m_frame->tree().top())->document()->firstPartyForCookies());
}

CachePolicy FrameFetchContext::cachePolicy(Document* document) const
{
    if (document && document->loadEventFinished())
        return CachePolicyVerify;

    FrameLoadType loadType = m_frame->loader().loadType();
    if (loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;

    Frame* parentFrame = m_frame->tree().parent();
    if (parentFrame && parentFrame->isLocalFrame()) {
        CachePolicy parentCachePolicy = toLocalFrame(parentFrame)->loader().fetchContext().cachePolicy(toLocalFrame(parentFrame)->document());
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }

    if (loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    DocumentLoader* loader = document ? document->loader() : 0;
    if (loader && loader->request().cachePolicy() == ReturnCacheDataElseLoad)
        return CachePolicyHistoryBuffer;
    return CachePolicyVerify;

}

// FIXME(http://crbug.com/274173):
// |loader| can be null if the resource is loaded from imported document.
// This means inspector, which uses DocumentLoader as an grouping entity,
// cannot see imported documents.
inline DocumentLoader* FrameFetchContext::ensureLoader(DocumentLoader* loader)
{
    return loader ? loader : m_frame->loader().documentLoader();
}

void FrameFetchContext::dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    m_frame->loader().client()->dispatchDidChangeResourcePriority(identifier, loadPriority, intraPriorityValue);
}

void FrameFetchContext::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    m_frame->loader().applyUserAgent(request);
    m_frame->loader().client()->dispatchWillSendRequest(loader, identifier, request, redirectResponse);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceSendRequest", "data", InspectorSendRequestEvent::data(identifier, m_frame, request));
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"), "CallStack", "stack", InspectorCallStackEvent::currentCallStack());
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::willSendRequest(m_frame, identifier, ensureLoader(loader), request, redirectResponse, initiatorInfo);
}

void FrameFetchContext::dispatchDidLoadResourceFromMemoryCache(const ResourceRequest& request, const ResourceResponse& response)
{
    m_frame->loader().client()->dispatchDidLoadResourceFromMemoryCache(request, response);
}

void FrameFetchContext::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r, ResourceLoader* resourceLoader)
{
    m_frame->loader().progress().incrementProgress(identifier, r);
    m_frame->loader().client()->dispatchDidReceiveResponse(loader, identifier, r);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceReceiveResponse", "data", InspectorReceiveResponseEvent::data(identifier, m_frame, r));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didReceiveResourceResponse(m_frame, identifier, ensureLoader(loader), r, resourceLoader);
}

void FrameFetchContext::dispatchDidReceiveData(DocumentLoader*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    m_frame->loader().progress().incrementProgress(identifier, data, dataLength);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceReceivedData", "data", InspectorReceiveDataEvent::data(identifier, m_frame, encodedDataLength));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didReceiveData(m_frame, identifier, data, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidDownloadData(DocumentLoader*, unsigned long identifier, int dataLength, int encodedDataLength)
{
    m_frame->loader().progress().incrementProgress(identifier, 0, dataLength);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceReceivedData", "data", InspectorReceiveDataEvent::data(identifier, m_frame, encodedDataLength));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didReceiveData(m_frame, identifier, 0, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier, double finishTime, int64_t encodedDataLength)
{
    m_frame->loader().progress().completeProgress(identifier);
    m_frame->loader().client()->dispatchDidFinishLoading(loader, identifier);

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceFinish", "data", InspectorResourceFinishEvent::data(identifier, finishTime, false));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didFinishLoading(m_frame, identifier, ensureLoader(loader), finishTime, encodedDataLength);
}

void FrameFetchContext::dispatchDidFail(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    m_frame->loader().progress().completeProgress(identifier);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceFinish", "data", InspectorResourceFinishEvent::data(identifier, 0, true));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didFailLoading(m_frame, identifier, error);
}

void FrameFetchContext::sendRemainingDelegateMessages(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response, int dataLength)
{
    if (!response.isNull())
        dispatchDidReceiveResponse(ensureLoader(loader), identifier, response);

    if (dataLength > 0)
        dispatchDidReceiveData(ensureLoader(loader), identifier, 0, dataLength, 0);

    dispatchDidFinishLoading(ensureLoader(loader), identifier, 0, 0);
}

} // namespace WebCore
