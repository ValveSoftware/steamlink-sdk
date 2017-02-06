// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/web_contents_top_sites_observer.h"

#include "base/logging.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(history::WebContentsTopSitesObserver);

namespace history {

// static
void WebContentsTopSitesObserver::CreateForWebContents(
    content::WebContents* web_contents,
    TopSites* top_sites) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(), new WebContentsTopSitesObserver(
                                                 web_contents, top_sites));
  }
}

WebContentsTopSitesObserver::WebContentsTopSitesObserver(
    content::WebContents* web_contents,
    TopSites* top_sites)
    : content::WebContentsObserver(web_contents), top_sites_(top_sites) {
}

WebContentsTopSitesObserver::~WebContentsTopSitesObserver() {
}

void WebContentsTopSitesObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  DCHECK(load_details.entry);
  if (top_sites_)
    top_sites_->OnNavigationCommitted(load_details.entry->GetURL());
}

}  // namespace history
