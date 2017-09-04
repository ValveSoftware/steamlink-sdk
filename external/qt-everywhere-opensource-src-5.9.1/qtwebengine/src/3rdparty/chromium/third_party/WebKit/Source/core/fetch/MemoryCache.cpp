/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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

#include "core/fetch/MemoryCache.h"

#include "core/fetch/ResourceLoadingLog.h"
#include "platform/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginHash.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/AutoReset.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/text/CString.h"

namespace blink {

static Persistent<MemoryCache>* gMemoryCache;

static const unsigned cDefaultCacheCapacity = 8192 * 1024;
static const int cMinDelayBeforeLiveDecodedPrune = 1;  // Seconds.
static const double cMaxPruneDeferralDelay = 0.5;      // Seconds.

// Percentage of capacity toward which we prune, to avoid immediately pruning
// again.
static const float cTargetPrunePercentage = .95f;

MemoryCache* memoryCache() {
  DCHECK(WTF::isMainThread());
  if (!gMemoryCache)
    gMemoryCache = new Persistent<MemoryCache>(MemoryCache::create());
  return gMemoryCache->get();
}

MemoryCache* replaceMemoryCacheForTesting(MemoryCache* cache) {
  memoryCache();
  MemoryCache* oldCache = gMemoryCache->release();
  *gMemoryCache = cache;
  MemoryCacheDumpProvider::instance()->setMemoryCache(cache);
  return oldCache;
}

DEFINE_TRACE(MemoryCacheEntry) {
  visitor->template registerWeakMembers<MemoryCacheEntry,
                                        &MemoryCacheEntry::clearResourceWeak>(
      this);
}

void MemoryCacheEntry::clearResourceWeak(Visitor* visitor) {
  if (!m_resource || ThreadHeap::isHeapObjectAlive(m_resource))
    return;
  memoryCache()->remove(m_resource.get());
  m_resource.clear();
}

inline MemoryCache::MemoryCache()
    : m_inPruneResources(false),
      m_prunePending(false),
      m_maxPruneDeferralDelay(cMaxPruneDeferralDelay),
      m_pruneTimeStamp(0.0),
      m_pruneFrameTimeStamp(0.0),
      m_lastFramePaintTimeStamp(0.0),
      m_capacity(cDefaultCacheCapacity),
      m_delayBeforeLiveDecodedPrune(cMinDelayBeforeLiveDecodedPrune),
      m_size(0) {
  MemoryCacheDumpProvider::instance()->setMemoryCache(this);
  if (MemoryCoordinator::isLowEndDevice())
    MemoryCoordinator::instance().registerClient(this);
}

MemoryCache* MemoryCache::create() {
  return new MemoryCache;
}

MemoryCache::~MemoryCache() {
  if (m_prunePending)
    Platform::current()->currentThread()->removeTaskObserver(this);
}

DEFINE_TRACE(MemoryCache) {
  visitor->trace(m_resourceMaps);
  MemoryCacheDumpClient::trace(visitor);
  MemoryCoordinatorClient::trace(visitor);
}

KURL MemoryCache::removeFragmentIdentifierIfNeeded(const KURL& originalURL) {
  if (!originalURL.hasFragmentIdentifier())
    return originalURL;
  // Strip away fragment identifier from HTTP URLs. Data URLs must be
  // unmodified. For file and custom URLs clients may expect resources to be
  // unique even when they differ by the fragment identifier only.
  if (!originalURL.protocolIsInHTTPFamily())
    return originalURL;
  KURL url = originalURL;
  url.removeFragmentIdentifier();
  return url;
}

String MemoryCache::defaultCacheIdentifier() {
  return emptyString();
}

MemoryCache::ResourceMap* MemoryCache::ensureResourceMap(
    const String& cacheIdentifier) {
  if (!m_resourceMaps.contains(cacheIdentifier)) {
    ResourceMapIndex::AddResult result =
        m_resourceMaps.add(cacheIdentifier, new ResourceMap);
    CHECK(result.isNewEntry);
  }
  return m_resourceMaps.get(cacheIdentifier);
}

void MemoryCache::add(Resource* resource) {
  DCHECK(resource);
  ResourceMap* resources = ensureResourceMap(resource->cacheIdentifier());
  addInternal(resources, MemoryCacheEntry::create(resource));
  RESOURCE_LOADING_DVLOG(1) << "MemoryCache::add Added "
                            << resource->url().getString() << ", resource "
                            << resource;
}

void MemoryCache::addInternal(ResourceMap* resourceMap,
                              MemoryCacheEntry* entry) {
  DCHECK(WTF::isMainThread());
  DCHECK(resourceMap);

  Resource* resource = entry->resource();
  if (!resource)
    return;
  DCHECK(resource->url().isValid());

  KURL url = removeFragmentIdentifierIfNeeded(resource->url());
  ResourceMap::iterator it = resourceMap->find(url);
  if (it != resourceMap->end()) {
    Resource* oldResource = it->value->resource();
    CHECK_NE(oldResource, resource);
    update(oldResource, oldResource->size(), 0);
  }
  resourceMap->set(url, entry);
  update(resource, 0, resource->size());
}

void MemoryCache::remove(Resource* resource) {
  DCHECK(WTF::isMainThread());
  DCHECK(resource);
  RESOURCE_LOADING_DVLOG(1) << "Evicting resource " << resource << " for "
                            << resource->url().getString() << " from cache";
  TRACE_EVENT1("blink", "MemoryCache::evict", "resource",
               resource->url().getString().utf8());

  ResourceMap* resources = m_resourceMaps.get(resource->cacheIdentifier());
  if (!resources)
    return;

  KURL url = removeFragmentIdentifierIfNeeded(resource->url());
  ResourceMap::iterator it = resources->find(url);
  if (it == resources->end() || it->value->resource() != resource)
    return;
  removeInternal(resources, it);
}

void MemoryCache::removeInternal(ResourceMap* resourceMap,
                                 const ResourceMap::iterator& it) {
  DCHECK(WTF::isMainThread());
  DCHECK(resourceMap);

  Resource* resource = it->value->resource();
  DCHECK(resource);

  update(resource, resource->size(), 0);
  resourceMap->remove(it);
}

bool MemoryCache::contains(const Resource* resource) const {
  if (!resource || resource->url().isEmpty())
    return false;
  const ResourceMap* resources =
      m_resourceMaps.get(resource->cacheIdentifier());
  if (!resources)
    return false;
  KURL url = removeFragmentIdentifierIfNeeded(resource->url());
  MemoryCacheEntry* entry = resources->get(url);
  return entry && resource == entry->resource();
}

Resource* MemoryCache::resourceForURL(const KURL& resourceURL) const {
  return resourceForURL(resourceURL, defaultCacheIdentifier());
}

Resource* MemoryCache::resourceForURL(const KURL& resourceURL,
                                      const String& cacheIdentifier) const {
  DCHECK(WTF::isMainThread());
  if (!resourceURL.isValid() || resourceURL.isNull())
    return nullptr;
  DCHECK(!cacheIdentifier.isNull());
  const ResourceMap* resources = m_resourceMaps.get(cacheIdentifier);
  if (!resources)
    return nullptr;
  MemoryCacheEntry* entry =
      resources->get(removeFragmentIdentifierIfNeeded(resourceURL));
  if (!entry)
    return nullptr;
  return entry->resource();
}

HeapVector<Member<Resource>> MemoryCache::resourcesForURL(
    const KURL& resourceURL) const {
  DCHECK(WTF::isMainThread());
  KURL url = removeFragmentIdentifierIfNeeded(resourceURL);
  HeapVector<Member<Resource>> results;
  for (const auto& resourceMapIter : m_resourceMaps) {
    if (MemoryCacheEntry* entry = resourceMapIter.value->get(url)) {
      Resource* resource = entry->resource();
      DCHECK(resource);
      results.append(resource);
    }
  }
  return results;
}

void MemoryCache::pruneResources(PruneStrategy strategy) {
  DCHECK(!m_prunePending);
  const size_t sizeLimit = (strategy == MaximalPrune) ? 0 : capacity();
  if (m_size <= sizeLimit)
    return;

  // Cut by a percentage to avoid immediately pruning again.
  size_t targetSize = static_cast<size_t>(sizeLimit * cTargetPrunePercentage);

  for (const auto& resourceMapIter : m_resourceMaps) {
    for (const auto& resourceIter : *resourceMapIter.value) {
      Resource* resource = resourceIter.value->resource();
      DCHECK(resource);
      if (resource->isLoaded() && resource->decodedSize()) {
        // Check to see if the remaining resources are too new to prune.
        double elapsedTime =
            m_pruneFrameTimeStamp - resourceIter.value->m_lastDecodedAccessTime;
        if (strategy == AutomaticPrune &&
            elapsedTime < m_delayBeforeLiveDecodedPrune)
          continue;
        resource->prune();
        if (m_size <= targetSize)
          return;
      }
    }
  }
}

void MemoryCache::setCapacity(size_t totalBytes) {
  m_capacity = totalBytes;
  prune();
}

void MemoryCache::update(Resource* resource, size_t oldSize, size_t newSize) {
  if (!contains(resource))
    return;
  ptrdiff_t delta = newSize - oldSize;
  DCHECK(delta >= 0 || m_size >= static_cast<size_t>(-delta));
  m_size += delta;
}

void MemoryCache::removeURLFromCache(const KURL& url) {
  HeapVector<Member<Resource>> resources = resourcesForURL(url);
  for (Resource* resource : resources)
    remove(resource);
}

void MemoryCache::TypeStatistic::addResource(Resource* o) {
  count++;
  size += o->size();
  decodedSize += o->decodedSize();
  encodedSize += o->encodedSize();
  overheadSize += o->overheadSize();
  encodedSizeDuplicatedInDataURLs +=
      o->url().protocolIsData() ? o->encodedSize() : 0;
}

MemoryCache::Statistics MemoryCache::getStatistics() const {
  Statistics stats;
  for (const auto& resourceMapIter : m_resourceMaps) {
    for (const auto& resourceIter : *resourceMapIter.value) {
      Resource* resource = resourceIter.value->resource();
      DCHECK(resource);
      switch (resource->getType()) {
        case Resource::Image:
          stats.images.addResource(resource);
          break;
        case Resource::CSSStyleSheet:
          stats.cssStyleSheets.addResource(resource);
          break;
        case Resource::Script:
          stats.scripts.addResource(resource);
          break;
        case Resource::XSLStyleSheet:
          stats.xslStyleSheets.addResource(resource);
          break;
        case Resource::Font:
          stats.fonts.addResource(resource);
          break;
        default:
          stats.other.addResource(resource);
          break;
      }
    }
  }
  return stats;
}

void MemoryCache::evictResources(EvictResourcePolicy policy) {
  for (auto resourceMapIter = m_resourceMaps.begin();
       resourceMapIter != m_resourceMaps.end();) {
    ResourceMap* resources = resourceMapIter->value.get();
    HeapVector<Member<MemoryCacheEntry>> unusedPreloads;
    for (auto resourceIter = resources->begin();
         resourceIter != resources->end(); resourceIter = resources->begin()) {
      DCHECK(resourceIter.get());
      DCHECK(resourceIter->value.get());
      DCHECK(resourceIter->value->resource());
      Resource* resource = resourceIter->value->resource();
      DCHECK(resource);
      if (policy != EvictAllResources && resource->isUnusedPreload()) {
        // Store unused preloads aside, so they could be added back later.
        // That is in order to avoid the performance impact of iterating over
        // the same resource multiple times.
        unusedPreloads.append(resourceIter->value.get());
      }
      removeInternal(resources, resourceIter);
    }
    for (const auto& unusedPreload : unusedPreloads) {
      addInternal(resources, unusedPreload);
    }
    // We may iterate multiple times over resourceMaps with unused preloads.
    // That's extremely unlikely to have any real-life performance impact.
    if (!resources->size()) {
      m_resourceMaps.remove(resourceMapIter);
      resourceMapIter = m_resourceMaps.begin();
    } else {
      ++resourceMapIter;
    }
  }
}

void MemoryCache::prune() {
  TRACE_EVENT0("renderer", "MemoryCache::prune()");

  if (m_inPruneResources)
    return;
  if (m_size <= m_capacity)  // Fast path.
    return;

  // To avoid burdening the current thread with repetitive pruning jobs, pruning
  // is postponed until the end of the current task. If it has been more than
  // m_maxPruneDeferralDelay since the last prune, then we prune immediately. If
  // the current thread's run loop is not active, then pruning will happen
  // immediately only if it has been over m_maxPruneDeferralDelay since the last
  // prune.
  double currentTime = WTF::currentTime();
  if (m_prunePending) {
    if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
      pruneNow(currentTime, AutomaticPrune);
    }
  } else {
    if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
      pruneNow(currentTime, AutomaticPrune);  // Delay exceeded, prune now.
    } else {
      // Defer.
      Platform::current()->currentThread()->addTaskObserver(this);
      m_prunePending = true;
    }
  }
}

void MemoryCache::willProcessTask() {}

void MemoryCache::didProcessTask() {
  // Perform deferred pruning
  DCHECK(m_prunePending);
  pruneNow(WTF::currentTime(), AutomaticPrune);
}

void MemoryCache::pruneAll() {
  double currentTime = WTF::currentTime();
  pruneNow(currentTime, MaximalPrune);
}

void MemoryCache::pruneNow(double currentTime, PruneStrategy strategy) {
  if (m_prunePending) {
    m_prunePending = false;
    Platform::current()->currentThread()->removeTaskObserver(this);
  }

  AutoReset<bool> reentrancyProtector(&m_inPruneResources, true);

  pruneResources(strategy);
  m_pruneFrameTimeStamp = m_lastFramePaintTimeStamp;
  m_pruneTimeStamp = currentTime;
}

void MemoryCache::updateFramePaintTimestamp() {
  m_lastFramePaintTimeStamp = currentTime();
}

bool MemoryCache::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail,
                               WebProcessMemoryDump* memoryDump) {
  if (levelOfDetail == WebMemoryDumpLevelOfDetail::Background) {
    Statistics stats = getStatistics();
    WebMemoryAllocatorDump* dump1 =
        memoryDump->createMemoryAllocatorDump("web_cache/Image_resources");
    dump1->addScalar("size", "bytes",
                     stats.images.encodedSize + stats.images.overheadSize);
    WebMemoryAllocatorDump* dump2 = memoryDump->createMemoryAllocatorDump(
        "web_cache/CSS stylesheet_resources");
    dump2->addScalar("size", "bytes", stats.cssStyleSheets.encodedSize +
                                          stats.cssStyleSheets.overheadSize);
    WebMemoryAllocatorDump* dump3 =
        memoryDump->createMemoryAllocatorDump("web_cache/Script_resources");
    dump3->addScalar("size", "bytes",
                     stats.scripts.encodedSize + stats.scripts.overheadSize);
    WebMemoryAllocatorDump* dump4 = memoryDump->createMemoryAllocatorDump(
        "web_cache/XSL stylesheet_resources");
    dump4->addScalar("size", "bytes", stats.xslStyleSheets.encodedSize +
                                          stats.xslStyleSheets.overheadSize);
    WebMemoryAllocatorDump* dump5 =
        memoryDump->createMemoryAllocatorDump("web_cache/Font_resources");
    dump5->addScalar("size", "bytes",
                     stats.fonts.encodedSize + stats.fonts.overheadSize);
    WebMemoryAllocatorDump* dump6 =
        memoryDump->createMemoryAllocatorDump("web_cache/Other_resources");
    dump6->addScalar("size", "bytes",
                     stats.other.encodedSize + stats.other.overheadSize);
    return true;
  }

  for (const auto& resourceMapIter : m_resourceMaps) {
    for (const auto& resourceIter : *resourceMapIter.value) {
      Resource* resource = resourceIter.value->resource();
      resource->onMemoryDump(levelOfDetail, memoryDump);
    }
  }
  return true;
}

void MemoryCache::onMemoryPressure(WebMemoryPressureLevel level) {
  pruneAll();
}

}  // namespace blink
