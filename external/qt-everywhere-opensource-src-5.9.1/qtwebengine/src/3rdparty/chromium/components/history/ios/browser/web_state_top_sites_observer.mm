// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/ios/browser/web_state_top_sites_observer.h"

#include "base/logging.h"
#include "components/history/core/browser/top_sites.h"
#include "ios/web/public/load_committed_details.h"
#include "ios/web/public/navigation_item.h"

DEFINE_WEB_STATE_USER_DATA_KEY(history::WebStateTopSitesObserver);

namespace history {

// static
void WebStateTopSitesObserver::CreateForWebState(web::WebState* web_state,
                                                 TopSites* top_sites) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           new WebStateTopSitesObserver(web_state, top_sites));
  }
}

WebStateTopSitesObserver::WebStateTopSitesObserver(web::WebState* web_state,
                                                   TopSites* top_sites)
    : web::WebStateObserver(web_state), top_sites_(top_sites) {
}

WebStateTopSitesObserver::~WebStateTopSitesObserver() {
}

void WebStateTopSitesObserver::NavigationItemCommitted(
    const web::LoadCommittedDetails& load_details) {
  DCHECK(load_details.item);
  if (top_sites_)
    top_sites_->OnNavigationCommitted(load_details.item->GetURL());
}

}  // namespace history
