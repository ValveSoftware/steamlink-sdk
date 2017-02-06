// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The separator placeholder '^' symbol is used in subpatterns to match any
// separator character, which is any ASCII symbol except letters, digits, and
// the following: '_', '-', '.', '%'. Note that the separator placeholder
// character '^' is itself a separator, as well as '\0'. In addition, a
// separator placeholder at the end of the pattern can be matched by the end of
// |text|.
//
// We define a fuzzy occurrence as an occurrence of a |subpattern| in |text|
// such that all its non-placeholder characters are equal to the corresponding
// characters of the |text|, whereas each '^' placeholder can correspond to any
// type of separator in |text|.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FUZZY_PATTERN_MATCHING_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FUZZY_PATTERN_MATCHING_H_

#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/knuth_morris_pratt.h"

namespace subresource_filter {

constexpr char kSeparatorPlaceholder = '^';

inline bool IsAscii(char c) {
  return !(c & ~0x7F);
}

inline bool IsAlphaNumericAscii(char c) {
  if (c <= '9')
    return c >= '0';
  c |= 0x20;  // Puts all alphabetics (and only them) into the 'a'-'z' range.
  return c >= 'a' && c <= 'z';
}

// Returns whether |c| is a separator.
inline bool IsSeparator(char c) {
  switch (c) {
    case '_':
    case '-':
    case '.':
    case '%':
      return false;
    case kSeparatorPlaceholder:
      return true;
    default:
      return !IsAlphaNumericAscii(c) && IsAscii(c);
  }
}

class IsEqualOrBothSeparators {
 public:
  bool operator()(char first, char second) const {
    return first == second || (IsSeparator(first) && IsSeparator(second));
  }
};

// Returns whether |text| starts with a fuzzy occurrence of |subpattern|.
bool StartsWithFuzzy(base::StringPiece text, base::StringPiece subpattern);

// Returns whether |text| ends with a fuzzy occurrence of |subpattern|.
bool EndsWithFuzzy(base::StringPiece text, base::StringPiece subpattern);

// Appends elements of the Knuth-Morris-Pratt failure function corresponding to
// |subpattern| at the end of the |failure| vector. All separator characters,
// including the separator placeholder, are considered equivalent. This is
// necessary in order to support fuzzy separator matching while also keeping the
// equality predicate transitive and symmetric which is needed for KMP. However,
// the separators will need to be checked in a second pass for KMP match
// candidates, which is implemented by AllOccurrencesFuzzy.
void BuildFailureFunctionFuzzy(base::StringPiece subpattern,
                               std::vector<size_t>* failure);

// Iterator range that yields all fuzzy occurrences of |subpattern| in |text|.
// The values in the range are the positions just beyond each occurrence.
//
// The |failure| array is the failure function created by
// BuildFailureFunctionFuzzy for the same |subpattern|, and containing at least
// subpattern.size() values.
class AllOccurrencesFuzzy : private AllOccurrences<IsEqualOrBothSeparators> {
 public:
  using Base = AllOccurrences<IsEqualOrBothSeparators>;

  class Iterator : public Base::Iterator {
   public:
    Iterator& operator++() {
      Base::Iterator::operator++();
      AdvanceWhileFalsePositive();
      return *this;
    }

    Iterator operator++(int) {
      Iterator copy(*this);
      operator++();
      return copy;
    }

   private:
    friend class AllOccurrencesFuzzy;

    explicit Iterator(const Base::Iterator& base_iterator)
        : Base::Iterator(base_iterator) {
      AdvanceWhileFalsePositive();
    }

    const AllOccurrencesFuzzy& owner() const {
      return *static_cast<const AllOccurrencesFuzzy*>(owner_);
    }

    // Moves the iterator forward until it either reaches the end, or meets an
    // occurrence exactly matching all non-placeholder separators.
    void AdvanceWhileFalsePositive() {
      while (match_end_ != base::StringPiece::npos && !IsMatch())
        Base::Iterator::operator++();
    }

    // Returns whether the currenct match meets all separators.
    bool IsMatch() const {
      DCHECK_LE(match_end_, owner().text_.size());
      DCHECK_GE(match_end_, owner().pattern_.size());

      // TODO(pkalinnikov): Store a vector of separator positions externally and
      // check if they are equal to the corresponding characters of the |text|.
      return StartsWithFuzzy(
          owner().text_.substr(match_end_ - owner().pattern_.size()),
          owner().pattern_);
    }
  };

  AllOccurrencesFuzzy(base::StringPiece text,
                      base::StringPiece subpattern,
                      const size_t* failure)
      : Base(text, subpattern, failure) {}

  Iterator begin() const { return Iterator(Base::begin()); }
  Iterator end() const { return Iterator(Base::end()); }
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FUZZY_PATTERN_MATCHING_H_
