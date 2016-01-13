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

#include "config.h"
#include "core/loader/DocumentLoader.h"

#include "core/FetchInitiatorTypeNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/events/Event.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "platform/Logging.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mhtml/ArchiveResourceCollection.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/plugins/PluginData.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMimeRegistry.h"
#include "public/platform/WebThreadedDataReceiver.h"
#include "wtf/Assertions.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

static bool isArchiveMIMEType(const String& mimeType)
{
    return mimeType == "multipart/related";
}

DocumentLoader::DocumentLoader(LocalFrame* frame, const ResourceRequest& req, const SubstituteData& substituteData)
    : m_frame(frame)
    , m_fetcher(ResourceFetcher::create(this))
    , m_originalRequest(req)
    , m_substituteData(substituteData)
    , m_request(req)
    , m_committed(false)
    , m_isClientRedirect(false)
    , m_replacesCurrentHistoryItem(false)
    , m_loadingMainResource(false)
    , m_timeOfLastDataReceived(0.0)
    , m_applicationCacheHost(adoptPtr(new ApplicationCacheHost(this)))
{
}

FrameLoader* DocumentLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return &m_frame->loader();
}

ResourceLoader* DocumentLoader::mainResourceLoader() const
{
    return m_mainResource ? m_mainResource->loader() : 0;
}

DocumentLoader::~DocumentLoader()
{
    ASSERT(!m_frame || !isLoading());
    m_fetcher->clearDocumentLoader();
    clearMainResourceHandle();
}

unsigned long DocumentLoader::mainResourceIdentifier() const
{
    return m_mainResource ? m_mainResource->identifier() : 0;
}

Document* DocumentLoader::document() const
{
    if (m_frame && m_frame->loader().documentLoader() == this)
        return m_frame->document();
    return 0;
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

void DocumentLoader::updateForSameDocumentNavigation(const KURL& newURL, SameDocumentNavigationSource sameDocumentNavigationSource)
{
    KURL oldURL = m_request.url();
    m_originalRequest.setURL(newURL);
    m_request.setURL(newURL);
    if (sameDocumentNavigationSource == SameDocumentNavigationHistoryApi) {
        m_request.setHTTPMethod("GET");
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

void DocumentLoader::setMainDocumentError(const ResourceError& error)
{
    m_mainDocumentError = error;
}

void DocumentLoader::mainReceivedError(const ResourceError& error)
{
    ASSERT(!error.isNull());
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading() || InspectorInstrumentation::isDebuggerPaused(m_frame));
    m_applicationCacheHost->failedLoadingMainResource();
    if (!frameLoader())
        return;
    setMainDocumentError(error);
    clearMainResourceLoader();
    frameLoader()->receivedMainResourceError(error);
    clearMainResourceHandle();
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources.
// stopLoading will stop all loads initiated by the data source,
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
void DocumentLoader::stopLoading()
{
    RefPtr<LocalFrame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    // In some rare cases, calling FrameLoader::stopLoading could cause isLoading() to return false.
    // (This can happen when there's a single XMLHttpRequest currently loading and stopLoading causes it
    // to stop loading. Because of this, we need to save it so we don't return early.
    bool loading = isLoading();

    if (m_committed) {
        // Attempt to stop the frame if the document loader is loading, or if it is done loading but
        // still  parsing. Failure to do so can cause a world leak.
        Document* doc = m_frame->document();

        if (loading || doc->parsing())
            m_frame->loader().stopLoading();
    }

    if (!loading) {
        m_fetcher->stopFetching();
        return;
    }

    if (m_loadingMainResource) {
        // Stop the main resource loader and let it send the cancelled message.
        cancelMainResourceLoad(ResourceError::cancelledError(m_request.url()));
    } else if (m_fetcher->isFetching()) {
        // The main resource loader already finished loading. Set the cancelled error on the
        // document and let the resourceLoaders send individual cancelled messages below.
        setMainDocumentError(ResourceError::cancelledError(m_request.url()));
    } else {
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        mainReceivedError(ResourceError::cancelledError(m_request.url()));
    }

    m_fetcher->stopFetching();
}

void DocumentLoader::commitIfReady()
{
    if (!m_committed) {
        m_committed = true;
        frameLoader()->commitProvisionalLoad();
    }
}

bool DocumentLoader::isLoading() const
{
    if (document() && document()->hasActiveParser())
        return true;

    return m_loadingMainResource || m_fetcher->isFetching();
}

void DocumentLoader::notifyFinished(Resource* resource)
{
    ASSERT_UNUSED(resource, m_mainResource == resource);
    ASSERT(m_mainResource);

    RefPtr<DocumentLoader> protect(this);

    if (!m_mainResource->errorOccurred() && !m_mainResource->wasCanceled()) {
        finishedLoading(m_mainResource->loadFinishTime());
        return;
    }

    mainReceivedError(m_mainResource->resourceError());
}

void DocumentLoader::finishedLoading(double finishTime)
{
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading() || InspectorInstrumentation::isDebuggerPaused(m_frame));

    RefPtr<DocumentLoader> protect(this);

    double responseEndTime = finishTime;
    if (!responseEndTime)
        responseEndTime = m_timeOfLastDataReceived;
    if (!responseEndTime)
        responseEndTime = monotonicallyIncreasingTime();
    timing()->setResponseEnd(responseEndTime);

    commitIfReady();
    if (!frameLoader())
        return;

    if (!maybeCreateArchive()) {
        // If this is an empty document, it will not have actually been created yet. Commit dummy data so that
        // DocumentWriter::begin() gets called and creates the Document.
        if (!m_writer)
            commitData(0, 0);
    }

    endWriting(m_writer.get());

    if (!m_mainDocumentError.isNull())
        return;
    clearMainResourceLoader();
    if (!frameLoader()->stateMachine()->creatingInitialEmptyDocument())
        frameLoader()->checkLoadComplete();

    // If the document specified an application cache manifest, it violates the author's intent if we store it in the memory cache
    // and deny the appcache the chance to intercept it in the future, so remove from the memory cache.
    if (m_frame) {
        if (m_mainResource && m_frame->document()->hasAppCacheManifest())
            memoryCache()->remove(m_mainResource.get());
    }
    m_applicationCacheHost->finishedLoadingMainResource();
    clearMainResourceHandle();
}

bool DocumentLoader::isRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    int status = redirectResponse.httpStatusCode();
    if (((status >= 301 && status <= 303) || status == 307)
        && m_originalRequest.httpMethod() == "POST")
        return true;

    return false;
}

bool DocumentLoader::shouldContinueForNavigationPolicy(const ResourceRequest& request)
{
    // Don't ask if we are loading an empty URL.
    if (request.url().isEmpty() || m_substituteData.isValid())
        return true;

    // If we're loading content into a subframe, check against the parent's Content Security Policy
    // and kill the load if that check fails.
    // FIXME: CSP checks are broken for OOPI. For now, this policy always allows frames with a remote parent...
    if (m_frame->deprecatedLocalOwner() && !m_frame->deprecatedLocalOwner()->document().contentSecurityPolicy()->allowChildFrameFromSource(request.url())) {
        // Fire a load event, as timing attacks would otherwise reveal that the
        // frame was blocked. This way, it looks like every other cross-origin
        // page load.
        m_frame->document()->enforceSandboxFlags(SandboxOrigin);
        m_frame->owner()->dispatchLoad();
        return false;
    }

    NavigationPolicy policy = m_triggeringAction.policy();
    policy = frameLoader()->client()->decidePolicyForNavigation(request, this, policy);
    if (policy == NavigationPolicyCurrentTab)
        return true;
    if (policy == NavigationPolicyIgnore)
        return false;
    if (!LocalDOMWindow::allowPopUp(*m_frame) && !UserGestureIndicator::processingUserGesture())
        return false;
    frameLoader()->client()->loadURLExternally(request, policy);
    return false;
}

void DocumentLoader::redirectReceived(Resource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(resource, resource == m_mainResource);
    willSendRequest(request, redirectResponse);
}

void DocumentLoader::updateRequest(Resource* resource, const ResourceRequest& request)
{
    ASSERT_UNUSED(resource, resource == m_mainResource);
    m_request = request;
}

static bool isFormSubmission(NavigationType type)
{
    return type == NavigationTypeFormSubmitted || type == NavigationTypeFormResubmitted;
}

void DocumentLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring
    // callbacks is meant to prevent.
    ASSERT(!newRequest.isNull());
    if (isFormSubmission(m_triggeringAction.type()) && !m_frame->document()->contentSecurityPolicy()->allowFormAction(newRequest.url())) {
        cancelMainResourceLoad(ResourceError::cancelledError(newRequest.url()));
        return;
    }

    ASSERT(timing()->fetchStart());
    if (!redirectResponse.isNull()) {
        // If the redirecting url is not allowed to display content from the target origin,
        // then block the redirect.
        RefPtr<SecurityOrigin> redirectingOrigin = SecurityOrigin::create(redirectResponse.url());
        if (!redirectingOrigin->canDisplay(newRequest.url())) {
            FrameLoader::reportLocalLoadFailed(m_frame, newRequest.url().string());
            cancelMainResourceLoad(ResourceError::cancelledError(newRequest.url()));
            return;
        }
        timing()->addRedirect(redirectResponse.url(), newRequest.url());
    }

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    if (newRequest.cachePolicy() == UseProtocolCachePolicy && isRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ReloadBypassingCache);

    // If this is a sub-frame, check for mixed content blocking against the top frame.
    if (m_frame->tree().parent()) {
        // FIXME: This does not yet work with out-of-process iframes.
        Frame* top = m_frame->tree().top();
        if (top->isLocalFrame() && !toLocalFrame(top)->loader().mixedContentChecker()->canRunInsecureContent(toLocalFrame(top)->document()->securityOrigin(), newRequest.url())) {
            cancelMainResourceLoad(ResourceError::cancelledError(newRequest.url()));
            return;
        }
    }

    m_request = newRequest;

    if (redirectResponse.isNull())
        return;

    appendRedirect(newRequest.url());
    frameLoader()->client()->dispatchDidReceiveServerRedirectForProvisionalLoad();
    if (!shouldContinueForNavigationPolicy(newRequest))
        cancelMainResourceLoad(ResourceError::cancelledError(m_request.url()));
}

static bool canShowMIMEType(const String& mimeType, Page* page)
{
    if (blink::Platform::current()->mimeRegistry()->supportsMIMEType(mimeType) == blink::WebMimeRegistry::IsSupported)
        return true;
    PluginData* pluginData = page->pluginData();
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

    if (contentDispositionType(m_response.httpHeaderField("Content-Disposition")) == ContentDispositionAttachment) {
        // The server wants us to download instead of replacing the page contents.
        // Downloading is handled by the embedder, but we still get the initial
        // response so that we can ignore it and clean up properly.
        return false;
    }

    if (!canShowMIMEType(m_response.mimeType(), m_frame->page()))
        return false;

    // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks.
    if (equalIgnoringCase("multipart/related", m_response.mimeType()) && !SchemeRegistry::shouldTreatURLSchemeAsLocal(m_request.url().protocol()))
        return false;

    return true;
}

void DocumentLoader::responseReceived(Resource* resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, m_mainResource == resource);
    RefPtr<DocumentLoader> protect(this);

    m_applicationCacheHost->didReceiveResponseForMainResource(response);

    // The memory cache doesn't understand the application cache or its caching rules. So if a main resource is served
    // from the application cache, ensure we don't save the result for future use. All responses loaded
    // from appcache will have a non-zero appCacheID().
    if (response.appCacheID())
        memoryCache()->remove(m_mainResource.get());

    DEFINE_STATIC_LOCAL(AtomicString, xFrameOptionHeader, ("x-frame-options", AtomicString::ConstructFromLiteral));
    HTTPHeaderMap::const_iterator it = response.httpHeaderFields().find(xFrameOptionHeader);
    if (it != response.httpHeaderFields().end()) {
        String content = it->value;
        ASSERT(m_mainResource);
        unsigned long identifier = mainResourceIdentifier();
        ASSERT(identifier);
        if (frameLoader()->shouldInterruptLoadForXFrameOptions(content, response.url(), identifier)) {
            InspectorInstrumentation::continueAfterXFrameOptionsDenied(m_frame, this, identifier, response);
            String message = "Refused to display '" + response.url().elidedString() + "' in a frame because it set 'X-Frame-Options' to '" + content + "'.";
            frame()->document()->addConsoleMessageWithRequestIdentifier(SecurityMessageSource, ErrorMessageLevel, message, identifier);
            frame()->document()->enforceSandboxFlags(SandboxOrigin);
            if (FrameOwner* owner = frame()->owner())
                owner->dispatchLoad();

            // The load event might have detached this frame. In that case, the load will already have been cancelled during detach.
            if (frameLoader())
                cancelMainResourceLoad(ResourceError::cancelledError(m_request.url()));
            return;
        }
    }

    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());

    m_response = response;

    if (isArchiveMIMEType(m_response.mimeType()) && m_mainResource->dataBufferingPolicy() != BufferData)
        m_mainResource->setDataBufferingPolicy(BufferData);

    if (!shouldContinueForResponse()) {
        InspectorInstrumentation::continueWithPolicyIgnore(m_frame, this, m_mainResource->identifier(), m_response);
        cancelMainResourceLoad(ResourceError::cancelledError(m_request.url()));
        return;
    }

    if (m_response.isHTTP()) {
        int status = m_response.httpStatusCode();
        // FIXME: Fallback content only works if the parent is in the same processs.
        if ((status < 200 || status >= 300) && m_frame->owner()) {
            if (!m_frame->deprecatedLocalOwner()) {
                ASSERT_NOT_REACHED();
            } else if (m_frame->deprecatedLocalOwner()->isObjectElement()) {
                m_frame->deprecatedLocalOwner()->renderFallbackContent();
                // object elements are no longer rendered after we fallback, so don't
                // keep trying to process data from their load
                cancelMainResourceLoad(ResourceError::cancelledError(m_request.url()));
            }
        }
    }
}

void DocumentLoader::ensureWriter(const AtomicString& mimeType, const KURL& overridingURL)
{
    if (m_writer)
        return;

    const AtomicString& encoding = overrideEncoding().isNull() ? response().textEncodingName() : overrideEncoding();
    m_writer = createWriterFor(m_frame, 0, url(), mimeType, encoding, false, false);
    m_writer->setDocumentWasLoadedAsPartOfNavigation();
    // This should be set before receivedFirstData().
    if (!overridingURL.isEmpty())
        m_frame->document()->setBaseURLOverride(overridingURL);

    // Call receivedFirstData() exactly once per load.
    frameLoader()->receivedFirstData();
    m_frame->document()->maybeHandleHttpRefresh(m_response.httpHeaderField("Refresh"), Document::HttpRefreshFromHeader);
}

void DocumentLoader::commitData(const char* bytes, size_t length)
{
    ensureWriter(m_response.mimeType());
    ASSERT(m_frame->document()->parsing());
    m_writer->addData(bytes, length);
}

void DocumentLoader::dataReceived(Resource* resource, const char* data, int length)
{
    ASSERT(data);
    ASSERT(length);
    ASSERT_UNUSED(resource, resource == m_mainResource);
    ASSERT(!m_response.isNull());
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());

    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    RefPtr<LocalFrame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    m_applicationCacheHost->mainResourceDataReceived(data, length);
    m_timeOfLastDataReceived = monotonicallyIncreasingTime();

    commitIfReady();
    if (!frameLoader())
        return;
    if (isArchiveMIMEType(response().mimeType()))
        return;
    commitData(data, length);

    // If we are sending data to MediaDocument, we should stop here
    // and cancel the request.
    if (m_frame && m_frame->document()->isMediaDocument())
        cancelMainResourceLoad(ResourceError::cancelledError(m_request.url()));
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
    RefPtr<LocalFrame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    // It never makes sense to have a document loader that is detached from its
    // frame have any loads active, so go ahead and kill all the loads.
    stopLoading();

    m_applicationCacheHost->setApplicationCache(0);
    InspectorInstrumentation::loaderDetachedFromFrame(m_frame, this);
    m_frame = 0;
}

void DocumentLoader::clearMainResourceLoader()
{
    m_loadingMainResource = false;
}

void DocumentLoader::clearMainResourceHandle()
{
    if (!m_mainResource)
        return;
    m_mainResource->removeClient(this);
    m_mainResource = 0;
}

bool DocumentLoader::maybeCreateArchive()
{
    // Only the top-frame can load MHTML.
    if (m_frame->tree().parent())
        return false;

    // Give the archive machinery a crack at this document. If the MIME type is not an archive type, it will return 0.
    if (!isArchiveMIMEType(m_response.mimeType()))
        return false;

    ASSERT(m_mainResource);
    m_archive = MHTMLArchive::create(m_response.url(), m_mainResource->resourceBuffer());
    // Invalid MHTML.
    if (!m_archive || !m_archive->mainResource()) {
        m_archive.clear();
        return false;
    }

    addAllArchiveResources(m_archive.get());
    ArchiveResource* mainResource = m_archive->mainResource();

    // The origin is the MHTML file, we need to set the base URL to the document encoded in the MHTML so
    // relative URLs are resolved properly.
    ensureWriter(mainResource->mimeType(), m_archive->mainResource()->url());

    // The Document has now been created.
    document()->enforceSandboxFlags(SandboxAll);

    commitData(mainResource->data()->data(), mainResource->data()->size());
    return true;
}

void DocumentLoader::addAllArchiveResources(MHTMLArchive* archive)
{
    ASSERT(archive);
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = adoptPtr(new ArchiveResourceCollection);
    m_archiveResourceCollection->addAllResources(archive);
}

void DocumentLoader::prepareSubframeArchiveLoadIfNeeded()
{
    if (!m_frame->tree().parent() || !m_frame->tree().parent()->isLocalFrame())
        return;

    ArchiveResourceCollection* parentCollection = toLocalFrame(m_frame->tree().parent())->loader().documentLoader()->m_archiveResourceCollection.get();
    if (!parentCollection)
        return;

    m_archive = parentCollection->popSubframeArchive(m_frame->tree().uniqueName(), m_request.url());

    if (!m_archive)
        return;
    addAllArchiveResources(m_archive.get());

    ArchiveResource* mainResource = m_archive->mainResource();
    m_substituteData = SubstituteData(mainResource->data(), mainResource->mimeType(), mainResource->textEncoding(), KURL());
}

bool DocumentLoader::scheduleArchiveLoad(Resource* cachedResource, const ResourceRequest& request)
{
    if (!m_archive)
        return false;

    ASSERT(m_archiveResourceCollection);
    ArchiveResource* archiveResource = m_archiveResourceCollection->archiveResourceForURL(request.url());
    if (!archiveResource) {
        cachedResource->error(Resource::LoadError);
        return true;
    }

    cachedResource->setLoading(true);
    cachedResource->responseReceived(archiveResource->response());
    SharedBuffer* data = archiveResource->data();
    if (data)
        cachedResource->appendData(data->data(), data->size());
    cachedResource->finish();
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

void DocumentLoader::setDefersLoading(bool defers)
{
    // Multiple frames may be loading the same main resource simultaneously. If deferral state changes,
    // each frame's DocumentLoader will try to send a setDefersLoading() to the same underlying ResourceLoader. Ensure only
    // the "owning" DocumentLoader does so, as setDefersLoading() is not resilient to setting the same value repeatedly.
    if (mainResourceLoader() && mainResourceLoader()->isLoadedBy(m_fetcher.get()))
        mainResourceLoader()->setDefersLoading(defers);

    m_fetcher->setDefersLoading(defers);
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
    RefPtr<DocumentLoader> protect(this);
    m_mainDocumentError = ResourceError();
    timing()->markNavigationStart();
    ASSERT(!m_mainResource);
    ASSERT(!m_loadingMainResource);
    m_loadingMainResource = true;

    if (maybeLoadEmpty())
        return;

    ASSERT(timing()->navigationStart());
    ASSERT(!timing()->fetchStart());
    timing()->markFetchStart();
    willSendRequest(m_request, ResourceResponse());

    // willSendRequest() may lead to our LocalFrame being detached or cancelling the load via nulling the ResourceRequest.
    if (!m_frame || m_request.isNull())
        return;

    m_applicationCacheHost->willStartLoadingMainResource(m_request);
    prepareSubframeArchiveLoadIfNeeded();

    ResourceRequest request(m_request);
    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, mainResourceLoadOptions,
        (SniffContent, DoNotBufferData, AllowStoredCredentials, ClientRequestedCredentials, CheckContentSecurityPolicy, DocumentContext));
    FetchRequest cachedResourceRequest(request, FetchInitiatorTypeNames::document, mainResourceLoadOptions);
    m_mainResource = m_fetcher->fetchMainResource(cachedResourceRequest, m_substituteData);
    if (!m_mainResource) {
        m_request = ResourceRequest();
        // If the load was aborted by clearing m_request, it's possible the ApplicationCacheHost
        // is now in a state where starting an empty load will be inconsistent. Replace it with
        // a new ApplicationCacheHost.
        m_applicationCacheHost = adoptPtr(new ApplicationCacheHost(this));
        maybeLoadEmpty();
        return;
    }
    m_mainResource->addClient(this);

    // A bunch of headers are set when the underlying ResourceLoader is created, and m_request needs to include those.
    if (mainResourceLoader())
        request = mainResourceLoader()->originalRequest();
    // If there was a fragment identifier on m_request, the cache will have stripped it. m_request should include
    // the fragment identifier, so add that back in.
    if (equalIgnoringFragmentIdentifier(m_request.url(), request.url()))
        request.setURL(m_request.url());
    m_request = request;
}

void DocumentLoader::cancelMainResourceLoad(const ResourceError& resourceError)
{
    RefPtr<DocumentLoader> protect(this);
    ResourceError error = resourceError.isNull() ? ResourceError::cancelledError(m_request.url()) : resourceError;

    if (mainResourceLoader())
        mainResourceLoader()->cancel(error);

    mainReceivedError(error);
}

void DocumentLoader::attachThreadedDataReceiver(PassOwnPtr<blink::WebThreadedDataReceiver> threadedDataReceiver)
{
    if (mainResourceLoader())
        mainResourceLoader()->attachThreadedDataReceiver(threadedDataReceiver);
}

void DocumentLoader::endWriting(DocumentWriter* writer)
{
    ASSERT_UNUSED(writer, m_writer == writer);
    m_writer->end();
    m_writer.clear();
}

PassRefPtrWillBeRawPtr<DocumentWriter> DocumentLoader::createWriterFor(LocalFrame* frame, const Document* ownerDocument, const KURL& url, const AtomicString& mimeType, const AtomicString& encoding, bool userChosen, bool dispatch)
{
    // Create a new document before clearing the frame, because it may need to
    // inherit an aliased security context.
    DocumentInit init(url, frame);
    init.withNewRegistrationContext();

    // In some rare cases, we'll re-used a LocalDOMWindow for a new Document. For example,
    // when a script calls window.open("..."), the browser gives JavaScript a window
    // synchronously but kicks off the load in the window asynchronously. Web sites
    // expect that modifications that they make to the window object synchronously
    // won't be blown away when the network load commits. To make that happen, we
    // "securely transition" the existing LocalDOMWindow to the Document that results from
    // the network load. See also SecurityContext::isSecureTransitionTo.
    bool shouldReuseDefaultView = frame->loader().stateMachine()->isDisplayingInitialEmptyDocument() && frame->document()->isSecureTransitionTo(url);

    frame->loader().clear();

    if (frame->document())
        frame->document()->prepareForDestruction();

    if (!shouldReuseDefaultView)
        frame->setDOMWindow(LocalDOMWindow::create(*frame));

    RefPtrWillBeRawPtr<Document> document = frame->domWindow()->installNewDocument(mimeType, init);
    if (ownerDocument) {
        document->setCookieURL(ownerDocument->cookieURL());
        document->setSecurityOrigin(ownerDocument->securityOrigin());
    }

    frame->loader().didBeginDocument(dispatch);

    return DocumentWriter::create(document.get(), mimeType, encoding, userChosen);
}

const AtomicString& DocumentLoader::mimeType() const
{
    if (m_writer)
        return m_writer->mimeType();
    return m_response.mimeType();
}

void DocumentLoader::setUserChosenEncoding(const String& charset)
{
    if (m_writer)
        m_writer->setUserChosenEncoding(charset);
}

// This is only called by ScriptController::executeScriptIfJavaScriptURL
// and always contains the result of evaluating a javascript: url.
// This is the <iframe src="javascript:'html'"> case.
void DocumentLoader::replaceDocument(const String& source, Document* ownerDocument)
{
    m_frame->loader().stopAllLoaders();
    m_writer = createWriterFor(m_frame, ownerDocument, m_frame->document()->url(), mimeType(), m_writer ? m_writer->encoding() : emptyAtom,  m_writer ? m_writer->encodingWasChosenByUser() : false, true);
    if (!source.isNull())
        m_writer->appendReplacingData(source);
    endWriting(m_writer.get());
}

} // namespace WebCore
