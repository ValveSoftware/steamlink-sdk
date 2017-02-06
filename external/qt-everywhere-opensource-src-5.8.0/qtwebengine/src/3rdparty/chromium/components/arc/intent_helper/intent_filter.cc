// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/intent_filter.h"

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace arc {

IntentFilter::IntentFilter(const mojom::IntentFilterPtr& mojo_intent_filter) {
  for (const mojom::AuthorityEntryPtr& authorityptr :
       mojo_intent_filter->data_authorities) {
    authorities_.emplace_back(authorityptr);
  }
  for (const mojom::PatternMatcherPtr& pattern :
       mojo_intent_filter->data_paths) {
    paths_.emplace_back(pattern);
  }
}

IntentFilter::IntentFilter(const IntentFilter& other) = default;

IntentFilter::~IntentFilter() = default;

// Logically, this maps to IntentFilter#match, but this code only deals with
// view intents for http/https URLs and so it really only implements the
// #matchData part of the match code.
bool IntentFilter::match(const GURL& url) const {
  // Chrome-side code only receives view intents for http/https URLs, so this
  // match code really only implements the matchData part of the android
  // IntentFilter class.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Match the authority and the path (if any).
  if (!authorities_.empty()) {
    return matchDataAuthority(url) && (paths_.empty() || hasDataPath(url));
  }

  return false;
}

// Transcribed from android's IntentFilter#hasDataPath.
bool IntentFilter::hasDataPath(const GURL& url) const {
  const std::string path = url.path();
  for (const PatternMatcher& pattern : paths_) {
    if (pattern.match(path)) {
      return true;
    }
  }
  return false;
}

// Transcribed from android's IntentFilter#matchDataAuthority.
bool IntentFilter::matchDataAuthority(const GURL& url) const {
  for (const AuthorityEntry& authority : authorities_) {
    if (authority.match(url)) {
      return true;
    }
  }
  return false;
}

IntentFilter::AuthorityEntry::AuthorityEntry(
    const mojom::AuthorityEntryPtr& entry)
    : host_(entry->host.get()), port_(entry->port) {
  // Wildcards are only allowed at the front of the host string.
  wild_ = !host_.empty() && host_[0] == '*';
  if (wild_) {
    host_ = host_.substr(1);
  }

  // TODO: Not i18n-friendly.  Figure out how to correctly deal with IDNs.
  host_ = base::ToLowerASCII(host_);
}

// Transcribed from android's IntentFilter.AuthorityEntry#match.
bool IntentFilter::AuthorityEntry::match(const GURL& url) const {
  if (!url.has_host()) {
    return false;
  }

  // Note: On android, intent filters with explicit port specifications only
  // match URLs with explict ports, even if the specified port is the default
  // port.  Using GURL::EffectiveIntPort instead of GURL::IntPort means that
  // this code differs in behaviour (i.e. it just matches the effective port,
  // ignoring whether it was implicitly or explicitly specified).
  //
  // We do this because it provides an optimistic match - ensuring that the
  // disambiguation code doesn't miss URLs that might be handled by android
  // apps.  This doesn't cause misrouted intents because this check is followed
  // up by a mojo call that actually verifies the list of packages that could
  // accept the given intent.
  if (port_ >= 0 && port_ != url.EffectiveIntPort()) {
    return false;
  }

  if (wild_) {
    return base::EndsWith(url.host_piece(), host_,
                          base::CompareCase::INSENSITIVE_ASCII);
  } else {
    // TODO: Not i18n-friendly.  Figure out how to correctly deal with IDNs.
    return host_ == base::ToLowerASCII(url.host_piece());
  }
}

IntentFilter::PatternMatcher::PatternMatcher(
    const mojom::PatternMatcherPtr& pattern)
    : pattern_(pattern->pattern.get()), match_type_(pattern->type) {}

// Transcribed from android's PatternMatcher#matchPattern.
bool IntentFilter::PatternMatcher::match(const std::string& str) const {
  if (str.empty()) {
    return false;
  }
  switch (match_type_) {
    case mojom::PatternType::PATTERN_LITERAL:
      return str == pattern_;
    case mojom::PatternType::PATTERN_PREFIX:
      return base::StartsWith(str, pattern_,
                              base::CompareCase::INSENSITIVE_ASCII);
    case mojom::PatternType::PATTERN_SIMPLE_GLOB:
      return matchGlob(str);
  }

  return false;
}

// Transcribed from android's PatternMatcher#matchPattern.
bool IntentFilter::PatternMatcher::matchGlob(const std::string& str) const {
#define GET_CHAR(s, i) ((UNLIKELY(i >= s.length())) ? '\0' : s[i])

  const size_t NP = pattern_.length();
  const size_t NS = str.length();
  if (NP == 0) {
    return NS == 0;
  }
  size_t ip = 0, is = 0;
  char nextChar = GET_CHAR(pattern_, 0);
  while (ip < NP && is < NS) {
    char c = nextChar;
    ++ip;
    nextChar = GET_CHAR(pattern_, ip);
    const bool escaped = (c == '\\');
    if (escaped) {
      c = nextChar;
      ++ip;
      nextChar = GET_CHAR(pattern_, ip);
    }
    if (nextChar == '*') {
      if (!escaped && c == '.') {
        if (ip >= (NP - 1)) {
          // At the end with a pattern match
          return true;
        }
        ++ip;
        nextChar = GET_CHAR(pattern_, ip);
        // Consume everything until the next char in the pattern is found.
        if (nextChar == '\\') {
          ++ip;
          nextChar = GET_CHAR(pattern_, ip);
        }
        do {
          if (GET_CHAR(str, is) == nextChar) {
            break;
          }
          ++is;
        } while (is < NS);
        if (is == NS) {
          // Next char in the pattern didn't exist in the match.
          return false;
        }
        ++ip;
        nextChar = GET_CHAR(pattern_, ip);
        ++is;
      } else {
        // Consume only characters matching the one before '*'.
        do {
          if (GET_CHAR(str, is) != c) {
            break;
          }
          ++is;
        } while (is < NS);
        ++ip;
        nextChar = GET_CHAR(pattern_, ip);
      }
    } else {
      if (c != '.' && GET_CHAR(str, is) != c)
        return false;
      ++is;
    }
  }

  if (ip >= NP && is >= NS) {
    // Reached the end of both strings
    return true;
  }

  // One last check: we may have finished the match string, but still have a
  // '.*' at the end of the pattern, which is still a match.
  if (ip == NP - 2 && GET_CHAR(pattern_, ip) == '.' &&
      GET_CHAR(pattern_, ip + 1) == '*') {
    return true;
  }

  return false;

#undef GET_CHAR
}

}  // namespace arc
