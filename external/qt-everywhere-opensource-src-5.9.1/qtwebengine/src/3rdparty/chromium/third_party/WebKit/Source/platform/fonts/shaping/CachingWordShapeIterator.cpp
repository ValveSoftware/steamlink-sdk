// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/CachingWordShapeIterator.h"

#include "platform/fonts/shaping/HarfBuzzShaper.h"

namespace blink {

PassRefPtr<const ShapeResult> CachingWordShapeIterator::shapeWordWithoutSpacing(
    const TextRun& wordRun,
    const Font* font) {
  ShapeCacheEntry* cacheEntry = m_shapeCache->add(wordRun, ShapeCacheEntry());
  if (cacheEntry && cacheEntry->m_shapeResult)
    return cacheEntry->m_shapeResult;

  HarfBuzzShaper shaper(font, wordRun);
  RefPtr<const ShapeResult> shapeResult = shaper.shapeResult();
  if (!shapeResult)
    return nullptr;

  if (cacheEntry)
    cacheEntry->m_shapeResult = shapeResult;

  return shapeResult.release();
}

}  // namespace blink
