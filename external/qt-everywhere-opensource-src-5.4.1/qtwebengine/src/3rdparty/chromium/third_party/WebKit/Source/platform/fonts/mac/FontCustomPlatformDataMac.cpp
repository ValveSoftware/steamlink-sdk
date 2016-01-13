/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "platform/fonts/FontCustomPlatformData.h"

#include "platform/SharedBuffer.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/opentype/OpenTypeSanitizer.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "wtf/PassOwnPtr.h"

#include <ApplicationServices/ApplicationServices.h>

namespace WebCore {

FontCustomPlatformData::FontCustomPlatformData(CGFontRef cgFont, PassRefPtr<SkTypeface> typeface)
    : m_cgFont(AdoptCF, cgFont)
    , m_typeface(typeface)
{
}

FontCustomPlatformData::~FontCustomPlatformData()
{
}

FontPlatformData FontCustomPlatformData::fontPlatformData(float size, bool bold, bool italic, FontOrientation orientation, FontWidthVariant widthVariant)
{
    return FontPlatformData(m_cgFont.get(), size, bold, italic, orientation, widthVariant);
}

PassOwnPtr<FontCustomPlatformData> FontCustomPlatformData::create(SharedBuffer* buffer)
{
    ASSERT_ARG(buffer, buffer);

    OpenTypeSanitizer sanitizer(buffer);
    RefPtr<SharedBuffer> transcodeBuffer = sanitizer.sanitize();
    if (!transcodeBuffer)
        return nullptr; // validation failed.
    buffer = transcodeBuffer.get();

    RetainPtr<CFDataRef> bufferData(AdoptCF, CFDataCreate(0, reinterpret_cast<const UInt8*>(buffer->data()), buffer->size()));
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithCFData(bufferData.get()));
    RetainPtr<CGFontRef> cgFontRef(AdoptCF, CGFontCreateWithDataProvider(dataProvider.get()));
    if (!cgFontRef)
        return nullptr;

    // It's unclear whether this is used. It seems like it has the effect of priming the cache.
    // Since we store this anyways, it might be worthwhile just plumbing this to FontMac.cpp in
    // a more obvious way.
    // FIXME: Remove this, add an explicit use, or add a comment explaining why this exists.
    RefPtr<SkMemoryStream> stream = adoptRef(new SkMemoryStream(buffer->getAsSkData().get()));
    RefPtr<SkTypeface> typeface = adoptRef(SkTypeface::CreateFromStream(stream.get()));
    if (!typeface)
        return nullptr;

    return adoptPtr(new FontCustomPlatformData(cgFontRef.leakRef(), typeface.release()));
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype") || OpenTypeSanitizer::supportsFormat(format);
}

}
