// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_site_request_manager.h"

#include "base/memory/singleton.h"

namespace content {

bool CrossSiteRequestManager::HasPendingCrossSiteRequest(int renderer_id,
                                                         int render_view_id) {
  base::AutoLock lock(lock_);

  std::pair<int, int> key(renderer_id, render_view_id);
  return pending_cross_site_views_.find(key) !=
      pending_cross_site_views_.end();
}

void CrossSiteRequestManager::SetHasPendingCrossSiteRequest(int renderer_id,
                                                            int render_view_id,
                                                            bool has_pending) {
  base::AutoLock lock(lock_);

  std::pair<int, int> key(renderer_id, render_view_id);
  if (has_pending) {
    pending_cross_site_views_.insert(key);
  } else {
    pending_cross_site_views_.erase(key);
  }
}

CrossSiteRequestManager::CrossSiteRequestManager() {}

CrossSiteRequestManager::~CrossSiteRequestManager() {}

// static
CrossSiteRequestManager* CrossSiteRequestManager::GetInstance() {
  return Singleton<CrossSiteRequestManager>::get();
}

}  // namespace content
