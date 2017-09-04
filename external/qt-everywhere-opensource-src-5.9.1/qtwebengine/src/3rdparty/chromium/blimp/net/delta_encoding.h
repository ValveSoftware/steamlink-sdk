// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_DELTA_ENCODING_H_
#define BLIMP_NET_DELTA_ENCODING_H_

#include <algorithm>
#include <functional>
#include <utility>

// This file contains utility functions to concisely compute and apply
// delta encodings of arbitrarily-typed sequences of values. Delta-encoded
// numeric values tend to be smaller in magnitude than their source values
// which, when combined with variable integer encoding (a la protobuf), results
// in a smaller wire size.
//
// e.g. DeltaEncode([4, 1, 5]) => [4, -3, 4]
//      DeltaDecode([4, -3, 4]) => [4, 1, 5]
//
// If ordering doesn't matter, then the function SortAndDeltaEncode()
// can be used to compute a delta encoding with the smallest possible values.
//
// e.g. SortAndDeltaEncode([4, 1, 5]) => [1, 3, 1]
//      DeltaDecode([1, 3, 1]) => [1, 4, 5]
//
// Delta encodings can also be computed for structured or non-numeric data
// types as well, so long as functions are provided for differencing and
// reconstituting the data. This can also be applied to delta encode or elide
// across fields of a struct, for example.

namespace blimp {

// Homogeneously-typed signature for functions that compute and apply deltas.
template <typename T>
using BinaryFunction = T (*)(const T&, const T&);

// Uses the subtraction operator to compute the delta across two values.
template <typename T>
inline T default_compute_delta(const T& lhs, const T& rhs) {
  return lhs - rhs;
}

// Uses the addition operator to add a delta value to a base value.
template <typename T>
inline T default_apply_delta(const T& lhs, const T& rhs) {
  return lhs + rhs;
}

// Uses the less-than (<) operator to determine if |lhs| is less-than |rhs|.
template <typename T>
inline bool lessthan(const T& lhs, const T& rhs) {
  return lhs < rhs;
}

// Delta-encodes a iterated sequence of values in-place.
// |begin|: An iterator pointing to the beginning of the sequence.
// |end|: An iterator pointing to the end of the sequence.
// |default_compute_delta_fn|: Optional function which computes the delta
// between
// every element and its next successor.
template <typename I>
void DeltaEncode(I begin,
                 I end,
                 BinaryFunction<typename std::iterator_traits<I>::value_type>
                     default_compute_delta_fn = &default_compute_delta) {
  if (begin == end) {
    return;
  }

  I current = begin;
  typename std::iterator_traits<I>::value_type value = *current;
  ++current;
  for (; current != end; ++current) {
    value = (*default_compute_delta_fn)(*current, value);

    // Put the delta in place and store the actual value into |value|, ready for
    // the next iteration.
    std::swap(*current, value);
  }
}

// Delta-decodes a iterated sequence of values in-place.
// |begin|: An iterator pointing to the beginning of the sequence.
// |end|: An iterator pointing to the end of the sequence.
// |apply_fn|: Optional binary function which combines the value of
// every element with that of its immediate predecessor.
template <typename I>
void DeltaDecode(I begin,
                 I end,
                 BinaryFunction<typename std::iterator_traits<I>::value_type>
                     apply_fn = &default_apply_delta) {
  if (begin == end) {
    return;
  }

  I current = begin;
  typename std::iterator_traits<I>::value_type previous_value = *current;
  ++current;
  for (; current != end; ++current) {
    *current = apply_fn(previous_value, *current);
    previous_value = *current;
  }
}

// Convenience function for sorting and delta-encoding a sequence of values.
// The value type must support std::sort(), either by natively supporting
// inequality comparison or with custom comparison function |comparison_fn|.
template <typename I>
inline void SortAndDeltaEncode(
    I begin,
    I end,
    BinaryFunction<typename std::iterator_traits<I>::value_type>
        default_compute_delta_fn = &default_compute_delta,
    bool (*comparison_fn)(const typename std::iterator_traits<I>::value_type&,
                          const typename std::iterator_traits<I>::value_type&) =
        lessthan<typename std::iterator_traits<I>::value_type>) {
  std::sort(begin, end, comparison_fn);
  DeltaEncode(begin, end, default_compute_delta_fn);
}

}  // namespace blimp

#endif  // BLIMP_NET_DELTA_ENCODING_H_
