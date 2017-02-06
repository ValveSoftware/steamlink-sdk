/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef ResourceFetcher_h
#define ResourceFetcher_h

#include "core/CoreExport.h"
#include "core/fetch/CachePolicy.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/fetch/ResourceLoaderSet.h"
#include "core/fetch/SubstituteData.h"
#include "platform/Timer.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceLoadPriority.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/text/StringHash.h"
#include <memory>

namespace blink {

class ArchiveResource;
class CSSStyleSheetResource;
class DocumentResource;
class FontResource;
class ImageResource;
class MHTMLArchive;
class RawResource;
class ScriptResource;
class XSLStyleSheetResource;
class KURL;
class ResourceTimingInfo;

// The ResourceFetcher provides a per-context interface to the MemoryCache
// and enforces a bunch of security checks and rules for resource revalidation.
// Its lifetime is roughly per-DocumentLoader, in that it is generally created
// in the DocumentLoader constructor and loses its ability to generate network
// requests when the DocumentLoader is destroyed. Documents also hold a pointer
// to ResourceFetcher for their lifetime (and will create one if they
// are initialized without a LocalFrame), so a Document can keep a ResourceFetcher
// alive past detach if scripts still reference the Document.
class CORE_EXPORT ResourceFetcher : public GarbageCollectedFinalized<ResourceFetcher> {
    WTF_MAKE_NONCOPYABLE(ResourceFetcher);
    USING_PRE_FINALIZER(ResourceFetcher, clearPreloads);
public:
    static ResourceFetcher* create(FetchContext* context) { return new ResourceFetcher(context); }
    virtual ~ResourceFetcher();
    DECLARE_VIRTUAL_TRACE();

    Resource* requestResource(FetchRequest&, const ResourceFactory&, const SubstituteData& = SubstituteData());

    Resource* cachedResource(const KURL&) const;

    using DocumentResourceMap = HeapHashMap<String, WeakMember<Resource>>;
    const DocumentResourceMap& allResources() const { return m_documentResources; }

    // Actually starts loading a Resource if it wasn't started during requestResource().
    bool startLoad(Resource*);

    void setAutoLoadImages(bool);
    void setImagesEnabled(bool);

    FetchContext& context() const { return m_context ? *m_context.get() : FetchContext::nullInstance(); }
    void clearContext() { m_context.clear(); }

    int requestCount() const;

    enum ClearPreloadsPolicy { ClearAllPreloads, ClearSpeculativeMarkupPreloads };

    bool isPreloaded(const KURL&) const;
    int countPreloads() const { return m_preloads ? m_preloads->size() : 0; }
    void clearPreloads(ClearPreloadsPolicy = ClearAllPreloads);
    void preloadStarted(Resource*);
    void logPreloadStats();

    MHTMLArchive* archive() const { return m_archive.get(); }
    ArchiveResource* createArchive(Resource*);

    void setDefersLoading(bool);
    void stopFetching();
    bool isFetching() const;

    bool willFollowRedirect(Resource*, ResourceRequest&, const ResourceResponse&);
    enum DidFinishLoadingReason {
        DidFinishLoading,
        DidFinishFirstPartInMultipart
    };
    void didFinishLoading(Resource*, double finishTime, int64_t encodedDataLength, DidFinishLoadingReason);
    void didFailLoading(Resource*, const ResourceError&);
    void didReceiveResponse(Resource*, const ResourceResponse&);
    void didReceiveData(const Resource*, const char* data, int dataLength, int encodedDataLength);
    void didDownloadData(const Resource*, int dataLength, int encodedDataLength);
    bool defersLoading() const;

    enum AccessControlLoggingDecision {
        ShouldLogAccessControlErrors,
        ShouldNotLogAccessControlErrors
    };
    bool canAccessResource(Resource*, SecurityOrigin*, const KURL&, AccessControlLoggingDecision) const;
    bool isControlledByServiceWorker() const;

    void acceptDataFromThreadedReceiver(unsigned long identifier, const char* data, int dataLength, int encodedDataLength);

    ResourceLoadPriority loadPriority(Resource::Type, const FetchRequest&, ResourcePriority::VisibilityStatus = ResourcePriority::NotVisible);

    enum ResourceLoadStartType {
        ResourceLoadingFromNetwork,
        ResourceLoadingFromCache
    };
    void requestLoadStarted(unsigned long identifier, Resource*, const FetchRequest&, ResourceLoadStartType, bool isStaticData = false);
    static const ResourceLoaderOptions& defaultResourceOptions();

    String getCacheIdentifier() const;

    static void determineRequestContext(ResourceRequest&, Resource::Type, bool isMainFrame);
    void determineRequestContext(ResourceRequest&, Resource::Type);

    void updateAllImageResourcePriorities();

    void reloadLoFiImages();

    // This is only exposed for testing purposes.
    HeapListHashSet<Member<Resource>>* preloads() { return m_preloads.get(); }

private:
    friend class ResourceCacheValidationSuppressor;

    explicit ResourceFetcher(FetchContext*);

    void initializeRevalidation(ResourceRequest&, Resource*);
    Resource* createResourceForLoading(FetchRequest&, const String& charset, const ResourceFactory&);
    void storeResourceTimingInitiatorInformation(Resource*);

    Resource* resourceForStaticData(const FetchRequest&, const ResourceFactory&, const SubstituteData&);

    // RevalidationPolicy enum values are used in UMAs https://crbug.com/579496.
    enum RevalidationPolicy { Use, Revalidate, Reload, Load };
    RevalidationPolicy determineRevalidationPolicy(Resource::Type, const FetchRequest&, Resource* existingResource, bool isStaticData) const;

    void moveCachedNonBlockingResourceToBlocking(Resource*, const FetchRequest&);
    void moveResourceLoaderToNonBlocking(ResourceLoader*);
    void removeResourceLoader(ResourceLoader*);

    void initializeResourceRequest(ResourceRequest&, Resource::Type, FetchRequest::DeferOption);
    void willSendRequest(unsigned long identifier, ResourceRequest&, const ResourceResponse&, const ResourceLoaderOptions&);

    bool resourceNeedsLoad(Resource*, const FetchRequest&, RevalidationPolicy);
    bool shouldDeferImageLoad(const KURL&) const;

    void resourceTimingReportTimerFired(Timer<ResourceFetcher>*);

    void reloadImagesIfNotDeferred();

    void updateMemoryCacheStats(Resource*, RevalidationPolicy, const FetchRequest&,  const ResourceFactory&, bool isStaticData) const;

    Member<FetchContext> m_context;

    HashSet<String> m_validatedURLs;
    mutable DocumentResourceMap m_documentResources;

    Member<HeapListHashSet<Member<Resource>>> m_preloads;
    Member<MHTMLArchive> m_archive;

    Timer<ResourceFetcher> m_resourceTimingReportTimer;

    using ResourceTimingInfoMap = HeapHashMap<Member<Resource>, std::unique_ptr<ResourceTimingInfo>>;
    ResourceTimingInfoMap m_resourceTimingInfoMap;

    Vector<std::unique_ptr<ResourceTimingInfo>> m_scheduledResourceTimingReports;

    ResourceLoaderSet m_loaders;
    ResourceLoaderSet m_nonBlockingLoaders;

    // Used in hit rate histograms.
    class DeadResourceStatsRecorder {
        DISALLOW_NEW();
    public:
        DeadResourceStatsRecorder();
        ~DeadResourceStatsRecorder();

        void update(RevalidationPolicy);

    private:
        int m_useCount;
        int m_revalidateCount;
        int m_loadCount;
    };
    DeadResourceStatsRecorder m_deadStatsRecorder;

    // 28 bits left
    bool m_autoLoadImages : 1;
    bool m_imagesEnabled : 1;
    bool m_allowStaleResources : 1;
    bool m_imageFetched : 1;
};

class ResourceCacheValidationSuppressor {
    WTF_MAKE_NONCOPYABLE(ResourceCacheValidationSuppressor);
    STACK_ALLOCATED();
public:
    ResourceCacheValidationSuppressor(ResourceFetcher* loader)
        : m_loader(loader)
        , m_previousState(false)
    {
        if (m_loader) {
            m_previousState = m_loader->m_allowStaleResources;
            m_loader->m_allowStaleResources = true;
        }
    }
    ~ResourceCacheValidationSuppressor()
    {
        if (m_loader)
            m_loader->m_allowStaleResources = m_previousState;
    }
private:
    Member<ResourceFetcher> m_loader;
    bool m_previousState;
};

} // namespace blink

#endif // ResourceFetcher_h
