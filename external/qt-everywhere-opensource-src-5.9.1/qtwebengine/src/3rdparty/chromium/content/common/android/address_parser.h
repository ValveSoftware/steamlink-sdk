// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_ADDRESS_PARSER_H_
#define CONTENT_COMMON_ANDROID_ADDRESS_PARSER_H_

#include <stddef.h>

#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// Provides methods to find a geographical address (currently US only)
// in a given text string.
namespace address_parser {

// Find the first address in some chunk of text.  If an address is found in
// |text| true is returned and the address is copied into |address|.
// Otherwise, false is returned.
bool FindAddress(const base::string16& text, base::string16* address);

// Find the first address in some chunk of test.  |begin| is the starting
// position to search from, |end| is the position to search to.  |start_pos|
// and |end_pos| are set to the starting and ending position of the address,
// if found.
CONTENT_EXPORT bool FindAddress(const base::string16::const_iterator& begin,
                                const base::string16::const_iterator& end,
                                size_t* start_pos,
                                size_t* end_pos);

}  // namespace address_parser

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_ADDRESS_PARSER_H_
