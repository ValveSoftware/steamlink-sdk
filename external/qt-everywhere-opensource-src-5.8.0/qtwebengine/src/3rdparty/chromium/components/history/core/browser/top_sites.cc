// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites.h"

#include "components/history/core/browser/top_sites_observer.h"

namespace history {

PrepopulatedPage::PrepopulatedPage()
    : most_visited(), favicon_id(-1), thumbnail_id(-1), color() {
}

PrepopulatedPage::PrepopulatedPage(const GURL& url,
                                   const base::string16& title,
                                   int favicon_id,
                                   int thumbnail_id,
                                   SkColor color)
    : most_visited(url, title),
      favicon_id(favicon_id),
      thumbnail_id(thumbnail_id),
      color(color) {
  most_visited.redirects.push_back(url);
}

TopSites::TopSites() {
}

TopSites::~TopSites() {
}

void TopSites::AddObserver(TopSitesObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TopSites::RemoveObserver(TopSitesObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TopSites::NotifyTopSitesLoaded() {
  FOR_EACH_OBSERVER(TopSitesObserver, observer_list_, TopSitesLoaded(this));
}

void TopSites::NotifyTopSitesChanged(
    const TopSitesObserver::ChangeReason reason) {
  FOR_EACH_OBSERVER(TopSitesObserver, observer_list_,
                    TopSitesChanged(this, reason));
}

}  // namespace history
