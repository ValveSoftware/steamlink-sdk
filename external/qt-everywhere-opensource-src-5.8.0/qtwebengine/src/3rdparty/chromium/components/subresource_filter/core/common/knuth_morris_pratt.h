// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the "failure function" notion used by the
// Knuth-Morris-Pratt algorithm for solving the string matching problem, i.e.
// finding all occurences of some pattern |P| in arbitrary text. It also
// provides tools for traversing these occurences.
//
// Consider a character string |P| of length |n|. The failure function of this
// string is an integer array |F| of length |n|, defined in the following way:
//   F[i] = max{0 <= j <= i : P[0..j-1] == P[i-j+1..i]},
// i.e. F[i] is the length of the longest proper suffix of P[0..i] that is also
// a prefix of |P|.
//
// For example, the "abacaba" string has the following failure function:
//   [0, 0, 1, 0, 1, 2, 3].
// For further examples see the "knuth_morris_pratt_unittest.cc" file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_KNUTH_MORRIS_PRATT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_KNUTH_MORRIS_PRATT_H_

#include <stddef.h>

#include <functional>
#include <iterator>
#include <vector>

#include "base/strings/string_piece.h"

namespace subresource_filter {

// Appends elements of the Knuth-Morris-Pratt failure function corresponding to
// |pattern| at the end of the |failure| vector. The EquivalentTo should be a
// bool(char, char) functor returning true iff the two characters are
// equivalent, and is required to be reflexive, symmetric and transitive.
template <typename EquivalentTo = std::equal_to<char>>
void BuildFailureFunction(base::StringPiece pattern,
                          std::vector<size_t>* failure,
                          EquivalentTo equivalent_to = EquivalentTo()) {
  if (pattern.empty())
    return;

  const size_t base_index = failure->size();
  failure->push_back(0);
  for (size_t i = 1; i != pattern.size(); ++i) {
    const char c = pattern[i];

    size_t prefix_match_size = failure->back();
    DCHECK_LT(prefix_match_size, i);
    while (prefix_match_size && !equivalent_to(pattern[prefix_match_size], c))
      prefix_match_size = (*failure)[base_index + prefix_match_size - 1];

    if (prefix_match_size || equivalent_to(pattern[0], c))
      ++prefix_match_size;
    failure->push_back(prefix_match_size);
  }
}

// Returns the position of the leftmost occurrence of the |pattern| in the
// |text|, i.e. the index of the character coming *after* the occurrence, within
// the [0, text.size()] range. If the pattern does not occur in the |text|, the
// returned value is base::StringPiece::npos.
//
// The method supports the use-case when the |text| is assumed to be the
// continuation of some imaginary string matching the initial
// |prefix_match_size| characters of the |pattern|. Hence, the |pattern| can be
// partially matched by this imaginary string and partially by the actual prefix
// of the |text|.
//
// The |failure| array is the failure function created by BuildFailureFunction
// with the same EquivalentTo and |pattern|, and containing at least
// pattern.size() values.
template <typename EquivalentTo = std::equal_to<char>>
size_t FindOccurrence(base::StringPiece text,
                      base::StringPiece pattern,
                      const size_t* failure,
                      size_t prefix_match_size,
                      EquivalentTo equivalent_to = EquivalentTo()) {
  DCHECK_LE(prefix_match_size, pattern.size());
  if (prefix_match_size == pattern.size())
    return 0;

  for (size_t i = 0; i != text.size(); ++i) {
    const char c = text[i];
    while (prefix_match_size && !equivalent_to(pattern[prefix_match_size], c))
      prefix_match_size = failure[prefix_match_size - 1];
    if (prefix_match_size || equivalent_to(pattern[0], c))
      ++prefix_match_size;
    if (prefix_match_size == pattern.size())
      return i + 1;
  }
  return base::StringPiece::npos;
}

// Same as FindOccurrence, but starts the search from scratch, i.e. with
// prefix_match_size == 0.
template <typename EquivalentTo = std::equal_to<char>>
size_t FindFirstOccurrence(base::StringPiece text,
                           base::StringPiece pattern,
                           const size_t* failure,
                           EquivalentTo equivalent_to = EquivalentTo()) {
  return FindOccurrence(text, pattern, failure, 0, equivalent_to);
}

// Same as FindOccurrence, but preforms the search assuming the |text| starts
// right after a full match of the |pattern|. The returned value is either
// base::StringPiece::npos if no more occurrences found, or is within the range
// [1, text.size()] if there are occurrences apart from the virtual prefix.
template <typename EquivalentTo = std::equal_to<char>>
size_t FindNextOccurrence(base::StringPiece text,
                          base::StringPiece pattern,
                          const size_t* failure,
                          EquivalentTo equivalent_to = EquivalentTo()) {
  if (pattern.empty())
    return text.empty() ? base::StringPiece::npos : 1;

  const size_t prefix_match_size = failure[pattern.size() - 1];
  return FindOccurrence(text, pattern, failure, prefix_match_size,
                        equivalent_to);
}

// AllOccurrences can be used to find all occurrences of a |pattern| in a |text|
// with respect to EquivalentTo relation between characters, and iterate over
// them using an input iterator. The values pointed to by an iterator indicate
// positions of occurrences, i.e. indices of |text| characters coming *after*
// the occurrences.
template <typename EquivalentTo = std::equal_to<char>>
class AllOccurrences {
 public:
  class Iterator : public std::iterator<std::input_iterator_tag, size_t> {
   public:
    bool operator==(const Iterator& rhs) const {
      return match_end_ == rhs.match_end_;
    }

    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

    size_t operator*() const { return match_end_; }
    const size_t* operator->() const { return &match_end_; }

    Iterator& operator++() {
      DCHECK(match_end_ == base::StringPiece::npos ||
             match_end_ <= owner_->text_.size());

      const size_t relative_match_end = owner_->FindNextOccurrence(match_end_);
      if (relative_match_end == base::StringPiece::npos)
        match_end_ = base::StringPiece::npos;
      else
        match_end_ += relative_match_end;

      return *this;
    }

    Iterator operator++(int) {
      Iterator copy(*this);
      operator++();
      return copy;
    }

   protected:
    friend class AllOccurrences;

    // Creates an iterator, which points to the occurrence ending just before
    // the position denoted by |match_end|. If |match_end| ==
    // base::StringPiece::npos, the iterator is assumed to be at the end.
    Iterator(const AllOccurrences* owner, size_t match_end)
        : owner_(owner), match_end_(match_end) {}

    const AllOccurrences* owner_;
    size_t match_end_;
  };

  AllOccurrences(base::StringPiece text,
                 base::StringPiece pattern,
                 const size_t* failure,
                 EquivalentTo equivalent_to = EquivalentTo())
      : text_(text),
        pattern_(pattern),
        failure_(failure),
        equivalent_to_(equivalent_to) {}

  Iterator begin() const {
    const size_t match_end =
        FindFirstOccurrence(text_, pattern_, failure_, equivalent_to_);
    return Iterator(this, match_end);
  }

  Iterator end() const { return Iterator(this, base::StringPiece::npos); }

 protected:
  // Returns the position of the next occurrence assuming the current occurrence
  // ends right before the |match_end| position.
  size_t FindNextOccurrence(size_t match_end) const {
    return subresource_filter::FindNextOccurrence(
        text_.substr(match_end), pattern_, failure_, equivalent_to_);
  }

  base::StringPiece text_;
  base::StringPiece pattern_;
  const size_t* failure_;

  EquivalentTo equivalent_to_;
};

// A helper function used to create an AllOccurrences instance without knowing
// the direct type of the |equivalent_to| functor.
//
// Typical usage:
//   auto occurrences = CreateAllOccurrences(
//       "word, word: word-word",
//       "word?",
//       <failure function for "word?">,
//       [](char c1, char c2) {
//         return c1 == c2 || (ispunct(c1) && ispunct(c2));
//       });
//
//   for (size_t match_end : occurrences) {
//     // The |pattern| matches the following substring of the |text|:
//     //   text[|match_end|-pattern.size()..|match_end|-1]
//     ... process the |match_end| ...
//   }
template <typename EquivalentTo = std::equal_to<char>>
AllOccurrences<EquivalentTo> CreateAllOccurrences(
    base::StringPiece text,
    base::StringPiece pattern,
    const size_t* failure,
    EquivalentTo equivalent_to = EquivalentTo()) {
  return AllOccurrences<EquivalentTo>(text, pattern, failure, equivalent_to);
}

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_KNUTH_MORRIS_PRATT_H_
