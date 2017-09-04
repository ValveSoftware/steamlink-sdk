// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/address_detector.h"

#include <bitset>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/android/address_parser.h"
#include "content/public/renderer/android_content_detection_prefixes.h"
#include "net/base/escape.h"

namespace {

// Maximum text length to be searched for address detection.
static const size_t kMaxAddressLength = 250;

}  // anonymous namespace

namespace content {

AddressDetector::AddressDetector() {
}

AddressDetector::~AddressDetector() {
}

GURL AddressDetector::GetIntentURL(const std::string& content_text) {
  return GURL(kAddressPrefix +
      net::EscapeQueryParamValue(content_text, true));
}

size_t AddressDetector::GetMaximumContentLength() {
  return kMaxAddressLength;
}

std::string AddressDetector::GetContentText(const base::string16& text) {
  // Get the address and replace unicode bullets with commas.
  base::string16 address_16 = base::CollapseWhitespace(text, false);
  std::replace(address_16.begin(), address_16.end(),
      static_cast<base::char16>(0x2022), static_cast<base::char16>(','));
  return base::UTF16ToUTF8(address_16);
}

bool AddressDetector::FindContent(
    const base::string16::const_iterator& begin,
    const base::string16::const_iterator& end,
    size_t* start_pos,
    size_t* end_pos,
    std::string* content_text) {
  if (address_parser::FindAddress(begin, end, start_pos, end_pos)) {
    content_text->assign(
        GetContentText(base::string16(begin + *start_pos, begin + *end_pos)));
    return true;
  }
  return false;
}

}  // namespace content
