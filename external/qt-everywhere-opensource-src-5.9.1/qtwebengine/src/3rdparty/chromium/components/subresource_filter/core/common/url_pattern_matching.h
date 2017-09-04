// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The matching logic distinguishes between the terms URL pattern and
// subpattern. A URL pattern usually stands for the full thing, e.g.
// "example.com^*path*par=val^", whereas subpattern denotes a maximal substring
// of a pattern not containing the wildcard '*' character. For the example above
// the subpatterns are: "example.com^", "path" and "par=val^".
//
// The separator placeholder '^' symbol is used in subpatterns to match any
// separator character, which is any ASCII symbol except letters, digits, and
// the following: '_', '-', '.', '%'. Note that the separator placeholder
// character '^' is itself a separator, as well as '\0'.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_MATCHING_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_MATCHING_H_

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/fuzzy_pattern_matching.h"
#include "components/subresource_filter/core/common/knuth_morris_pratt.h"
#include "components/subresource_filter/core/common/string_splitter.h"
#include "components/subresource_filter/core/common/url_pattern.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace subresource_filter {

// Public interface declaration ------------------------------------------------

// Builds a compound Knuth-Morris-Pratt failure function used to match URLs
// against the |pattern|.
//
// The |pattern| is split on the '*' wildcard symbol and then a failure function
// is built for each subpattern by BuildFailureFunctionFuzzy or
// BuildFailureFunction (depending on whether the subpattern contains separator
// placeholders), and appended to the returned vector. Some of the subpatterns
// can be exempted from being indexed. E.g., if the |pattern| has a BOUNDARY
// left anchor, the first subpattern can be matched by checking if it's a prefix
// of a URL.
//
// Each subpattern indexed with BuildFailureFunctionFuzzy is prepended with a
// value 1 (to distinguish them from the subpatterns indexed with
// BuildFailureFunction, their failure functions always start with 0).
//
// The URL |pattern| must be normalized. Namely, it must not have the following
// substrings: **, *<END>, <BEGIN>*. If the the |pattern| has a BOUNDARY anchor,
// the corresponding side of its string must not end with a '*' wildcard.
template <typename IntType>
void BuildFailureFunction(const UrlPattern& pattern,
                          std::vector<IntType>* failure);

// Returns whether the |url| matches the URL |pattern|. The |failure| function
// between |failure_begin| and |failure_end| must be the output of
// BuildFailureFunction() called with the same |pattern|.
//
// TODO(pkalinnikov): Outline algorithms implemented in this function.
template <typename FailureIter>
bool IsUrlPatternMatch(const GURL& url,
                       const UrlPattern& pattern,
                       FailureIter failure_begin,
                       FailureIter failure_end);

// Implementation --------------------------------------------------------------

namespace impl {

class IsWildcard {
 public:
  bool operator()(char c) const { return c == '*'; }
};

// Returns whether |position| within the |url| belongs to its |host| component
// and corresponds to the beginning of a (sub-)domain.
inline bool IsSubdomainAnchored(base::StringPiece url,
                                url::Component host,
                                size_t position) {
  DCHECK_LE(position, url.size());
  const size_t host_begin = static_cast<size_t>(host.begin);
  const size_t host_end = static_cast<size_t>(host.end());
  DCHECK_LE(host_end, url.size());

  return position == host_begin ||
         (position > host_begin && position <= host_end &&
          url[position - 1] == '.');
}

// Returns the position just beyond the leftmost fuzzy occurrence of
// |subpattern| in the |text|.
template <typename IntType>
inline size_t FindFirstOccurrenceFuzzy(base::StringPiece text,
                                       base::StringPiece subpattern,
                                       const IntType* failure) {
  return *AllOccurrencesFuzzy<IntType>(text, subpattern, failure).begin();
}

// Returns the position just beyond the leftmost occurrence of |subpattern| in
// the |url|, such that it satisfies a SUBDOMAIN anchor.
template <typename IntType>
inline size_t FindSubdomainAnchored(base::StringPiece url,
                                    url::Component host,
                                    base::StringPiece subpattern,
                                    const IntType* failure) {
  auto occurrences = AllOccurrences<IntType>(url, subpattern, failure);
  return *std::find_if(occurrences.begin(), occurrences.end(),
                       [url, host, subpattern](size_t match_end_position) {
                         DCHECK_GE(match_end_position, subpattern.size());
                         return IsSubdomainAnchored(
                             url, host, match_end_position - subpattern.size());
                       });
}

// Returns the position just beyond the leftmost fuzzy occurrence of
// |subpattern| in the |url|, such that it satisfies a SUBDOMAIN anchor.
template <typename IntType>
inline size_t FindSubdomainAnchoredFuzzy(base::StringPiece url,
                                         url::Component host,
                                         base::StringPiece subpattern,
                                         const IntType* failure) {
  auto occurrences = AllOccurrencesFuzzy<IntType>(url, subpattern, failure);
  return *std::find_if(occurrences.begin(), occurrences.end(),
                       [url, host, subpattern](size_t match_end_position) {
                         DCHECK_GE(match_end_position, subpattern.size());
                         return IsSubdomainAnchored(
                             url, host, match_end_position - subpattern.size());
                       });
}

}  // namespace impl

template <typename IntType>
void BuildFailureFunction(const UrlPattern& pattern,
                          std::vector<IntType>* failure) {
  StringSplitter<impl::IsWildcard> subpatterns(pattern.url_pattern);
  auto subpattern_it = subpatterns.begin();
  auto subpattern_end = subpatterns.end();

  if (subpattern_it == subpattern_end)
    return;
  if (pattern.anchor_left == proto::ANCHOR_TYPE_BOUNDARY)
    ++subpattern_it;

  while (subpattern_it != subpattern_end) {
    const base::StringPiece part = *subpattern_it++;
    DCHECK(!part.empty());
    if (pattern.anchor_right == proto::ANCHOR_TYPE_BOUNDARY &&
        subpattern_it == subpattern_end) {
      break;
    }

    if (part.find(kSeparatorPlaceholder) == base::StringPiece::npos) {
      BuildFailureFunction(part, failure);
    } else {
      // Prepend the value '1' to the failure function to let matchers
      // distinguish between the subpatterns with separator placeholders and
      // those without.
      failure->push_back(1);
      BuildFailureFunctionFuzzy(part, failure);
    }
  }
}

template <typename FailureIter>
bool IsUrlPatternMatch(const GURL& url,
                       const UrlPattern& pattern,
                       FailureIter failure_begin,
                       FailureIter failure_end) {
  DCHECK(url.is_valid());

  StringSplitter<impl::IsWildcard> subpatterns(pattern.url_pattern);
  auto subpattern_it = subpatterns.begin();
  auto subpattern_end = subpatterns.end();

  if (subpattern_it == subpattern_end) {
    return pattern.anchor_left != proto::ANCHOR_TYPE_BOUNDARY ||
           pattern.anchor_right != proto::ANCHOR_TYPE_BOUNDARY ||
           url.is_empty();
  }

  const base::StringPiece spec = url.possibly_invalid_spec();
  const url::Component host_part = url.parsed_for_possibly_invalid_spec().host;

  base::StringPiece subpattern = *subpattern_it++;
  if (subpattern_it == subpattern_end &&
      pattern.anchor_right == proto::ANCHOR_TYPE_BOUNDARY) {
    if (!EndsWithFuzzy(spec, subpattern))
      return false;
    if (pattern.anchor_left == proto::ANCHOR_TYPE_BOUNDARY)
      return spec.size() == subpattern.size();
    if (pattern.anchor_left == proto::ANCHOR_TYPE_SUBDOMAIN) {
      DCHECK_LE(subpattern.size(), spec.size());
      return url.has_host() &&
             impl::IsSubdomainAnchored(spec, host_part,
                                       spec.size() - subpattern.size());
    }
    return true;
  }

  base::StringPiece text = spec;
  if (pattern.anchor_left == proto::ANCHOR_TYPE_BOUNDARY) {
    if (!StartsWithFuzzy(spec, subpattern))
      return false;
    if (subpattern_it == subpattern_end)
      return true;
    text.remove_prefix(subpattern.size());
  } else if (pattern.anchor_left == proto::ANCHOR_TYPE_SUBDOMAIN) {
    if (!url.has_host())
      return false;

    const bool has_separator_placeholders = (*failure_begin != 0);
    if (has_separator_placeholders)
      ++failure_begin;

    const size_t position =
        has_separator_placeholders
            ? impl::FindSubdomainAnchoredFuzzy(spec, host_part, subpattern,
                                               &*failure_begin)
            : impl::FindSubdomainAnchored(spec, host_part, subpattern,
                                          &*failure_begin);
    if (position == base::StringPiece::npos)
      return false;
    if (subpattern_it == subpattern_end)
      return true;
    text.remove_prefix(position);
  } else {
    DCHECK_EQ(pattern.anchor_left, proto::ANCHOR_TYPE_NONE);
    // Get back to the initial subpattern, process it in the loop below.
    subpattern_it = subpatterns.begin();
  }

  while (subpattern_it != subpattern_end) {
    subpattern = *subpattern_it++;
    DCHECK(!subpattern.empty());

    if (subpattern_it == subpattern_end &&
        pattern.anchor_right == proto::ANCHOR_TYPE_BOUNDARY) {
      break;
    }

    // The subpatterns with separator placeholders were indexed differently and
    // should be matched accordingly. Their failure functions were prepended by
    // a non-zero value, and we need to skip it. If the value turned to be zero,
    // it is the initial value of a failure function of a regular substring.
    CHECK(failure_begin < failure_end);
    const bool has_separator_placeholders = (*failure_begin != 0);
    if (has_separator_placeholders)
      ++failure_begin;
    CHECK(static_cast<size_t>(std::distance(failure_begin, failure_end)) >=
          subpattern.size());

    // If the subpattern has separator placeholders, it should be matched with
    // FindFirstOccurrenceOfSubpattern, otherwise it can be found as a regular
    // substring.
    const size_t match_end =
        (has_separator_placeholders
             ? impl::FindFirstOccurrenceFuzzy(text, subpattern, &*failure_begin)
             : FindFirstOccurrence(text, subpattern, &*failure_begin));
    if (match_end == base::StringPiece::npos)
      return false;
    text.remove_prefix(match_end);
    failure_begin += subpattern.size();
  }

  return pattern.anchor_right != proto::ANCHOR_TYPE_BOUNDARY ||
         EndsWithFuzzy(text, subpattern);
}

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_MATCHING_H_
