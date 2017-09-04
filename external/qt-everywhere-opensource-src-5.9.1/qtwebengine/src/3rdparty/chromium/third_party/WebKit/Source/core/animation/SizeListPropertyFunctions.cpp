// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SizeListPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

static const FillLayer* getFillLayer(CSSPropertyID property,
                                     const ComputedStyle& style) {
  switch (property) {
    case CSSPropertyBackgroundSize:
      return &style.backgroundLayers();
    case CSSPropertyWebkitMaskSize:
      return &style.maskLayers();
    default:
      NOTREACHED();
      return nullptr;
  }
}

static FillLayer* accessFillLayer(CSSPropertyID property,
                                  ComputedStyle& style) {
  switch (property) {
    case CSSPropertyBackgroundSize:
      return &style.accessBackgroundLayers();
    case CSSPropertyWebkitMaskSize:
      return &style.accessMaskLayers();
    default:
      NOTREACHED();
      return nullptr;
  }
}

SizeList SizeListPropertyFunctions::getInitialSizeList(CSSPropertyID property) {
  return getSizeList(property, ComputedStyle::initialStyle());
}

SizeList SizeListPropertyFunctions::getSizeList(CSSPropertyID property,
                                                const ComputedStyle& style) {
  SizeList result;
  for (const FillLayer* fillLayer = getFillLayer(property, style);
       fillLayer && fillLayer->isSizeSet(); fillLayer = fillLayer->next())
    result.append(fillLayer->size());
  return result;
}

void SizeListPropertyFunctions::setSizeList(CSSPropertyID property,
                                            ComputedStyle& style,
                                            const SizeList& sizeList) {
  FillLayer* fillLayer = accessFillLayer(property, style);
  FillLayer* prev = nullptr;
  for (const FillSize& size : sizeList) {
    if (!fillLayer)
      fillLayer = prev->ensureNext();
    fillLayer->setSize(size);
    prev = fillLayer;
    fillLayer = fillLayer->next();
  }
  while (fillLayer) {
    fillLayer->clearSize();
    fillLayer = fillLayer->next();
  }
}

}  // namespace blink
