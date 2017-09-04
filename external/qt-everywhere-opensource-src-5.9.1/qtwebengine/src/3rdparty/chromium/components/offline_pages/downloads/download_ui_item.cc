// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/downloads/download_ui_item.h"

#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

DownloadUIItem::DownloadUIItem()
    : total_bytes(0) {
}

DownloadUIItem::DownloadUIItem(const OfflinePageItem& page)
    : guid(page.client_id.id),
      url(page.url),
      title(page.title),
      target_path(page.file_path),
      start_time(page.creation_time),
      total_bytes(page.file_size) {}

DownloadUIItem::DownloadUIItem(const SavePageRequest& request)
    : guid(request.client_id().id),
      url(request.url()),
      start_time(request.creation_time()),
      total_bytes(-1L) {}

DownloadUIItem::DownloadUIItem(const DownloadUIItem& other)
    : guid(other.guid),
      url(other.url),
      title(other.title),
      target_path(other.target_path),
      start_time(other.start_time),
      total_bytes(other.total_bytes) {}

DownloadUIItem::~DownloadUIItem() {
}

}  // namespace offline_pages
