// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/HitTestCache.h"

#include "platform/Histogram.h"
#include "public/platform/Platform.h"

namespace blink {

bool HitTestCache::lookupCachedResult(HitTestResult& hitResult,
                                      uint64_t domTreeVersion) {
  bool result = false;
  HitHistogramMetric metric = HitHistogramMetric::MISS;
  if (hitResult.hitTestRequest().avoidCache()) {
    metric = HitHistogramMetric::MISS_EXPLICIT_AVOID;
    // For now we don't support rect based hit results.
  } else if (domTreeVersion == m_domTreeVersion &&
             !hitResult.hitTestLocation().isRectBasedTest()) {
    for (const auto& cachedItem : m_items) {
      if (cachedItem.hitTestLocation().point() ==
          hitResult.hitTestLocation().point()) {
        if (hitResult.hitTestRequest().equalForCacheability(
                cachedItem.hitTestRequest())) {
          metric = HitHistogramMetric::HIT_EXACT_MATCH;
          result = true;
          hitResult = cachedItem;
          break;
        }
        metric = HitHistogramMetric::MISS_VALIDITY_RECT_MATCHES;
      }
    }
  }
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, hitTestHistogram,
      ("Event.HitTest",
       static_cast<int32_t>(HitHistogramMetric::MAX_HIT_METRIC)));
  hitTestHistogram.count(static_cast<int32_t>(metric));
  return result;
}

void HitTestCache::addCachedResult(const HitTestResult& result,
                                   uint64_t domTreeVersion) {
  if (!result.isCacheable())
    return;

  // If the result was a hit test on an LayoutPart and the request allowed
  // querying of the layout part; then the part hasn't been loaded yet.
  if (result.isOverWidget() &&
      result.hitTestRequest().allowsChildFrameContent())
    return;

  // For now don't support rect based or list based requests.
  if (result.hitTestLocation().isRectBasedTest() ||
      result.hitTestRequest().listBased())
    return;
  if (domTreeVersion != m_domTreeVersion)
    clear();
  if (m_items.size() < HIT_TEST_CACHE_SIZE)
    m_items.resize(m_updateIndex + 1);

  m_items.at(m_updateIndex).cacheValues(result);
  m_domTreeVersion = domTreeVersion;

  m_updateIndex++;
  if (m_updateIndex >= HIT_TEST_CACHE_SIZE)
    m_updateIndex = 0;
}

void HitTestCache::clear() {
  m_updateIndex = 0;
  m_items.clear();
}

DEFINE_TRACE(HitTestCache) {
  visitor->trace(m_items);
}

}  // namespace blink
