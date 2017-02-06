/*
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
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

#include "web/AssociatedURLLoader.h"

#include "core/dom/ContextLifecycleObserver.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchUtils.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "platform/Timer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceError.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebDataSource.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/HashSet.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/WTFString.h"
#include <limits.h>
#include <memory>

namespace blink {

namespace {

class HTTPRequestHeaderValidator : public WebHTTPHeaderVisitor {
    WTF_MAKE_NONCOPYABLE(HTTPRequestHeaderValidator);
public:
    HTTPRequestHeaderValidator() : m_isSafe(true) { }

    void visitHeader(const WebString& name, const WebString& value);
    bool isSafe() const { return m_isSafe; }

private:
    bool m_isSafe;
};

void HTTPRequestHeaderValidator::visitHeader(const WebString& name, const WebString& value)
{
    m_isSafe = m_isSafe && isValidHTTPToken(name) && !FetchUtils::isForbiddenHeaderName(name) && isValidHTTPHeaderValue(value);
}

} // namespace

// This class bridges the interface differences between WebCore and WebKit loader clients.
// It forwards its ThreadableLoaderClient notifications to a WebURLLoaderClient.
class AssociatedURLLoader::ClientAdapter final : public DocumentThreadableLoaderClient {
    WTF_MAKE_NONCOPYABLE(ClientAdapter);
public:
    static std::unique_ptr<ClientAdapter> create(AssociatedURLLoader*, WebURLLoaderClient*, const WebURLLoaderOptions&);

    // ThreadableLoaderClient
    void didSendData(unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/) override;
    void didReceiveResponse(unsigned long, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) override;
    void didDownloadData(int /*dataLength*/) override;
    void didReceiveData(const char*, unsigned /*dataLength*/) override;
    void didReceiveCachedMetadata(const char*, int /*dataLength*/) override;
    void didFinishLoading(unsigned long /*identifier*/, double /*finishTime*/) override;
    void didFail(const ResourceError&) override;
    void didFailRedirectCheck() override;
    // DocumentThreadableLoaderClient
    void willFollowRedirect(ResourceRequest& /*newRequest*/, const ResourceResponse& /*redirectResponse*/) override;

    // Sets an error to be reported back to the client, asychronously.
    void setDelayedError(const ResourceError&);

    // Enables forwarding of error notifications to the WebURLLoaderClient. These must be
    // deferred until after the call to AssociatedURLLoader::loadAsynchronously() completes.
    void enableErrorNotifications();

    // Stops loading and releases the DocumentThreadableLoader as early as possible.
    WebURLLoaderClient* releaseClient()
    {
        WebURLLoaderClient* client = m_client;
        m_client = nullptr;
        return client;
    }

private:
    ClientAdapter(AssociatedURLLoader*, WebURLLoaderClient*, const WebURLLoaderOptions&);

    void notifyError(Timer<ClientAdapter>*);

    AssociatedURLLoader* m_loader;
    WebURLLoaderClient* m_client;
    WebURLLoaderOptions m_options;
    WebURLError m_error;

    Timer<ClientAdapter> m_errorTimer;
    bool m_enableErrorNotifications;
    bool m_didFail;
};

std::unique_ptr<AssociatedURLLoader::ClientAdapter> AssociatedURLLoader::ClientAdapter::create(AssociatedURLLoader* loader, WebURLLoaderClient* client, const WebURLLoaderOptions& options)
{
    return wrapUnique(new ClientAdapter(loader, client, options));
}

AssociatedURLLoader::ClientAdapter::ClientAdapter(AssociatedURLLoader* loader, WebURLLoaderClient* client, const WebURLLoaderOptions& options)
    : m_loader(loader)
    , m_client(client)
    , m_options(options)
    , m_errorTimer(this, &ClientAdapter::notifyError)
    , m_enableErrorNotifications(false)
    , m_didFail(false)
{
    DCHECK(m_loader);
    DCHECK(m_client);
}

void AssociatedURLLoader::ClientAdapter::willFollowRedirect(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (!m_client)
        return;

    WrappedResourceRequest wrappedNewRequest(newRequest);
    WrappedResourceResponse wrappedRedirectResponse(redirectResponse);
    m_client->willFollowRedirect(m_loader, wrappedNewRequest, wrappedRedirectResponse);
}

void AssociatedURLLoader::ClientAdapter::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!m_client)
        return;

    m_client->didSendData(m_loader, bytesSent, totalBytesToBeSent);
}

void AssociatedURLLoader::ClientAdapter::didReceiveResponse(unsigned long, const ResourceResponse& response, std::unique_ptr<WebDataConsumerHandle> handle)
{
    ASSERT_UNUSED(handle, !handle);
    if (!m_client)
        return;

    if (m_options.exposeAllResponseHeaders || m_options.crossOriginRequestPolicy != WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl) {
        // Use the original ResourceResponse.
        m_client->didReceiveResponse(m_loader, WrappedResourceResponse(response));
        return;
    }

    HTTPHeaderSet exposedHeaders;
    extractCorsExposedHeaderNamesList(response, exposedHeaders);
    HTTPHeaderSet blockedHeaders;
    for (const auto& header : response.httpHeaderFields()) {
        if (FetchUtils::isForbiddenResponseHeaderName(header.key) || (!isOnAccessControlResponseHeaderWhitelist(header.key) && !exposedHeaders.contains(header.key)))
            blockedHeaders.add(header.key);
    }

    if (blockedHeaders.isEmpty()) {
        // Use the original ResourceResponse.
        m_client->didReceiveResponse(m_loader, WrappedResourceResponse(response));
        return;
    }

    // If there are blocked headers, copy the response so we can remove them.
    WebURLResponse validatedResponse = WrappedResourceResponse(response);
    for (const auto& header : blockedHeaders)
        validatedResponse.clearHTTPHeaderField(header);
    m_client->didReceiveResponse(m_loader, validatedResponse);
}

void AssociatedURLLoader::ClientAdapter::didDownloadData(int dataLength)
{
    if (!m_client)
        return;

    m_client->didDownloadData(m_loader, dataLength, -1);
}

void AssociatedURLLoader::ClientAdapter::didReceiveData(const char* data, unsigned dataLength)
{
    if (!m_client)
        return;

    CHECK_LE(dataLength, static_cast<unsigned>(std::numeric_limits<int>::max()));

    m_client->didReceiveData(m_loader, data, dataLength, -1);
}

void AssociatedURLLoader::ClientAdapter::didReceiveCachedMetadata(const char* data, int dataLength)
{
    if (!m_client)
        return;

    m_client->didReceiveCachedMetadata(m_loader, data, dataLength);
}

void AssociatedURLLoader::ClientAdapter::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (!m_client)
        return;

    m_loader->clientAdapterDone();

    releaseClient()->didFinishLoading(m_loader, finishTime, WebURLLoaderClient::kUnknownEncodedDataLength);
    // |this| may be dead here.
}

void AssociatedURLLoader::ClientAdapter::didFail(const ResourceError& error)
{
    if (!m_client)
        return;

    m_loader->clientAdapterDone();

    m_didFail = true;
    m_error = WebURLError(error);
    if (m_enableErrorNotifications)
        notifyError(&m_errorTimer);
}

void AssociatedURLLoader::ClientAdapter::didFailRedirectCheck()
{
    didFail(ResourceError());
}

void AssociatedURLLoader::ClientAdapter::enableErrorNotifications()
{
    m_enableErrorNotifications = true;
    // If an error has already been received, start a timer to report it to the client
    // after AssociatedURLLoader::loadAsynchronously has returned to the caller.
    if (m_didFail)
        m_errorTimer.startOneShot(0, BLINK_FROM_HERE);
}

void AssociatedURLLoader::ClientAdapter::notifyError(Timer<ClientAdapter>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_errorTimer);

    if (!m_client)
        return;

    releaseClient()->didFail(m_loader, m_error);
    // |this| may be dead here.
}

class AssociatedURLLoader::Observer final : public GarbageCollected<Observer>, public ContextLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(Observer);
public:
    Observer(AssociatedURLLoader* parent, Document* document)
        : ContextLifecycleObserver(document)
        , m_parent(parent)
    {
    }

    void dispose()
    {
        m_parent = nullptr;
        clearContext();
    }

    void contextDestroyed() override
    {
        if (m_parent)
            m_parent->documentDestroyed();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ContextLifecycleObserver::trace(visitor);
    }

    AssociatedURLLoader* m_parent;
};

AssociatedURLLoader::AssociatedURLLoader(WebLocalFrameImpl* frameImpl, const WebURLLoaderOptions& options)
    : m_client(nullptr)
    , m_options(options)
    , m_observer(new Observer(this, frameImpl->frame()->document()))
{
}

AssociatedURLLoader::~AssociatedURLLoader()
{
    cancel();
}

#define STATIC_ASSERT_ENUM(a, b)                              \
    static_assert(static_cast<int>(a) == static_cast<int>(b), \
        "mismatching enum: " #a)

STATIC_ASSERT_ENUM(WebURLLoaderOptions::CrossOriginRequestPolicyDeny, DenyCrossOriginRequests);
STATIC_ASSERT_ENUM(WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl, UseAccessControl);
STATIC_ASSERT_ENUM(WebURLLoaderOptions::CrossOriginRequestPolicyAllow, AllowCrossOriginRequests);

STATIC_ASSERT_ENUM(WebURLLoaderOptions::ConsiderPreflight, ConsiderPreflight);
STATIC_ASSERT_ENUM(WebURLLoaderOptions::ForcePreflight, ForcePreflight);
STATIC_ASSERT_ENUM(WebURLLoaderOptions::PreventPreflight, PreventPreflight);

void AssociatedURLLoader::loadSynchronously(const WebURLRequest& request, WebURLResponse& response, WebURLError& error, WebData& data)
{
    DCHECK(0); // Synchronous loading is not supported.
}

void AssociatedURLLoader::loadAsynchronously(const WebURLRequest& request, WebURLLoaderClient* client)
{
    DCHECK(!m_client);
    DCHECK(!m_loader);
    DCHECK(!m_clientAdapter);

    DCHECK(client);

    bool allowLoad = true;
    WebURLRequest newRequest(request);
    if (m_options.untrustedHTTP) {
        WebString method = newRequest.httpMethod();
        allowLoad = m_observer && isValidHTTPToken(method) && FetchUtils::isUsefulMethod(method);
        if (allowLoad) {
            newRequest.setHTTPMethod(FetchUtils::normalizeMethod(method));
            HTTPRequestHeaderValidator validator;
            newRequest.visitHTTPHeaderFields(&validator);
            allowLoad = validator.isSafe();
        }
    }

    m_client = client;
    m_clientAdapter = ClientAdapter::create(this, client, m_options);

    if (allowLoad) {
        ThreadableLoaderOptions options;
        options.preflightPolicy = static_cast<PreflightPolicy>(m_options.preflightPolicy);
        options.crossOriginRequestPolicy = static_cast<CrossOriginRequestPolicy>(m_options.crossOriginRequestPolicy);

        ResourceLoaderOptions resourceLoaderOptions;
        resourceLoaderOptions.allowCredentials = m_options.allowCredentials ? AllowStoredCredentials : DoNotAllowStoredCredentials;
        resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;

        const ResourceRequest& webcoreRequest = newRequest.toResourceRequest();
        if (webcoreRequest.requestContext() == WebURLRequest::RequestContextUnspecified) {
            // FIXME: We load URLs without setting a TargetType (and therefore a request context) in several
            // places in content/ (P2PPortAllocatorSession::AllocateLegacyRelaySession, for example). Remove
            // this once those places are patched up.
            newRequest.setRequestContext(WebURLRequest::RequestContextInternal);
        }

        Document* document = toDocument(m_observer->lifecycleContext());
        DCHECK(document);
        m_loader = DocumentThreadableLoader::create(*document, m_clientAdapter.get(), options, resourceLoaderOptions);
        m_loader->start(webcoreRequest);
    }

    if (!m_loader) {
        // FIXME: return meaningful error codes.
        m_clientAdapter->didFail(ResourceError());
    }
    m_clientAdapter->enableErrorNotifications();
}

void AssociatedURLLoader::cancel()
{
    disposeObserver();
    cancelLoader();
    releaseClient();
}

void AssociatedURLLoader::clientAdapterDone()
{
    disposeObserver();
    releaseClient();
}

void AssociatedURLLoader::cancelLoader()
{
    if (!m_clientAdapter)
        return;

    // Prevent invocation of the WebURLLoaderClient methods.
    m_clientAdapter->releaseClient();

    if (m_loader) {
        m_loader->cancel();
        m_loader.reset();
    }
    m_clientAdapter.reset();
}

void AssociatedURLLoader::setDefersLoading(bool defersLoading)
{
    if (m_loader)
        m_loader->setDefersLoading(defersLoading);
}

void AssociatedURLLoader::setLoadingTaskRunner(blink::WebTaskRunner*)
{
    // TODO(alexclarke): Maybe support this one day if it proves worthwhile.
}

void AssociatedURLLoader::documentDestroyed()
{
    disposeObserver();
    cancelLoader();

    if (!m_client)
        return;

    releaseClient()->didFail(this, ResourceError());
    // |this| may be dead here.
}

void AssociatedURLLoader::disposeObserver()
{
    if (!m_observer)
        return;

    // TODO(tyoshino): Remove this assert once Document is fixed so that
    // contextDestroyed() is invoked for all kinds of Documents.
    //
    // Currently, the method of detecting Document destruction implemented here
    // doesn't work for all kinds of Documents. In case we reached here after
    // the Oilpan is destroyed, we just crash the renderer process to prevent
    // UaF.
    //
    // We could consider just skipping the rest of code in case
    // ThreadState::current() is null. However, the fact we reached here
    // without cancelling the loader means that it's possible there're some
    // non-Blink non-on-heap objects still facing on-heap Blink objects. E.g.
    // there could be a WebURLLoader instance behind the
    // DocumentThreadableLoader instance. So, for safety, we chose to just
    // crash here.
    RELEASE_ASSERT(ThreadState::current());

    m_observer->dispose();
    m_observer = nullptr;
}

} // namespace blink
