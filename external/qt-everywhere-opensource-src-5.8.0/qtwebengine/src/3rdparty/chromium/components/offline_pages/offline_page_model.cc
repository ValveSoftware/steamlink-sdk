// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include "url/gurl.h"

namespace offline_pages {

// static
bool OfflinePageModel::CanSaveURL(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

OfflinePageModel::OfflinePageModel() {}

OfflinePageModel::~OfflinePageModel() {}

}  // namespace offline_pages
