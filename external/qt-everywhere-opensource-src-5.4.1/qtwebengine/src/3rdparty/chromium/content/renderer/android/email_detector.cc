// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/email_detector.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/android_content_detection_prefixes.h"
#include "net/base/escape.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace {

// Maximum length of an email address.
const size_t kMaximumEmailLength = 254;

// Regex to match email addresses.
// This is more specific than RFC 2822 (uncommon special characters are
// disallowed) in order to avoid false positives.
// Delimiters are word boundaries to allow punctuation, quote marks etc. around
// the address.
const char kEmailRegex[] = "\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,6}\\b";

}  // anonymous namespace

namespace content {

EmailDetector::EmailDetector() {
}

size_t EmailDetector::GetMaximumContentLength() {
  return kMaximumEmailLength;
}

GURL EmailDetector::GetIntentURL(const std::string& content_text) {
  if (content_text.empty())
    return GURL();

  return GURL(kEmailPrefix +
      net::EscapeQueryParamValue(content_text, true));
}

bool EmailDetector::FindContent(const base::string16::const_iterator& begin,
                                const base::string16::const_iterator& end,
                                size_t* start_pos,
                                size_t* end_pos,
                                std::string* content_text) {
  base::string16 utf16_input = base::string16(begin, end);
  icu::UnicodeString pattern(kEmailRegex);
  icu::UnicodeString input(utf16_input.data(), utf16_input.length());
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<icu::RegexMatcher> matcher(
      new icu::RegexMatcher(pattern,
                            input,
                            UREGEX_CASE_INSENSITIVE,
                            status));
  if (matcher->find()) {
    *start_pos = matcher->start(status);
    DCHECK(U_SUCCESS(status));
    *end_pos = matcher->end(status);
    DCHECK(U_SUCCESS(status));
    icu::UnicodeString content_ustr(matcher->group(status));
    DCHECK(U_SUCCESS(status));
    base::UTF16ToUTF8(content_ustr.getBuffer(), content_ustr.length(),
        content_text);
    return true;
  }

  return false;
}

}  // namespace content
