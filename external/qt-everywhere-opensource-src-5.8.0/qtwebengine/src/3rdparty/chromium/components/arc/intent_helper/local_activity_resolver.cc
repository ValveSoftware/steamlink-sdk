// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/local_activity_resolver.h"

#include "url/gurl.h"

namespace arc {

LocalActivityResolver::LocalActivityResolver() = default;

LocalActivityResolver::~LocalActivityResolver() = default;

bool LocalActivityResolver::ShouldChromeHandleUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    // Chrome will handle everything that is not http and https.
    return true;
  }

  for (const IntentFilter& filter : intent_filters_) {
    if (filter.match(url)) {
      return false;
    }
  }

  // Didn't find any matches for Android so let Chrome handle it.
  return true;
}

void LocalActivityResolver::UpdateIntentFilters(
    mojo::Array<mojom::IntentFilterPtr> mojo_intent_filters) {
  intent_filters_.clear();
  for (mojom::IntentFilterPtr& mojo_filter : mojo_intent_filters) {
    intent_filters_.emplace_back(mojo_filter);
  }
}

}  // namespace arc
