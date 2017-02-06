// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_item.h"

#include "net/base/filename_util.h"

namespace offline_pages {

namespace {
const int kCurrentVersion = 1;
}

ClientId::ClientId() : name_space(""), id("") {}

ClientId::ClientId(std::string name_space, std::string id)
    : name_space(name_space), id(id) {}

bool ClientId::operator==(const ClientId& client_id) const {
  return name_space == client_id.name_space && id == client_id.id;
}

OfflinePageItem::OfflinePageItem()
    : version(kCurrentVersion),
      file_size(0),
      access_count(0),
      flags(NO_FLAG) {
}

OfflinePageItem::OfflinePageItem(const GURL& url,
                                 int64_t offline_id,
                                 const ClientId& client_id,
                                 const base::FilePath& file_path,
                                 int64_t file_size)
    : url(url),
      offline_id(offline_id),
      client_id(client_id),
      version(kCurrentVersion),
      file_path(file_path),
      file_size(file_size),
      access_count(0),
      flags(NO_FLAG) {}

OfflinePageItem::OfflinePageItem(const GURL& url,
                                 int64_t offline_id,
                                 const ClientId& client_id,
                                 const base::FilePath& file_path,
                                 int64_t file_size,
                                 const base::Time& creation_time)
    : url(url),
      offline_id(offline_id),
      client_id(client_id),
      version(kCurrentVersion),
      file_path(file_path),
      file_size(file_size),
      creation_time(creation_time),
      last_access_time(creation_time),
      access_count(0),
      flags(NO_FLAG) {}

OfflinePageItem::OfflinePageItem(const OfflinePageItem& other) = default;

OfflinePageItem::~OfflinePageItem() {
}

GURL OfflinePageItem::GetOfflineURL() const {
  return net::FilePathToFileURL(file_path);
}

bool OfflinePageItem::IsExpired() const {
  return creation_time < expiration_time;
}

}  // namespace offline_pages
