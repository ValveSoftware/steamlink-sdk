// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSColorValue.h"

#include "core/css/CSSValuePool.h"

namespace blink {

CSSColorValue* CSSColorValue::create(RGBA32 color)
{
    // These are the empty and deleted values of the hash table.
    if (color == Color::transparent)
        return cssValuePool().transparentColor();
    if (color == Color::white)
        return cssValuePool().whiteColor();
    // Just because it is common.
    if (color == Color::black)
        return cssValuePool().blackColor();

    CSSValuePool::ColorValueCache::AddResult entry = cssValuePool().getColorCacheEntry(color);
    if (entry.isNewEntry)
        entry.storedValue->value = new CSSColorValue(color);
    return entry.storedValue->value;
}

} // namespace blink
