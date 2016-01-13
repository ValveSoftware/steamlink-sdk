// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_plurals.h"

#include "base/memory/scoped_ptr.h"
#include "ui/base/l10n/l10n_util.h"

namespace l10n_util {

scoped_ptr<icu::PluralRules> BuildPluralRules() {
  UErrorCode err = U_ZERO_ERROR;
  scoped_ptr<icu::PluralRules> rules(
      icu::PluralRules::forLocale(icu::Locale::getDefault(), err));
  if (U_FAILURE(err)) {
    err = U_ZERO_ERROR;
    icu::UnicodeString fallback_rules("one: n is 1", -1, US_INV);
    rules.reset(icu::PluralRules::createRules(fallback_rules, err));
    DCHECK(U_SUCCESS(err));
  }
  return rules.Pass();
}

scoped_ptr<icu::PluralFormat> BuildPluralFormat(
    const std::vector<int>& message_ids) {
  const icu::UnicodeString kKeywords[] = {
      UNICODE_STRING_SIMPLE("other"),
      UNICODE_STRING_SIMPLE("one"),
      UNICODE_STRING_SIMPLE("zero"),
      UNICODE_STRING_SIMPLE("two"),
      UNICODE_STRING_SIMPLE("few"),
      UNICODE_STRING_SIMPLE("many"),
  };
  DCHECK_EQ(message_ids.size(), arraysize(kKeywords));
  UErrorCode err = U_ZERO_ERROR;
  scoped_ptr<icu::PluralRules> rules(BuildPluralRules());

  icu::UnicodeString pattern;
  for (size_t i = 0; i < arraysize(kKeywords); ++i) {
    int msg_id = message_ids[i];
    std::string sub_pattern = GetStringUTF8(msg_id);
    // NA means this keyword is not used in the current locale.
    // Even if a translator translated for this keyword, we do not
    // use it unless it's 'other' (i=0) or it's defined in the rules
    // for the current locale. Special-casing of 'other' will be removed
    // once ICU's isKeyword is fixed to return true for isKeyword('other').
    if (sub_pattern.compare("NA") != 0 &&
        (i == 0 || rules->isKeyword(kKeywords[i]))) {
      pattern += kKeywords[i];
      pattern += UNICODE_STRING_SIMPLE("{");
      pattern += icu::UnicodeString(sub_pattern.c_str(), "UTF-8");
      pattern += UNICODE_STRING_SIMPLE("}");
    }
  }
  scoped_ptr<icu::PluralFormat> format = scoped_ptr<icu::PluralFormat>(
      new icu::PluralFormat(*rules, pattern, err));
  if (!U_SUCCESS(err)) {
    return scoped_ptr<icu::PluralFormat>();
  }
  return format.Pass();
}

}  // namespace l10n_util
