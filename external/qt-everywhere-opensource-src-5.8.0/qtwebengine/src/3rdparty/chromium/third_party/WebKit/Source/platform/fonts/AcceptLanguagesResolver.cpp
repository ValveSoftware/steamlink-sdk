// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/AcceptLanguagesResolver.h"

#include "platform/Language.h"
#include "platform/text/LocaleToScriptMapping.h"
#include <unicode/locid.h>

namespace blink {

UScriptCode AcceptLanguagesResolver::m_preferredHanScript = USCRIPT_COMMON;
const char* AcceptLanguagesResolver::m_preferredHanSkFontMgrLocale = nullptr;

// SkFontMgr requires script-based locale names, like "zh-Hant" and "zh-Hans",
// instead of "zh-CN" and "zh-TW".
static const char* toSkFontMgrLocaleForHan(UScriptCode script)
{
    switch (script) {
    case USCRIPT_KATAKANA_OR_HIRAGANA:
        return "ja-jp";
    case USCRIPT_HANGUL:
        return "ko-kr";
    case USCRIPT_SIMPLIFIED_HAN:
        return "zh-Hans";
    case USCRIPT_TRADITIONAL_HAN:
        return "zh-Hant";
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

void AcceptLanguagesResolver::acceptLanguagesChanged(
    const String& acceptLanguages)
{
    // Use the UI locale if it can disambiguate the Unified Han.
    // Historically we use ICU on Windows. crbug.com/586520
#if OS(WIN)
    // Since Chrome synchronizes the ICU default locale with its UI locale,
    // this ICU locale tells the current UI locale of Chrome.
    m_preferredHanScript = scriptCodeForHanFromLocale(
        icu::Locale::getDefault().getName(), '_');
#else
    m_preferredHanScript = scriptCodeForHanFromLocale(defaultLanguage());
#endif
    if (m_preferredHanScript != USCRIPT_COMMON) {
        // We don't need additional locale if defaultLanguage() can disambiguate
        // since it's always passed to matchFamilyStyleCharacter() separately.
        m_preferredHanSkFontMgrLocale = nullptr;
        return;
    }

    updateFromAcceptLanguages(acceptLanguages);
}

void AcceptLanguagesResolver::updateFromAcceptLanguages(
    const String& acceptLanguages)
{
    // Use the first acceptLanguages that can disambiguate.
    Vector<String> languages;
    acceptLanguages.split(',', languages);
    for (String token : languages) {
        token = token.stripWhiteSpace();
        m_preferredHanScript = scriptCodeForHanFromLocale(token);
        if (m_preferredHanScript != USCRIPT_COMMON) {
            m_preferredHanSkFontMgrLocale = toSkFontMgrLocaleForHan(
                m_preferredHanScript);
            return;
        }
    }

    m_preferredHanScript = USCRIPT_COMMON;
    m_preferredHanSkFontMgrLocale = nullptr;
}

} // namespace blink
