/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/DocumentLoader.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/WeakIdentifierMap.h"
#include "core/events/Event.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameFetchContext.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "platform/HTTPNames.h"
#include "platform/Logging.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/HTTPParsers.h"
#include "platform/plugins/PluginData.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebMimeRegistry.h"
#include "wtf/Assertions.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

static bool isArchiveMIMEType(const String& mimeType)
{
    return equalIgnoringCase("multipart/related", mimeType);
}

static bool shouldInheritSecurityOriginFromOwner(const KURL& url)
{
    // https://html.spec.whatwg.org/multipage/browsers.html#origin
    //
    // If a Document is the initial "about:blank" document
    //     The origin and effective script origin of the Document are those it
    //     was assigned when its browsing context was created.
    //
    // Note: We generalize this to all "blank" URLs and invalid URLs because we
    // treat all of these URLs as about:blank.
    return url.isEmpty() || url.protocolIsAbout();
}

DocumentLoader::DocumentLoader(LocalFrame* frame, const ResourceRequest& req, const SubstituteData& substituteData)
    : m_frame(frame)
    , m_fetcher(FrameFetchContext::createContextAndFetcher(this, nullptr))
    , m_originalRequest(req)
    , m_substituteData(substituteData)
    , m_request(req)
    , m_isClientRedirect(false)
    , m_replacesCurrentHistoryItem(false)
    , m_navigationType(NavigationTypeOther)
    , m_documentLoadTiming(*this)
    , m_timeOfLastDataReceived(0.0)
    , m_applicationCacheHost(ApplicationCacheHost::create(this))
    , m_wasBlockedAfterXFrameOptionsOrCSP(false)
    , m_state(NotStarted)
    , m_inDataReceived(false)
    , m_dataBuffer(SharedBuffer::create())
{
}

FrameLoader* DocumentLoader::frameLoader() const
{
    if (!m_frame)
        return nullptr;
    return &m_frame->loader();
}

ResourceLoader* DocumentLoader::mainResourceLoader() const
{
    return m_mainResource ? m_mainResource->loader() : nullptr;
}

DocumentLoader::~DocumentLoader()
{
    ASSERT(!m_frame);
    ASSERT(!m_mainResource);
    ASSERT(!m_applicationCacheHost);
}

DEFINE_TRACE(DocumentLoader)
{
    visitor->trace(m_frame);
    visitor->trace(m_fetcher);
    visitor->trace(m_mainResource);
    visitor->trace(m_writer);
    visitor->trace(m_documentLoadTiming);
    visitor->trace(m_applicationCacheHost);
    visitor->trace(m_contentSecurityPolicy);
}

unsigned long DocumentLoader::mainResourceIdentifier() const
{
    return m_mainResource ? m_mainResource->identifier() : 0;
}

const ResourceRequest& DocumentLoader::originalRequest() const
{
    return m_originalRequest;
}

const ResourceRequest& DocumentLoader::request() const
{
    return m_request;
}

const KURL& DocumentLoader::url() const
{
    return m_request.url();
}

void DocumentLoader::setSubresourceFilter(std::unique_ptr<WebDocumentSubresourceFilter> subresourceFilter)
{
    m_subresourceFilter = std::move(subresourceFilter);
}

Resource* DocumentLoader::startPreload(Resource::Type type, FetchRequest& request)
{
    Resource* resource = nullptr;
    switch (type) {
    case Resource::Image:
        resource = ImageResource::fetch(request, fetcher());
        break;
    case Resource::Script:
        resource = ScriptResource::fetch(request, fetcher());
        break;
    case Resource::CSSStyleSheet:
        resource = CSSStyleSheetResource::fetch(request, fetcher());
        break;
    case Resource::Font:
        resource = FontResource::fetch(request, fetcher());
        break;
    case Resource::Media:
        resource = RawResource::fetchMedia(request, fetcher());
        break;
    case Resource::TextTrack:
        resource = RawResource::fetchTextTrack(request, fetcher());
        break;
    case Resource::ImportResource:
        resource = RawResource::fetchImport(request, fetcher());
        break;
    case Resource::LinkPreload:
        resource = RawResource::fetch(request, fetcher());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (resource)
        fetcher()->preloadStarted(resource);
    return resource;
}

void DocumentLoader::didChangePerformanceTiming()
{
    if (frame() && frame()->isMainFrame() && m_state >= Committed) {
        frameLoader()->client()->didChangePerformanceTiming();
    }
}

void DocumentLoader::didObserveLoadingBehavior(WebLoadingBehaviorFlag behavior)
{
    if (frame() && frame()->isMainFrame()) {
        ASSERT(m_state >= Committed);
        frameLoader()->client()->didObserveLoadingBehavior(behavior);
    }
}

void DocumentLoader::updateForSameDocumentNavigation(const KURL& newURL, SameDocumentNavigationSource sameDocumentNavigationSource)
{
    KURL oldURL = m_request.url();
    m_originalRequest.setURL(newURL);
    m_request.setURL(newURL);
    if (sameDocumentNavigationSource == SameDocumentNavigationHistoryApi) {
        m_request.setHTTPMethod(HTTPNames::GET);
        m_request.setHTTPBody(nullptr);
    }
    clearRedirectChain();
    if (m_isClientRedirect)
        appendRedirect(oldURL);
    appendRedirect(newURL);
}

const KURL& DocumentLoader::urlForHistory() const
{
    return unreachableURL().isEmpty() ? url() : unreachableURL();
}

void DocumentLoader::commitIfReady()
{
    if (m_state < Committed) {
        m_state = Committed;
        frameLoader()->commitProvisionalLoad();
    }
}

void DocumentLoader::notifyFinished(Resource* resource)
{
    ASSERT_UNUSED(resource, m_mainResource == resource);
    ASSERT(m_mainResource);

    if (!m_mainResource->errorOccurred() && !m_mainResource->wasCanceled()) {
        finishedLoading(m_mainResource->loadFinishTime());
        return;
    }

    if (m_applicationCacheHost)
        m_applicationCacheHost->failedLoadingMainResource();
    m_state = MainResourceDone;
    frameLoader()->loadFailed(this, m_mainResource->resourceError());
    clearMainResourceHandle();
}

void DocumentLoader::finishedLoading(double finishTime)
{
    DCHECK(m_frame->loader().stateMachine()->creatingInitialEmptyDocument()
        || !m_frame->page()->defersLoading()
        || InspectorInstrumentation::isDebuggerPaused(m_frame));

    double responseEndTime = finishTime;
    if (!responseEndTime)
        responseEndTime = m_timeOfLastDataReceived;
    if (!responseEndTime)
        responseEndTime = monotonicallyIncreasingTime();
    timing().setResponseEnd(responseEndTime);

    commitIfReady();
    if (!frameLoader())
        return;

    if (!maybeCreateArchive()) {
        // If this is an empty document, it will not have actually been created yet. Commit dummy data so that
        // DocumentWriter::begin() gets called and creates the Document.
        if (!m_writer)
            commitData(0, 0);
    }

    if (!m_frame)
        return;

    m_applicationCacheHost->finishedLoadingMainResource();
    endWriting(m_writer.get());
    if (m_state < MainResourceDone)
        m_state = MainResourceDone;
    clearMainResourceHandle();
}

void DocumentLoader::redirectReceived(Resource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(resource, resource == m_mainResource);
    ASSERT(!redirectResponse.isNull());
    m_request = request;

    // If the redirecting url is not allowed to display content from the target origin,
    // then block the redirect.
    const KURL& requestURL = m_request.url();
    RefPtr<SecurityOrigin> redirectingOrigin = SecurityOrigin::create(redirectResponse.url());
    if (!redirectingOrigin->canDisplay(requestURL)) {
        FrameLoader::reportLocalLoadFailed(m_frame, requestURL.getString());
        m_fetcher->stopFetching();
        return;
    }
    if (!frameLoader()->shouldContinueForNavigationPolicy(m_request, SubstituteData(), this, CheckContentSecurityPolicy, m_navigationType, NavigationPolicyCurrentTab, replacesCurrentHistoryItem(), isClientRedirect())) {
        m_fetcher->stopFetching();
        return;
    }

    ASSERT(timing().fetchStart());
    timing().addRedirect(redirectResponse.url(), requestURL);
    appendRedirect(requestURL);
    frameLoader()->receivedMainResourceRedirect(requestURL);
}

static bool canShowMIMEType(const String& mimeType, LocalFrame* frame)
{
    if (Platform::current()->mimeRegistry()->supportsMIMEType(mimeType) == WebMimeRegistry::IsSupported)
        return true;
    PluginData* pluginData = frame->pluginData();
    return !mimeType.isEmpty() && pluginData && pluginData->supportsMimeType(mimeType);
}

bool DocumentLoader::shouldContinueForResponse() const
{
    if (m_substituteData.isValid())
        return true;

    int statusCode = m_response.httpStatusCode();
    if (statusCode == 204 || statusCode == 205) {
        // The server does not want us to replace the page contents.
        return false;
    }

    if (getContentDispositionType(m_response.httpHeaderField(HTTPNames::Content_Disposition)) == ContentDispositionAttachment) {
        // The server wants us to download instead of replacing the page contents.
        // Downloading is handled by the embedder, but we still get the initial
        // response so that we can ignore it and clean up properly.
        return false;
    }

    if (!canShowMIMEType(m_response.mimeType(), m_frame))
        return false;
    return true;
}

void DocumentLoader::cancelLoadAfterXFrameOptionsOrCSPDenied(const ResourceResponse& response)
{
    InspectorInstrumentation::continueAfterXFrameOptionsDenied(m_frame, this, mainResourceIdentifier(), response, m_mainResource.get());

    setWasBlockedAfterXFrameOptionsOrCSP();

    // Pretend that this was an empty HTTP 200 response.  Don't reuse the
    // original URL for the empty page (https://crbug.com/622385).
    //
    // TODO(mkwst):  Remove this once XFO moves to the browser.
    // https://crbug.com/555418.
    clearMainResourceHandle();
    KURL blockedURL = SecurityOrigin::urlWithUniqueSecurityOrigin();
    m_originalRequest.setURL(blockedURL);
    m_request.setURL(blockedURL);
    m_redirectChain.removeLast();
    appendRedirect(blockedURL);
    m_response = ResourceResponse(blockedURL, "text/html", 0, nullAtom, String());
    finishedLoading(monotonicallyIncreasingTime());

    return;
}

void DocumentLoader::responseReceived(Resource* resource, const ResourceResponse& response, std::unique_ptr<WebDataConsumerHandle> handle)
{
    ASSERT_UNUSED(resource, m_mainResource == resource);
    ASSERT_UNUSED(handle, !handle);
    ASSERT(frame());

    m_applicationCacheHost->didReceiveResponseForMainResource(response);

    // The memory cache doesn't understand the application cache or its caching rules. So if a main resource is served
    // from the application cache, ensure we don't save the result for future use. All responses loaded
    // from appcache will have a non-zero appCacheID().
    if (response.appCacheID())
        memoryCache()->remove(m_mainResource.get());

    m_contentSecurityPolicy = ContentSecurityPolicy::create();
    m_contentSecurityPolicy->setOverrideURLForSelf(response.url());
    m_contentSecurityPolicy->didReceiveHeaders(ContentSecurityPolicyResponseHeaders(response));
    if (!m_contentSecurityPolicy->allowAncestors(m_frame, response.url())) {
        cancelLoadAfterXFrameOptionsOrCSPDenied(response);
        return;
    }

    // 'frame-ancestors' obviates 'x-frame-options': https://w3c.github.io/webappsec/specs/content-security-policy/#frame-ancestors-and-frame-options
    if (!m_contentSecurityPolicy->isFrameAncestorsEnforced()) {
        HTTPHeaderMap::const_iterator it = response.httpHeaderFields().find(HTTPNames::X_Frame_Options);
        if (it != response.httpHeaderFields().end()) {
            String content = it->value;
            if (frameLoader()->shouldInterruptLoadForXFrameOptions(content, response.url(), mainResourceIdentifier())) {
                String message = "Refused to display '" + response.url().elidedString() + "' in a frame because it set 'X-Frame-Options' to '" + content + "'.";
                ConsoleMessage* consoleMessage = ConsoleMessage::createForRequest(SecurityMessageSource, ErrorMessageLevel, message, response.url(), mainResourceIdentifier());
                frame()->document()->addConsoleMessage(consoleMessage);

                cancelLoadAfterXFrameOptionsOrCSPDenied(response);
                return;
            }
        }
    }

    ASSERT(!m_frame->page()->defersLoading());

    m_response = response;

    if (isArchiveMIMEType(m_response.mimeType()) && m_mainResource->getDataBufferingPolicy() != BufferData)
        m_mainResource->setDataBufferingPolicy(BufferData);

    if (!shouldContinueForResponse()) {
        InspectorInstrumentation::continueWithPolicyIgnore(m_frame, this, m_mainResource->identifier(), m_response, m_mainResource.get());
        m_fetcher->stopFetching();
        return;
    }

    if (m_response.isHTTP()) {
        int status = m_response.httpStatusCode();
        if ((status < 200 || status >= 300) && m_frame->owner())
            m_frame->owner()->renderFallbackContent();
    }
}

void DocumentLoader::ensureWriter(const AtomicString& mimeType, const KURL& overridingURL)
{
    if (m_writer)
        return;

    const AtomicString& encoding = m_frame->host()->overrideEncoding().isNull() ? response().textEncodingName() : m_frame->host()->overrideEncoding();

    // Prepare a DocumentInit before clearing the frame, because it may need to
    // inherit an aliased security context.
    Document* owner = nullptr;
    // TODO(dcheng): This differs from the behavior of both IE and Firefox: the
    // origin is inherited from the document that loaded the URL.
    if (shouldInheritSecurityOriginFromOwner(url())) {
        Frame* ownerFrame = m_frame->tree().parent();
        if (!ownerFrame)
            ownerFrame = m_frame->loader().opener();
        if (ownerFrame && ownerFrame->isLocalFrame())
            owner = toLocalFrame(ownerFrame)->document();
    }
    DocumentInit init(owner, url(), m_frame);
    init.withNewRegistrationContext();
    m_frame->loader().clear();
    ASSERT(m_frame->page());

    ParserSynchronizationPolicy parsingPolicy = AllowAsynchronousParsing;
    if ((m_substituteData.isValid() && m_substituteData.forceSynchronousLoad()) || !Document::threadedParsingEnabledForTesting())
        parsingPolicy = ForceSynchronousParsing;

    m_writer = createWriterFor(init, mimeType, encoding, false, parsingPolicy, overridingURL);
    m_writer->setDocumentWasLoadedAsPartOfNavigation();
    m_frame->document()->maybeHandleHttpRefresh(m_response.httpHeaderField(HTTPNames::Refresh), Document::HttpRefreshFromHeader);
}

void DocumentLoader::commitData(const char* bytes, size_t length)
{
    ASSERT(m_state < MainResourceDone);
    ensureWriter(m_response.mimeType());

    // This can happen if document.close() is called by an event handler while
    // there's still pending incoming data.
    if (m_frame && !m_frame->document()->parsing())
        return;

    if (length)
        m_state = DataReceived;

    m_writer->addData(bytes, length);
}

void DocumentLoader::dataReceived(Resource* resource, const char* data, size_t length)
{
    ASSERT(data);
    ASSERT(length);
    ASSERT_UNUSED(resource, resource == m_mainResource);
    ASSERT(!m_response.isNull());
    ASSERT(!m_frame->page()->defersLoading());

    if (m_inDataReceived) {
        // If this function is reentered, defer processing of the additional
        // data to the top-level invocation. Reentrant calls can occur because
        // of web platform (mis-)features that require running a nested message
        // loop:
        // - alert(), confirm(), prompt()
        // - Detach of plugin elements.
        // - Synchronous XMLHTTPRequest
        m_dataBuffer->append(data, length);
        return;
    }

    TemporaryChange<bool> reentrancyProtector(m_inDataReceived, true);
    processData(data, length);

    // Process data received in reentrant invocations. Note that the
    // invocations of processData() may queue more data in reentrant
    // invocations, so iterate until it's empty.
    const char* segment;
    size_t pos = 0;
    while (size_t length = m_dataBuffer->getSomeData(segment, pos)) {
        processData(segment, length);
        pos += length;
    }
    // All data has been consumed, so flush the buffer.
    m_dataBuffer->clear();
}

void DocumentLoader::processData(const char* data, size_t length)
{
    m_applicationCacheHost->mainResourceDataReceived(data, length);
    m_timeOfLastDataReceived = monotonicallyIncreasingTime();

    if (isArchiveMIMEType(response().mimeType()))
        return;
    commitIfReady();
    if (!frameLoader())
        return;
    commitData(data, length);

    // If we are sending data to MediaDocument, we should stop here
    // and cancel the request.
    if (m_frame && m_frame->document()->isMediaDocument())
        m_fetcher->stopFetching();
}

void DocumentLoader::clearRedirectChain()
{
    m_redirectChain.clear();
}

void DocumentLoader::appendRedirect(const KURL& url)
{
    m_redirectChain.append(url);
}

void DocumentLoader::detachFromFrame()
{
    ASSERT(m_frame);

    // It never makes sense to have a document loader that is detached from its
    // frame have any loads active, so go ahead and kill all the loads.
    m_fetcher->stopFetching();

    // If that load cancellation triggered another detach, leave.
    // (fast/frames/detach-frame-nested-no-crash.html is an example of this.)
    if (!m_frame)
        return;

    m_fetcher->clearContext();
    m_applicationCacheHost->detachFromDocumentLoader();
    m_applicationCacheHost.clear();
    WeakIdentifierMap<DocumentLoader>::notifyObjectDestroyed(this);
    clearMainResourceHandle();
    m_frame = nullptr;
}

void DocumentLoader::clearMainResourceHandle()
{
    if (!m_mainResource)
        return;
    m_mainResource->removeClient(this);
    m_mainResource = nullptr;
}

bool DocumentLoader::maybeCreateArchive()
{
    // Give the archive machinery a crack at this document. If the MIME type is not an archive type, it will return 0.
    if (!isArchiveMIMEType(m_response.mimeType()))
        return false;

    ASSERT(m_mainResource);
    ArchiveResource* mainResource = m_fetcher->createArchive(m_mainResource.get());
    if (!mainResource)
        return false;
    // The origin is the MHTML file, we need to set the base URL to the document encoded in the MHTML so
    // relative URLs are resolved properly.
    ensureWriter(mainResource->mimeType(), mainResource->url());

    // The Document has now been created.
    m_frame->document()->enforceSandboxFlags(SandboxAll);

    commitData(mainResource->data()->data(), mainResource->data()->size());
    return true;
}

const AtomicString& DocumentLoader::responseMIMEType() const
{
    return m_response.mimeType();
}

const KURL& DocumentLoader::unreachableURL() const
{
    return m_substituteData.failingURL();
}

bool DocumentLoader::maybeLoadEmpty()
{
    bool shouldLoadEmpty = !m_substituteData.isValid() && (m_request.url().isEmpty() || SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(m_request.url().protocol()));
    if (!shouldLoadEmpty)
        return false;

    if (m_request.url().isEmpty() && !frameLoader()->stateMachine()->creatingInitialEmptyDocument())
        m_request.setURL(blankURL());
    m_response = ResourceResponse(m_request.url(), "text/html", 0, nullAtom, String());
    finishedLoading(monotonicallyIncreasingTime());
    return true;
}

void DocumentLoader::startLoadingMainResource()
{
    timing().markNavigationStart();
    ASSERT(!m_mainResource);
    ASSERT(m_state == NotStarted);
    m_state = Provisional;

    if (maybeLoadEmpty())
        return;

    ASSERT(timing().navigationStart());
    ASSERT(!timing().fetchStart());
    timing().markFetchStart();

    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, mainResourceLoadOptions,
        (DoNotBufferData, AllowStoredCredentials, ClientRequestedCredentials, CheckContentSecurityPolicy, DocumentContext));
    FetchRequest fetchRequest(m_request, FetchInitiatorTypeNames::document, mainResourceLoadOptions);
    m_mainResource = RawResource::fetchMainResource(fetchRequest, fetcher(), m_substituteData);
    if (!m_mainResource) {
        m_request = ResourceRequest(blankURL());
        maybeLoadEmpty();
        return;
    }
    // A bunch of headers are set when the underlying ResourceLoader is created, and m_request needs to include those.
    // Even when using a cached resource, we may make some modification to the request, e.g. adding the referer header.
    m_request = mainResourceLoader() ? m_mainResource->resourceRequest() : fetchRequest.resourceRequest();
    m_mainResource->addClient(this);
}

void DocumentLoader::endWriting(DocumentWriter* writer)
{
    ASSERT_UNUSED(writer, m_writer == writer);
    m_writer->end();
    m_writer.clear();
}

DocumentWriter* DocumentLoader::createWriterFor(const DocumentInit& init, const AtomicString& mimeType, const AtomicString& encoding, bool dispatchWindowObjectAvailable, ParserSynchronizationPolicy parsingPolicy, const KURL& overridingURL)
{
    LocalFrame* frame = init.frame();

    ASSERT(!frame->document() || !frame->document()->isActive());
    ASSERT(frame->tree().childCount() == 0);

    if (!init.shouldReuseDefaultView())
        frame->setDOMWindow(LocalDOMWindow::create(*frame));

    Document* document = frame->localDOMWindow()->installNewDocument(mimeType, init);

    // This should be set before receivedFirstData().
    if (!overridingURL.isEmpty())
        frame->document()->setBaseURLOverride(overridingURL);

    frame->loader().didInstallNewDocument(dispatchWindowObjectAvailable);

    // This must be called before DocumentWriter is created, otherwise HTML parser
    // will use stale values from HTMLParserOption.
    if (!dispatchWindowObjectAvailable)
        frame->loader().receivedFirstData();

    frame->loader().didBeginDocument();

    return DocumentWriter::create(document, parsingPolicy, mimeType, encoding);
}

const AtomicString& DocumentLoader::mimeType() const
{
    if (m_writer)
        return m_writer->mimeType();
    return m_response.mimeType();
}

// This is only called by FrameLoader::replaceDocumentWhileExecutingJavaScriptURL()
void DocumentLoader::replaceDocumentWhileExecutingJavaScriptURL(const DocumentInit& init, const String& source)
{
    m_writer = createWriterFor(init, mimeType(), m_writer ? m_writer->encoding() : emptyAtom, true, ForceSynchronousParsing);
    if (!source.isNull())
        m_writer->appendReplacingData(source);
    endWriting(m_writer.get());
}

DEFINE_WEAK_IDENTIFIER_MAP(DocumentLoader);

} // namespace blink
