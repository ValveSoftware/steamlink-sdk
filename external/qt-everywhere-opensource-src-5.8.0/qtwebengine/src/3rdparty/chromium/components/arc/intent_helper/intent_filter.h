// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_
#define COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_

#include <string>
#include <vector>

#include "components/arc/common/intent_helper.mojom.h"

class GURL;

namespace arc {

// A chrome-side implementation of Android's IntentFilter class.  This is used
// to approximate the intent filtering and determine whether a given URL is
// likely to be handled by any android-side apps, prior to making expensive IPC
// calls.
class IntentFilter {
 public:
  explicit IntentFilter(const mojom::IntentFilterPtr& mojo_intent_filter);
  IntentFilter(const IntentFilter& other);
  ~IntentFilter();

  bool match(const GURL& url) const;

 private:
  // A helper class for handling matching of the host part of the URL.
  class AuthorityEntry {
   public:
    explicit AuthorityEntry(const mojom::AuthorityEntryPtr& entry);
    bool match(const GURL& url) const;

   private:
    std::string host_;
    bool wild_;
    int port_;
  };

  // A helper class for handling matching of various patterns in the URL.
  class PatternMatcher {
   public:
    explicit PatternMatcher(const mojom::PatternMatcherPtr& pattern);
    bool match(const std::string& match) const;

   private:
    bool matchGlob(const std::string& match) const;

    std::string pattern_;
    mojom::PatternType match_type_;
  };

  bool matchDataAuthority(const GURL& url) const;
  bool hasDataPath(const GURL& url) const;

  std::vector<AuthorityEntry> authorities_;
  std::vector<PatternMatcher> paths_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_
