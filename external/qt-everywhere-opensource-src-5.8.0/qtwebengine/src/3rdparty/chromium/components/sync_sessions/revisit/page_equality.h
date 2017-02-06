// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_PAGE_EQUALITY_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_PAGE_EQUALITY_H_

#include "url/gurl.h"

namespace sync_sessions {

// An extremely simplistic approach to determining page equality, given two
// URLs. Some of the notable examples this fails to accommodate are varying
// schemes, mobile subdomains, unimpactful query parameters/fragments, and
// page changing headers/cookies.
class PageEquality {
 public:
  explicit PageEquality(const GURL& url) : url_(url) {}
  PageEquality(const PageEquality&) = default;
  bool IsSamePage(const GURL& url) const { return url == url_; }

 private:
  const GURL url_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_PAGE_EQUALITY_H_
