// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/fuzzy_pattern_matching.h"

namespace subresource_filter {

namespace {

bool StartsWithFuzzyImpl(base::StringPiece text, base::StringPiece subpattern) {
  DCHECK_LE(subpattern.size(), text.size());

  for (size_t i = 0; i != subpattern.size(); ++i) {
    const char text_char = text[i];
    const char pattern_char = subpattern[i];
    if (text_char != pattern_char &&
        (pattern_char != kSeparatorPlaceholder || !IsSeparator(text_char))) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool StartsWithFuzzy(base::StringPiece text, base::StringPiece subpattern) {
  return subpattern.size() <= text.size() &&
         StartsWithFuzzyImpl(text, subpattern);
}

bool EndsWithFuzzy(base::StringPiece text, base::StringPiece subpattern) {
  return subpattern.size() <= text.size() &&
         StartsWithFuzzyImpl(text.substr(text.size() - subpattern.size()),
                             subpattern);
}

}  // namespace subresource_filter
