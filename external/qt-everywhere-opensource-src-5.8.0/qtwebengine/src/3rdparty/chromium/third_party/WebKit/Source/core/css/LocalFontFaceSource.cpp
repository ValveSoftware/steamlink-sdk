// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/LocalFontFaceSource.h"

#include "platform/Histogram.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"

namespace blink {

bool LocalFontFaceSource::isLocalFontAvailable(const FontDescription& fontDescription)
{
    return FontCache::fontCache()->isPlatformFontAvailable(fontDescription, m_fontName);
}

PassRefPtr<SimpleFontData> LocalFontFaceSource::createFontData(const FontDescription& fontDescription)
{
    // We don't want to check alternate font family names here, so pass true as the checkingAlternateName parameter.
    RefPtr<SimpleFontData> fontData = FontCache::fontCache()->getFontData(fontDescription, m_fontName, true);
    m_histograms.record(fontData.get());
    return fontData.release();
}

void LocalFontFaceSource::LocalFontHistograms::record(bool loadSuccess)
{
    if (m_reported)
        return;
    m_reported = true;
    DEFINE_STATIC_LOCAL(EnumerationHistogram, localFontUsedHistogram, ("WebFont.LocalFontUsed", 2));
    localFontUsedHistogram.count(loadSuccess ? 1 : 0);
}

} // namespace blink
