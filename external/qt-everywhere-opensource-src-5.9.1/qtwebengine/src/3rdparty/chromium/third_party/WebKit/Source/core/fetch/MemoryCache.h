/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

    This class provides all functionality needed for loading images, style
   sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef MemoryCache_h
#define MemoryCache_h

#include "core/CoreExport.h"
#include "core/fetch/Resource.h"
#include "platform/MemoryCoordinator.h"
#include "platform/tracing/MemoryCacheDumpProvider.h"
#include "public/platform/WebThread.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class KURL;

// Member<MemoryCacheEntry> + MemoryCacheEntry::clearResourceWeak() monitors
// eviction from MemoryCache due to Resource garbage collection.
// WeakMember<Resource> + Resource's prefinalizer cannot determine whether the
// Resource was on MemoryCache or not, because WeakMember is already cleared
// when the prefinalizer is executed.
class MemoryCacheEntry final : public GarbageCollected<MemoryCacheEntry> {
 public:
  static MemoryCacheEntry* create(Resource* resource) {
    return new MemoryCacheEntry(resource);
  }
  DECLARE_TRACE();
  Resource* resource() const { return m_resource; }

  double m_lastDecodedAccessTime;  // Used as a thrash guard

 private:
  explicit MemoryCacheEntry(Resource* resource)
      : m_lastDecodedAccessTime(0.0), m_resource(resource) {}

  void clearResourceWeak(Visitor*);

  WeakMember<Resource> m_resource;
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(MemoryCacheEntry);

// This cache holds subresources used by Web pages: images, scripts,
// stylesheets, etc.
class CORE_EXPORT MemoryCache final
    : public GarbageCollectedFinalized<MemoryCache>,
      public WebThread::TaskObserver,
      public MemoryCacheDumpClient,
      public MemoryCoordinatorClient {
  USING_GARBAGE_COLLECTED_MIXIN(MemoryCache);
  WTF_MAKE_NONCOPYABLE(MemoryCache);

 public:
  static MemoryCache* create();
  ~MemoryCache();
  DECLARE_TRACE();

  struct TypeStatistic {
    STACK_ALLOCATED();
    size_t count;
    size_t size;
    size_t decodedSize;
    size_t encodedSize;
    size_t overheadSize;
    size_t encodedSizeDuplicatedInDataURLs;

    TypeStatistic()
        : count(0),
          size(0),
          decodedSize(0),
          encodedSize(0),
          overheadSize(0),
          encodedSizeDuplicatedInDataURLs(0) {}

    void addResource(Resource*);
  };

  struct Statistics {
    STACK_ALLOCATED();
    TypeStatistic images;
    TypeStatistic cssStyleSheets;
    TypeStatistic scripts;
    TypeStatistic xslStyleSheets;
    TypeStatistic fonts;
    TypeStatistic other;
  };

  Resource* resourceForURL(const KURL&) const;
  Resource* resourceForURL(const KURL&, const String& cacheIdentifier) const;
  HeapVector<Member<Resource>> resourcesForURL(const KURL&) const;

  void add(Resource*);
  void remove(Resource*);
  bool contains(const Resource*) const;

  static KURL removeFragmentIdentifierIfNeeded(const KURL& originalURL);

  static String defaultCacheIdentifier();

  // Sets the cache's memory capacities, in bytes. These will hold only
  // approximately, since the decoded cost of resources like scripts and
  // stylesheets is not known.
  //  - totalBytes: The maximum number of bytes that the cache should consume
  //    overall.
  void setCapacity(size_t totalBytes);
  void setDelayBeforeLiveDecodedPrune(double seconds) {
    m_delayBeforeLiveDecodedPrune = seconds;
  }
  void setMaxPruneDeferralDelay(double seconds) {
    m_maxPruneDeferralDelay = seconds;
  }

  enum EvictResourcePolicy { EvictAllResources, DoNotEvictUnusedPreloads };
  void evictResources(EvictResourcePolicy = EvictAllResources);

  void prune();

  // Called to update MemoryCache::size().
  void update(Resource*, size_t oldSize, size_t newSize);

  void removeURLFromCache(const KURL&);

  Statistics getStatistics() const;

  size_t capacity() const { return m_capacity; }
  size_t size() const { return m_size; }

  // TaskObserver implementation
  void willProcessTask() override;
  void didProcessTask() override;

  void pruneAll();

  void updateFramePaintTimestamp();

  // Take memory usage snapshot for tracing.
  bool onMemoryDump(WebMemoryDumpLevelOfDetail, WebProcessMemoryDump*) override;

  void onMemoryPressure(WebMemoryPressureLevel) override;

 private:
  enum PruneStrategy {
    // Automatically decide how much to prune.
    AutomaticPrune,
    // Maximally prune resources.
    MaximalPrune
  };

  // A URL-based map of all resources that are in the cache (including the
  // freshest version of objects that are currently being referenced by a Web
  // page). removeFragmentIdentifierIfNeeded() should be called for the url
  // before using it as a key for the map.
  using ResourceMap = HeapHashMap<String, Member<MemoryCacheEntry>>;
  using ResourceMapIndex = HeapHashMap<String, Member<ResourceMap>>;
  ResourceMap* ensureResourceMap(const String& cacheIdentifier);
  ResourceMapIndex m_resourceMaps;

  MemoryCache();

  void addInternal(ResourceMap*, MemoryCacheEntry*);
  void removeInternal(ResourceMap*, const ResourceMap::iterator&);

  void pruneResources(PruneStrategy);
  void pruneNow(double currentTime, PruneStrategy);

  bool m_inPruneResources;
  bool m_prunePending;
  double m_maxPruneDeferralDelay;
  double m_pruneTimeStamp;
  double m_pruneFrameTimeStamp;
  double m_lastFramePaintTimeStamp;  // used for detecting decoded resource
                                     // thrash in the cache

  size_t m_capacity;
  double m_delayBeforeLiveDecodedPrune;

  // The number of bytes currently consumed by resources in the cache.
  size_t m_size;

  friend class MemoryCacheTest;
};

// Returns the global cache.
CORE_EXPORT MemoryCache* memoryCache();

// Sets the global cache, used to swap in a test instance. Returns the old
// MemoryCache object.
CORE_EXPORT MemoryCache* replaceMemoryCacheForTesting(MemoryCache*);

}  // namespace blink

#endif
