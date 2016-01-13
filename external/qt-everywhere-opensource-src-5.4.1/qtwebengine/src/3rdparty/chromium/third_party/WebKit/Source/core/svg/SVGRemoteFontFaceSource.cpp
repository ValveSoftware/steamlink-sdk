// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#if ENABLE(SVG_FONTS)
#include "core/svg/SVGRemoteFontFaceSource.h"

#include "core/SVGNames.h"
#include "core/css/FontLoader.h"
#include "core/dom/ElementTraversal.h"
#include "core/svg/SVGFontData.h"
#include "core/svg/SVGFontElement.h"
#include "core/svg/SVGFontFaceElement.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"

namespace WebCore {

SVGRemoteFontFaceSource::SVGRemoteFontFaceSource(const String& uri, FontResource* font, PassRefPtrWillBeRawPtr<FontLoader> fontLoader)
    : RemoteFontFaceSource(font, fontLoader)
    , m_uri(uri)
{
}

SVGRemoteFontFaceSource::~SVGRemoteFontFaceSource()
{
}

bool SVGRemoteFontFaceSource::ensureFontData()
{
    return resource()->ensureSVGFontData();
}

PassRefPtr<SimpleFontData> SVGRemoteFontFaceSource::createFontData(const FontDescription& fontDescription)
{
    if (!isLoaded())
        return createLoadingFallbackFontData(fontDescription);

    // Parse the external SVG document, and extract the <font> element.
    if (!resource()->ensureSVGFontData())
        return nullptr;

    if (!m_externalSVGFontElement) {
        String fragmentIdentifier;
        size_t start = m_uri.find('#');
        if (start != kNotFound)
            fragmentIdentifier = m_uri.substring(start + 1);
        m_externalSVGFontElement = resource()->getSVGFontById(fragmentIdentifier);
    }

    if (!m_externalSVGFontElement)
        return nullptr;

    // Select first <font-face> child
    if (SVGFontFaceElement* fontFaceElement = Traversal<SVGFontFaceElement>::firstChild(*m_externalSVGFontElement)) {
        return SimpleFontData::create(
            SVGFontData::create(fontFaceElement),
            fontDescription.effectiveFontSize(),
            fontDescription.isSyntheticBold(),
            fontDescription.isSyntheticItalic());
    }
    return nullptr;
}

void SVGRemoteFontFaceSource::trace(Visitor* visitor)
{
    visitor->trace(m_externalSVGFontElement);
    RemoteFontFaceSource::trace(visitor);
}

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
