// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGFontFaceSource.h"

#include "core/svg/SVGFontData.h"
#include "core/svg/SVGFontFaceElement.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"

namespace WebCore {

SVGFontFaceSource::SVGFontFaceSource(SVGFontFaceElement* element)
    : m_svgFontFaceElement(element)
{
}

PassRefPtr<SimpleFontData> SVGFontFaceSource::createFontData(const FontDescription& fontDescription)
{
    return SimpleFontData::create(
        SVGFontData::create(m_svgFontFaceElement.get()),
        fontDescription.effectiveFontSize(),
        fontDescription.isSyntheticBold(),
        fontDescription.isSyntheticItalic());
}

void SVGFontFaceSource::trace(Visitor* visitor)
{
    visitor->trace(m_svgFontFaceElement);
    CSSFontFaceSource::trace(visitor);
}

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
