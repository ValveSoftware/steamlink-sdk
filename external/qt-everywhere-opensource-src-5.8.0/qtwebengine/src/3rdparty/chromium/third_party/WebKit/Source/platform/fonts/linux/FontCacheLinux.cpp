/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontCache.h"

#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/SimpleFontData.h"
#include "public/platform/linux/WebFallbackFont.h"
#include "public/platform/linux/WebSandboxSupport.h"
#include "public/platform/Platform.h"
#include "ui/gfx/font_fallback_linux.h"
#include "wtf/text/CString.h"

namespace blink {

FontCache::FontCache()
    : m_purgePreventCount(0)
{
    if (s_fontManager) {
        adopted(s_fontManager);
        m_fontManager = s_fontManager;
    } else {
        m_fontManager = nullptr;
    }
}

void FontCache::getFontForCharacter(UChar32 c, const char* preferredLocale, FontCache::PlatformFallbackFont* fallbackFont)
{
    if (Platform::current()->sandboxSupport()) {
        WebFallbackFont webFallbackFont;
        Platform::current()->sandboxSupport()->getFallbackFontForCharacter(c, preferredLocale, &webFallbackFont);
        fallbackFont->name = String::fromUTF8(CString(webFallbackFont.name));
        fallbackFont->filename = webFallbackFont.filename;
        fallbackFont->fontconfigInterfaceId = webFallbackFont.fontconfigInterfaceId;
        fallbackFont->ttcIndex = webFallbackFont.ttcIndex;
        fallbackFont->isBold = webFallbackFont.isBold;
        fallbackFont->isItalic = webFallbackFont.isItalic;
    } else {
        std::string locale = preferredLocale ? preferredLocale : std::string();
        gfx::FallbackFontData fallbackData = gfx::GetFallbackFontForChar(c, locale);
        fallbackFont->name = String::fromUTF8(fallbackData.name.data(), fallbackData.name.length());
        fallbackFont->filename = CString(fallbackData.filename.data(), fallbackData.filename.length());
        fallbackFont->fontconfigInterfaceId = 0;
        fallbackFont->ttcIndex = fallbackData.ttc_index;
        fallbackFont->isBold = fallbackData.is_bold;
        fallbackFont->isItalic = fallbackData.is_italic;
    }
}

#if !OS(ANDROID)
PassRefPtr<SimpleFontData> FontCache::fallbackFontForCharacter(
    const FontDescription& fontDescription,
    UChar32 c,
    const SimpleFontData*,
    FontFallbackPriority fallbackPriority)
{
    // The m_fontManager is set only if it was provided by the embedder with WebFontRendering::setSkiaFontManager. This is
    // used to emulate android fonts on linux so we always request the family from the font manager and if none is found, we return
    // the LastResort fallback font and avoid using FontCache::getFontForCharacter which would use sandbox support to
    // query the underlying system for the font family.
    if (m_fontManager) {
        AtomicString familyName = getFamilyNameForCharacter(m_fontManager.get(), c, fontDescription, fallbackPriority);
        if (familyName.isEmpty())
            return getLastResortFallbackFont(fontDescription, DoNotRetain);
        return fontDataFromFontPlatformData(getFontPlatformData(fontDescription, FontFaceCreationParams(familyName)), DoNotRetain);
    }

    if (fallbackPriority == FontFallbackPriority::EmojiEmoji) {
        // FIXME crbug.com/591346: We're overriding the fallback character here
        // with the FAMILY emoji in the hope to find a suitable emoji font.
        // This should be improved by supporting fallback for character
        // sequences like DIGIT ONE + COMBINING keycap etc.
        c = familyCharacter;
    }

    // First try the specified font with standard style & weight.
    if (fallbackPriority != FontFallbackPriority::EmojiEmoji
        && (fontDescription.style() == FontStyleItalic
        || fontDescription.weight() >= FontWeight600)) {
        RefPtr<SimpleFontData> fontData = fallbackOnStandardFontStyle(
            fontDescription, c);
        if (fontData)
            return fontData;
    }

    FontCache::PlatformFallbackFont fallbackFont;
    FontCache::getFontForCharacter(c, fontDescription.locale().ascii().data(), &fallbackFont);
    if (fallbackFont.name.isEmpty())
        return nullptr;

    FontFaceCreationParams creationParams;
    creationParams = FontFaceCreationParams(fallbackFont.filename, fallbackFont.fontconfigInterfaceId, fallbackFont.ttcIndex);

    // Changes weight and/or italic of given FontDescription depends on
    // the result of fontconfig so that keeping the correct font mapping
    // of the given character. See http://crbug.com/32109 for details.
    bool shouldSetSyntheticBold = false;
    bool shouldSetSyntheticItalic = false;
    FontDescription description(fontDescription);
    if (fallbackFont.isBold && description.weight() < FontWeightBold)
        description.setWeight(FontWeightBold);
    if (!fallbackFont.isBold && description.weight() >= FontWeightBold) {
        shouldSetSyntheticBold = true;
        description.setWeight(FontWeightNormal);
    }
    if (fallbackFont.isItalic && description.style() == FontStyleNormal)
        description.setStyle(FontStyleItalic);
    if (!fallbackFont.isItalic && (description.style() == FontStyleItalic || description.style() == FontStyleOblique)) {
        shouldSetSyntheticItalic = true;
        description.setStyle(FontStyleNormal);
    }

    FontPlatformData* substitutePlatformData = getFontPlatformData(description, creationParams);
    if (!substitutePlatformData)
        return nullptr;

    std::unique_ptr<FontPlatformData> platformData(new FontPlatformData(*substitutePlatformData));
    platformData->setSyntheticBold(shouldSetSyntheticBold);
    platformData->setSyntheticItalic(shouldSetSyntheticItalic);
    return fontDataFromFontPlatformData(platformData.get(), DoNotRetain);
}

#endif // !OS(ANDROID)

} // namespace blink
