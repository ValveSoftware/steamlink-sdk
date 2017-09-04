/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebCache.h"

#include "core/fetch/MemoryCache.h"

namespace blink {

// A helper method for coverting a MemoryCache::TypeStatistic to a
// WebCache::ResourceTypeStat.
static void ToResourceTypeStat(const MemoryCache::TypeStatistic& from,
                               WebCache::ResourceTypeStat& to) {
  to.count = from.count;
  to.size = from.size;
  // TODO(hiroshige): remove |liveSize| as it is no longer meaningful.
  to.liveSize = from.size;
  to.decodedSize = from.decodedSize;
}

// TODO(hiroshige): remove parameters that are no longer meaningful.
void WebCache::setCapacities(size_t, size_t, size_t capacity) {
  MemoryCache* cache = memoryCache();
  if (cache)
    cache->setCapacity(static_cast<unsigned>(capacity));
}

void WebCache::clear() {
  MemoryCache* cache = memoryCache();
  if (cache)
    cache->evictResources();
}

void WebCache::getUsageStats(UsageStats* result) {
  DCHECK(result);

  MemoryCache* cache = memoryCache();
  if (cache) {
    // TODO(hiroshige): remove members that are no longer meaningful.
    result->minDeadCapacity = 0;
    result->maxDeadCapacity = 0;
    result->capacity = cache->capacity();
    result->liveSize = cache->size();
    result->deadSize = 0;
  } else
    memset(result, 0, sizeof(UsageStats));
}

void WebCache::getResourceTypeStats(ResourceTypeStats* result) {
  MemoryCache* cache = memoryCache();
  if (cache) {
    MemoryCache::Statistics stats = cache->getStatistics();
    ToResourceTypeStat(stats.images, result->images);
    ToResourceTypeStat(stats.cssStyleSheets, result->cssStyleSheets);
    ToResourceTypeStat(stats.scripts, result->scripts);
    ToResourceTypeStat(stats.xslStyleSheets, result->xslStyleSheets);
    ToResourceTypeStat(stats.fonts, result->fonts);
    ToResourceTypeStat(stats.other, result->other);
  } else
    memset(result, 0, sizeof(WebCache::ResourceTypeStats));
}

}  // namespace blink
