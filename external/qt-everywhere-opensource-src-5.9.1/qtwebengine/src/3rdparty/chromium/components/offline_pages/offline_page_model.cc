// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include "url/gurl.h"

namespace offline_pages {

OfflinePageModel::SavePageParams::SavePageParams()
    : proposed_offline_id(OfflinePageModel::kInvalidOfflineId) {
}

OfflinePageModel::SavePageParams::SavePageParams(const SavePageParams& other) {
  url = other.url;
  client_id = other.client_id;
  proposed_offline_id = other.proposed_offline_id;
  original_url = other.original_url;
}

// static
bool OfflinePageModel::CanSaveURL(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

OfflinePageModel::OfflinePageModel() {}

OfflinePageModel::~OfflinePageModel() {}

}  // namespace offline_pages
