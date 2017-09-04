/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.

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

#ifndef Resource_h
#define Resource_h

#include "core/CoreExport.h"
#include "core/fetch/CachedMetadataHandler.h"
#include "core/fetch/IntegrityMetadata.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/MemoryCoordinator.h"
#include "platform/SharedBuffer.h"
#include "platform/Timer.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/tracing/web_process_memory_dump.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/Allocator.h"
#include "wtf/AutoReset.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashSet.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

struct FetchInitiatorInfo;
class CachedMetadata;
class FetchRequest;
class ResourceClient;
class ResourceTimingInfo;
class ResourceLoader;
class SecurityOrigin;

// A resource that is held in the cache. Classes who want to use this object
// should derive from ResourceClient, to get the function calls in case the
// requested data has arrived. This class also does the actual communication
// with the loader to obtain the resource from the network.
class CORE_EXPORT Resource : public GarbageCollectedFinalized<Resource>,
                             public MemoryCoordinatorClient {
  USING_GARBAGE_COLLECTED_MIXIN(Resource);
  WTF_MAKE_NONCOPYABLE(Resource);

 public:
  // |Type| enum values are used in UMAs, so do not change the values of
  // existing |Type|. When adding a new |Type|, append it at the end and update
  // |kLastResourceType|.
  enum Type {
    MainResource,
    Image,
    CSSStyleSheet,
    Script,
    Font,
    Raw,
    SVGDocument,
    XSLStyleSheet,
    LinkPrefetch,
    TextTrack,
    ImportResource,
    Media,  // Audio or video file requested by a HTML5 media element
    Manifest
  };
  static const int kLastResourceType = Manifest + 1;

  enum Status {
    NotStarted,
    Pending,  // load in progress
    Cached,   // load completed successfully
    LoadError,
    DecodeError
  };

  // Whether a resource client for a preload should mark the preload as
  // referenced.
  enum PreloadReferencePolicy {
    MarkAsReferenced,
    DontMarkAsReferenced,
  };

  virtual ~Resource();

  DECLARE_VIRTUAL_TRACE();

  virtual void setEncoding(const String&) {}
  virtual String encoding() const { return String(); }
  virtual void appendData(const char*, size_t);
  virtual void error(const ResourceError&);
  virtual void setCORSFailed() {}

  void setNeedsSynchronousCacheHit(bool needsSynchronousCacheHit) {
    m_needsSynchronousCacheHit = needsSynchronousCacheHit;
  }

  void setLinkPreload(bool isLinkPreload) { m_linkPreload = isLinkPreload; }
  bool isLinkPreload() const { return m_linkPreload; }

  void setPreloadDiscoveryTime(double preloadDiscoveryTime) {
    m_preloadDiscoveryTime = preloadDiscoveryTime;
  }

  const ResourceError& resourceError() const { return m_error; }

  void setIdentifier(unsigned long identifier) { m_identifier = identifier; }
  unsigned long identifier() const { return m_identifier; }

  virtual bool shouldIgnoreHTTPStatusCodeErrors() const { return false; }

  const ResourceRequest& resourceRequest() const { return m_resourceRequest; }
  const ResourceRequest& lastResourceRequest() const;

  virtual void setRevalidatingRequest(const ResourceRequest&);

  void setFetcherSecurityOrigin(SecurityOrigin* origin) {
    m_fetcherSecurityOrigin = origin;
  }

  // This url can have a fragment, but it can match resources that differ by the
  // fragment only.
  const KURL& url() const { return m_resourceRequest.url(); }
  Type getType() const { return static_cast<Type>(m_type); }
  const ResourceLoaderOptions& options() const { return m_options; }
  ResourceLoaderOptions& mutableOptions() { return m_options; }

  void didChangePriority(ResourceLoadPriority, int intraPriorityValue);
  virtual ResourcePriority priorityFromObservers() {
    return ResourcePriority();
  }

  // The reference policy indicates that the client should not affect whether
  // a preload is considered referenced or not. This allows for "passive"
  // resource clients that simply observe the resource.
  void addClient(ResourceClient*, PreloadReferencePolicy = MarkAsReferenced);
  void removeClient(ResourceClient*);

  enum PreloadResult {
    PreloadNotReferenced,
    PreloadReferenced,
    PreloadReferencedWhileLoading,
    PreloadReferencedWhileComplete
  };
  PreloadResult getPreloadResult() const { return m_preloadResult; }

  Status getStatus() const { return m_status; }
  void setStatus(Status status) { m_status = status; }

  size_t size() const { return encodedSize() + decodedSize() + overheadSize(); }

  // Returns the size of content (response body) before decoding. Adding a new
  // usage of this function is not recommended (See the TODO below).
  //
  // TODO(hiroshige): Now encodedSize/decodedSize states are inconsistent and
  // need to be refactored (crbug/643135).
  size_t encodedSize() const { return m_encodedSize; }

  // Returns the current memory usage for the encoded data. Adding a new usage
  // of this function is not recommended as the same reason as |encodedSize()|.
  //
  // |encodedSize()| and |encodedSizeMemoryUsageForTesting()| can return
  // different values, e.g., when ImageResource purges encoded image data after
  // finishing loading.
  size_t encodedSizeMemoryUsageForTesting() const {
    return m_encodedSizeMemoryUsage;
  }

  size_t decodedSize() const { return m_decodedSize; }
  size_t overheadSize() const { return m_overheadSize; }

  bool isLoaded() const { return m_status > Pending; }

  bool isLoading() const { return m_status == Pending; }
  bool stillNeedsLoad() const { return m_status < Pending; }

  void setLoader(ResourceLoader*);
  ResourceLoader* loader() const { return m_loader.get(); }

  virtual bool isImage() const { return false; }
  bool shouldBlockLoadEvent() const;
  bool isLoadEventBlockingResourceType() const;

  // Computes the status of an object after loading. Updates the expire date on
  // the cache entry file
  virtual void finish(double finishTime);
  void finish() { finish(0.0); }

  // FIXME: Remove the stringless variant once all the callsites' error messages
  // are updated.
  bool passesAccessControlCheck(SecurityOrigin*) const;
  bool passesAccessControlCheck(SecurityOrigin*,
                                String& errorDescription) const;

  virtual PassRefPtr<const SharedBuffer> resourceBuffer() const {
    return m_data;
  }
  void setResourceBuffer(PassRefPtr<SharedBuffer>);

  virtual bool willFollowRedirect(const ResourceRequest&,
                                  const ResourceResponse&);

  // Called when a redirect response was received but a decision has already
  // been made to not follow it.
  virtual void willNotFollowRedirect() {}

  virtual void responseReceived(const ResourceResponse&,
                                std::unique_ptr<WebDataConsumerHandle>);
  void setResponse(const ResourceResponse&);
  const ResourceResponse& response() const { return m_response; }

  virtual void reportResourceTimingToClients(const ResourceTimingInfo&) {}

  // Sets the serialized metadata retrieved from the platform's cache.
  virtual void setSerializedCachedMetadata(const char*, size_t);

  // This may return nullptr when the resource isn't cacheable.
  CachedMetadataHandler* cacheHandler();

  AtomicString httpContentType() const;

  bool wasCanceled() const { return m_error.isCancellation(); }
  bool errorOccurred() const {
    return m_status == LoadError || m_status == DecodeError;
  }
  bool loadFailedOrCanceled() const { return !m_error.isNull(); }

  DataBufferingPolicy getDataBufferingPolicy() const {
    return m_options.dataBufferingPolicy;
  }
  void setDataBufferingPolicy(DataBufferingPolicy);

  // The isPreloaded() flag is using a counter in order to make sure that even
  // when multiple ResourceFetchers are preloading the resource, it will remain
  // marked as preloaded until *all* of them have used it.
  bool isUnusedPreload() const {
    return isPreloaded() && getPreloadResult() == PreloadNotReferenced;
  }
  bool isPreloaded() const { return m_preloadCount; }
  void increasePreloadCount() { ++m_preloadCount; }
  void decreasePreloadCount() {
    DCHECK(m_preloadCount);
    --m_preloadCount;
  }

  bool canReuseRedirectChain();
  bool mustRevalidateDueToCacheHeaders();
  bool canUseCacheValidator();
  bool isCacheValidator() const { return m_isRevalidating; }
  bool hasCacheControlNoStoreHeader() const;
  bool hasVaryHeader() const;

  bool isEligibleForIntegrityCheck(SecurityOrigin*) const;

  void setIntegrityMetadata(const IntegrityMetadataSet& metadata) {
    m_integrityMetadata = metadata;
  }
  const IntegrityMetadataSet& integrityMetadata() const {
    return m_integrityMetadata;
  }
  // The argument must never be |NotChecked|.
  void setIntegrityDisposition(ResourceIntegrityDisposition);
  ResourceIntegrityDisposition integrityDisposition() const {
    return m_integrityDisposition;
  }
  bool mustRefetchDueToIntegrityMetadata(const FetchRequest&) const;

  double currentAge() const;
  double freshnessLifetime();
  double stalenessLifetime();

  bool isAlive() const { return m_isAlive; }

  void setCacheIdentifier(const String& cacheIdentifier) {
    m_cacheIdentifier = cacheIdentifier;
  }
  String cacheIdentifier() const { return m_cacheIdentifier; }

  virtual void didSendData(unsigned long long /* bytesSent */,
                           unsigned long long /* totalBytesToBeSent */) {}
  virtual void didDownloadData(int) {}

  double loadFinishTime() const { return m_loadFinishTime; }

  void addToEncodedBodyLength(int value) {
    m_response.addToEncodedBodyLength(value);
  }
  void addToDecodedBodyLength(int value) {
    m_response.addToDecodedBodyLength(value);
  }

  virtual bool canReuse(const ResourceRequest&) const { return true; }

  // If cache-aware loading is activated, this callback is called when the first
  // disk-cache-only request failed due to cache miss. After this callback,
  // cache-aware loading is deactivated and a reload with original request will
  // be triggered right away in ResourceLoader.
  virtual void willReloadAfterDiskCacheMiss() {}

  // TODO(shaochuan): This is for saving back the actual ResourceRequest sent
  // in ResourceFetcher::startLoad() for retry in cache-aware loading, remove
  // once ResourceRequest is not modified in startLoad(). crbug.com/632580
  void setResourceRequest(const ResourceRequest& resourceRequest) {
    m_resourceRequest = resourceRequest;
  }

  // Used by the MemoryCache to reduce the memory consumption of the entry.
  void prune();

  virtual void onMemoryDump(WebMemoryDumpLevelOfDetail,
                            WebProcessMemoryDump*) const;

  static const char* resourceTypeToString(Type, const FetchInitiatorInfo&);

 protected:
  Resource(const ResourceRequest&, Type, const ResourceLoaderOptions&);

  virtual void checkNotify();

  enum class MarkFinishedOption { ShouldMarkFinished, DoNotMarkFinished };
  void notifyClientsInternal(MarkFinishedOption);
  void markClientFinished(ResourceClient*);

  virtual bool hasClientsOrObservers() const {
    return !m_clients.isEmpty() || !m_clientsAwaitingCallback.isEmpty() ||
           !m_finishedClients.isEmpty();
  }
  virtual void destroyDecodedDataForFailedRevalidation() {}

  void setEncodedSize(size_t);
  void setDecodedSize(size_t);

  void finishPendingClients();

  virtual void didAddClient(ResourceClient*);
  void willAddClientOrObserver(PreloadReferencePolicy);

  void didRemoveClientOrObserver();
  virtual void allClientsAndObserversRemoved();

  bool hasClient(ResourceClient* client) {
    return m_clients.contains(client) ||
           m_clientsAwaitingCallback.contains(client) ||
           m_finishedClients.contains(client);
  }

  struct RedirectPair {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    explicit RedirectPair(const ResourceRequest& request,
                          const ResourceResponse& redirectResponse)
        : m_request(request), m_redirectResponse(redirectResponse) {}

    ResourceRequest m_request;
    ResourceResponse m_redirectResponse;
  };
  const Vector<RedirectPair>& redirectChain() const { return m_redirectChain; }

  virtual void destroyDecodedDataIfPossible() {}

  // Returns the memory dump name used for tracing. See Resource::onMemoryDump.
  String getMemoryDumpName() const;

  const HeapHashCountedSet<WeakMember<ResourceClient>>& clients() const {
    return m_clients;
  }
  DataBufferingPolicy dataBufferingPolicy() const {
    return m_options.dataBufferingPolicy;
  }

  void setCachePolicyBypassingCache();
  void setLoFiStateOff();
  void clearRangeRequestHeader();

  SharedBuffer* data() const { return m_data.get(); }
  void clearData();

  class ProhibitAddRemoveClientInScope : public AutoReset<bool> {
   public:
    ProhibitAddRemoveClientInScope(Resource* resource)
        : AutoReset(&resource->m_isAddRemoveClientProhibited, true) {}
  };

 private:
  class ResourceCallback;
  class CachedMetadataHandlerImpl;
  class ServiceWorkerResponseCachedMetadataHandler;

  void cancelTimerFired(TimerBase*);

  void revalidationSucceeded(const ResourceResponse&);
  void revalidationFailed();

  size_t calculateOverheadSize() const;

  String reasonNotDeletable() const;

  // MemoryCoordinatorClient overrides:
  void onMemoryStateChange(MemoryState) override;

  Member<CachedMetadataHandlerImpl> m_cacheHandler;
  RefPtr<SecurityOrigin> m_fetcherSecurityOrigin;

  ResourceError m_error;

  double m_loadFinishTime;

  unsigned long m_identifier;

  size_t m_encodedSize;
  size_t m_encodedSizeMemoryUsage;
  size_t m_decodedSize;

  // Resource::calculateOverheadSize() is affected by changes in
  // |m_resourceRequest.url()|, but |m_overheadSize| is not updated after
  // initial |m_resourceRequest| is given, to reduce MemoryCache manipulation
  // and thus potential bugs. crbug.com/594644
  const size_t m_overheadSize;

  unsigned m_preloadCount;

  double m_preloadDiscoveryTime;

  String m_cacheIdentifier;

  PreloadResult m_preloadResult;
  Type m_type;
  Status m_status;

  bool m_needsSynchronousCacheHit;
  bool m_linkPreload;
  bool m_isRevalidating;
  bool m_isAlive;
  bool m_isAddRemoveClientProhibited;

  ResourceIntegrityDisposition m_integrityDisposition;
  IntegrityMetadataSet m_integrityMetadata;

  // Ordered list of all redirects followed while fetching this resource.
  Vector<RedirectPair> m_redirectChain;

  HeapHashCountedSet<WeakMember<ResourceClient>> m_clients;
  HeapHashCountedSet<WeakMember<ResourceClient>> m_clientsAwaitingCallback;
  HeapHashCountedSet<WeakMember<ResourceClient>> m_finishedClients;

  ResourceLoaderOptions m_options;

  double m_responseTimestamp;

  Timer<Resource> m_cancelTimer;

  ResourceRequest m_resourceRequest;
  Member<ResourceLoader> m_loader;
  ResourceResponse m_response;

  RefPtr<SharedBuffer> m_data;
};

class ResourceFactory {
  STACK_ALLOCATED();

 public:
  virtual Resource* create(const ResourceRequest&,
                           const ResourceLoaderOptions&,
                           const String&) const = 0;
  Resource::Type type() const { return m_type; }

 protected:
  explicit ResourceFactory(Resource::Type type) : m_type(type) {}

  Resource::Type m_type;
};

#define DEFINE_RESOURCE_TYPE_CASTS(typeName)                   \
  DEFINE_TYPE_CASTS(typeName##Resource, Resource, resource,    \
                    resource->getType() == Resource::typeName, \
                    resource.getType() == Resource::typeName);

}  // namespace blink

#endif
