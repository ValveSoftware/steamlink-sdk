/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "core/fetch/ResourceFetcher.h"

#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourceLoaderSet.h"
#include "core/fetch/UniqueIdentifier.h"
#include "platform/Histogram.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <memory>

using blink::WebURLRequest;

namespace blink {

namespace {

// Events for UMA. Do not reorder or delete. Add new events at the end, but
// before SriResourceIntegrityMismatchEventCount.
enum SriResourceIntegrityMismatchEvent {
    CheckingForIntegrityMismatch = 0,
    RefetchDueToIntegrityMismatch = 1,
    SriResourceIntegrityMismatchEventCount
};

#define DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, name)                                                                                                                      \
    case Resource::name: {                                                                                                                                                  \
        DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, resourceHistogram, new EnumerationHistogram("Blink.MemoryCache.RevalidationPolicy." prefix #name, Load + 1)); \
        resourceHistogram.count(policy);                                                                                                                                    \
        break;                                                                                                                                                              \
    }

#define DEFINE_RESOURCE_HISTOGRAM(prefix)                        \
    switch (factory.type()) {                                    \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, CSSStyleSheet)  \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Font)           \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Image)          \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, ImportResource) \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, LinkPrefetch)   \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, LinkPreload)    \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, MainResource)   \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Manifest)       \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Media)          \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Raw)            \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Script)         \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, SVGDocument)    \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, TextTrack)      \
        DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, XSLStyleSheet)  \
    }

} // namespace

static void RecordSriResourceIntegrityMismatchEvent(SriResourceIntegrityMismatchEvent event)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, integrityHistogram, new EnumerationHistogram("sri.resource_integrity_mismatch_event", SriResourceIntegrityMismatchEventCount));
    integrityHistogram.count(event);
}

static ResourceLoadPriority typeToPriority(Resource::Type type)
{
    switch (type) {
    case Resource::MainResource:
    case Resource::CSSStyleSheet:
    case Resource::Font:
        // Also parser-blocking scripts (set explicitly in loadPriority)
        return ResourceLoadPriorityVeryHigh;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::Raw:
    case Resource::ImportResource:
    case Resource::Script:
        // Also visible resources/images (set explicitly in loadPriority)
        return ResourceLoadPriorityHigh;
    case Resource::Manifest:
        // Also late-body scripts discovered by the preload scanner (set explicitly in loadPriority)
        return ResourceLoadPriorityMedium;
    case Resource::Image:
    case Resource::LinkPreload:
    case Resource::TextTrack:
    case Resource::Media:
    case Resource::SVGDocument:
        // Also async scripts (set explicitly in loadPriority)
        return ResourceLoadPriorityLow;
    case Resource::LinkPrefetch:
        return ResourceLoadPriorityVeryLow;
    }

    ASSERT_NOT_REACHED();
    return ResourceLoadPriorityUnresolved;
}

ResourceLoadPriority ResourceFetcher::loadPriority(Resource::Type type, const FetchRequest& request, ResourcePriority::VisibilityStatus visibility)
{
    // TODO(yoav): Change it here so that priority can be changed even after it was resolved.
    if (request.priority() != ResourceLoadPriorityUnresolved)
        return request.priority();

    // Synchronous requests should always be max priority, lest they hang the renderer.
    if (request.options().synchronousPolicy == RequestSynchronously)
        return ResourceLoadPriorityHighest;

    ResourceLoadPriority priority = typeToPriority(type);

    // Visible resources (images in practice) get a boost to High priority.
    if (visibility == ResourcePriority::Visible)
        priority = ResourceLoadPriorityHigh;

    // Resources before the first image are considered "early" in the document
    // and resources after the first image are "late" in the document.  Important to
    // note that this is based on when the preload scanner discovers a resource
    // for the most part so the main parser may not have reached the image element yet.
    if (type == Resource::Image)
        m_imageFetched = true;

    // Special handling for scripts.
    // Default/Parser-Blocking/Preload early in document: High (set in typeToPriority)
    // Async/Defer: Low Priority (applies to both preload and parser-inserted)
    // Preload late in document: Medium
    if (type == Resource::Script) {
        if (FetchRequest::LazyLoad == request.defer())
            priority = ResourceLoadPriorityLow;
        else if (request.forPreload() && m_imageFetched)
            priority = ResourceLoadPriorityMedium;
    }

    return context().modifyPriorityForExperiments(priority);
}

static void populateResourceTiming(ResourceTimingInfo* info, Resource* resource)
{
    info->setInitialRequest(resource->resourceRequest());
    info->setFinalResponse(resource->response());
}

static WebURLRequest::RequestContext requestContextFromType(bool isMainFrame, Resource::Type type)
{
    switch (type) {
    case Resource::MainResource:
        if (!isMainFrame)
            return WebURLRequest::RequestContextIframe;
        // FIXME: Change this to a context frame type (once we introduce them): http://fetch.spec.whatwg.org/#concept-request-context-frame-type
        return WebURLRequest::RequestContextHyperlink;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::CSSStyleSheet:
        return WebURLRequest::RequestContextStyle;
    case Resource::Script:
        return WebURLRequest::RequestContextScript;
    case Resource::Font:
        return WebURLRequest::RequestContextFont;
    case Resource::Image:
        return WebURLRequest::RequestContextImage;
    case Resource::Raw:
        return WebURLRequest::RequestContextSubresource;
    case Resource::ImportResource:
        return WebURLRequest::RequestContextImport;
    case Resource::LinkPrefetch:
        return WebURLRequest::RequestContextPrefetch;
    case Resource::LinkPreload:
        return WebURLRequest::RequestContextSubresource;
    case Resource::TextTrack:
        return WebURLRequest::RequestContextTrack;
    case Resource::SVGDocument:
        return WebURLRequest::RequestContextImage;
    case Resource::Media: // TODO: Split this.
        return WebURLRequest::RequestContextVideo;
    case Resource::Manifest:
        return WebURLRequest::RequestContextManifest;
    }
    ASSERT_NOT_REACHED();
    return WebURLRequest::RequestContextSubresource;
}

ResourceFetcher::ResourceFetcher(FetchContext* newContext)
    : m_context(newContext)
    , m_archive(context().isMainFrame() ? nullptr : context().archive())
    , m_resourceTimingReportTimer(this, &ResourceFetcher::resourceTimingReportTimerFired)
    , m_autoLoadImages(true)
    , m_imagesEnabled(true)
    , m_allowStaleResources(false)
    , m_imageFetched(false)
{
    ThreadState::current()->registerPreFinalizer(this);
}

ResourceFetcher::~ResourceFetcher()
{
}

Resource* ResourceFetcher::cachedResource(const KURL& resourceURL) const
{
    KURL url = MemoryCache::removeFragmentIdentifierIfNeeded(resourceURL);
    const WeakMember<Resource>& resource = m_documentResources.get(url);
    return resource.get();
}

bool ResourceFetcher::canAccessResource(Resource* resource, SecurityOrigin* sourceOrigin, const KURL& url, AccessControlLoggingDecision logErrorsDecision) const
{
    // Redirects can change the response URL different from one of request.
    bool forPreload = resource->isUnusedPreload();
    if (!context().canRequest(resource->getType(), resource->resourceRequest(), url, resource->options(), forPreload, FetchRequest::UseDefaultOriginRestrictionForType))
        return false;

    if (!sourceOrigin)
        sourceOrigin = context().getSecurityOrigin();

    if (sourceOrigin->canRequestNoSuborigin(url))
        return true;

    String errorDescription;
    if (!resource->passesAccessControlCheck(sourceOrigin, errorDescription)) {
        resource->setCORSFailed();
        if (!forPreload && (logErrorsDecision == ShouldLogAccessControlErrors)) {
            String resourceType = Resource::resourceTypeToString(resource->getType(), resource->options().initiatorInfo);
            context().addConsoleMessage(resourceType + " from origin '" + SecurityOrigin::create(url)->toString() + "' has been blocked from loading by Cross-Origin Resource Sharing policy: " + errorDescription);
        }
        return false;
    }
    return true;
}

bool ResourceFetcher::isControlledByServiceWorker() const
{
    return context().isControlledByServiceWorker();
}

bool ResourceFetcher::resourceNeedsLoad(Resource* resource, const FetchRequest& request, RevalidationPolicy policy)
{
    // Defer a font load until it is actually needed unless this is a preload.
    if (resource->getType() == Resource::Font && !request.forPreload())
        return false;
    if (resource->isImage() && shouldDeferImageLoad(resource->url()))
        return false;
    return policy != Use || resource->stillNeedsLoad();
}

// Limit the number of URLs in m_validatedURLs to avoid memory bloat.
// http://crbug.com/52411
static const int kMaxValidatedURLsSize = 10000;

void ResourceFetcher::requestLoadStarted(unsigned long identifier, Resource* resource, const FetchRequest& request, ResourceLoadStartType type, bool isStaticData)
{
    if (type == ResourceLoadingFromCache && resource->getStatus() == Resource::Cached && !m_validatedURLs.contains(resource->url()))
        context().dispatchDidLoadResourceFromMemoryCache(identifier, resource, request.resourceRequest().frameType(), request.resourceRequest().requestContext());

    if (isStaticData)
        return;

    if (type == ResourceLoadingFromCache && !resource->stillNeedsLoad() && !m_validatedURLs.contains(request.resourceRequest().url())) {
        // Resources loaded from memory cache should be reported the first time they're used.
        std::unique_ptr<ResourceTimingInfo> info = ResourceTimingInfo::create(request.options().initiatorInfo.name, monotonicallyIncreasingTime(), resource->getType() == Resource::MainResource);
        populateResourceTiming(info.get(), resource);
        info->clearLoadTimings();
        info->setLoadFinishTime(info->initialTime());
        m_scheduledResourceTimingReports.append(std::move(info));
        if (!m_resourceTimingReportTimer.isActive())
            m_resourceTimingReportTimer.startOneShot(0, BLINK_FROM_HERE);
    }

    if (m_validatedURLs.size() >= kMaxValidatedURLsSize) {
        m_validatedURLs.clear();
    }
    m_validatedURLs.add(request.resourceRequest().url());
}

static std::unique_ptr<TracedValue> urlForTraceEvent(const KURL& url)
{
    std::unique_ptr<TracedValue> value = TracedValue::create();
    value->setString("url", url.getString());
    return value;
}

Resource* ResourceFetcher::resourceForStaticData(const FetchRequest& request, const ResourceFactory& factory, const SubstituteData& substituteData)
{
    const KURL& url = request.resourceRequest().url();
    ASSERT(url.protocolIsData() || substituteData.isValid() || m_archive);

    // TODO(japhet): We only send main resource data: urls through WebURLLoader for the benefit of
    // a service worker test (RenderViewImplTest.ServiceWorkerNetworkProviderSetup), which is at a
    // layer where it isn't easy to mock out a network load. It uses data: urls to emulate the
    // behavior it wants to test, which would otherwise be reserved for network loads.
    if (!m_archive && !substituteData.isValid() && (factory.type() == Resource::MainResource || factory.type() == Resource::Raw))
        return nullptr;

    const String cacheIdentifier = getCacheIdentifier();
    if (Resource* oldResource = memoryCache()->resourceForURL(url, cacheIdentifier)) {
        // There's no reason to re-parse if we saved the data from the previous parse.
        if (request.options().dataBufferingPolicy != DoNotBufferData)
            return oldResource;
        memoryCache()->remove(oldResource);
    }

    WebString mimetype;
    WebString charset;
    RefPtr<SharedBuffer> data;
    if (substituteData.isValid()) {
        mimetype = substituteData.mimeType();
        charset = substituteData.textEncoding();
        data = substituteData.content();
    } else if (url.protocolIsData()) {
        data = PassRefPtr<SharedBuffer>(Platform::current()->parseDataURL(url, mimetype, charset));
        if (!data)
            return nullptr;
    } else {
        ArchiveResource* archiveResource = m_archive->subresourceForURL(request.url());
        // Fall back to the network if the archive doesn't contain the resource.
        if (!archiveResource)
            return nullptr;
        mimetype = archiveResource->mimeType();
        charset = archiveResource->textEncoding();
        data = archiveResource->data();
    }

    ResourceResponse response(url, mimetype, data->size(), charset, String());
    response.setHTTPStatusCode(200);
    response.setHTTPStatusText("OK");

    Resource* resource = factory.create(request.resourceRequest(), request.options(), request.charset());
    resource->setNeedsSynchronousCacheHit(substituteData.forceSynchronousLoad());
    // FIXME: We should provide a body stream here.
    resource->responseReceived(response, nullptr);
    resource->setDataBufferingPolicy(BufferData);
    if (data->size())
        resource->setResourceBuffer(data);
    resource->setIdentifier(createUniqueIdentifier());
    resource->setCacheIdentifier(cacheIdentifier);
    resource->finish();

    if (!substituteData.isValid())
        memoryCache()->add(resource);

    return resource;
}

void ResourceFetcher::moveCachedNonBlockingResourceToBlocking(Resource* resource, const FetchRequest& request)
{
    // TODO(yoav): Test that non-blocking resources (video/audio/track) continue to not-block even after being preloaded and discovered.
    if (resource && resource->loader() && resource->isLoadEventBlockingResourceType() && resource->isLinkPreload() && !request.forPreload()) {
        m_nonBlockingLoaders.remove(resource->loader());
        m_loaders.add(resource->loader());
    }
}

void ResourceFetcher::updateMemoryCacheStats(Resource* resource, RevalidationPolicy policy, const FetchRequest& request,  const ResourceFactory& factory, bool isStaticData) const
{
    if (isStaticData)
        return;

    if (request.forPreload()) {
        DEFINE_RESOURCE_HISTOGRAM("Preload.");
    } else {
        DEFINE_RESOURCE_HISTOGRAM("");
    }

    // Aims to count Resource only referenced from MemoryCache (i.e. what
    // would be dead if MemoryCache holds weak references to Resource).
    // Currently we check references to Resource from ResourceClient and
    // |m_preloads| only, because they are major sources of references.
    if (resource && !resource->hasClientsOrObservers() && (!m_preloads || !m_preloads->contains(resource))) {
        DEFINE_RESOURCE_HISTOGRAM("Dead.");
    }
}

Resource* ResourceFetcher::requestResource(FetchRequest& request, const ResourceFactory& factory, const SubstituteData& substituteData)
{
    ASSERT(request.options().synchronousPolicy == RequestAsynchronously || factory.type() == Resource::Raw || factory.type() == Resource::XSLStyleSheet);

    context().upgradeInsecureRequest(request);
    context().addClientHintsIfNecessary(request);
    context().addCSPHeaderIfNecessary(factory.type(), request);

    TRACE_EVENT1("blink", "ResourceFetcher::requestResource", "url", urlForTraceEvent(request.url()));

    if (!request.url().isValid())
        return nullptr;

    if (!context().canRequest(factory.type(), request.resourceRequest(), MemoryCache::removeFragmentIdentifierIfNeeded(request.url()), request.options(), request.forPreload(), request.getOriginRestriction()))
        return nullptr;

    unsigned long identifier = createUniqueIdentifier();
    request.setPriority(loadPriority(factory.type(), request, ResourcePriority::NotVisible));
    request.mutableResourceRequest().setPriority(request.priority());
    initializeResourceRequest(request.mutableResourceRequest(), factory.type(), request.defer());
    context().willStartLoadingResource(identifier, request.mutableResourceRequest(), factory.type());
    if (!request.url().isValid())
        return nullptr;

    if (!request.forPreload()) {
        V8DOMActivityLogger* activityLogger = nullptr;
        if (request.options().initiatorInfo.name == FetchInitiatorTypeNames::xmlhttprequest)
            activityLogger = V8DOMActivityLogger::currentActivityLogger();
        else
            activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();

        if (activityLogger) {
            Vector<String> argv;
            argv.append(Resource::resourceTypeToString(factory.type(), request.options().initiatorInfo));
            argv.append(request.url());
            activityLogger->logEvent("blinkRequestResource", argv.size(), argv.data());
        }
    }

    bool isStaticData = request.resourceRequest().url().protocolIsData() || substituteData.isValid() || m_archive;
    Resource* resource(nullptr);
    if (isStaticData) {
        resource = resourceForStaticData(request, factory, substituteData);
        // Abort the request if the archive doesn't contain the resource.
        if (!resource && m_archive)
            return nullptr;
    }
    if (!resource)
        resource = memoryCache()->resourceForURL(request.url(), getCacheIdentifier());

    // See if we can use an existing resource from the cache. If so, we need to move it to be load blocking.
    moveCachedNonBlockingResourceToBlocking(resource, request);

    const RevalidationPolicy policy = determineRevalidationPolicy(factory.type(), request, resource, isStaticData);

    updateMemoryCacheStats(resource, policy, request, factory, isStaticData);

    request.mutableResourceRequest().setAllowStoredCredentials(request.options().allowCredentials == AllowStoredCredentials);

    switch (policy) {
    case Reload:
        memoryCache()->remove(resource);
        // Fall through
    case Load:
        resource = createResourceForLoading(request, request.charset(), factory);
        break;
    case Revalidate:
        initializeRevalidation(request.mutableResourceRequest(), resource);
        break;
    case Use:
        memoryCache()->updateForAccess(resource);
        break;
    }

    if (!resource)
        return nullptr;
    if (resource->getType() != factory.type()) {
        ASSERT(request.forPreload());
        return nullptr;
    }

    if (!resource->hasClientsOrObservers())
        m_deadStatsRecorder.update(policy);

    if (policy != Use)
        resource->setIdentifier(identifier);

    if (!request.forPreload() || policy != Use) {
        // When issuing another request for a resource that is already in-flight make
        // sure to not demote the priority of the in-flight request. If the new request
        // isn't at the same priority as the in-flight request, only allow promotions.
        // This can happen when a visible image's priority is increased and then another
        // reference to the image is parsed (which would be at a lower priority).
        if (request.priority() > resource->resourceRequest().priority())
            resource->didChangePriority(request.priority(), 0);
    }

    // If only the fragment identifiers differ, it is the same resource.
    DCHECK(equalIgnoringFragmentIdentifier(resource->url(), request.url()));
    requestLoadStarted(identifier, resource, request, policy == Use ? ResourceLoadingFromCache : ResourceLoadingFromNetwork, isStaticData);
    m_documentResources.set(MemoryCache::removeFragmentIdentifierIfNeeded(request.url()), resource);

    // Returns with an existing resource if the resource does not need to start
    // loading immediately.
    // If revalidation policy was determined as |Revalidate|, the resource was
    // already initialized for the revalidation here, but won't start loading.
    if (!resourceNeedsLoad(resource, request, policy))
        return resource;

    if (!startLoad(resource))
        return nullptr;
    ASSERT(!resource->errorOccurred() || request.options().synchronousPolicy == RequestSynchronously);
    return resource;
}

void ResourceFetcher::resourceTimingReportTimerFired(Timer<ResourceFetcher>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_resourceTimingReportTimer);
    Vector<std::unique_ptr<ResourceTimingInfo>> timingReports;
    timingReports.swap(m_scheduledResourceTimingReports);
    for (const auto& timingInfo : timingReports)
        context().addResourceTiming(*timingInfo);
}

void ResourceFetcher::determineRequestContext(ResourceRequest& request, Resource::Type type, bool isMainFrame)
{
    WebURLRequest::RequestContext requestContext = requestContextFromType(isMainFrame, type);
    request.setRequestContext(requestContext);
}

void ResourceFetcher::determineRequestContext(ResourceRequest& request, Resource::Type type)
{
    determineRequestContext(request, type, context().isMainFrame());
}

void ResourceFetcher::initializeResourceRequest(ResourceRequest& request, Resource::Type type, FetchRequest::DeferOption defer)
{
    if (request.getCachePolicy() == WebCachePolicy::UseProtocolCachePolicy)
        request.setCachePolicy(context().resourceRequestCachePolicy(request, type, defer));
    if (request.requestContext() == WebURLRequest::RequestContextUnspecified)
        determineRequestContext(request, type);
    if (type == Resource::LinkPrefetch)
        request.setHTTPHeaderField(HTTPNames::Purpose, "prefetch");

    context().addAdditionalRequestHeaders(request, (type == Resource::MainResource) ? FetchMainResource : FetchSubresource);
}

void ResourceFetcher::initializeRevalidation(ResourceRequest& revalidatingRequest, Resource* resource)
{
    ASSERT(resource);
    ASSERT(memoryCache()->contains(resource));
    ASSERT(resource->isLoaded());
    ASSERT(resource->canUseCacheValidator());
    ASSERT(!resource->isCacheValidator());
    ASSERT(!context().isControlledByServiceWorker());

    const AtomicString& lastModified = resource->response().httpHeaderField(HTTPNames::Last_Modified);
    const AtomicString& eTag = resource->response().httpHeaderField(HTTPNames::ETag);
    if (!lastModified.isEmpty() || !eTag.isEmpty()) {
        ASSERT(context().getCachePolicy() != CachePolicyReload);
        if (context().getCachePolicy() == CachePolicyRevalidate)
            revalidatingRequest.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
    }
    if (!lastModified.isEmpty())
        revalidatingRequest.setHTTPHeaderField(HTTPNames::If_Modified_Since, lastModified);
    if (!eTag.isEmpty())
        revalidatingRequest.setHTTPHeaderField(HTTPNames::If_None_Match, eTag);

    double stalenessLifetime = resource->stalenessLifetime();
    if (std::isfinite(stalenessLifetime) && stalenessLifetime > 0) {
        revalidatingRequest.setHTTPHeaderField(HTTPNames::Resource_Freshness, AtomicString(String::format("max-age=%.0lf,stale-while-revalidate=%.0lf,age=%.0lf", resource->freshnessLifetime(), stalenessLifetime, resource->currentAge())));
    }

    resource->setRevalidatingRequest(revalidatingRequest);
}

Resource* ResourceFetcher::createResourceForLoading(FetchRequest& request, const String& charset, const ResourceFactory& factory)
{
    const String cacheIdentifier = getCacheIdentifier();
    ASSERT(!memoryCache()->resourceForURL(request.resourceRequest().url(), cacheIdentifier));

    WTF_LOG(ResourceLoading, "Loading Resource for '%s'.", request.resourceRequest().url().elidedString().latin1().data());

    Resource* resource = factory.create(request.resourceRequest(), request.options(), charset);
    resource->setLinkPreload(request.isLinkPreload());
    if (request.forPreload()) {
        resource->setPreloadDiscoveryTime(request.preloadDiscoveryTime());
    }
    resource->setCacheIdentifier(cacheIdentifier);

    // Don't add main resource to cache to prevent reuse.
    if (factory.type() != Resource::MainResource)
        memoryCache()->add(resource);
    return resource;
}

void ResourceFetcher::storeResourceTimingInitiatorInformation(Resource* resource)
{
    const AtomicString& fetchInitiator = resource->options().initiatorInfo.name;
    if (fetchInitiator == FetchInitiatorTypeNames::internal)
        return;

    bool isMainResource = resource->getType() == Resource::MainResource;
    std::unique_ptr<ResourceTimingInfo> info = ResourceTimingInfo::create(fetchInitiator, monotonicallyIncreasingTime(), isMainResource);

    if (resource->isCacheValidator()) {
        const AtomicString& timingAllowOrigin = resource->response().httpHeaderField(HTTPNames::Timing_Allow_Origin);
        if (!timingAllowOrigin.isEmpty())
            info->setOriginalTimingAllowOrigin(timingAllowOrigin);
    }

    if (!isMainResource || context().updateTimingInfoForIFrameNavigation(info.get()))
        m_resourceTimingInfoMap.add(resource, std::move(info));
}

ResourceFetcher::RevalidationPolicy ResourceFetcher::determineRevalidationPolicy(Resource::Type type, const FetchRequest& fetchRequest, Resource* existingResource, bool isStaticData) const
{
    const ResourceRequest& request = fetchRequest.resourceRequest();

    if (!existingResource)
        return Load;

    // Checks if the resource has an explicit policy about integrity metadata.
    // Currently only applies to ScriptResources.
    //
    // This is necessary because ScriptResource objects do not keep the raw
    // data around after the source is accessed once, so if the resource is
    // accessed from the MemoryCache for a second time, there is no way to redo
    // an integrity check.
    //
    // Thus, Blink implements a scheme where it caches the integrity
    // information for a ScriptResource after the first time it is checked, and
    // if there is another request for that resource, with the same integrity
    // metadata, Blink skips the integrity calculation. However, if the
    // integrity metadata is a mismatch, the MemoryCache must be skipped here,
    // and a new request for the resource must be made to get the raw data.
    // This is expected to be an uncommon case, however, as it implies two
    // same-origin requests to the same resource, but with different integrity
    // metadata.
    RecordSriResourceIntegrityMismatchEvent(CheckingForIntegrityMismatch);
    if (existingResource->mustRefetchDueToIntegrityMetadata(fetchRequest)) {
        RecordSriResourceIntegrityMismatchEvent(RefetchDueToIntegrityMismatch);
        return Reload;
    }

    // Service Worker's CORS fallback message must not be cached.
    if (existingResource->response().wasFallbackRequiredByServiceWorker())
        return Reload;

    // We already have a preload going for this URL.
    if (fetchRequest.forPreload() && existingResource->isPreloaded())
        return Use;

    // If the same URL has been loaded as a different type, we need to reload.
    if (existingResource->getType() != type) {
        // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
        // We really should discard the new prefetch since the preload has more
        // specific type information! crbug.com/379893
        // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to type mismatch.");
        return Reload;
    }

    // Do not load from cache if images are not enabled.
    // There are two general cases:
    // 1. Images are disabled. Don't ever load images, even if the image is
    //    cached or it is a data: url. In this case, we "Reload" the image,
    //    then defer it with resourceNeedsLoad() so that it never actually
    //    goes to the network.
    // 2. Images are enabled, but not loaded automatically. In this case, we
    //    will Use cached resources or data: urls, but will similarly fall back
    //    to a deferred network load if we don't have the data available
    //    without a network request. We check allowImage() here, which is
    //    affected by m_imagesEnabled but not m_autoLoadImages, in order to
    //    allow for this differing behavior.
    // TODO(japhet): Can we get rid of one of these settings?
    if (existingResource->isImage() && !context().allowImage(m_imagesEnabled, existingResource->url()))
        return Reload;

    // Never use cache entries for downloadToFile / useStreamOnResponse
    // requests. The data will be delivered through other paths.
    if (request.downloadToFile() || request.useStreamOnResponse())
        return Reload;

    // Never reuse opaque responses from a service worker for requests that
    // are not no-cors. https://crbug.com/625575
    if (existingResource->response().wasFetchedViaServiceWorker() && existingResource->response().serviceWorkerResponseType() == WebServiceWorkerResponseTypeOpaque && request.fetchRequestMode() != WebURLRequest::FetchRequestModeNoCORS)
        return Reload;

    // If resource was populated from a SubstituteData load or data: url, use it.
    if (isStaticData)
        return Use;

    if (!existingResource->canReuse(request))
        return Reload;

    // Certain requests (e.g., XHRs) might have manually set headers that require revalidation.
    // FIXME: In theory, this should be a Revalidate case. In practice, the MemoryCache revalidation path assumes a whole bunch
    // of things about how revalidation works that manual headers violate, so punt to Reload instead.
    if (request.isConditional())
        return Reload;

    // Don't try to reuse an in-progress async request for a new sync request.
    if (fetchRequest.options().synchronousPolicy == RequestSynchronously && existingResource->isLoading())
        return Reload;

    // Don't reload resources while pasting.
    if (m_allowStaleResources)
        return Use;

    if (!fetchRequest.options().canReuseRequest(existingResource->options()))
        return Reload;

    // Always use preloads.
    if (existingResource->isPreloaded())
        return Use;

    // CachePolicyHistoryBuffer uses the cache no matter what.
    CachePolicy cachePolicy = context().getCachePolicy();
    if (cachePolicy == CachePolicyHistoryBuffer)
        return Use;

    // Don't reuse resources with Cache-control: no-store.
    if (existingResource->hasCacheControlNoStoreHeader()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to Cache-control: no-store.");
        return Reload;
    }

    // If credentials were sent with the previous request and won't be
    // with this one, or vice versa, re-fetch the resource.
    //
    // This helps with the case where the server sends back
    // "Access-Control-Allow-Origin: *" all the time, but some of the
    // client's requests are made without CORS and some with.
    if (existingResource->resourceRequest().allowStoredCredentials() != request.allowStoredCredentials()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to difference in credentials settings.");
        return Reload;
    }

    // During the initial load, avoid loading the same resource multiple times for a single document,
    // even if the cache policies would tell us to.
    // We also group loads of the same resource together.
    // Raw resources are exempted, as XHRs fall into this category and may have user-set Cache-Control:
    // headers or other factors that require separate requests.
    if (type != Resource::Raw) {
        if (!context().isLoadComplete() && m_validatedURLs.contains(existingResource->url()))
            return Use;
        if (existingResource->isLoading())
            return Use;
    }

    if (request.getCachePolicy() == WebCachePolicy::BypassingCache)
        return Reload;

    // CachePolicyReload always reloads
    if (cachePolicy == CachePolicyReload) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to CachePolicyReload.");
        return Reload;
    }

    // We'll try to reload the resource if it failed last time.
    if (existingResource->errorOccurred()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicye reloading due to resource being in the error state");
        return Reload;
    }

    // List of available images logic allows images to be re-used without cache validation. We restrict this only to images
    // from memory cache which are the same as the version in the current document.
    if (type == Resource::Image && existingResource == cachedResource(request.url()))
        return Use;

    // Defer to the browser process cache for Vary header handling.
    if (existingResource->hasVaryHeader())
        return Reload;

    // If any of the redirects in the chain to loading the resource were not cacheable, we cannot reuse our cached resource.
    if (!existingResource->canReuseRedirectChain()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to an uncacheable redirect");
        return Reload;
    }

    // Check if the cache headers requires us to revalidate (cache expiration for example).
    if (cachePolicy == CachePolicyRevalidate || existingResource->mustRevalidateDueToCacheHeaders()
        || request.cacheControlContainsNoCache()) {
        // See if the resource has usable ETag or Last-modified headers.
        // If the page is controlled by the ServiceWorker, we choose the Reload policy because the revalidation headers should not be exposed to the ServiceWorker.(crbug.com/429570)
        if (existingResource->canUseCacheValidator() && !context().isControlledByServiceWorker()) {
            // If the resource is already a cache validator but not started yet,
            // the |Use| policy should be applied to subsequent requests.
            if (existingResource->isCacheValidator()) {
                DCHECK(existingResource->stillNeedsLoad());
                return Use;
            }
            return Revalidate;
        }

        // No, must reload.
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to missing cache validators.");
        return Reload;
    }

    return Use;
}

void ResourceFetcher::setAutoLoadImages(bool enable)
{
    if (enable == m_autoLoadImages)
        return;

    m_autoLoadImages = enable;

    if (!m_autoLoadImages)
        return;

    reloadImagesIfNotDeferred();
}

void ResourceFetcher::setImagesEnabled(bool enable)
{
    if (enable == m_imagesEnabled)
        return;

    m_imagesEnabled = enable;

    if (!m_imagesEnabled)
        return;

    reloadImagesIfNotDeferred();
}

bool ResourceFetcher::shouldDeferImageLoad(const KURL& url) const
{
    return !context().allowImage(m_imagesEnabled, url) || !m_autoLoadImages;
}

void ResourceFetcher::reloadImagesIfNotDeferred()
{
    for (Resource* resource : m_documentResources.values()) {
        if (resource->getType() == Resource::Image && resource->stillNeedsLoad() && !shouldDeferImageLoad(resource->url()))
            startLoad(resource);
    }
}

int ResourceFetcher::requestCount() const
{
    return m_loaders.size();
}

void ResourceFetcher::preloadStarted(Resource* resource)
{
    if (m_preloads && m_preloads->contains(resource))
        return;
    TRACE_EVENT_ASYNC_STEP_INTO0("blink.net", "Resource", resource->identifier(), "Preload");
    resource->increasePreloadCount();

    if (!m_preloads)
        m_preloads = new HeapListHashSet<Member<Resource>>;
    m_preloads->add(resource);
}

bool ResourceFetcher::isPreloaded(const KURL& url) const
{
    if (m_preloads) {
        for (auto resource : *m_preloads) {
            if (resource->url() == url)
                return true;
        }
    }

    return false;
}

void ResourceFetcher::clearPreloads(ClearPreloadsPolicy policy)
{
    if (!m_preloads)
        return;

    logPreloadStats();

    for (auto resource : *m_preloads) {
        resource->decreasePreloadCount();
        if (resource->getPreloadResult() == Resource::PreloadNotReferenced && (policy == ClearAllPreloads || !resource->isLinkPreload()))
            memoryCache()->remove(resource.get());
    }
    m_preloads.clear();
}

ArchiveResource* ResourceFetcher::createArchive(Resource* resource)
{
    // Only the top-frame can load MHTML.
    if (!context().isMainFrame())
        return nullptr;
    m_archive = MHTMLArchive::create(resource->url(), resource->resourceBuffer());
    return m_archive ? m_archive->mainResource() : nullptr;
}

void ResourceFetcher::didFinishLoading(Resource* resource, double finishTime, int64_t encodedDataLength, DidFinishLoadingReason finishReason)
{
    TRACE_EVENT_ASYNC_END0("blink.net", "Resource", resource->identifier());
    DCHECK(resource);

    // When loading a multipart resource, make the loader non-block when
    // finishing loading the first part.
    if (finishReason == DidFinishFirstPartInMultipart)
        moveResourceLoaderToNonBlocking(resource->loader());
    else
        removeResourceLoader(resource->loader());
    DCHECK(!m_loaders.contains(resource->loader()));
    DCHECK(finishReason == DidFinishFirstPartInMultipart || !m_nonBlockingLoaders.contains(resource->loader()));

    if (std::unique_ptr<ResourceTimingInfo> info = m_resourceTimingInfoMap.take(resource)) {
        if (resource->response().isHTTP() && resource->response().httpStatusCode() < 400) {
            populateResourceTiming(info.get(), resource);
            info->setLoadFinishTime(finishTime);
            if (resource->options().requestInitiatorContext == DocumentContext)
                context().addResourceTiming(*info);
            resource->reportResourceTimingToClients(*info);
        }
    }
    context().dispatchDidFinishLoading(resource->identifier(), finishTime, encodedDataLength);
    if (finishReason == DidFinishLoading)
        resource->finish(finishTime);
    context().didLoadResource(resource);
}

void ResourceFetcher::didFailLoading(Resource* resource, const ResourceError& error)
{
    TRACE_EVENT_ASYNC_END0("blink.net", "Resource", resource->identifier());
    removeResourceLoader(resource->loader());
    m_resourceTimingInfoMap.take(const_cast<Resource*>(resource));
    bool isInternalRequest = resource->options().initiatorInfo.name == FetchInitiatorTypeNames::internal;
    context().dispatchDidFail(resource->identifier(), error, isInternalRequest);
    resource->error(error);
    context().didLoadResource(resource);
}

void ResourceFetcher::didReceiveResponse(Resource* resource, const ResourceResponse& response)
{
    // If the response is fetched via ServiceWorker, the original URL of the response could be different from the URL of the request.
    // We check the URL not to load the resources which are forbidden by the page CSP.
    // https://w3c.github.io/webappsec-csp/#should-block-response
    if (response.wasFetchedViaServiceWorker()) {
        const KURL& originalURL = response.originalURLViaServiceWorker();
        if (!originalURL.isEmpty() && !context().allowResponse(resource->getType(), resource->resourceRequest(), originalURL, resource->options())) {
            resource->loader()->cancel();
            bool isInternalRequest = resource->options().initiatorInfo.name == FetchInitiatorTypeNames::internal;
            context().dispatchDidFail(resource->identifier(), ResourceError(errorDomainBlinkInternal, 0, originalURL.getString(), "Unsafe attempt to load URL " + originalURL.elidedString() + " fetched by a ServiceWorker."), isInternalRequest);
            return;
        }
    }
    context().dispatchDidReceiveResponse(resource->identifier(), response, resource->resourceRequest().frameType(), resource->resourceRequest().requestContext(), resource);
}

void ResourceFetcher::didReceiveData(const Resource* resource, const char* data, int dataLength, int encodedDataLength)
{
    context().dispatchDidReceiveData(resource->identifier(), data, dataLength, encodedDataLength);
}

void ResourceFetcher::didDownloadData(const Resource* resource, int dataLength, int encodedDataLength)
{
    context().dispatchDidDownloadData(resource->identifier(), dataLength, encodedDataLength);
}

void ResourceFetcher::acceptDataFromThreadedReceiver(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    context().dispatchDidReceiveData(identifier, data, dataLength, encodedDataLength);
}

void ResourceFetcher::moveResourceLoaderToNonBlocking(ResourceLoader* loader)
{
    m_nonBlockingLoaders.add(loader);
    m_loaders.remove(loader);
}

bool ResourceFetcher::startLoad(Resource* resource)
{
    DCHECK(resource && resource->stillNeedsLoad());
    if (!context().shouldLoadNewResource(resource->getType())) {
        memoryCache()->remove(resource);
        return false;
    }

    ResourceRequest request(resource->resourceRequest());
    willSendRequest(resource->identifier(), request, ResourceResponse(), resource->options());

    ResourceLoader* loader = ResourceLoader::create(this, resource);
    if (resource->shouldBlockLoadEvent())
        m_loaders.add(loader);
    else
        m_nonBlockingLoaders.add(loader);

    storeResourceTimingInitiatorInformation(resource);
    resource->setFetcherSecurityOrigin(context().getSecurityOrigin());
    loader->start(request, context().loadingTaskRunner(), context().defersLoading());
    return true;
}

void ResourceFetcher::removeResourceLoader(ResourceLoader* loader)
{
    if (m_loaders.contains(loader))
        m_loaders.remove(loader);
    else if (m_nonBlockingLoaders.contains(loader))
        m_nonBlockingLoaders.remove(loader);
    else
        ASSERT_NOT_REACHED();
}

void ResourceFetcher::stopFetching()
{
    m_nonBlockingLoaders.cancelAll();
    m_loaders.cancelAll();
}

bool ResourceFetcher::isFetching() const
{
    return !m_loaders.isEmpty();
}

void ResourceFetcher::setDefersLoading(bool defers)
{
    m_loaders.setAllDefersLoading(defers);
    m_nonBlockingLoaders.setAllDefersLoading(defers);
}

bool ResourceFetcher::defersLoading() const
{
    return context().defersLoading();
}

static bool isManualRedirectFetchRequest(const ResourceRequest& request)
{
    return request.fetchRedirectMode() == WebURLRequest::FetchRedirectModeManual && request.requestContext() == WebURLRequest::RequestContextFetch;
}

bool ResourceFetcher::willFollowRedirect(Resource* resource, ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (!isManualRedirectFetchRequest(resource->resourceRequest())) {
        if (!context().canRequest(resource->getType(), newRequest, newRequest.url(), resource->options(), resource->isUnusedPreload(), FetchRequest::UseDefaultOriginRestrictionForType))
            return false;
        if (resource->options().corsEnabled == IsCORSEnabled) {
            SecurityOrigin* sourceOrigin = resource->options().securityOrigin.get();
            if (!sourceOrigin)
                sourceOrigin = context().getSecurityOrigin();

            String errorMessage;
            StoredCredentials withCredentials = resource->lastResourceRequest().allowStoredCredentials() ? AllowStoredCredentials : DoNotAllowStoredCredentials;
            if (!CrossOriginAccessControl::handleRedirect(sourceOrigin, newRequest, redirectResponse, withCredentials, resource->mutableOptions(), errorMessage)) {
                resource->setCORSFailed();
                context().addConsoleMessage(errorMessage);
                return false;
            }
        }
        if (resource->getType() == Resource::Image && shouldDeferImageLoad(newRequest.url()))
            return false;
    }

    ResourceTimingInfoMap::iterator it = m_resourceTimingInfoMap.find(resource);
    if (it != m_resourceTimingInfoMap.end())
        it->value->addRedirect(redirectResponse);
    newRequest.setAllowStoredCredentials(resource->options().allowCredentials == AllowStoredCredentials);
    willSendRequest(resource->identifier(), newRequest, redirectResponse, resource->options());
    return true;
}

void ResourceFetcher::willSendRequest(unsigned long identifier, ResourceRequest& newRequest, const ResourceResponse& redirectResponse, const ResourceLoaderOptions& options)
{
    context().dispatchWillSendRequest(identifier, newRequest, redirectResponse, options.initiatorInfo);
}

void ResourceFetcher::updateAllImageResourcePriorities()
{
    TRACE_EVENT0("blink", "ResourceLoadPriorityOptimizer::updateAllImageResourcePriorities");
    for (const auto& documentResource : m_documentResources) {
        Resource* resource = documentResource.value.get();
        if (!resource || !resource->isImage() || !resource->isLoading())
            continue;

        ResourcePriority resourcePriority = resource->priorityFromObservers();
        ResourceLoadPriority resourceLoadPriority = loadPriority(Resource::Image, FetchRequest(resource->resourceRequest(), FetchInitiatorInfo()), resourcePriority.visibility);
        if (resourceLoadPriority == resource->resourceRequest().priority())
            continue;

        resource->didChangePriority(resourceLoadPriority, resourcePriority.intraPriorityValue);
        TRACE_EVENT_ASYNC_STEP_INTO1("blink.net", "Resource", resource->identifier(), "ChangePriority", "priority", resourceLoadPriority);
        context().dispatchDidChangeResourcePriority(resource->identifier(), resourceLoadPriority, resourcePriority.intraPriorityValue);
    }
}

void ResourceFetcher::reloadLoFiImages()
{
    for (const auto& documentResource : m_documentResources) {
        Resource* resource = documentResource.value.get();
        if (resource && resource->isImage()) {
            ImageResource* imageResource = toImageResource(resource);
            imageResource->reloadIfLoFi(this);
        }
    }
}

void ResourceFetcher::logPreloadStats()
{
    if (!m_preloads)
        return;
    unsigned scripts = 0;
    unsigned scriptMisses = 0;
    unsigned stylesheets = 0;
    unsigned stylesheetMisses = 0;
    unsigned images = 0;
    unsigned imageMisses = 0;
    unsigned fonts = 0;
    unsigned fontMisses = 0;
    unsigned medias = 0;
    unsigned mediaMisses = 0;
    unsigned textTracks = 0;
    unsigned textTrackMisses = 0;
    unsigned imports = 0;
    unsigned importMisses = 0;
    unsigned raws = 0;
    unsigned rawMisses = 0;
    for (auto resource : *m_preloads) {
        int missCount = resource->getPreloadResult() == Resource::PreloadNotReferenced ? 1 : 0;
        switch (resource->getType()) {
        case Resource::Image:
            images++;
            imageMisses += missCount;
            break;
        case Resource::Script:
            scripts++;
            scriptMisses += missCount;
            break;
        case Resource::CSSStyleSheet:
            stylesheets++;
            stylesheetMisses += missCount;
            break;
        case Resource::Font:
            fonts++;
            fontMisses += missCount;
            break;
        case Resource::Media:
            medias++;
            mediaMisses += missCount;
            break;
        case Resource::TextTrack:
            textTracks++;
            textTrackMisses += missCount;
            break;
        case Resource::ImportResource:
            imports++;
            importMisses += missCount;
            break;
        case Resource::Raw:
            raws++;
            rawMisses += missCount;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
    // TODO(csharrison): These can falsely attribute link rel="preload" requests
    // as misses if they are referenced after parsing completes. Migrate this
    // logic to the memory cache / individual resources to prevent this.
    DEFINE_STATIC_LOCAL(CustomCountHistogram, imagePreloads, ("PreloadScanner.Counts2.Image", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, imagePreloadMisses, ("PreloadScanner.Counts2.Miss.Image", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, scriptPreloads, ("PreloadScanner.Counts2.Script", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, scriptPreloadMisses, ("PreloadScanner.Counts2.Miss.Script", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, stylesheetPreloads, ("PreloadScanner.Counts2.CSSStyleSheet", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, stylesheetPreloadMisses, ("PreloadScanner.Counts2.Miss.CSSStyleSheet", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, fontPreloads, ("PreloadScanner.Counts2.Font", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, fontPreloadMisses, ("PreloadScanner.Counts2.Miss.Font", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, mediaPreloads, ("PreloadScanner.Counts2.Media", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, mediaPreloadMisses, ("PreloadScanner.Counts2.Miss.Media", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, textTrackPreloads, ("PreloadScanner.Counts2.TextTrack", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, textTrackPreloadMisses, ("PreloadScanner.Counts2.Miss.TextTrack", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, importPreloads, ("PreloadScanner.Counts2.Import", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, importPreloadMisses, ("PreloadScanner.Counts2.Miss.Import", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, rawPreloads, ("PreloadScanner.Counts2.Raw", 0, 100, 25));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, rawPreloadMisses, ("PreloadScanner.Counts2.Miss.Raw", 0, 100, 25));
    if (images)
        imagePreloads.count(images);
    if (imageMisses)
        imagePreloadMisses.count(imageMisses);
    if (scripts)
        scriptPreloads.count(scripts);
    if (scriptMisses)
        scriptPreloadMisses.count(scriptMisses);
    if (stylesheets)
        stylesheetPreloads.count(stylesheets);
    if (stylesheetMisses)
        stylesheetPreloadMisses.count(stylesheetMisses);
    if (fonts)
        fontPreloads.count(fonts);
    if (fontMisses)
        fontPreloadMisses.count(fontMisses);
    if (medias)
        mediaPreloads.count(medias);
    if (mediaMisses)
        mediaPreloadMisses.count(mediaMisses);
    if (textTracks)
        textTrackPreloads.count(textTracks);
    if (textTrackMisses)
        textTrackPreloadMisses.count(textTrackMisses);
    if (imports)
        importPreloads.count(imports);
    if (importMisses)
        importPreloadMisses.count(importMisses);
    if (raws)
        rawPreloads.count(raws);
    if (rawMisses)
        rawPreloadMisses.count(rawMisses);
}

const ResourceLoaderOptions& ResourceFetcher::defaultResourceOptions()
{
    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, options, (BufferData, AllowStoredCredentials, ClientRequestedCredentials, CheckContentSecurityPolicy, DocumentContext));
    return options;
}

String ResourceFetcher::getCacheIdentifier() const
{
    if (context().isControlledByServiceWorker())
        return String::number(context().serviceWorkerID());
    return MemoryCache::defaultCacheIdentifier();
}

ResourceFetcher::DeadResourceStatsRecorder::DeadResourceStatsRecorder()
    : m_useCount(0)
    , m_revalidateCount(0)
    , m_loadCount(0)
{
}

ResourceFetcher::DeadResourceStatsRecorder::~DeadResourceStatsRecorder()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, hitCountHistogram, new CustomCountHistogram("WebCore.ResourceFetcher.HitCount", 0, 1000, 50));
    hitCountHistogram.count(m_useCount);
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, revalidateCountHistogram, new CustomCountHistogram("WebCore.ResourceFetcher.RevalidateCount", 0, 1000, 50));
    revalidateCountHistogram.count(m_revalidateCount);
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, loadCountHistogram, new CustomCountHistogram("WebCore.ResourceFetcher.LoadCount", 0, 1000, 50));
    loadCountHistogram.count(m_loadCount);
}

void ResourceFetcher::DeadResourceStatsRecorder::update(RevalidationPolicy policy)
{
    switch (policy) {
    case Reload:
    case Load:
        ++m_loadCount;
        return;
    case Revalidate:
        ++m_revalidateCount;
        return;
    case Use:
        ++m_useCount;
        return;
    }
}

DEFINE_TRACE(ResourceFetcher)
{
    visitor->trace(m_context);
    visitor->trace(m_archive);
    visitor->trace(m_loaders);
    visitor->trace(m_nonBlockingLoaders);
    visitor->trace(m_documentResources);
    visitor->trace(m_preloads);
    visitor->trace(m_resourceTimingInfoMap);
}

} // namespace blink
