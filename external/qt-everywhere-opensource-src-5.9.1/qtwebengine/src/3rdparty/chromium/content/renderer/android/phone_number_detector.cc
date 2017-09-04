// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/phone_number_detector.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/android_content_detection_prefixes.h"
#include "net/base/escape.h"
#include "third_party/libphonenumber/dist/cpp/src/phonenumbers/phonenumbermatch.h"
#include "third_party/libphonenumber/dist/cpp/src/phonenumbers/phonenumbermatcher.h"
#include "third_party/libphonenumber/dist/cpp/src/phonenumbers/region_code.h"
#include "third_party/libphonenumber/phonenumber_api.h"

using i18n::phonenumbers::PhoneNumberMatch;
using i18n::phonenumbers::PhoneNumberMatcher;
using i18n::phonenumbers::PhoneNumberUtil;
using i18n::phonenumbers::RegionCode;

namespace {

// Maximum number of characters to look around for phone number detection.
const size_t kMaximumTelephoneLength = 20;

}  // anonymous namespace

namespace content {

PhoneNumberDetector::PhoneNumberDetector()
    : region_code_(RegionCode::GetUnknown()) {
}

// Region should be empty or an ISO 3166-1 alpha-2 country code.
PhoneNumberDetector::PhoneNumberDetector(const std::string& region)
    : region_code_(region.empty() ? RegionCode::GetUnknown()
                                  : base::ToUpperASCII(region)) {
}

PhoneNumberDetector::~PhoneNumberDetector() {
}

size_t PhoneNumberDetector::GetMaximumContentLength() {
  return kMaximumTelephoneLength;
}

GURL PhoneNumberDetector::GetIntentURL(const std::string& content_text) {
  if (content_text.empty())
    return GURL();

  return GURL(kPhoneNumberPrefix +
      net::EscapeQueryParamValue(content_text, true));
}

bool PhoneNumberDetector::FindContent(
    const base::string16::const_iterator& begin,
    const base::string16::const_iterator& end,
    size_t* start_pos,
    size_t* end_pos,
    std::string* content_text) {
  base::string16 utf16_input = base::string16(begin, end);
  std::string utf8_input = base::UTF16ToUTF8(utf16_input);

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  if (phone_util->IsAlphaNumber(utf8_input))
    phone_util->ConvertAlphaCharactersInNumber(&utf8_input);
  PhoneNumberMatcher matcher(utf8_input, region_code_);
  if (matcher.HasNext()) {
    PhoneNumberMatch match;
    matcher.Next(&match);
    phone_util->FormatNumberForMobileDialing(match.number(), region_code_,
                                             false, /* with_formatting */
                                             content_text);

    // If the number can't be dialed from the current region, the formatted
    // string will be empty.
    if (content_text->empty())
      return false;

    // Need to return start_pos and end_pos relative to a UTF16 encoding.
    *start_pos =
        base::UTF8ToUTF16(utf8_input.substr(0, match.start())).length();
    *end_pos = *start_pos + base::UTF8ToUTF16(match.raw_string()).length();

    return true;
  }

  return false;
}

}  // namespace content
