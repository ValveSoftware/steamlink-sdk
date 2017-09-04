// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontDataForRangeSet.h"

#include "platform/fonts/FontCache.h"

namespace blink {

FontDataForRangeSet::FontDataForRangeSet(const FontDataForRangeSet& other) {
  m_fontData = other.m_fontData;
  m_rangeSet = other.m_rangeSet;
}

FontDataForRangeSetFromCache::~FontDataForRangeSetFromCache() {
  if (m_fontData && !m_fontData->isCustomFont()) {
    FontCache::fontCache()->releaseFontData(m_fontData.get());
  }
}

}  // namespace blink
