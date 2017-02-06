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

#include "platform/Logging.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginHash.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/CString.h"

namespace blink {

static Persistent<MemoryCache>* gMemoryCache;

static const unsigned cDefaultCacheCapacity = 8192 * 1024;
static const unsigned cDeferredPruneDeadCapacityFactor = 2;
static const int cMinDelayBeforeLiveDecodedPrune = 1; // Seconds.
static const double cMaxPruneDeferralDelay = 0.5; // Seconds.
static const float cTargetPrunePercentage = .95f; // Percentage of capacity toward which we prune, to avoid immediately pruning again.

MemoryCache* memoryCache()
{
    ASSERT(WTF::isMainThread());
    if (!gMemoryCache)
        gMemoryCache = new Persistent<MemoryCache>(MemoryCache::create());
    return gMemoryCache->get();
}

MemoryCache* replaceMemoryCacheForTesting(MemoryCache* cache)
{
    memoryCache();
    MemoryCache* oldCache = gMemoryCache->release();
    *gMemoryCache = cache;
    MemoryCacheDumpProvider::instance()->setMemoryCache(cache);
    return oldCache;
}

DEFINE_TRACE(MemoryCacheEntry)
{
    visitor->trace(m_resource);
    visitor->trace(m_previousInLiveResourcesList);
    visitor->trace(m_nextInLiveResourcesList);
    visitor->trace(m_previousInAllResourcesList);
    visitor->trace(m_nextInAllResourcesList);
}

void MemoryCacheEntry::dispose()
{
    m_resource.clear();
}

Resource* MemoryCacheEntry::resource()
{
    return m_resource.get();
}

DEFINE_TRACE(MemoryCacheLRUList)
{
    visitor->trace(m_head);
    visitor->trace(m_tail);
}

inline MemoryCache::MemoryCache()
    : m_inPruneResources(false)
    , m_prunePending(false)
    , m_maxPruneDeferralDelay(cMaxPruneDeferralDelay)
    , m_pruneTimeStamp(0.0)
    , m_pruneFrameTimeStamp(0.0)
    , m_lastFramePaintTimeStamp(0.0)
    , m_capacity(cDefaultCacheCapacity)
    , m_minDeadCapacity(0)
    , m_maxDeadCapacity(cDefaultCacheCapacity)
    , m_maxDeferredPruneDeadCapacity(cDeferredPruneDeadCapacityFactor * cDefaultCacheCapacity)
    , m_delayBeforeLiveDecodedPrune(cMinDelayBeforeLiveDecodedPrune)
    , m_liveSize(0)
    , m_deadSize(0)
#ifdef MEMORY_CACHE_STATS
    , m_statsTimer(this, &MemoryCache::dumpStats)
#endif
{
    MemoryCacheDumpProvider::instance()->setMemoryCache(this);
#ifdef MEMORY_CACHE_STATS
    const double statsIntervalInSeconds = 15;
    m_statsTimer.startRepeating(statsIntervalInSeconds, BLINK_FROM_HERE);
#endif
}

MemoryCache* MemoryCache::create()
{
    return new MemoryCache;
}

MemoryCache::~MemoryCache()
{
    if (m_prunePending)
        Platform::current()->currentThread()->removeTaskObserver(this);
}

DEFINE_TRACE(MemoryCache)
{
    visitor->trace(m_allResources);
    visitor->trace(m_liveDecodedResources);
    visitor->trace(m_resourceMaps);
    MemoryCacheDumpClient::trace(visitor);
}

KURL MemoryCache::removeFragmentIdentifierIfNeeded(const KURL& originalURL)
{
    if (!originalURL.hasFragmentIdentifier())
        return originalURL;
    // Strip away fragment identifier from HTTP URLs.
    // Data URLs must be unmodified. For file and custom URLs clients may expect resources
    // to be unique even when they differ by the fragment identifier only.
    if (!originalURL.protocolIsInHTTPFamily())
        return originalURL;
    KURL url = originalURL;
    url.removeFragmentIdentifier();
    return url;
}

String MemoryCache::defaultCacheIdentifier()
{
    return emptyString();
}

MemoryCache::ResourceMap* MemoryCache::ensureResourceMap(const String& cacheIdentifier)
{
    if (!m_resourceMaps.contains(cacheIdentifier)) {
        ResourceMapIndex::AddResult result = m_resourceMaps.add(cacheIdentifier, new ResourceMap);
        RELEASE_ASSERT(result.isNewEntry);
    }
    return m_resourceMaps.get(cacheIdentifier);
}

void MemoryCache::add(Resource* resource)
{
    ASSERT(WTF::isMainThread());
    ASSERT(resource->url().isValid());
    ResourceMap* resources = ensureResourceMap(resource->cacheIdentifier());
    KURL url = removeFragmentIdentifierIfNeeded(resource->url());
    RELEASE_ASSERT(!resources->contains(url));
    resources->set(url, MemoryCacheEntry::create(resource));
    update(resource, 0, resource->size(), true);

    WTF_LOG(ResourceLoading, "MemoryCache::add Added '%s', resource %p\n", resource->url().getString().latin1().data(), resource);
}

void MemoryCache::remove(Resource* resource)
{
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload.
    if (MemoryCacheEntry* entry = getEntryForResource(resource))
        evict(entry);
}

bool MemoryCache::contains(const Resource* resource) const
{
    return getEntryForResource(resource);
}

Resource* MemoryCache::resourceForURL(const KURL& resourceURL)
{
    return resourceForURL(resourceURL, defaultCacheIdentifier());
}

Resource* MemoryCache::resourceForURL(const KURL& resourceURL, const String& cacheIdentifier)
{
    ASSERT(WTF::isMainThread());
    if (!resourceURL.isValid() || resourceURL.isNull())
        return nullptr;
    ASSERT(!cacheIdentifier.isNull());
    ResourceMap* resources = m_resourceMaps.get(cacheIdentifier);
    if (!resources)
        return nullptr;
    KURL url = removeFragmentIdentifierIfNeeded(resourceURL);
    MemoryCacheEntry* entry = resources->get(url);
    if (!entry)
        return nullptr;
    Resource* resource = entry->resource();
    if (resource && !resource->lock())
        return nullptr;
    return resource;
}

HeapVector<Member<Resource>> MemoryCache::resourcesForURL(const KURL& resourceURL)
{
    ASSERT(WTF::isMainThread());
    KURL url = removeFragmentIdentifierIfNeeded(resourceURL);
    HeapVector<Member<Resource>> results;
    for (const auto& resourceMapIter : m_resourceMaps) {
        if (MemoryCacheEntry* entry = resourceMapIter.value->get(url))
            results.append(entry->resource());
    }
    return results;
}

size_t MemoryCache::deadCapacity() const
{
    // Dead resource capacity is whatever space is not occupied by live resources, bounded by an independent minimum and maximum.
    size_t capacity = m_capacity - std::min(m_liveSize, m_capacity); // Start with available capacity.
    capacity = std::max(capacity, m_minDeadCapacity); // Make sure it's above the minimum.
    capacity = std::min(capacity, m_maxDeadCapacity); // Make sure it's below the maximum.
    return capacity;
}

size_t MemoryCache::liveCapacity() const
{
    // Live resource capacity is whatever is left over after calculating dead resource capacity.
    return m_capacity - deadCapacity();
}

void MemoryCache::pruneLiveResources(PruneStrategy strategy)
{
    ASSERT(!m_prunePending);
    size_t capacity = liveCapacity();
    if (strategy == MaximalPrune)
        capacity = 0;
    if (!m_liveSize || (capacity && m_liveSize <= capacity))
        return;

    size_t targetSize = static_cast<size_t>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.

    // Destroy any decoded data in live objects that we can.
    // Start from the tail, since this is the lowest priority
    // and least recently accessed of the objects.

    // The list might not be sorted by the m_lastDecodedFrameTimeStamp. The impact
    // of this weaker invariant is minor as the below if statement to check the
    // elapsedTime will evaluate to false as the current time will be a lot
    // greater than the current->m_lastDecodedFrameTimeStamp.
    // For more details see: https://bugs.webkit.org/show_bug.cgi?id=30209

    MemoryCacheEntry* current = m_liveDecodedResources.m_tail;
    while (current) {
        Resource* resource = current->resource();
        MemoryCacheEntry* previous = current->m_previousInLiveResourcesList;
        ASSERT(resource->hasClientsOrObservers());

        if (resource->isLoaded() && resource->decodedSize()) {
            // Check to see if the remaining resources are too new to prune.
            double elapsedTime = m_pruneFrameTimeStamp - current->m_lastDecodedAccessTime;
            if (strategy == AutomaticPrune && elapsedTime < m_delayBeforeLiveDecodedPrune)
                return;

            // Destroy our decoded data if possible. This will remove us
            // from m_liveDecodedResources, and possibly move us to a
            // different LRU list in m_allResources.
            resource->prune();

            if (targetSize && m_liveSize <= targetSize)
                return;
        }
        current = previous;
    }
}

void MemoryCache::pruneDeadResources(PruneStrategy strategy)
{
    size_t capacity = deadCapacity();
    if (strategy == MaximalPrune)
        capacity = 0;
    if (!m_deadSize || (capacity && m_deadSize <= capacity))
        return;

    size_t targetSize = static_cast<size_t>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.

    int size = m_allResources.size();
    if (targetSize && m_deadSize <= targetSize)
        return;

    bool canShrinkLRULists = true;
    for (int i = size - 1; i >= 0; i--) {
        // Remove from the tail, since this is the least frequently accessed of the objects.
        MemoryCacheEntry* current = m_allResources[i].m_tail;

        // First flush all the decoded data in this queue.
        while (current) {
            Resource* resource = current->resource();
            MemoryCacheEntry* previous = current->m_previousInAllResourcesList;

            // Decoded data may reference other resources. Skip |current| if
            // |current| somehow got kicked out of cache during
            // destroyDecodedData().
            if (!resource || !contains(resource)) {
                current = previous;
                continue;
            }

            if (!resource->hasClientsOrObservers() && !resource->isPreloaded() && resource->isLoaded()) {
                // Destroy our decoded data. This will remove us from
                // m_liveDecodedResources, and possibly move us to a different
                // LRU list in m_allResources.
                resource->prune();

                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            current = previous;
        }

        // Now evict objects from this queue.
        current = m_allResources[i].m_tail;
        while (current) {
            Resource* resource = current->resource();
            MemoryCacheEntry* previous = current->m_previousInAllResourcesList;
            if (!resource || !contains(resource)) {
                current = previous;
                continue;
            }
            if (!resource->hasClientsOrObservers() && !resource->isPreloaded()) {
                evict(current);
                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            current = previous;
        }

        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (m_allResources[i].m_head)
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_allResources.resize(i);
    }
}

void MemoryCache::setCapacities(size_t minDeadBytes, size_t maxDeadBytes, size_t totalBytes)
{
    ASSERT(minDeadBytes <= maxDeadBytes);
    ASSERT(maxDeadBytes <= totalBytes);
    m_minDeadCapacity = minDeadBytes;
    m_maxDeadCapacity = maxDeadBytes;
    m_maxDeferredPruneDeadCapacity = cDeferredPruneDeadCapacityFactor * maxDeadBytes;
    m_capacity = totalBytes;
    prune();
}

void MemoryCache::evict(MemoryCacheEntry* entry)
{
    ASSERT(WTF::isMainThread());

    Resource* resource = entry->resource();
    WTF_LOG(ResourceLoading, "Evicting resource %p for '%s' from cache", resource, resource->url().getString().latin1().data());
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload. See <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
    update(resource, resource->size(), 0, false);
    removeFromLiveDecodedResourcesList(entry);

    ResourceMap* resources = m_resourceMaps.get(resource->cacheIdentifier());
    ASSERT(resources);
    KURL url = removeFragmentIdentifierIfNeeded(resource->url());
    ResourceMap::iterator it = resources->find(url);
    ASSERT(it != resources->end());

    MemoryCacheEntry* entryPtr = it->value;
    resources->remove(it);
    if (entryPtr)
        entryPtr->dispose();
}

MemoryCacheEntry* MemoryCache::getEntryForResource(const Resource* resource) const
{
    if (resource->url().isNull() || resource->url().isEmpty())
        return nullptr;
    ResourceMap* resources = m_resourceMaps.get(resource->cacheIdentifier());
    if (!resources)
        return nullptr;
    KURL url = removeFragmentIdentifierIfNeeded(resource->url());
    MemoryCacheEntry* entry = resources->get(url);
    if (!entry || entry->resource() != resource)
        return nullptr;
    return entry;
}

MemoryCacheLRUList* MemoryCache::lruListFor(unsigned accessCount, size_t size)
{
    ASSERT(accessCount > 0);
    unsigned queueIndex = WTF::fastLog2(size / accessCount);
    if (m_allResources.size() <= queueIndex)
        m_allResources.grow(queueIndex + 1);
    return &m_allResources[queueIndex];
}

void MemoryCache::removeFromLRUList(MemoryCacheEntry* entry, MemoryCacheLRUList* list)
{
    ASSERT(containedInLRUList(entry, list));

    MemoryCacheEntry* next = entry->m_nextInAllResourcesList;
    MemoryCacheEntry* previous = entry->m_previousInAllResourcesList;
    entry->m_nextInAllResourcesList = nullptr;
    entry->m_previousInAllResourcesList = nullptr;

    if (next)
        next->m_previousInAllResourcesList = previous;
    else
        list->m_tail = previous;

    if (previous)
        previous->m_nextInAllResourcesList = next;
    else
        list->m_head = next;

    ASSERT(!containedInLRUList(entry, list));
}

void MemoryCache::insertInLRUList(MemoryCacheEntry* entry, MemoryCacheLRUList* list)
{
    ASSERT(!containedInLRUList(entry, list));

    entry->m_nextInAllResourcesList = list->m_head;
    list->m_head = entry;

    if (entry->m_nextInAllResourcesList)
        entry->m_nextInAllResourcesList->m_previousInAllResourcesList = entry;
    else
        list->m_tail = entry;

    ASSERT(containedInLRUList(entry, list));
}

bool MemoryCache::containedInLRUList(MemoryCacheEntry* entry, MemoryCacheLRUList* list)
{
    for (MemoryCacheEntry* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == entry)
            return true;
    }
    ASSERT(!entry->m_nextInAllResourcesList && !entry->m_previousInAllResourcesList);
    return false;
}

void MemoryCache::removeFromLiveDecodedResourcesList(MemoryCacheEntry* entry)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (!entry->m_inLiveDecodedResourcesList)
        return;
    ASSERT(containedInLiveDecodedResourcesList(entry));

    entry->m_inLiveDecodedResourcesList = false;

    MemoryCacheEntry* next = entry->m_nextInLiveResourcesList;
    MemoryCacheEntry* previous = entry->m_previousInLiveResourcesList;

    entry->m_nextInLiveResourcesList = nullptr;
    entry->m_previousInLiveResourcesList = nullptr;

    if (next)
        next->m_previousInLiveResourcesList = previous;
    else
        m_liveDecodedResources.m_tail = previous;

    if (previous)
        previous->m_nextInLiveResourcesList = next;
    else
        m_liveDecodedResources.m_head = next;

    ASSERT(!containedInLiveDecodedResourcesList(entry));
}

void MemoryCache::insertInLiveDecodedResourcesList(MemoryCacheEntry* entry)
{
    ASSERT(!containedInLiveDecodedResourcesList(entry));

    entry->m_inLiveDecodedResourcesList = true;

    entry->m_nextInLiveResourcesList = m_liveDecodedResources.m_head;
    if (m_liveDecodedResources.m_head)
        m_liveDecodedResources.m_head->m_previousInLiveResourcesList = entry;
    m_liveDecodedResources.m_head = entry;

    if (!entry->m_nextInLiveResourcesList)
        m_liveDecodedResources.m_tail = entry;

    ASSERT(containedInLiveDecodedResourcesList(entry));
}

bool MemoryCache::containedInLiveDecodedResourcesList(MemoryCacheEntry* entry)
{
    for (MemoryCacheEntry* current = m_liveDecodedResources.m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == entry) {
            ASSERT(entry->m_inLiveDecodedResourcesList);
            return true;
        }
    }
    ASSERT(!entry->m_nextInLiveResourcesList && !entry->m_previousInLiveResourcesList && !entry->m_inLiveDecodedResourcesList);
    return false;
}

void MemoryCache::makeLive(Resource* resource)
{
    if (!contains(resource))
        return;
    ASSERT(m_deadSize >= resource->size());
    m_liveSize += resource->size();
    m_deadSize -= resource->size();
}

void MemoryCache::makeDead(Resource* resource)
{
    if (!contains(resource))
        return;
    m_liveSize -= resource->size();
    m_deadSize += resource->size();
    removeFromLiveDecodedResourcesList(getEntryForResource(resource));
}

void MemoryCache::update(Resource* resource, size_t oldSize, size_t newSize, bool wasAccessed)
{
    MemoryCacheEntry* entry = getEntryForResource(resource);
    if (!entry)
        return;

    // The object must now be moved to a different queue, since either its size or its accessCount has been changed,
    // and both of those are used to determine which LRU queue the resource should be in.
    if (oldSize)
        removeFromLRUList(entry, lruListFor(entry->m_accessCount, oldSize));
    if (wasAccessed)
        entry->m_accessCount++;
    if (newSize)
        insertInLRUList(entry, lruListFor(entry->m_accessCount, newSize));

    ptrdiff_t delta = newSize - oldSize;
    if (resource->hasClientsOrObservers()) {
        ASSERT(delta >= 0 || m_liveSize >= static_cast<size_t>(-delta) );
        m_liveSize += delta;
    } else {
        ASSERT(delta >= 0 || m_deadSize >= static_cast<size_t>(-delta) );
        m_deadSize += delta;
    }
}

void MemoryCache::updateDecodedResource(Resource* resource, UpdateReason reason)
{
    MemoryCacheEntry* entry = getEntryForResource(resource);
    if (!entry)
        return;

    removeFromLiveDecodedResourcesList(entry);
    if (resource->decodedSize() && resource->hasClientsOrObservers())
        insertInLiveDecodedResourcesList(entry);

    if (reason != UpdateForAccess)
        return;

    double timestamp = resource->isImage() ? m_lastFramePaintTimeStamp : 0.0;
    if (!timestamp)
        timestamp = currentTime();
    entry->m_lastDecodedAccessTime = timestamp;
}

void MemoryCache::removeURLFromCache(const KURL& url)
{
    HeapVector<Member<Resource>> resources = resourcesForURL(url);
    for (Resource* resource : resources)
        memoryCache()->remove(resource);
}

void MemoryCache::TypeStatistic::addResource(Resource* o)
{
    bool purgeable = o->isPurgeable();
    size_t pageSize = (o->encodedSize() + o->overheadSize() + 4095) & ~4095;
    count++;
    size += o->size();
    liveSize += o->hasClientsOrObservers() ? o->size() : 0;
    decodedSize += o->decodedSize();
    encodedSize += o->encodedSize();
    encodedSizeDuplicatedInDataURLs += o->url().protocolIsData() ? o->encodedSize() : 0;
    purgeableSize += purgeable ? pageSize : 0;
}

MemoryCache::Statistics MemoryCache::getStatistics()
{
    Statistics stats;
    for (const auto& resourceMapIter : m_resourceMaps) {
        for (const auto& resourceIter : *resourceMapIter.value) {
            Resource* resource = resourceIter.value->resource();
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

void MemoryCache::evictResources()
{
    while (true) {
        ResourceMapIndex::iterator resourceMapIter = m_resourceMaps.begin();
        if (resourceMapIter == m_resourceMaps.end())
            break;
        ResourceMap* resources = resourceMapIter->value.get();
        while (true) {
            ResourceMap::iterator resourceIter = resources->begin();
            if (resourceIter == resources->end())
                break;
            evict(resourceIter->value.get());
        }
        m_resourceMaps.remove(resourceMapIter);
    }
}

void MemoryCache::prune(Resource* justReleasedResource)
{
    TRACE_EVENT0("renderer", "MemoryCache::prune()");

    if (m_inPruneResources)
        return;
    if (m_liveSize + m_deadSize <= m_capacity && m_maxDeadCapacity && m_deadSize <= m_maxDeadCapacity) // Fast path.
        return;

    // To avoid burdening the current thread with repetitive pruning jobs,
    // pruning is postponed until the end of the current task. If it has
    // been more than m_maxPruneDeferralDelay since the last prune,
    // then we prune immediately.
    // If the current thread's run loop is not active, then pruning will happen
    // immediately only if it has been over m_maxPruneDeferralDelay
    // since the last prune.
    double currentTime = WTF::currentTime();
    if (m_prunePending) {
        if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
            pruneNow(currentTime, AutomaticPrune);
        }
    } else {
        if (currentTime - m_pruneTimeStamp >= m_maxPruneDeferralDelay) {
            pruneNow(currentTime, AutomaticPrune); // Delay exceeded, prune now.
        } else {
            // Defer.
            Platform::current()->currentThread()->addTaskObserver(this);
            m_prunePending = true;
        }
    }

    if (m_prunePending && m_deadSize > m_maxDeferredPruneDeadCapacity && justReleasedResource) {
        // The following eviction does not respect LRU order, but it can be done
        // immediately in constant time, as opposed to pruneDeadResources, which
        // we would rather defer because it is O(N), which would make tear-down of N
        // objects O(N^2) if we pruned immediately. This immediate eviction is a
        // safeguard against runaway memory consumption by dead resources
        // while a prune is pending.
        if (MemoryCacheEntry* entry = getEntryForResource(justReleasedResource))
            evict(entry);

        // As a last resort, prune immediately
        if (m_deadSize > m_maxDeferredPruneDeadCapacity)
            pruneNow(currentTime, AutomaticPrune);
    }
}

void MemoryCache::willProcessTask()
{
}

void MemoryCache::didProcessTask()
{
    // Perform deferred pruning
    ASSERT(m_prunePending);
    pruneNow(WTF::currentTime(), AutomaticPrune);
}

void MemoryCache::pruneAll()
{
    double currentTime = WTF::currentTime();
    pruneNow(currentTime, MaximalPrune);
}

void MemoryCache::pruneNow(double currentTime, PruneStrategy strategy)
{
    if (m_prunePending) {
        m_prunePending = false;
        Platform::current()->currentThread()->removeTaskObserver(this);
    }

    TemporaryChange<bool> reentrancyProtector(m_inPruneResources, true);
    pruneDeadResources(strategy); // Prune dead first, in case it was "borrowing" capacity from live.
    pruneLiveResources(strategy);
    m_pruneFrameTimeStamp = m_lastFramePaintTimeStamp;
    m_pruneTimeStamp = currentTime;
}

void MemoryCache::updateFramePaintTimestamp()
{
    m_lastFramePaintTimeStamp = currentTime();
}

bool MemoryCache::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail, WebProcessMemoryDump* memoryDump)
{
    for (const auto& resourceMapIter : m_resourceMaps) {
        for (const auto& resourceIter : *resourceMapIter.value) {
            Resource* resource = resourceIter.value->resource();
            resource->onMemoryDump(levelOfDetail, memoryDump);
        }
    }
    return true;
}

bool MemoryCache::isInSameLRUListForTest(const Resource* x, const Resource* y)
{
    MemoryCacheEntry* ex = getEntryForResource(x);
    MemoryCacheEntry* ey = getEntryForResource(y);
    ASSERT(ex);
    ASSERT(ey);
    return lruListFor(ex->m_accessCount, x->size()) == lruListFor(ey->m_accessCount, y->size());
}

#ifdef MEMORY_CACHE_STATS

void MemoryCache::dumpStats(Timer<MemoryCache>*)
{
    Statistics s = getStatistics();
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "", "Count", "Size", "LiveSize", "DecodedSize", "PurgeableSize", "PurgedSize");
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Images", s.images.count, s.images.size, s.images.liveSize, s.images.decodedSize, s.images.purgeableSize, s.images.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "CSS", s.cssStyleSheets.count, s.cssStyleSheets.size, s.cssStyleSheets.liveSize, s.cssStyleSheets.decodedSize, s.cssStyleSheets.purgeableSize, s.cssStyleSheets.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "XSL", s.xslStyleSheets.count, s.xslStyleSheets.size, s.xslStyleSheets.liveSize, s.xslStyleSheets.decodedSize, s.xslStyleSheets.purgeableSize, s.xslStyleSheets.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "JavaScript", s.scripts.count, s.scripts.size, s.scripts.liveSize, s.scripts.decodedSize, s.scripts.purgeableSize, s.scripts.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Fonts", s.fonts.count, s.fonts.size, s.fonts.liveSize, s.fonts.decodedSize, s.fonts.purgeableSize, s.fonts.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Other", s.other.count, s.other.size, s.other.liveSize, s.other.decodedSize, s.other.purgeableSize, s.other.purgedSize);
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");

    printf("Duplication of encoded data from data URLs\n");
    printf("%-13s %13d of %13d\n", "Images",     s.images.encodedSizeDuplicatedInDataURLs,         s.images.encodedSize);
    printf("%-13s %13d of %13d\n", "CSS",        s.cssStyleSheets.encodedSizeDuplicatedInDataURLs, s.cssStyleSheets.encodedSize);
    printf("%-13s %13d of %13d\n", "XSL",        s.xslStyleSheets.encodedSizeDuplicatedInDataURLs, s.xslStyleSheets.encodedSize);
    printf("%-13s %13d of %13d\n", "JavaScript", s.scripts.encodedSizeDuplicatedInDataURLs,        s.scripts.encodedSize);
    printf("%-13s %13d of %13d\n", "Fonts",      s.fonts.encodedSizeDuplicatedInDataURLs,          s.fonts.encodedSize);
    printf("%-13s %13d of %13d\n", "Other",      s.other.encodedSizeDuplicatedInDataURLs,          s.other.encodedSize);
}

void MemoryCache::dumpLRULists(bool includeLive) const
{
    printf("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced, isPurgeable):\n");

    int size = m_allResources.size();
    for (int i = size - 1; i >= 0; i--) {
        printf("\n\nList %d: ", i);
        MemoryCacheEntry* current = m_allResources[i].m_tail;
        while (current) {
            Resource* currentResource = current->resource();
            if (includeLive || !currentResource->hasClientsOrObservers())
                printf("(%.1fK, %.1fK, %uA, %dR, %d); ", currentResource->decodedSize() / 1024.0f, (currentResource->encodedSize() + currentResource->overheadSize()) / 1024.0f, current->m_accessCount, currentResource->hasClientsOrObservers(), currentResource->isPurgeable());

            current = current->m_previousInAllResourcesList;
        }
    }
}

#endif // MEMORY_CACHE_STATS

} // namespace blink
