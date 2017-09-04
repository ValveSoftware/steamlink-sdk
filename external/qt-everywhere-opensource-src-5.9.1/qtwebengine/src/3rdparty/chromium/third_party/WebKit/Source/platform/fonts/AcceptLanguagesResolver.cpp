// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/AcceptLanguagesResolver.h"

#include "platform/LayoutLocale.h"

namespace blink {

static String& currentAcceptLanguages() {
  DEFINE_STATIC_LOCAL(String, s_current, ());
  return s_current;
}

void AcceptLanguagesResolver::acceptLanguagesChanged(
    const String& acceptLanguages) {
  String& currentValue = currentAcceptLanguages();
  if (currentValue == acceptLanguages)
    return;

  currentValue = acceptLanguages;
  LayoutLocale::invalidateLocaleForHan();
}

const LayoutLocale* AcceptLanguagesResolver::localeForHan() {
  return localeForHanFromAcceptLanguages(currentAcceptLanguages());
}

const LayoutLocale* AcceptLanguagesResolver::localeForHanFromAcceptLanguages(
    const String& acceptLanguages) {
  // Use the first acceptLanguages that can disambiguate.
  Vector<String> languages;
  acceptLanguages.split(',', languages);
  for (String token : languages) {
    token = token.stripWhiteSpace();
    const LayoutLocale* locale = LayoutLocale::get(AtomicString(token));
    if (locale->hasScriptForHan())
      return locale;
  }

  return nullptr;
}

}  // namespace blink
