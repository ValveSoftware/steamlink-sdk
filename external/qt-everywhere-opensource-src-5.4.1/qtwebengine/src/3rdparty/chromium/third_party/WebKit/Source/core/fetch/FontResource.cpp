/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/fetch/FontResource.h"

#include "core/dom/TagCollection.h"
#include "core/fetch/ResourceClientWalker.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "platform/SharedBuffer.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontPlatformData.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

#if ENABLE(SVG_FONTS)
#include "core/SVGNames.h"
#include "core/dom/XMLDocument.h"
#include "core/html/HTMLCollection.h"
#include "core/svg/SVGFontElement.h"
#endif

namespace WebCore {

static const double fontLoadWaitLimitSec = 3.0;

enum FontPackageFormat {
    PackageFormatUnknown,
    PackageFormatSFNT,
    PackageFormatWOFF,
    PackageFormatWOFF2,
    PackageFormatSVG,
    PackageFormatEnumMax
};

static FontPackageFormat packageFormatOf(SharedBuffer* buffer)
{
    if (buffer->size() < 4)
        return PackageFormatUnknown;

    const char* data = buffer->data();
    if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F')
        return PackageFormatWOFF;
    if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2')
        return PackageFormatWOFF2;
    return PackageFormatSFNT;
}

static void recordPackageFormatHistogram(FontPackageFormat format)
{
    blink::Platform::current()->histogramEnumeration("WebFont.PackageFormat", format, PackageFormatEnumMax);
}

FontResource::FontResource(const ResourceRequest& resourceRequest)
    : Resource(resourceRequest, Font)
    , m_loadInitiated(false)
    , m_exceedsFontLoadWaitLimit(false)
    , m_fontLoadWaitLimitTimer(this, &FontResource::fontLoadWaitLimitCallback)
{
}

FontResource::~FontResource()
{
}

void FontResource::load(ResourceFetcher*, const ResourceLoaderOptions& options)
{
    // Don't load the file yet. Wait for an access before triggering the load.
    setLoading(true);
    m_options = options;
}

void FontResource::didAddClient(ResourceClient* c)
{
    ASSERT(c->resourceClientType() == FontResourceClient::expectedType());
    Resource::didAddClient(c);
    if (!isLoading())
        static_cast<FontResourceClient*>(c)->fontLoaded(this);
}

void FontResource::beginLoadIfNeeded(ResourceFetcher* dl)
{
    if (!m_loadInitiated) {
        m_loadInitiated = true;
        Resource::load(dl, m_options);
        m_fontLoadWaitLimitTimer.startOneShot(fontLoadWaitLimitSec, FROM_HERE);

        ResourceClientWalker<FontResourceClient> walker(m_clients);
        while (FontResourceClient* client = walker.next())
            client->didStartFontLoad(this);
    }
}

bool FontResource::ensureCustomFontData()
{
    if (!m_fontData && !errorOccurred() && !isLoading()) {
        if (m_data)
            m_fontData = FontCustomPlatformData::create(m_data.get());

        if (m_fontData) {
            recordPackageFormatHistogram(packageFormatOf(m_data.get()));
        } else {
            setStatus(DecodeError);
            recordPackageFormatHistogram(PackageFormatUnknown);
        }
    }
    return m_fontData;
}

FontPlatformData FontResource::platformDataFromCustomData(float size, bool bold, bool italic, FontOrientation orientation, FontWidthVariant widthVariant)
{
#if ENABLE(SVG_FONTS)
    if (m_externalSVGDocument)
        return FontPlatformData(size, bold, italic);
#endif
    ASSERT(m_fontData);
    return m_fontData->fontPlatformData(size, bold, italic, orientation, widthVariant);
}

#if ENABLE(SVG_FONTS)
bool FontResource::ensureSVGFontData()
{
    if (!m_externalSVGDocument && !errorOccurred() && !isLoading()) {
        if (m_data) {
            m_externalSVGDocument = XMLDocument::createSVG();

            OwnPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("application/xml");
            String svgSource = decoder->decode(m_data->data(), m_data->size());
            svgSource = svgSource + decoder->flush();

            m_externalSVGDocument->setContent(svgSource);

            if (decoder->sawError())
                m_externalSVGDocument = nullptr;
        }
        if (m_externalSVGDocument) {
            recordPackageFormatHistogram(PackageFormatSVG);
        } else {
            setStatus(DecodeError);
            recordPackageFormatHistogram(PackageFormatUnknown);
        }
    }

    return m_externalSVGDocument;
}

SVGFontElement* FontResource::getSVGFontById(const String& fontName) const
{
    RefPtrWillBeRawPtr<TagCollection> collection = m_externalSVGDocument->getElementsByTagNameNS(SVGNames::fontTag.namespaceURI(), SVGNames::fontTag.localName());
    if (!collection)
        return 0;

    unsigned collectionLength = collection->length();
    if (!collectionLength)
        return 0;

#ifndef NDEBUG
    for (unsigned i = 0; i < collectionLength; ++i) {
        ASSERT(collection->item(i));
        ASSERT(isSVGFontElement(collection->item(i)));
    }
#endif

    if (fontName.isEmpty())
        return toSVGFontElement(collection->item(0));

    for (unsigned i = 0; i < collectionLength; ++i) {
        SVGFontElement* element = toSVGFontElement(collection->item(i));
        if (element->getIdAttribute() == fontName)
            return element;
    }

    return 0;
}
#endif

bool FontResource::isSafeToUnlock() const
{
    return m_data->hasOneRef();
}

void FontResource::fontLoadWaitLimitCallback(Timer<FontResource>*)
{
    if (!isLoading())
        return;
    m_exceedsFontLoadWaitLimit = true;
    ResourceClientWalker<FontResourceClient> walker(m_clients);
    while (FontResourceClient* client = walker.next())
        client->fontLoadWaitLimitExceeded(this);
}

void FontResource::allClientsRemoved()
{
    m_fontData.clear();
    Resource::allClientsRemoved();
}

void FontResource::checkNotify()
{
    m_fontLoadWaitLimitTimer.stop();
    ResourceClientWalker<FontResourceClient> w(m_clients);
    while (FontResourceClient* c = w.next())
        c->fontLoaded(this);
}

}
