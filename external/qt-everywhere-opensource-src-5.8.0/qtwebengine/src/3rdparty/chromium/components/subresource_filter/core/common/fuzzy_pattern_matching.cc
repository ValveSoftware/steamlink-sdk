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

void BuildFailureFunctionFuzzy(base::StringPiece subpattern,
                               std::vector<size_t>* failure) {
  BuildFailureFunction<IsEqualOrBothSeparators>(subpattern, failure);
}

bool StartsWithFuzzy(base::StringPiece text, base::StringPiece subpattern) {
  if (subpattern.size() <= text.size())
    return StartsWithFuzzyImpl(text, subpattern);

  return subpattern.size() == text.size() + 1 &&
         subpattern.back() == kSeparatorPlaceholder &&
         StartsWithFuzzyImpl(text, subpattern.substr(0, text.size()));
}

bool EndsWithFuzzy(base::StringPiece text, base::StringPiece subpattern) {
  if (subpattern.size() > text.size() + 1)
    return false;

  if (subpattern.size() <= text.size() &&
      StartsWithFuzzyImpl(text.substr(text.size() - subpattern.size()),
                          subpattern)) {
    return true;
  }

  DCHECK(!subpattern.empty());
  if (subpattern.back() != kSeparatorPlaceholder)
    return false;
  subpattern.remove_suffix(1);
  DCHECK_LE(subpattern.size(), text.size());
  return StartsWithFuzzy(text.substr(text.size() - subpattern.size()),
                         subpattern);
}

}  // namespace subresource_filter
