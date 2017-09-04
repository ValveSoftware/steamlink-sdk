// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_MATCH_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_MATCH_H_

#include <stddef.h>

#include <deque>

#include "components/history/core/browser/url_row.h"

namespace history {

// Used for intermediate history result operations.
struct HistoryMatch {
  // Required for STL, we don't use this directly.
  HistoryMatch();

  HistoryMatch(const URLRow& url_info,
               size_t input_location,
               bool match_in_scheme,
               bool innermost_match);

  static bool EqualsGURL(const HistoryMatch& h, const GURL& url);

  // Returns true if url in this HistoryMatch is just a host
  // (e.g. "http://www.google.com/") and not some other subpage
  // (e.g. "http://www.google.com/foo.html").
  bool IsHostOnly() const;

  URLRow url_info;

  // The offset of the user's input within the URL.
  size_t input_location;

  // Whether this is a match in the scheme.  This determines whether we'll go
  // ahead and show a scheme on the URL even if the user didn't type one.
  // If our best match was in the scheme, not showing the scheme is both
  // confusing and, for inline autocomplete of the fill_into_edit, dangerous.
  // (If the user types "h" and we match "http://foo/", we need to inline
  // autocomplete that, not "foo/", which won't show anything at all, and
  // will mislead the user into thinking the What You Typed match is what's
  // selected.)
  bool match_in_scheme;

  // A match after any scheme/"www.", if the user input could match at both
  // locations.  If the user types "w", an innermost match ("website.com") is
  // better than a non-innermost match ("www.google.com").  If the user types
  // "x", no scheme in our prefix list (or "www.") begins with x, so all
  // matches are, vacuously, "innermost matches".
  bool innermost_match;
};
typedef std::deque<HistoryMatch> HistoryMatches;

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_MATCH_H_
