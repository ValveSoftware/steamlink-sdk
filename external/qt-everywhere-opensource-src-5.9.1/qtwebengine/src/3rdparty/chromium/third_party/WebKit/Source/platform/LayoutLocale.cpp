// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/LayoutLocale.h"

#include "platform/Language.h"
#include "platform/fonts/AcceptLanguagesResolver.h"
#include "platform/text/LocaleToScriptMapping.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

#include <hb.h>
#include <unicode/locid.h>

namespace blink {

const LayoutLocale* LayoutLocale::s_default = nullptr;
const LayoutLocale* LayoutLocale::s_system = nullptr;
const LayoutLocale* LayoutLocale::s_defaultForHan = nullptr;
bool LayoutLocale::s_defaultForHanComputed = false;

static hb_language_t toHarfbuzLanguage(const AtomicString& locale) {
  CString localeAsLatin1 = locale.latin1();
  return hb_language_from_string(localeAsLatin1.data(),
                                 localeAsLatin1.length());
}

// SkFontMgr requires script-based locale names, like "zh-Hant" and "zh-Hans",
// instead of "zh-CN" and "zh-TW".
static const char* toSkFontMgrLocale(UScriptCode script) {
  switch (script) {
    case USCRIPT_KATAKANA_OR_HIRAGANA:
      return "ja-JP";
    case USCRIPT_HANGUL:
      return "ko-KR";
    case USCRIPT_SIMPLIFIED_HAN:
      return "zh-Hans";
    case USCRIPT_TRADITIONAL_HAN:
      return "zh-Hant";
    default:
      return nullptr;
  }
}

const char* LayoutLocale::localeForSkFontMgr() const {
  if (m_stringForSkFontMgr.isNull()) {
    m_stringForSkFontMgr = toSkFontMgrLocale(m_script);
    if (m_stringForSkFontMgr.isNull())
      m_stringForSkFontMgr = m_string.ascii();
  }
  return m_stringForSkFontMgr.data();
}

void LayoutLocale::computeScriptForHan() const {
  if (isUnambiguousHanScript(m_script)) {
    m_scriptForHan = m_script;
    m_hasScriptForHan = true;
    return;
  }

  m_scriptForHan = scriptCodeForHanFromSubtags(m_string);
  if (m_scriptForHan == USCRIPT_COMMON)
    m_scriptForHan = USCRIPT_SIMPLIFIED_HAN;
  else
    m_hasScriptForHan = true;
  DCHECK(isUnambiguousHanScript(m_scriptForHan));
}

UScriptCode LayoutLocale::scriptForHan() const {
  if (m_scriptForHan == USCRIPT_COMMON)
    computeScriptForHan();
  return m_scriptForHan;
}

bool LayoutLocale::hasScriptForHan() const {
  if (m_scriptForHan == USCRIPT_COMMON)
    computeScriptForHan();
  return m_hasScriptForHan;
}

const LayoutLocale* LayoutLocale::localeForHan(
    const LayoutLocale* contentLocale) {
  if (contentLocale && contentLocale->hasScriptForHan())
    return contentLocale;
  if (!s_defaultForHanComputed)
    computeLocaleForHan();
  return s_defaultForHan;
}

void LayoutLocale::computeLocaleForHan() {
  if (const LayoutLocale* locale = AcceptLanguagesResolver::localeForHan())
    s_defaultForHan = locale;
  else if (getDefault().hasScriptForHan())
    s_defaultForHan = &getDefault();
  else if (getSystem().hasScriptForHan())
    s_defaultForHan = &getSystem();
  else
    s_defaultForHan = nullptr;
  s_defaultForHanComputed = true;
}

const char* LayoutLocale::localeForHanForSkFontMgr() const {
  const char* locale = toSkFontMgrLocale(scriptForHan());
  DCHECK(locale);
  return locale;
}

LayoutLocale::LayoutLocale(const AtomicString& locale)
    : m_string(locale),
      m_harfbuzzLanguage(toHarfbuzLanguage(locale)),
      m_script(localeToScriptCodeForFontSelection(locale)),
      m_scriptForHan(USCRIPT_COMMON),
      m_hasScriptForHan(false),
      m_hyphenationComputed(false) {}

using LayoutLocaleMap =
    HashMap<AtomicString, RefPtr<LayoutLocale>, CaseFoldingHash>;

static LayoutLocaleMap& getLocaleMap() {
  DEFINE_STATIC_LOCAL(LayoutLocaleMap, localeMap, ());
  return localeMap;
}

const LayoutLocale* LayoutLocale::get(const AtomicString& locale) {
  if (locale.isNull())
    return nullptr;

  auto result = getLocaleMap().add(locale, nullptr);
  if (result.isNewEntry)
    result.storedValue->value = adoptRef(new LayoutLocale(locale));
  return result.storedValue->value.get();
}

const LayoutLocale& LayoutLocale::getDefault() {
  if (s_default)
    return *s_default;

  AtomicString locale = defaultLanguage();
  s_default = get(!locale.isEmpty() ? locale : "en");
  return *s_default;
}

const LayoutLocale& LayoutLocale::getSystem() {
  if (s_system)
    return *s_system;

  // Platforms such as Windows can give more information than the default
  // locale, such as "en-JP" for English speakers in Japan.
  String name = icu::Locale::getDefault().getName();
  s_system = get(AtomicString(name.replace('_', '-')));
  return *s_system;
}

PassRefPtr<LayoutLocale> LayoutLocale::createForTesting(
    const AtomicString& locale) {
  return adoptRef(new LayoutLocale(locale));
}

void LayoutLocale::clearForTesting() {
  s_default = nullptr;
  s_system = nullptr;
  s_defaultForHan = nullptr;
  s_defaultForHanComputed = false;
  getLocaleMap().clear();
}

Hyphenation* LayoutLocale::getHyphenation() const {
  if (m_hyphenationComputed)
    return m_hyphenation.get();

  m_hyphenationComputed = true;
  m_hyphenation = Hyphenation::platformGetHyphenation(localeString());
  return m_hyphenation.get();
}

void LayoutLocale::setHyphenationForTesting(
    const AtomicString& localeString,
    PassRefPtr<Hyphenation> hyphenation) {
  const LayoutLocale& locale = valueOrDefault(get(localeString));
  locale.m_hyphenationComputed = true;
  locale.m_hyphenation = hyphenation;
}

}  // namespace blink
