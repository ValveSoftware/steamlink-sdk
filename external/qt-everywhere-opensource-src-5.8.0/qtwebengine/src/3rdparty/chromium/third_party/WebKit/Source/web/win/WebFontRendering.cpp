// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/win/WebFontRendering.h"

#include "platform/fonts/FontCache.h"

namespace blink {

// static
void WebFontRendering::setSkiaFontManager(SkFontMgr* fontMgr)
{
    WTF::adopted(fontMgr);
    FontCache::setFontManager(RefPtr<SkFontMgr>(fontMgr));
}

// static
void WebFontRendering::setDeviceScaleFactor(float deviceScaleFactor)
{
    FontCache::setDeviceScaleFactor(deviceScaleFactor);
}

// static
void WebFontRendering::addSideloadedFontForTesting(SkTypeface* typeface)
{
    FontCache::addSideloadedFontForTesting(typeface);
}

// static
void WebFontRendering::setMenuFontMetrics(const wchar_t* familyName, int32_t fontHeight)
{
    FontCache::setMenuFontMetrics(familyName, fontHeight);
}

// static
void WebFontRendering::setSmallCaptionFontMetrics(const wchar_t* familyName, int32_t fontHeight)
{
    FontCache::setSmallCaptionFontMetrics(familyName, fontHeight);
}

// static
void WebFontRendering::setStatusFontMetrics(const wchar_t* familyName, int32_t fontHeight)
{
    FontCache::setStatusFontMetrics(familyName, fontHeight);
}

// static
void WebFontRendering::setAntialiasedTextEnabled(bool enabled)
{
    FontCache::setAntialiasedTextEnabled(enabled);
}

// static
void WebFontRendering::setLCDTextEnabled(bool enabled)
{
    FontCache::setLCDTextEnabled(enabled);
}

// static
void WebFontRendering::setUseSkiaFontFallback(bool useSkiaFontFallback)
{
    FontCache::setUseSkiaFontFallback(useSkiaFontFallback);
}

} // namespace blink
