/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "core/fetch/Resource.h"

#include "core/fetch/CachedMetadata.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceClientOrObserverWalker.h"
#include "core/fetch/ResourceLoader.h"
#include "core/inspector/InstanceCounters.h"
#include "platform/Histogram.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebSecurityOrigin.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <memory>

namespace blink {

// These response headers are not copied from a revalidated response to the
// cached response headers. For compatibility, this list is based on Chromium's
// net/http/http_response_headers.cc.
const char* const headersToIgnoreAfterRevalidation[] = {
    "allow",
    "connection",
    "etag",
    "expires",
    "keep-alive",
    "last-modified",
    "proxy-authenticate",
    "proxy-connection",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "www-authenticate",
    "x-frame-options",
    "x-xss-protection",
};

// Some header prefixes mean "Don't copy this header from a 304 response.".
// Rather than listing all the relevant headers, we can consolidate them into
// this list, also grabbed from Chromium's net/http/http_response_headers.cc.
const char* const headerPrefixesToIgnoreAfterRevalidation[] = {
    "content-",
    "x-content-",
    "x-webkit-"
};

static inline bool shouldUpdateHeaderAfterRevalidation(const AtomicString& header)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(headersToIgnoreAfterRevalidation); i++) {
        if (equalIgnoringCase(header, headersToIgnoreAfterRevalidation[i]))
            return false;
    }
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(headerPrefixesToIgnoreAfterRevalidation); i++) {
        if (header.startsWith(headerPrefixesToIgnoreAfterRevalidation[i], TextCaseInsensitive))
            return false;
    }
    return true;
}

class Resource::CachedMetadataHandlerImpl : public CachedMetadataHandler {
public:
    static Resource::CachedMetadataHandlerImpl* create(Resource* resource)
    {
        return new CachedMetadataHandlerImpl(resource);
    }
    ~CachedMetadataHandlerImpl() override {}
    DECLARE_VIRTUAL_TRACE();
    void setCachedMetadata(unsigned, const char*, size_t, CacheType) override;
    void clearCachedMetadata(CacheType) override;
    CachedMetadata* cachedMetadata(unsigned) const override;
    String encoding() const override;
    // Sets the serialized metadata retrieved from the platform's cache.
    void setSerializedCachedMetadata(const char*, size_t);

protected:
    explicit CachedMetadataHandlerImpl(Resource*);
    virtual void sendToPlatform();
    const ResourceResponse& response() const { return m_resource->response(); }

    RefPtr<CachedMetadata> m_cachedMetadata;

private:
    Member<Resource> m_resource;
};

Resource::CachedMetadataHandlerImpl::CachedMetadataHandlerImpl(Resource* resource)
    : m_resource(resource)
{
}

DEFINE_TRACE(Resource::CachedMetadataHandlerImpl)
{
    visitor->trace(m_resource);
    CachedMetadataHandler::trace(visitor);
}

void Resource::CachedMetadataHandlerImpl::setCachedMetadata(unsigned dataTypeID, const char* data, size_t size, CachedMetadataHandler::CacheType cacheType)
{
    // Currently, only one type of cached metadata per resource is supported.
    // If the need arises for multiple types of metadata per resource this could
    // be enhanced to store types of metadata in a map.
    ASSERT(!m_cachedMetadata);
    m_cachedMetadata = CachedMetadata::create(dataTypeID, data, size);
    if (cacheType == CachedMetadataHandler::SendToPlatform)
        sendToPlatform();
}

void Resource::CachedMetadataHandlerImpl::clearCachedMetadata(CachedMetadataHandler::CacheType cacheType)
{
    m_cachedMetadata.clear();
    if (cacheType == CachedMetadataHandler::SendToPlatform)
        sendToPlatform();
}

CachedMetadata* Resource::CachedMetadataHandlerImpl::cachedMetadata(unsigned dataTypeID) const
{
    if (!m_cachedMetadata || m_cachedMetadata->dataTypeID() != dataTypeID)
        return nullptr;
    return m_cachedMetadata.get();
}

String Resource::CachedMetadataHandlerImpl::encoding() const
{
    return m_resource->encoding();
}

void Resource::CachedMetadataHandlerImpl::setSerializedCachedMetadata(const char* data, size_t size)
{
    // We only expect to receive cached metadata from the platform once.
    // If this triggers, it indicates an efficiency problem which is most
    // likely unexpected in code designed to improve performance.
    ASSERT(!m_cachedMetadata);
    m_cachedMetadata = CachedMetadata::deserialize(data, size);
}

void Resource::CachedMetadataHandlerImpl::sendToPlatform()
{
    if (m_cachedMetadata) {
        const Vector<char>& serializedData = m_cachedMetadata->serialize();
        Platform::current()->cacheMetadata(response().url(), response().responseTime(), serializedData.data(), serializedData.size());
    } else {
        Platform::current()->cacheMetadata(response().url(), response().responseTime(), nullptr, 0);
    }
}

class Resource::ServiceWorkerResponseCachedMetadataHandler : public Resource::CachedMetadataHandlerImpl {
public:
    static Resource::CachedMetadataHandlerImpl* create(Resource* resource, SecurityOrigin* securityOrigin)
    {
        return new ServiceWorkerResponseCachedMetadataHandler(resource, securityOrigin);
    }
    ~ServiceWorkerResponseCachedMetadataHandler() override {}
    DECLARE_VIRTUAL_TRACE();

protected:
    void sendToPlatform() override;

private:
    explicit ServiceWorkerResponseCachedMetadataHandler(Resource*, SecurityOrigin*);
    String m_cacheStorageCacheName;
    RefPtr<SecurityOrigin> m_securityOrigin;
};

Resource::ServiceWorkerResponseCachedMetadataHandler::ServiceWorkerResponseCachedMetadataHandler(Resource* resource, SecurityOrigin* securityOrigin)
    : CachedMetadataHandlerImpl(resource)
    , m_securityOrigin(securityOrigin)
{
}

DEFINE_TRACE(Resource::ServiceWorkerResponseCachedMetadataHandler)
{
    CachedMetadataHandlerImpl::trace(visitor);
}

void Resource::ServiceWorkerResponseCachedMetadataHandler::sendToPlatform()
{
    // We don't support sending the metadata to the platform when the response
    // was directly fetched via a ServiceWorker
    // (eg: FetchEvent.respondWith(fetch(FetchEvent.request))) to prevent an
    // attacker's Service Worker from poisoning the metadata cache of HTTPCache.
    if (response().cacheStorageCacheName().isNull())
        return;

    if (m_cachedMetadata) {
        const Vector<char>& serializedData = m_cachedMetadata->serialize();
        Platform::current()->cacheMetadataInCacheStorage(response().url(), response().responseTime(), serializedData.data(), serializedData.size(), WebSecurityOrigin(m_securityOrigin), response().cacheStorageCacheName());
    } else {
        Platform::current()->cacheMetadataInCacheStorage(response().url(), response().responseTime(), nullptr, 0, WebSecurityOrigin(m_securityOrigin), response().cacheStorageCacheName());
    }
}

class Resource::ResourceCallback final : public GarbageCollectedFinalized<ResourceCallback> {
public:
    static ResourceCallback& callbackHandler();
    DECLARE_TRACE();
    void schedule(Resource*);
    void cancel(Resource*);
    bool isScheduled(Resource*) const;
private:
    ResourceCallback();

    void runTask();
    std::unique_ptr<CancellableTaskFactory> m_callbackTaskFactory;
    HeapHashSet<Member<Resource>> m_resourcesWithPendingClients;
};

Resource::ResourceCallback& Resource::ResourceCallback::callbackHandler()
{
    // Oilpan + LSan: as the callbackHandler() singleton is used by Resource
    // and ResourcePtr finalizers, it cannot be released upon shutdown in
    // preparation for leak detection.
    //
    // Keep it out of LSan's reach instead.
    LEAK_SANITIZER_DISABLED_SCOPE;
    DEFINE_STATIC_LOCAL(ResourceCallback, callbackHandler, (new ResourceCallback));
    return callbackHandler;
}

DEFINE_TRACE(Resource::ResourceCallback)
{
    visitor->trace(m_resourcesWithPendingClients);
}

Resource::ResourceCallback::ResourceCallback()
    : m_callbackTaskFactory(CancellableTaskFactory::create(this, &ResourceCallback::runTask))
{
}

void Resource::ResourceCallback::schedule(Resource* resource)
{
    if (!m_callbackTaskFactory->isPending())
        Platform::current()->currentThread()->scheduler()->loadingTaskRunner()->postTask(BLINK_FROM_HERE, m_callbackTaskFactory->cancelAndCreate());
    m_resourcesWithPendingClients.add(resource);
}

void Resource::ResourceCallback::cancel(Resource* resource)
{
    m_resourcesWithPendingClients.remove(resource);
    if (m_callbackTaskFactory->isPending() && m_resourcesWithPendingClients.isEmpty())
        m_callbackTaskFactory->cancel();
}

bool Resource::ResourceCallback::isScheduled(Resource* resource) const
{
    return m_resourcesWithPendingClients.contains(resource);
}

void Resource::ResourceCallback::runTask()
{
    HeapVector<Member<Resource>> resources;
    for (const Member<Resource>& resource : m_resourcesWithPendingClients)
        resources.append(resource.get());
    m_resourcesWithPendingClients.clear();

    for (const auto& resource : resources)
        resource->finishPendingClients();
}

Resource::Resource(const ResourceRequest& request, Type type, const ResourceLoaderOptions& options)
    : m_resourceRequest(request)
    , m_options(options)
    , m_responseTimestamp(currentTime())
    , m_cancelTimer(this, &Resource::cancelTimerFired)
    , m_loadFinishTime(0)
    , m_identifier(0)
    , m_encodedSize(0)
    , m_decodedSize(0)
    , m_overheadSize(calculateOverheadSize())
    , m_preloadCount(0)
    , m_preloadDiscoveryTime(0.0)
    , m_cacheIdentifier(MemoryCache::defaultCacheIdentifier())
    , m_preloadResult(PreloadNotReferenced)
    , m_type(type)
    , m_status(NotStarted)
    , m_needsSynchronousCacheHit(false)
    , m_linkPreload(false)
    , m_isRevalidating(false)
{
    ASSERT(m_type == unsigned(type)); // m_type is a bitfield, so this tests careless updates of the enum.
    InstanceCounters::incrementCounter(InstanceCounters::ResourceCounter);

    // Currently we support the metadata caching only for HTTP family.
    if (m_resourceRequest.url().protocolIsInHTTPFamily())
        m_cacheHandler = CachedMetadataHandlerImpl::create(this);
}

Resource::~Resource()
{
    InstanceCounters::decrementCounter(InstanceCounters::ResourceCounter);
}

DEFINE_TRACE(Resource)
{
    visitor->trace(m_loader);
    visitor->trace(m_cacheHandler);
}

void Resource::setLoader(ResourceLoader* loader)
{
    RELEASE_ASSERT(!m_loader);
    ASSERT(stillNeedsLoad());
    m_loader = loader;
    m_status = Pending;
}

void Resource::checkNotify()
{
    notifyClientsInternal(MarkFinishedOption::ShouldMarkFinished);
}

void Resource::notifyClientsInternal(MarkFinishedOption markFinishedOption)
{
    if (isLoading())
        return;

    ResourceClientWalker<ResourceClient> w(m_clients);
    while (ResourceClient* c = w.next()) {
        if (markFinishedOption == MarkFinishedOption::ShouldMarkFinished)
            markClientFinished(c);
        c->notifyFinished(this);
    }
}

void Resource::markClientFinished(ResourceClient* client)
{
    if (m_clients.contains(client)) {
        m_finishedClients.add(client);
        m_clients.remove(client);
    }
}

void Resource::appendData(const char* data, size_t length)
{
    TRACE_EVENT0("blink", "Resource::appendData");
    DCHECK(!m_isRevalidating);
    ASSERT(!errorOccurred());
    if (m_options.dataBufferingPolicy == DoNotBufferData)
        return;
    if (m_data)
        m_data->append(data, length);
    else
        m_data = SharedBuffer::createPurgeable(data, length);
    setEncodedSize(m_data->size());
}

void Resource::setResourceBuffer(PassRefPtr<SharedBuffer> resourceBuffer)
{
    DCHECK(!m_isRevalidating);
    ASSERT(!errorOccurred());
    ASSERT(m_options.dataBufferingPolicy == BufferData);
    m_data = resourceBuffer;
    setEncodedSize(m_data->size());
}

void Resource::setDataBufferingPolicy(DataBufferingPolicy dataBufferingPolicy)
{
    m_options.dataBufferingPolicy = dataBufferingPolicy;
    m_data.clear();
    setEncodedSize(0);
}

void Resource::error(const ResourceError& error)
{
    ASSERT(!error.isNull());
    m_error = error;
    m_isRevalidating = false;

    if (m_error.isCancellation() || !isPreloaded())
        memoryCache()->remove(this);

    setStatus(LoadError);
    ASSERT(errorOccurred());
    m_data.clear();
    m_loader = nullptr;
    checkNotify();
}

void Resource::finish(double loadFinishTime)
{
    DCHECK(!m_isRevalidating);
    m_loadFinishTime = loadFinishTime;
    if (!errorOccurred())
        m_status = Cached;
    m_loader = nullptr;
    checkNotify();
}

AtomicString Resource::httpContentType() const
{
    return extractMIMETypeFromMediaType(m_response.httpHeaderField(HTTPNames::Content_Type).lower());
}

bool Resource::passesAccessControlCheck(SecurityOrigin* securityOrigin) const
{
    String ignoredErrorDescription;
    return passesAccessControlCheck(securityOrigin, ignoredErrorDescription);
}

bool Resource::passesAccessControlCheck(SecurityOrigin* securityOrigin, String& errorDescription) const
{
    return blink::passesAccessControlCheck(m_response, lastResourceRequest().allowStoredCredentials() ? AllowStoredCredentials : DoNotAllowStoredCredentials, securityOrigin, errorDescription, lastResourceRequest().requestContext());
}

bool Resource::isEligibleForIntegrityCheck(SecurityOrigin* securityOrigin) const
{
    String ignoredErrorDescription;
    return securityOrigin->canRequest(resourceRequest().url()) || passesAccessControlCheck(securityOrigin, ignoredErrorDescription);
}

static double currentAge(const ResourceResponse& response, double responseTimestamp)
{
    // RFC2616 13.2.3
    // No compensation for latency as that is not terribly important in practice
    double dateValue = response.date();
    double apparentAge = std::isfinite(dateValue) ? std::max(0., responseTimestamp - dateValue) : 0;
    double ageValue = response.age();
    double correctedReceivedAge = std::isfinite(ageValue) ? std::max(apparentAge, ageValue) : apparentAge;
    double residentTime = currentTime() - responseTimestamp;
    return correctedReceivedAge + residentTime;
}

double Resource::currentAge() const
{
    return blink::currentAge(m_response, m_responseTimestamp);
}

static double freshnessLifetime(ResourceResponse& response, double responseTimestamp)
{
#if !OS(ANDROID)
    // On desktop, local files should be reloaded in case they change.
    if (response.url().isLocalFile())
        return 0;
#endif

    // Cache other non-http / non-filesystem resources liberally.
    if (!response.url().protocolIsInHTTPFamily()
        && !response.url().protocolIs("filesystem"))
        return std::numeric_limits<double>::max();

    // RFC2616 13.2.4
    double maxAgeValue = response.cacheControlMaxAge();
    if (std::isfinite(maxAgeValue))
        return maxAgeValue;
    double expiresValue = response.expires();
    double dateValue = response.date();
    double creationTime = std::isfinite(dateValue) ? dateValue : responseTimestamp;
    if (std::isfinite(expiresValue))
        return expiresValue - creationTime;
    double lastModifiedValue = response.lastModified();
    if (std::isfinite(lastModifiedValue))
        return (creationTime - lastModifiedValue) * 0.1;
    // If no cache headers are present, the specification leaves the decision to the UA. Other browsers seem to opt for 0.
    return 0;
}

double Resource::freshnessLifetime()
{
    return blink::freshnessLifetime(m_response, m_responseTimestamp);
}

double Resource::stalenessLifetime()
{
    return m_response.cacheControlStaleWhileRevalidate();
}

static bool canUseResponse(ResourceResponse& response, double responseTimestamp)
{
    if (response.isNull())
        return false;

    // FIXME: Why isn't must-revalidate considered a reason we can't use the response?
    if (response.cacheControlContainsNoCache() || response.cacheControlContainsNoStore())
        return false;

    if (response.httpStatusCode() == 303)  {
        // Must not be cached.
        return false;
    }

    if (response.httpStatusCode() == 302 || response.httpStatusCode() == 307) {
        // Default to not cacheable unless explicitly allowed.
        bool hasMaxAge = std::isfinite(response.cacheControlMaxAge());
        bool hasExpires = std::isfinite(response.expires());
        // TODO: consider catching Cache-Control "private" and "public" here.
        if (!hasMaxAge && !hasExpires)
            return false;
    }

    return currentAge(response, responseTimestamp) <= freshnessLifetime(response, responseTimestamp);
}

const ResourceRequest& Resource::lastResourceRequest() const
{
    if (!m_redirectChain.size())
        return m_resourceRequest;
    return m_redirectChain.last().m_request;
}

void Resource::setRevalidatingRequest(const ResourceRequest& request)
{
    SECURITY_CHECK(m_redirectChain.isEmpty());
    DCHECK(!request.isNull());
    m_isRevalidating = true;
    m_resourceRequest = request;
    m_status = NotStarted;
}

void Resource::willFollowRedirect(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (m_isRevalidating)
        revalidationFailed();
    m_redirectChain.append(RedirectPair(newRequest, redirectResponse));
}

void Resource::setResponse(const ResourceResponse& response)
{
    m_response = response;
    if (m_response.wasFetchedViaServiceWorker())
        m_cacheHandler = ServiceWorkerResponseCachedMetadataHandler::create(this, m_fetcherSecurityOrigin.get());
}

bool Resource::unlock()
{
    if (!m_data)
        return false;

    if (!m_data->isLocked())
        return true;

    if (!memoryCache()->contains(this) || hasClientsOrObservers() || !isLoaded() || !isSafeToUnlock())
        return false;

    if (RuntimeEnabledFeatures::doNotUnlockSharedBufferEnabled())
        return false;

    m_data->unlock();
    return true;
}

void Resource::responseReceived(const ResourceResponse& response, std::unique_ptr<WebDataConsumerHandle>)
{
    m_responseTimestamp = currentTime();
    if (m_preloadDiscoveryTime) {
        int timeSinceDiscovery = static_cast<int>(1000 * (monotonicallyIncreasingTime() - m_preloadDiscoveryTime));
        DEFINE_STATIC_LOCAL(CustomCountHistogram, preloadDiscoveryToFirstByteHistogram, ("PreloadScanner.TTFB", 0, 10000, 50));
        preloadDiscoveryToFirstByteHistogram.count(timeSinceDiscovery);
    }

    if (m_isRevalidating) {
        if (response.httpStatusCode() == 304) {
            revalidationSucceeded(response);
            return;
        }
        revalidationFailed();
    }
    setResponse(response);
    String encoding = response.textEncodingName();
    if (!encoding.isNull())
        setEncoding(encoding);
}

void Resource::setSerializedCachedMetadata(const char* data, size_t size)
{
    DCHECK(!m_isRevalidating);
    ASSERT(!m_response.isNull());
    if (m_cacheHandler)
        m_cacheHandler->setSerializedCachedMetadata(data, size);
}

CachedMetadataHandler* Resource::cacheHandler()
{
    return m_cacheHandler.get();
}


String Resource::reasonNotDeletable() const
{
    StringBuilder builder;
    if (hasClientsOrObservers()) {
        builder.append("hasClients(");
        builder.appendNumber(m_clients.size());
        if (!m_clientsAwaitingCallback.isEmpty()) {
            builder.append(", AwaitingCallback=");
            builder.appendNumber(m_clientsAwaitingCallback.size());
        }
        if (!m_finishedClients.isEmpty()) {
            builder.append(", Finished=");
            builder.appendNumber(m_finishedClients.size());
        }
        builder.append(")");
    }
    if (m_loader) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append("m_loader");
    }
    if (m_preloadCount) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append("m_preloadCount(");
        builder.appendNumber(m_preloadCount);
        builder.append(")");
    }
    if (memoryCache()->contains(this)) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append("in_memory_cache");
    }
    return builder.toString();
}

void Resource::didAddClient(ResourceClient* c)
{
    if (isLoaded()) {
        c->notifyFinished(this);
        if (m_clients.contains(c)) {
            m_finishedClients.add(c);
            m_clients.remove(c);
        }
    }
}

static bool shouldSendCachedDataSynchronouslyForType(Resource::Type type)
{
    // Some resources types default to return data synchronously.
    // For most of these, it's because there are layout tests that
    // expect data to return synchronously in case of cache hit. In
    // the case of fonts, there was a performance regression.
    // FIXME: Get to the point where we don't need to special-case sync/async
    // behavior for different resource types.
    if (type == Resource::Image)
        return true;
    if (type == Resource::CSSStyleSheet)
        return true;
    if (type == Resource::Script)
        return true;
    if (type == Resource::Font)
        return true;
    return false;
}

void Resource::willAddClientOrObserver()
{
    ASSERT(!isPurgeable());
    if (m_preloadResult == PreloadNotReferenced) {
        if (isLoaded())
            m_preloadResult = PreloadReferencedWhileComplete;
        else if (isLoading())
            m_preloadResult = PreloadReferencedWhileLoading;
        else
            m_preloadResult = PreloadReferenced;

        if (m_preloadDiscoveryTime) {
            int timeSinceDiscovery = static_cast<int>(1000 * (monotonicallyIncreasingTime() - m_preloadDiscoveryTime));
            DEFINE_STATIC_LOCAL(CustomCountHistogram, preloadDiscoveryHistogram, ("PreloadScanner.ReferenceTime", 0, 10000, 50));
            preloadDiscoveryHistogram.count(timeSinceDiscovery);
        }
    }
    if (!hasClientsOrObservers())
        memoryCache()->makeLive(this);
}

void Resource::addClient(ResourceClient* client)
{
    willAddClientOrObserver();

    if (m_isRevalidating) {
        m_clients.add(client);
        return;
    }

    // If we have existing data to send to the new client and the resource type supprts it, send it asynchronously.
    if (!m_response.isNull() && !shouldSendCachedDataSynchronouslyForType(getType()) && !m_needsSynchronousCacheHit) {
        m_clientsAwaitingCallback.add(client);
        ResourceCallback::callbackHandler().schedule(this);
        return;
    }

    m_clients.add(client);
    didAddClient(client);
    return;
}

void Resource::removeClient(ResourceClient* client)
{
    ASSERT(hasClient(client));
    if (m_finishedClients.contains(client))
        m_finishedClients.remove(client);
    else if (m_clientsAwaitingCallback.contains(client))
        m_clientsAwaitingCallback.remove(client);
    else
        m_clients.remove(client);

    if (m_clientsAwaitingCallback.isEmpty())
        ResourceCallback::callbackHandler().cancel(this);

    didRemoveClientOrObserver();
    // This object may be dead here.
}

void Resource::didRemoveClientOrObserver()
{
    if (!hasClientsOrObservers()) {
        memoryCache()->makeDead(this);
        allClientsAndObserversRemoved();

        // RFC2616 14.9.2:
        // "no-store: ... MUST make a best-effort attempt to remove the information from volatile storage as promptly as possible"
        // "... History buffers MAY store such responses as part of their normal operation."
        // We allow non-secure content to be reused in history, but we do not allow secure content to be reused.
        if (hasCacheControlNoStoreHeader() && url().protocolIs("https")) {
            memoryCache()->remove(this);
            memoryCache()->prune();
        } else {
            memoryCache()->prune(this);
        }
    }
    // This object may be dead here.
}

void Resource::allClientsAndObserversRemoved()
{
    if (!m_loader)
        return;
    if (m_type == Raw)
        cancelTimerFired(&m_cancelTimer);
    else if (!m_cancelTimer.isActive())
        m_cancelTimer.startOneShot(0, BLINK_FROM_HERE);

    unlock();
}

void Resource::cancelTimerFired(Timer<Resource>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_cancelTimer);
    if (!hasClientsOrObservers() && m_loader)
        m_loader->cancel();
}

void Resource::setDecodedSize(size_t decodedSize)
{
    if (decodedSize == m_decodedSize)
        return;
    size_t oldSize = size();
    m_decodedSize = decodedSize;
    memoryCache()->update(this, oldSize, size());
    memoryCache()->updateDecodedResource(this, UpdateForPropertyChange);
}

void Resource::setEncodedSize(size_t encodedSize)
{
    if (encodedSize == m_encodedSize)
        return;
    size_t oldSize = size();
    m_encodedSize = encodedSize;
    memoryCache()->update(this, oldSize, size());
}

void Resource::didAccessDecodedData()
{
    memoryCache()->updateDecodedResource(this, UpdateForAccess);
    memoryCache()->prune();
}

void Resource::finishPendingClients()
{
    // We're going to notify clients one by one. It is simple if the client does nothing.
    // However there are a couple other things that can happen.
    //
    // 1. Clients can be added during the loop. Make sure they are not processed.
    // 2. Clients can be removed during the loop. Make sure they are always available to be
    //    removed. Also don't call removed clients or add them back.

    // Handle case (1) by saving a list of clients to notify. A separate list also ensure
    // a client is either in m_clients or m_clientsAwaitingCallback.
    Vector<ResourceClient*> clientsToNotify;
    copyToVector(m_clientsAwaitingCallback, clientsToNotify);

    for (const auto& client : clientsToNotify) {
        // Handle case (2) to skip removed clients.
        if (!m_clientsAwaitingCallback.remove(client))
            continue;
        m_clients.add(client);
        didAddClient(client);
    }

    // It is still possible for the above loop to finish a new client synchronously.
    // If there's no client waiting we should deschedule.
    bool scheduled = ResourceCallback::callbackHandler().isScheduled(this);
    if (scheduled && m_clientsAwaitingCallback.isEmpty())
        ResourceCallback::callbackHandler().cancel(this);

    // Prevent the case when there are clients waiting but no callback scheduled.
    ASSERT(m_clientsAwaitingCallback.isEmpty() || scheduled);
}

void Resource::prune()
{
    destroyDecodedDataIfPossible();
    unlock();
}

void Resource::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail, WebProcessMemoryDump* memoryDump) const
{
    static const size_t kMaxURLReportLength = 128;
    static const int kMaxResourceClientToShowInMemoryInfra = 10;

    const String dumpName = getMemoryDumpName();
    WebMemoryAllocatorDump* dump = memoryDump->createMemoryAllocatorDump(dumpName);
    dump->addScalar("encoded_size", "bytes", m_encodedSize);
    if (m_data && m_data->isLocked())
        dump->addScalar("live_size", "bytes", m_encodedSize);
    else
        dump->addScalar("dead_size", "bytes", m_encodedSize);

    if (m_data) {
        dump->addScalar("purgeable_size", "bytes", isPurgeable() ? encodedSize() + overheadSize() : 0);
        m_data->onMemoryDump(dumpName, memoryDump);
    }

    if (levelOfDetail == WebMemoryDumpLevelOfDetail::Detailed) {
        String urlToReport = url().getString();
        if (urlToReport.length() > kMaxURLReportLength) {
            urlToReport.truncate(kMaxURLReportLength);
            urlToReport = urlToReport + "...";
        }
        dump->addString("url", "", urlToReport);

        dump->addString("reason_not_deletable", "", reasonNotDeletable());

        Vector<String> clientNames;
        ResourceClientWalker<ResourceClient> walker(m_clients);
        while (ResourceClient* client = walker.next())
            clientNames.append(client->debugName());
        ResourceClientWalker<ResourceClient> walker2(m_clientsAwaitingCallback);
        while (ResourceClient* client = walker2.next())
            clientNames.append("(awaiting) " + client->debugName());
        ResourceClientWalker<ResourceClient> walker3(m_finishedClients);
        while (ResourceClient* client = walker3.next())
            clientNames.append("(finished) " + client->debugName());
        std::sort(clientNames.begin(), clientNames.end(), WTF::codePointCompareLessThan);

        StringBuilder builder;
        for (size_t i = 0; i < clientNames.size() && i < kMaxResourceClientToShowInMemoryInfra; ++i) {
            if (i > 0)
                builder.append(" / ");
            builder.append(clientNames[i]);
        }
        if (clientNames.size() > kMaxResourceClientToShowInMemoryInfra) {
            builder.append(" / and ");
            builder.appendNumber(clientNames.size() - kMaxResourceClientToShowInMemoryInfra);
            builder.append(" more");
        }
        dump->addString("ResourceClient", "", builder.toString());
    }

    const String overheadName = dumpName + "/metadata";
    WebMemoryAllocatorDump* overheadDump = memoryDump->createMemoryAllocatorDump(overheadName);
    overheadDump->addScalar("size", "bytes", overheadSize());
    memoryDump->addSuballocation(overheadDump->guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

String Resource::getMemoryDumpName() const
{
    return String::format("web_cache/%s_resources/%ld", resourceTypeToString(getType(), options().initiatorInfo), m_identifier);
}

void Resource::revalidationSucceeded(const ResourceResponse& validatingResponse)
{
    SECURITY_CHECK(m_redirectChain.isEmpty());
    SECURITY_CHECK(equalIgnoringFragmentIdentifier(validatingResponse.url(), m_response.url()));
    m_response.setResourceLoadTiming(validatingResponse.resourceLoadTiming());

    // RFC2616 10.3.5
    // Update cached headers from the 304 response
    const HTTPHeaderMap& newHeaders = validatingResponse.httpHeaderFields();
    for (const auto& header : newHeaders) {
        // Entity headers should not be sent by servers when generating a 304
        // response; misconfigured servers send them anyway. We shouldn't allow
        // such headers to update the original request. We'll base this on the
        // list defined by RFC2616 7.1, with a few additions for extension headers
        // we care about.
        if (!shouldUpdateHeaderAfterRevalidation(header.key))
            continue;
        m_response.setHTTPHeaderField(header.key, header.value);
    }

    m_isRevalidating = false;
}

void Resource::revalidationFailed()
{
    SECURITY_CHECK(m_redirectChain.isEmpty());
    m_data.clear();
    m_cacheHandler.clear();
    destroyDecodedDataForFailedRevalidation();
    m_isRevalidating = false;
}

bool Resource::canReuseRedirectChain()
{
    for (auto& redirect : m_redirectChain) {
        if (!canUseResponse(redirect.m_redirectResponse, m_responseTimestamp))
            return false;
        if (redirect.m_request.cacheControlContainsNoCache() || redirect.m_request.cacheControlContainsNoStore())
            return false;
    }
    return true;
}

bool Resource::hasCacheControlNoStoreHeader() const
{
    return m_response.cacheControlContainsNoStore() || m_resourceRequest.cacheControlContainsNoStore();
}

bool Resource::hasVaryHeader() const
{
    return !m_response.httpHeaderField(HTTPNames::Vary).isNull();
}

bool Resource::mustRevalidateDueToCacheHeaders()
{
    return !canUseResponse(m_response, m_responseTimestamp) || m_resourceRequest.cacheControlContainsNoCache() || m_resourceRequest.cacheControlContainsNoStore();
}

bool Resource::canUseCacheValidator()
{
    if (isLoading() || errorOccurred())
        return false;

    if (hasCacheControlNoStoreHeader())
        return false;

    // Do not revalidate Resource with redirects. https://crbug.com/613971
    if (!redirectChain().isEmpty())
        return false;

    return m_response.hasCacheValidatorFields() || m_resourceRequest.hasCacheValidatorFields();
}

bool Resource::isPurgeable() const
{
    return m_data && !m_data->isLocked();
}

bool Resource::lock()
{
    if (!m_data)
        return true;
    if (m_data->isLocked())
        return true;

    ASSERT(!hasClientsOrObservers());

    // If locking fails, our buffer has been purged. There's no point
    // in leaving a purged resource in MemoryCache.
    if (!m_data->lock()) {
        m_data.clear();
        setEncodedSize(0);
        memoryCache()->remove(this);
        return false;
    }
    return true;
}

size_t Resource::calculateOverheadSize() const
{
    static const int kAverageClientsHashMapSize = 384;
    return sizeof(Resource) + m_response.memoryUsage() + kAverageClientsHashMapSize + m_resourceRequest.url().getString().length() * 2;
}

void Resource::didChangePriority(ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    m_resourceRequest.setPriority(loadPriority, intraPriorityValue);
    if (m_loader)
        m_loader->didChangePriority(loadPriority, intraPriorityValue);
}

static const char* initatorTypeNameToString(const AtomicString& initiatorTypeName)
{
    if (initiatorTypeName == FetchInitiatorTypeNames::css)
        return "CSS resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::document)
        return "Document";
    if (initiatorTypeName == FetchInitiatorTypeNames::icon)
        return "Icon";
    if (initiatorTypeName == FetchInitiatorTypeNames::internal)
        return "Internal resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::link)
        return "Link element resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::processinginstruction)
        return "Processing instruction";
    if (initiatorTypeName == FetchInitiatorTypeNames::texttrack)
        return "Text track";
    if (initiatorTypeName == FetchInitiatorTypeNames::xml)
        return "XML resource";
    if (initiatorTypeName == FetchInitiatorTypeNames::xmlhttprequest)
        return "XMLHttpRequest";

    return "Resource";
}

const char* Resource::resourceTypeToString(Type type, const FetchInitiatorInfo& initiatorInfo)
{
    switch (type) {
    case Resource::MainResource:
        return "Main resource";
    case Resource::Image:
        return "Image";
    case Resource::CSSStyleSheet:
        return "CSS stylesheet";
    case Resource::Script:
        return "Script";
    case Resource::Font:
        return "Font";
    case Resource::Raw:
        return initatorTypeNameToString(initiatorInfo.name);
    case Resource::SVGDocument:
        return "SVG document";
    case Resource::XSLStyleSheet:
        return "XSL stylesheet";
    case Resource::LinkPrefetch:
        return "Link prefetch resource";
    case Resource::LinkPreload:
        return "Link preload resource";
    case Resource::TextTrack:
        return "Text track";
    case Resource::ImportResource:
        return "Imported resource";
    case Resource::Media:
        return "Media";
    case Resource::Manifest:
        return "Manifest";
    }
    ASSERT_NOT_REACHED();
    return initatorTypeNameToString(initiatorInfo.name);
}

bool Resource::shouldBlockLoadEvent() const
{
    return !m_linkPreload && isLoadEventBlockingResourceType();
}

bool Resource::isLoadEventBlockingResourceType() const
{
    switch (m_type) {
    case Resource::MainResource:
    case Resource::Image:
    case Resource::CSSStyleSheet:
    case Resource::Script:
    case Resource::Font:
    case Resource::SVGDocument:
    case Resource::XSLStyleSheet:
    case Resource::ImportResource:
        return true;
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkPreload:
    case Resource::TextTrack:
    case Resource::Media:
    case Resource::Manifest:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// Do not modify existing strings below because they are used as UMA names.
// https://crbug.com/579496
const char* Resource::resourceTypeName(Resource::Type type)
{
    switch (type) {
    case Resource::MainResource:
        return "MainResource";
    case Resource::Image:
        return "Image";
    case Resource::CSSStyleSheet:
        return "CSSStyleSheet";
    case Resource::Script:
        return "Script";
    case Resource::Font:
        return "Font";
    case Resource::Raw:
        return "Raw";
    case Resource::SVGDocument:
        return "SVGDocument";
    case Resource::XSLStyleSheet:
        return "XSLStyleSheet";
    case Resource::LinkPrefetch:
        return "LinkPrefetch";
    case Resource::LinkPreload:
        return "LinkPreload";
    case Resource::TextTrack:
        return "TextTrack";
    case Resource::ImportResource:
        return "ImportResource";
    case Resource::Media:
        return "Media";
    case Resource::Manifest:
        return "Manifest";
    }
    ASSERT_NOT_REACHED();
    return "Unknown";
}

} // namespace blink
