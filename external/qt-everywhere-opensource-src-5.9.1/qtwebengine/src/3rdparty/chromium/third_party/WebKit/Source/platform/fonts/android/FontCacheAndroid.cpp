/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontCache.h"

#include "platform/Language.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFaceCreationParams.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontMgr.h"

namespace blink {

static AtomicString defaultFontFamily(SkFontMgr* fontManager) {
  // Pass nullptr to get the default typeface. The default typeface in Android
  // is "sans-serif" if exists, or the first entry in fonts.xml.
  sk_sp<SkTypeface> typeface(
      fontManager->legacyCreateTypeface(nullptr, SkFontStyle()));
  if (typeface) {
    SkString familyName;
    typeface->getFamilyName(&familyName);
    if (familyName.size())
      return toAtomicString(familyName);
  }

  // Some devices do not return the default typeface. There's not much we can
  // do here, use "Arial", the value LayoutTheme uses for CSS system font
  // keywords such as "menu".
  NOTREACHED();
  return "Arial";
}

static AtomicString defaultFontFamily() {
  if (SkFontMgr* fontManager = FontCache::fontCache()->fontManager())
    return defaultFontFamily(fontManager);
  sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
  return defaultFontFamily(fm.get());
}

// static
const AtomicString& FontCache::systemFontFamily() {
  DEFINE_STATIC_LOCAL(AtomicString, systemFontFamily, (defaultFontFamily()));
  return systemFontFamily;
}

// static
void FontCache::setSystemFontFamily(const AtomicString&) {}

PassRefPtr<SimpleFontData> FontCache::fallbackFontForCharacter(
    const FontDescription& fontDescription,
    UChar32 c,
    const SimpleFontData*,
    FontFallbackPriority fallbackPriority) {
  sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
  AtomicString familyName =
      getFamilyNameForCharacter(fm.get(), c, fontDescription, fallbackPriority);
  if (familyName.isEmpty())
    return getLastResortFallbackFont(fontDescription, DoNotRetain);
  return fontDataFromFontPlatformData(
      getFontPlatformData(fontDescription, FontFaceCreationParams(familyName)),
      DoNotRetain);
}

// static
AtomicString FontCache::getGenericFamilyNameForScript(
    const AtomicString& familyName,
    const FontDescription& fontDescription) {
  // If monospace, do not apply CJK hack to find i18n fonts, because
  // i18n fonts are likely not monospace. Monospace is mostly used
  // for code, but when i18n characters appear in monospace, system
  // fallback can still render the characters.
  if (familyName == FontFamilyNames::webkit_monospace)
    return familyName;

  // The CJK hack below should be removed, at latest when we have
  // serif and sans-serif versions of CJK fonts. Until then, limit it
  // to only when the content locale is available. crbug.com/652146
  const LayoutLocale* contentLocale = fontDescription.locale();
  if (!contentLocale)
    return familyName;

  // This is a hack to use the preferred font for CJK scripts.
  // TODO(kojii): This logic disregards either generic family name
  // or locale. We need an API that honors both to find appropriate
  // fonts. crbug.com/642340
  UChar32 examplerChar;
  switch (contentLocale->script()) {
    case USCRIPT_SIMPLIFIED_HAN:
    case USCRIPT_TRADITIONAL_HAN:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
      examplerChar = 0x4E00;  // A common character in Japanese and Chinese.
      break;
    case USCRIPT_HANGUL:
      examplerChar = 0xAC00;
      break;
    default:
      // For other scripts, use the default generic family mapping logic.
      return familyName;
  }

  sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
  return getFamilyNameForCharacter(fm.get(), examplerChar, fontDescription,
                                   FontFallbackPriority::Text);
}

}  // namespace blink
