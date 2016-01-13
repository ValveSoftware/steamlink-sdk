// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "encodings/compact_lang_det/win/cld_unicodetext.h"

#include <string>
#include <vector>  // to compile bar/common/component.h

#include "encodings/compact_lang_det/compact_lang_det.h"
#include "encodings/compact_lang_det/string_byte_sink.h"
#include "base/string_util.h"
#include "unicode/normlzr.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"

std::string NormalizeText(const UChar* text) {
  // To avoid a copy, use the read-only aliasing ctor.
  icu::UnicodeString source(1, text, -1);
  icu::UnicodeString normalized;
  UErrorCode status = U_ZERO_ERROR;
  icu::Normalizer::normalize(source, UNORM_NFC, 0, normalized, status);
  if (U_FAILURE(status))
    return std::string();
  normalized.toLower();
  std::string utf8;
  // Internally, toUTF8 uses a 1kB stack buffer (which is not large enough
  // for most web pages) and does pre-flighting followed by malloc for larger
  // strings. We have to switch to obtaining the buffer with the maximum size
  // (UTF-16 length * 3) without pre-flighting if necessary.
  StringByteSink sink(&utf8);
  normalized.toUTF8(sink);
  return utf8;
}


// Detects a language of the UTF-16 encoded zero-terminated text.
// Returns: Language enum.
Language DetectLanguageOfUnicodeText(
    const CompactLangDet::DetectionTables* detection_tables,
    const UChar* text, bool is_plain_text,
    bool* is_reliable, int* num_languages,
    int* error_code, int* text_bytes) {
  if (!text || !num_languages)
    return NUM_LANGUAGES;
  // Normalize text to NFC, lowercase and convert to UTF-8.
  std::string utf8_encoded = NormalizeText(text);
  if (utf8_encoded.empty())
    return NUM_LANGUAGES;

  // Engage core CLD library language detection.
  Language language3[3] = {
    UNKNOWN_LANGUAGE, UNKNOWN_LANGUAGE, UNKNOWN_LANGUAGE
  };
  int percent3[3] = { 0, 0, 0 };
  int text_bytes_tmp = 0;
  // We ignore return value here due to the problem described in bug 1800161.
  // For example, translate.google.com was detected as Indonesian.  It happened
  // due to the heuristic in CLD, which ignores English as a top language
  // in the presence of another reliably detected language.
  // See the actual code in compact_lang_det_impl.cc, CalcSummaryLang function.
  // language3 array is always set according to the detection results and
  // is not affected by this heuristic.
  CompactLangDet::DetectLanguageSummary(detection_tables,
                                        utf8_encoded.c_str(),
                                        utf8_encoded.length(),
                                        is_plain_text, language3, percent3,
                                        &text_bytes_tmp, is_reliable);

  // Calcualte a number of languages detected in more than 20% of the text.
  const int kMinTextPercentToCountLanguage = 20;
  *num_languages = 0;
  if (text_bytes)
    *text_bytes = text_bytes_tmp;
  COMPILE_ASSERT(arraysize(language3) == arraysize(percent3),
                 language3_and_percent3_should_be_of_the_same_size);
  for (int i = 0; i < arraysize(language3); ++i) {
    if (IsValidLanguage(language3[i]) && !IS_LANGUAGE_UNKNOWN(language3[i]) &&
        percent3[i] >= kMinTextPercentToCountLanguage) {
      ++*num_languages;
    }
  }

  return language3[0];
}
