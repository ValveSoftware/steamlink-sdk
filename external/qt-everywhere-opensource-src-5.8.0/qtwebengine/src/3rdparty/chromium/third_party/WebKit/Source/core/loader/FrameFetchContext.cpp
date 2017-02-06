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

#include "core/loader/FrameFetchContext.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/loader/PingLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/page/Page.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/Logging.h"
#include "platform/TracedValue.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include <algorithm>
#include <memory>

namespace blink {

namespace {

void emitWarningForDocWriteScripts(const String& url, Document& document)
{
    String message = "A Parser-blocking, cross-origin script, " + url + ", is invoked via document.write. This may be blocked by the browser if the device has poor network connectivity.";
    document.addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
    WTFLogAlways("%s", message.utf8().data());
}

bool shouldDisallowFetchForMainFrameScript(const ResourceRequest& request, FetchRequest::DeferOption defer, Document& document)
{
    // Only scripts inserted via document.write are candidates for having their
    // fetch disallowed.
    if (!document.isInDocumentWrite())
        return false;

    if (!document.settings())
        return false;

    if (!document.frame())
        return false;

    // Only block synchronously loaded (parser blocking) scripts.
    if (defer != FetchRequest::NoDefer)
        return false;

    if (!request.url().protocolIsInHTTPFamily())
        return false;

    // Avoid blocking same origin scripts, as they may be used to render main
    // page content, whereas cross-origin scripts inserted via document.write
    // are likely to be third party content.
    if (request.url().host() == document.getSecurityOrigin()->domain())
        return false;

    emitWarningForDocWriteScripts(request.url().getString(), document);

    // Do not block scripts if it is a page reload. This is to enable pages to
    // recover if blocking of a script is leading to a page break and the user
    // reloads the page.
    const FrameLoadType loadType = document.frame()->loader().loadType();
    const bool isReload = loadType == FrameLoadTypeReload || loadType == FrameLoadTypeReloadBypassingCache || loadType == FrameLoadTypeReloadMainResource;
    if (isReload) {
        // Recording this metric since an increase in number of reloads for pages
        // where a script was blocked could be indicative of a page break.
        document.loader()->didObserveLoadingBehavior(WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlockReload);
        return false;
    }

    // Add the metadata that this page has scripts inserted via document.write
    // that are eligible for blocking. Note that if there are multiple scripts
    // the flag will be conveyed to the browser process only once.
    document.loader()->didObserveLoadingBehavior(WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlock);

    const bool isSlowConnection = networkStateNotifier().connectionType() == WebConnectionTypeCellular2G;
    const bool disallowFetch = document.settings()->disallowFetchForDocWrittenScriptsInMainFrame() || (document.settings()->disallowFetchForDocWrittenScriptsInMainFrameOnSlowConnections() && isSlowConnection);

    return disallowFetch;
}

} // namespace

FrameFetchContext::FrameFetchContext(DocumentLoader* loader, Document* document)
    : m_document(document)
    , m_documentLoader(loader)
{
    ASSERT(frame());
}

FrameFetchContext::~FrameFetchContext()
{
    m_document = nullptr;
    m_documentLoader = nullptr;
}

LocalFrame* FrameFetchContext::frame() const
{
    LocalFrame* frame = nullptr;
    if (m_documentLoader)
        frame = m_documentLoader->frame();
    else if (m_document && m_document->importsController())
        frame = m_document->importsController()->master()->frame();
    ASSERT(frame);
    return frame;
}

void FrameFetchContext::addAdditionalRequestHeaders(ResourceRequest& request, FetchResourceType type)
{
    bool isMainResource = type == FetchMainResource;
    if (!isMainResource) {
        RefPtr<SecurityOrigin> outgoingOrigin;
        if (!request.didSetHTTPReferrer()) {
            ASSERT(m_document);
            outgoingOrigin = m_document->getSecurityOrigin();
            request.setHTTPReferrer(SecurityPolicy::generateReferrer(m_document->getReferrerPolicy(), request.url(), m_document->outgoingReferrer()));
        } else {
            RELEASE_ASSERT(SecurityPolicy::generateReferrer(request.getReferrerPolicy(), request.url(), request.httpReferrer()).referrer == request.httpReferrer());
            outgoingOrigin = SecurityOrigin::createFromString(request.httpReferrer());
        }

        request.addHTTPOriginIfNeeded(outgoingOrigin);
    }

    if (m_document)
        request.setExternalRequestStateFromRequestorAddressSpace(m_document->addressSpace());

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    if (frame()->loader().loadType() == FrameLoadTypeReload)
        request.clearHTTPHeaderField("Save-Data");

    if (frame()->settings() && frame()->settings()->dataSaverEnabled())
        request.setHTTPHeaderField("Save-Data", "on");

    frame()->loader().applyUserAgent(request);
}

void FrameFetchContext::setFirstPartyForCookies(ResourceRequest& request)
{
    if (frame()->tree().top()->isLocalFrame())
        request.setFirstPartyForCookies(toLocalFrame(frame()->tree().top())->document()->firstPartyForCookies());
}

CachePolicy FrameFetchContext::getCachePolicy() const
{
    if (m_document && m_document->loadEventFinished())
        return CachePolicyVerify;

    FrameLoadType loadType = frame()->loader().loadType();
    if (loadType == FrameLoadTypeReloadBypassingCache)
        return CachePolicyReload;

    Frame* parentFrame = frame()->tree().parent();
    if (parentFrame && parentFrame->isLocalFrame()) {
        CachePolicy parentCachePolicy = toLocalFrame(parentFrame)->document()->fetcher()->context().getCachePolicy();
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }

    if (loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    if (m_documentLoader && m_documentLoader->request().getCachePolicy() == WebCachePolicy::ReturnCacheDataElseLoad)
        return CachePolicyHistoryBuffer;
    return CachePolicyVerify;
}

static WebCachePolicy memoryCachePolicyToResourceRequestCachePolicy(const CachePolicy policy)
{
    if (policy == CachePolicyVerify)
        return WebCachePolicy::UseProtocolCachePolicy;
    if (policy == CachePolicyRevalidate)
        return WebCachePolicy::ValidatingCacheData;
    if (policy == CachePolicyReload)
        return WebCachePolicy::BypassingCache;
    if (policy == CachePolicyHistoryBuffer)
        return WebCachePolicy::ReturnCacheDataElseLoad;
    return WebCachePolicy::UseProtocolCachePolicy;
}

WebCachePolicy FrameFetchContext::resourceRequestCachePolicy(const ResourceRequest& request, Resource::Type type, FetchRequest::DeferOption defer) const
{
    ASSERT(frame());
    if (type == Resource::MainResource) {
        FrameLoadType frameLoadType = frame()->loader().loadType();
        if (request.httpMethod() == "POST" && frameLoadType == FrameLoadTypeBackForward)
            return WebCachePolicy::ReturnCacheDataDontLoad;
        if (!frame()->host()->overrideEncoding().isEmpty())
            return WebCachePolicy::ReturnCacheDataElseLoad;
        if (frameLoadType == FrameLoadTypeReloadMainResource || request.isConditional() || request.httpMethod() == "POST")
            return WebCachePolicy::ValidatingCacheData;

        for (Frame* f = frame(); f; f = f->tree().parent()) {
            if (!f->isLocalFrame())
                continue;
            frameLoadType = toLocalFrame(f)->loader().loadType();
            if (frameLoadType == FrameLoadTypeBackForward)
                return WebCachePolicy::ReturnCacheDataElseLoad;
            if (frameLoadType == FrameLoadTypeReloadBypassingCache)
                return WebCachePolicy::BypassingCache;
            if (frameLoadType == FrameLoadTypeReload)
                return WebCachePolicy::ValidatingCacheData;
        }
        return WebCachePolicy::UseProtocolCachePolicy;
    }

    // For users on slow connections, we want to avoid blocking the parser in
    // the main frame on script loads inserted via document.write, since it can
    // add significant delays before page content is displayed on the screen.
    if (type == Resource::Script && isMainFrame() && m_document && shouldDisallowFetchForMainFrameScript(request, defer, *m_document))
        return WebCachePolicy::ReturnCacheDataDontLoad;

    if (request.isConditional())
        return WebCachePolicy::ValidatingCacheData;

    if (m_documentLoader && m_document && !m_document->loadEventFinished()) {
        // For POST requests, we mutate the main resource's cache policy to avoid form resubmission.
        // This policy should not be inherited by subresources.
        WebCachePolicy mainResourceCachePolicy = m_documentLoader->request().getCachePolicy();
        if (m_documentLoader->request().httpMethod() == "POST") {
            if (mainResourceCachePolicy == WebCachePolicy::ReturnCacheDataDontLoad)
                return WebCachePolicy::ReturnCacheDataElseLoad;
            return WebCachePolicy::UseProtocolCachePolicy;
        }
        return memoryCachePolicyToResourceRequestCachePolicy(getCachePolicy());
    }
    return WebCachePolicy::UseProtocolCachePolicy;
}

// The |m_documentLoader| is null in the FrameFetchContext of an imported document.
// FIXME(http://crbug.com/274173): This means Inspector, which uses DocumentLoader
// as a grouping entity, cannot see imported documents.
inline DocumentLoader* FrameFetchContext::masterDocumentLoader() const
{
    return m_documentLoader ? m_documentLoader.get() : frame()->loader().documentLoader();
}

void FrameFetchContext::dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    frame()->loader().client()->dispatchDidChangeResourcePriority(identifier, loadPriority, intraPriorityValue);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceChangePriority", TRACE_EVENT_SCOPE_THREAD, "data", InspectorChangeResourcePriorityEvent::data(identifier, loadPriority));
    InspectorInstrumentation::didChangeResourcePriority(frame(), identifier, loadPriority);
}

void FrameFetchContext::prepareRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    frame()->loader().applyUserAgent(request);
    frame()->loader().client()->dispatchWillSendRequest(m_documentLoader, identifier, request, redirectResponse);
}

void FrameFetchContext::dispatchWillSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    // For initial requests, prepareRequest() is called in
    // willStartLoadingResource(), before revalidation policy is determined.
    // That call doesn't exist for redirects, so call preareRequest() here.
    if (!redirectResponse.isNull())
        prepareRequest(identifier, request, redirectResponse);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceSendRequest", TRACE_EVENT_SCOPE_THREAD, "data", InspectorSendRequestEvent::data(identifier, frame(), request));
    InspectorInstrumentation::willSendRequest(frame(), identifier, masterDocumentLoader(), request, redirectResponse, initiatorInfo);
}

void FrameFetchContext::dispatchDidReceiveResponse(unsigned long identifier, const ResourceResponse& response, WebURLRequest::FrameType frameType, WebURLRequest::RequestContext requestContext, Resource* resource)
{
    LinkLoader::CanLoadResources resourceLoadingPolicy = LinkLoader::LoadResourcesAndPreconnect;
    MixedContentChecker::checkMixedPrivatePublic(frame(), response.remoteIPAddress());
    if (m_documentLoader == frame()->loader().provisionalDocumentLoader()) {
        ResourceFetcher* fetcher = nullptr;
        if (frame()->document())
            fetcher = frame()->document()->fetcher();
        m_documentLoader->clientHintsPreferences().updateFromAcceptClientHintsHeader(response.httpHeaderField(HTTPNames::Accept_CH), fetcher);
        // When response is received with a provisional docloader, the resource haven't committed yet, and we cannot load resources, only preconnect.
        resourceLoadingPolicy = LinkLoader::DoNotLoadResources;
    }
    LinkLoader::loadLinksFromHeader(response.httpHeaderField(HTTPNames::Link), response.url(), frame()->document(), NetworkHintsInterfaceImpl(), resourceLoadingPolicy, nullptr);

    if (response.hasMajorCertificateErrors())
        MixedContentChecker::handleCertificateError(frame(), response, frameType, requestContext);

    frame()->loader().progress().incrementProgress(identifier, response);
    frame()->loader().client()->dispatchDidReceiveResponse(m_documentLoader, identifier, response);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceiveResponse", TRACE_EVENT_SCOPE_THREAD, "data", InspectorReceiveResponseEvent::data(identifier, frame(), response));
    DocumentLoader* documentLoader = masterDocumentLoader();
    InspectorInstrumentation::didReceiveResourceResponse(frame(), identifier, documentLoader, response, resource);
    // It is essential that inspector gets resource response BEFORE console.
    frame()->console().reportResourceResponseReceived(documentLoader, identifier, response);
}

void FrameFetchContext::dispatchDidReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    frame()->loader().progress().incrementProgress(identifier, dataLength);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceivedData", TRACE_EVENT_SCOPE_THREAD, "data", InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
    InspectorInstrumentation::didReceiveData(frame(), identifier, data, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidDownloadData(unsigned long identifier, int dataLength, int encodedDataLength)
{
    frame()->loader().progress().incrementProgress(identifier, dataLength);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceivedData", TRACE_EVENT_SCOPE_THREAD, "data", InspectorReceiveDataEvent::data(identifier, frame(), encodedDataLength));
    InspectorInstrumentation::didReceiveData(frame(), identifier, 0, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidFinishLoading(unsigned long identifier, double finishTime, int64_t encodedDataLength)
{
    frame()->loader().progress().completeProgress(identifier);
    frame()->loader().client()->dispatchDidFinishLoading(m_documentLoader, identifier);

    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(identifier, finishTime, false));
    InspectorInstrumentation::didFinishLoading(frame(), identifier, finishTime, encodedDataLength);
}

void FrameFetchContext::dispatchDidFail(unsigned long identifier, const ResourceError& error, bool isInternalRequest)
{
    frame()->loader().progress().completeProgress(identifier);
    frame()->loader().client()->dispatchDidFinishLoading(m_documentLoader, identifier);
    TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data", InspectorResourceFinishEvent::data(identifier, 0, true));
    InspectorInstrumentation::didFailLoading(frame(), identifier, error);
    // Notification to FrameConsole should come AFTER InspectorInstrumentation call, DevTools front-end relies on this.
    if (!isInternalRequest)
        frame()->console().didFailLoading(identifier, error);
}

void FrameFetchContext::dispatchDidLoadResourceFromMemoryCache(unsigned long identifier, Resource* resource, WebURLRequest::FrameType frameType, WebURLRequest::RequestContext requestContext)
{
    ResourceRequest request(resource->url());
    frame()->loader().client()->dispatchDidLoadResourceFromMemoryCache(request, resource->response());
    dispatchWillSendRequest(identifier, request, ResourceResponse(), resource->options().initiatorInfo);

    InspectorInstrumentation::markResourceAsCached(frame(), identifier);
    if (!resource->response().isNull())
        dispatchDidReceiveResponse(identifier, resource->response(), frameType, requestContext, resource);

    if (resource->encodedSize() > 0)
        dispatchDidReceiveData(identifier, 0, resource->encodedSize(), 0);

    dispatchDidFinishLoading(identifier, 0, 0);
}

bool FrameFetchContext::shouldLoadNewResource(Resource::Type type) const
{
    if (!m_documentLoader)
        return true;
    if (type == Resource::MainResource)
        return m_documentLoader == frame()->loader().provisionalDocumentLoader();
    return m_documentLoader == frame()->loader().documentLoader();
}

static std::unique_ptr<TracedValue> loadResourceTraceData(unsigned long identifier, const KURL& url, int priority)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    std::unique_ptr<TracedValue> value = TracedValue::create();
    value->setString("requestId", requestId);
    value->setString("url", url.getString());
    value->setInteger("priority", priority);
    return value;
}

void FrameFetchContext::willStartLoadingResource(unsigned long identifier, ResourceRequest& request, Resource::Type type)
{
    TRACE_EVENT_ASYNC_BEGIN1("blink.net", "Resource", identifier, "data", loadResourceTraceData(identifier, request.url(), request.priority()));
    frame()->loader().progress().willStartLoading(identifier);
    prepareRequest(identifier, request, ResourceResponse());

    if (!m_documentLoader || m_documentLoader->fetcher()->archive() || !request.url().isValid())
        return;
    if (type == Resource::MainResource)
        m_documentLoader->applicationCacheHost()->willStartLoadingMainResource(request);
    else
        m_documentLoader->applicationCacheHost()->willStartLoadingResource(request);
}

void FrameFetchContext::didLoadResource(Resource* resource)
{
    if (resource->isLoadEventBlockingResourceType())
        frame()->loader().checkCompleted();
}

void FrameFetchContext::addResourceTiming(const ResourceTimingInfo& info)
{
    Document* initiatorDocument = m_document && info.isMainResource() ? m_document->parentDocument() : m_document.get();
    if (!initiatorDocument || !initiatorDocument->domWindow())
        return;
    DOMWindowPerformance::performance(*initiatorDocument->domWindow())->addResourceTiming(info);
}

bool FrameFetchContext::allowImage(bool imagesEnabled, const KURL& url) const
{
    return frame()->loader().client()->allowImage(imagesEnabled, url);
}

void FrameFetchContext::printAccessDeniedMessage(const KURL& url) const
{
    if (url.isNull())
        return;

    String message;
    if (!m_document || m_document->url().isNull())
        message = "Unsafe attempt to load URL " + url.elidedString() + '.';
    else if (url.isLocalFile() || m_document->url().isLocalFile())
        message = "Unsafe attempt to load URL " + url.elidedString() + " from frame with URL " + m_document->url().elidedString() + ". 'file:' URLs are treated as unique security origins.\n";
    else
        message = "Unsafe attempt to load URL " + url.elidedString() + " from frame with URL " + m_document->url().elidedString() + ". Domains, protocols and ports must match.\n";

    frame()->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
}

bool FrameFetchContext::canRequest(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options, bool forPreload, FetchRequest::OriginRestriction originRestriction) const
{
    ResourceRequestBlockedReason reason = canRequestInternal(type, resourceRequest, url, options, forPreload, originRestriction, resourceRequest.redirectStatus());
    if (reason != ResourceRequestBlockedReasonNone) {
        if (!forPreload)
            InspectorInstrumentation::didBlockRequest(frame(), resourceRequest, masterDocumentLoader(), options.initiatorInfo, reason);
        return false;
    }
    return true;
}

bool FrameFetchContext::allowResponse(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options) const
{
    ResourceRequestBlockedReason reason = canRequestInternal(type, resourceRequest, url, options, false, FetchRequest::UseDefaultOriginRestrictionForType, RedirectStatus::FollowedRedirect);
    if (reason != ResourceRequestBlockedReasonNone) {
        InspectorInstrumentation::didBlockRequest(frame(), resourceRequest, masterDocumentLoader(), options.initiatorInfo, reason);
        return false;
    }
    return true;
}

ResourceRequestBlockedReason FrameFetchContext::canRequestInternal(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options, bool forPreload, FetchRequest::OriginRestriction originRestriction, ResourceRequest::RedirectStatus redirectStatus) const
{
    if (InspectorInstrumentation::shouldBlockRequest(frame(), resourceRequest))
        return ResourceRequestBlockedReasonInspector;

    SecurityOrigin* securityOrigin = options.securityOrigin.get();
    if (!securityOrigin && m_document)
        securityOrigin = m_document->getSecurityOrigin();

    if (originRestriction != FetchRequest::NoOriginRestriction && securityOrigin && !securityOrigin->canDisplay(url)) {
        if (!forPreload)
            FrameLoader::reportLocalLoadFailed(frame(), url.elidedString());
        WTF_LOG(ResourceLoading, "ResourceFetcher::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return ResourceRequestBlockedReasonOther;
    }

    // Some types of resources can be loaded only from the same origin. Other
    // types of resources, like Images, Scripts, and CSS, can be loaded from
    // any URL.
    switch (type) {
    case Resource::MainResource:
    case Resource::Image:
    case Resource::CSSStyleSheet:
    case Resource::Script:
    case Resource::Font:
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkPreload:
    case Resource::TextTrack:
    case Resource::ImportResource:
    case Resource::Media:
    case Resource::Manifest:
        // By default these types of resources can be loaded from any origin.
        // FIXME: Are we sure about Resource::Font?
        if (originRestriction == FetchRequest::RestrictToSameOrigin && !securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return ResourceRequestBlockedReasonOrigin;
        }
        break;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::SVGDocument:
        if (!securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return ResourceRequestBlockedReasonOrigin;
        }
        break;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldCSP = frame()->script().shouldBypassMainWorldCSP() || options.contentSecurityPolicyOption == DoNotCheckContentSecurityPolicy;

    // Don't send CSP messages for preloads, we might never actually display those items.
    ContentSecurityPolicy::ReportingStatus cspReporting = forPreload ?
        ContentSecurityPolicy::SuppressReport : ContentSecurityPolicy::SendReport;

    if (m_document) {
        DCHECK(m_document->contentSecurityPolicy());
        if (!shouldBypassMainWorldCSP && !m_document->contentSecurityPolicy()->allowRequest(resourceRequest.requestContext(), url, options.contentSecurityPolicyNonce, redirectStatus, cspReporting))
            return ResourceRequestBlockedReasonCSP;
    }

    if (type == Resource::Script || type == Resource::ImportResource) {
        ASSERT(frame());
        if (!frame()->loader().client()->allowScriptFromSource(!frame()->settings() || frame()->settings()->scriptEnabled(), url)) {
            frame()->loader().client()->didNotAllowScript();
            // TODO(estark): Use a different ResourceRequestBlockedReason
            // here, since this check has nothing to do with
            // CSP. https://crbug.com/600795
            return ResourceRequestBlockedReasonCSP;
        }
    } else if (type == Resource::Media || type == Resource::TextTrack) {
        ASSERT(frame());
        if (!frame()->loader().client()->allowMedia(url))
            return ResourceRequestBlockedReasonOther;
    }

    // SVG Images have unique security rules that prevent all subresource requests
    // except for data urls.
    if (type != Resource::MainResource && frame()->chromeClient().isSVGImageChromeClient() && !url.protocolIsData())
        return ResourceRequestBlockedReasonOrigin;

    // Measure the number of legacy URL schemes ('ftp://') and the number of embedded-credential
    // ('http://user:password@...') resources embedded as subresources. in the hopes that we can
    // block them at some point in the future.
    if (resourceRequest.frameType() != WebURLRequest::FrameTypeTopLevel) {
        ASSERT(frame()->document());
        if (SchemeRegistry::shouldTreatURLSchemeAsLegacy(url.protocol()) && !SchemeRegistry::shouldTreatURLSchemeAsLegacy(frame()->document()->getSecurityOrigin()->protocol()))
            UseCounter::count(frame()->document(), UseCounter::LegacyProtocolEmbeddedAsSubresource);
        if (!url.user().isEmpty() || !url.pass().isEmpty())
            UseCounter::count(frame()->document(), UseCounter::RequestedSubresourceWithEmbeddedCredentials);
    }

    // Check for mixed content. We do this second-to-last so that when folks block
    // mixed content with a CSP policy, they don't get a warning. They'll still
    // get a warning in the console about CSP blocking the load.
    MixedContentChecker::ReportingStatus mixedContentReporting = forPreload ?
        MixedContentChecker::SuppressReport : MixedContentChecker::SendReport;
    if (MixedContentChecker::shouldBlockFetch(frame(), resourceRequest, url, mixedContentReporting))
        return ResourceRequestBlockedReasonMixedContent;

    // Let the client have the final say into whether or not the load should proceed.
    DocumentLoader* documentLoader = masterDocumentLoader();
    if (documentLoader && documentLoader->subresourceFilter() && type != Resource::MainResource && type != Resource::ImportResource && !documentLoader->subresourceFilter()->allowLoad(url, resourceRequest.requestContext()))
        return ResourceRequestBlockedReasonSubresourceFilter;

    return ResourceRequestBlockedReasonNone;
}

bool FrameFetchContext::isControlledByServiceWorker() const
{
    ASSERT(m_documentLoader || frame()->loader().documentLoader());
    if (m_documentLoader)
        return frame()->loader().client()->isControlledByServiceWorker(*m_documentLoader);
    // m_documentLoader is null while loading resources from an HTML import.
    // In such cases whether the request is controlled by ServiceWorker or not
    // is determined by the document loader of the frame.
    return frame()->loader().client()->isControlledByServiceWorker(*frame()->loader().documentLoader());
}

int64_t FrameFetchContext::serviceWorkerID() const
{
    ASSERT(m_documentLoader || frame()->loader().documentLoader());
    if (m_documentLoader)
        return frame()->loader().client()->serviceWorkerID(*m_documentLoader);
    // m_documentLoader is null while loading resources from an HTML import.
    // In such cases a service worker ID could be retrieved from the document
    // loader of the frame.
    return frame()->loader().client()->serviceWorkerID(*frame()->loader().documentLoader());
}

bool FrameFetchContext::isMainFrame() const
{
    return frame()->isMainFrame();
}

bool FrameFetchContext::defersLoading() const
{
    return frame()->page()->defersLoading();
}

bool FrameFetchContext::isLoadComplete() const
{
    return m_document && m_document->loadEventFinished();
}

bool FrameFetchContext::pageDismissalEventBeingDispatched() const
{
    return m_document && m_document->pageDismissalEventBeingDispatched() != Document::NoDismissal;
}

bool FrameFetchContext::updateTimingInfoForIFrameNavigation(ResourceTimingInfo* info)
{
    // <iframe>s should report the initial navigation requested by the parent document, but not subsequent navigations.
    // FIXME: Resource timing is broken when the parent is a remote frame.
    if (!frame()->deprecatedLocalOwner() || frame()->deprecatedLocalOwner()->loadedNonEmptyDocument())
        return false;
    frame()->deprecatedLocalOwner()->didLoadNonEmptyDocument();
    // Do not report iframe navigation that restored from history, since its
    // location may have been changed after initial navigation.
    if (frame()->loader().loadType() == FrameLoadTypeInitialHistoryLoad)
        return false;
    info->setInitiatorType(frame()->deprecatedLocalOwner()->localName());
    return true;
}

void FrameFetchContext::sendImagePing(const KURL& url)
{
    PingLoader::loadImage(frame(), url);
}

void FrameFetchContext::addConsoleMessage(const String& message) const
{
    if (frame()->document())
        frame()->document()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

SecurityOrigin* FrameFetchContext::getSecurityOrigin() const
{
    return m_document ? m_document->getSecurityOrigin() : nullptr;
}

void FrameFetchContext::upgradeInsecureRequest(FetchRequest& fetchRequest)
{
    KURL url = fetchRequest.resourceRequest().url();

    // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational requests, as described in
    // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
    if (fetchRequest.resourceRequest().frameType() != WebURLRequest::FrameTypeNone)
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("Upgrade-Insecure-Requests", "1");

    // If we don't yet have an |m_document| (because we're loading an iframe, for instance), check the FrameLoader's policy.
    WebInsecureRequestPolicy relevantPolicy = m_document ? m_document->getInsecureRequestPolicy() : frame()->loader().getInsecureRequestPolicy();
    SecurityContext::InsecureNavigationsSet* relevantNavigationSet = m_document ? m_document->insecureNavigationsToUpgrade() : frame()->loader().insecureNavigationsToUpgrade();

    if (url.protocolIs("http") && relevantPolicy & kUpgradeInsecureRequests) {
        // We always upgrade requests that meet any of the following criteria:
        //
        // 1. Are for subresources (including nested frames).
        // 2. Are form submissions.
        // 3. Whose hosts are contained in the document's InsecureNavigationSet.
        const ResourceRequest& request = fetchRequest.resourceRequest();
        if (request.frameType() == WebURLRequest::FrameTypeNone
            || request.frameType() == WebURLRequest::FrameTypeNested
            || request.requestContext() == WebURLRequest::RequestContextForm
            || (!url.host().isNull() && relevantNavigationSet->contains(url.host().impl()->hash())))
        {
            UseCounter::count(m_document, UseCounter::UpgradeInsecureRequestsUpgradedRequest);
            url.setProtocol("https");
            if (url.port() == 80)
                url.setPort(443);
            fetchRequest.mutableResourceRequest().setURL(url);
        }
    }
}

void FrameFetchContext::addClientHintsIfNecessary(FetchRequest& fetchRequest)
{
    if (!RuntimeEnabledFeatures::clientHintsEnabled() || !m_document)
        return;

    bool shouldSendDPR = m_document->clientHintsPreferences().shouldSendDPR() || fetchRequest.clientHintsPreferences().shouldSendDPR();
    bool shouldSendResourceWidth = m_document->clientHintsPreferences().shouldSendResourceWidth() || fetchRequest.clientHintsPreferences().shouldSendResourceWidth();
    bool shouldSendViewportWidth = m_document->clientHintsPreferences().shouldSendViewportWidth() || fetchRequest.clientHintsPreferences().shouldSendViewportWidth();

    if (shouldSendDPR)
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("DPR", AtomicString(String::number(m_document->devicePixelRatio())));

    if (shouldSendResourceWidth) {
        FetchRequest::ResourceWidth resourceWidth = fetchRequest.getResourceWidth();
        if (resourceWidth.isSet) {
            float physicalWidth = resourceWidth.width * m_document->devicePixelRatio();
            fetchRequest.mutableResourceRequest().addHTTPHeaderField("Width", AtomicString(String::number(ceil(physicalWidth))));
        }
    }

    if (shouldSendViewportWidth && frame()->view())
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("Viewport-Width", AtomicString(String::number(frame()->view()->viewportWidth())));
}

void FrameFetchContext::addCSPHeaderIfNecessary(Resource::Type type, FetchRequest& fetchRequest)
{
    if (!m_document)
        return;

    const ContentSecurityPolicy* csp = m_document->contentSecurityPolicy();
    if (csp->shouldSendCSPHeader(type))
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("CSP", "active");
}

MHTMLArchive* FrameFetchContext::archive() const
{
    ASSERT(!isMainFrame());
    // TODO(nasko): How should this work with OOPIF?
    // The MHTMLArchive is parsed as a whole, but can be constructed from
    // frames in mutliple processes. In that case, which process should parse
    // it and how should the output be spread back across multiple processes?
    if (!frame()->tree().parent()->isLocalFrame())
        return nullptr;
    return toLocalFrame(frame()->tree().parent())->loader().documentLoader()->fetcher()->archive();
}

void FrameFetchContext::countClientHintsDPR()
{
    UseCounter::count(frame(), UseCounter::ClientHintsDPR);
}

void FrameFetchContext::countClientHintsResourceWidth()
{
    UseCounter::count(frame(), UseCounter::ClientHintsResourceWidth);
}

void FrameFetchContext::countClientHintsViewportWidth()
{
    UseCounter::count(frame(), UseCounter::ClientHintsViewportWidth);
}

ResourceLoadPriority FrameFetchContext::modifyPriorityForExperiments(ResourceLoadPriority priority)
{
    // If Settings is null, we can't verify any experiments are in force.
    if (!frame()->settings())
        return priority;

    // If enabled, drop the priority of all resources in a subframe.
    if (frame()->settings()->lowPriorityIframes() && !frame()->isMainFrame())
        return ResourceLoadPriorityVeryLow;

    return priority;
}

WebTaskRunner* FrameFetchContext::loadingTaskRunner() const
{
    return frame()->frameScheduler()->loadingTaskRunner();
}

DEFINE_TRACE(FrameFetchContext)
{
    visitor->trace(m_document);
    visitor->trace(m_documentLoader);
    FetchContext::trace(visitor);
}

} // namespace blink
