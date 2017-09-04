// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

ClientId::ClientId() : name_space(""), id("") {}

ClientId::ClientId(std::string name_space, std::string id)
    : name_space(name_space), id(id) {}

bool ClientId::operator==(const ClientId& client_id) const {
  return name_space == client_id.name_space && id == client_id.id;
}

bool ClientId::operator<(const ClientId& client_id) const {
  if (name_space == client_id.name_space)
    return (id < client_id.id);

  return name_space < client_id.name_space;
}

OfflinePageItem::OfflinePageItem()
    : file_size(0), access_count(0), flags(NO_FLAG) {}

OfflinePageItem::OfflinePageItem(const GURL& url,
                                 int64_t offline_id,
                                 const ClientId& client_id,
                                 const base::FilePath& file_path,
                                 int64_t file_size)
    : url(url),
      offline_id(offline_id),
      client_id(client_id),
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
      file_path(file_path),
      file_size(file_size),
      creation_time(creation_time),
      last_access_time(creation_time),
      access_count(0),
      flags(NO_FLAG) {}

OfflinePageItem::OfflinePageItem(const OfflinePageItem& other) = default;

OfflinePageItem::~OfflinePageItem() {
}

bool OfflinePageItem::operator==(const OfflinePageItem& other) const {
  return url == other.url &&
         offline_id == other.offline_id &&
         client_id == other.client_id &&
         file_path == other.file_path &&
         creation_time == other.creation_time &&
         last_access_time == other.last_access_time &&
         expiration_time == other.expiration_time &&
         access_count == other.access_count &&
         title == other.title &&
         flags == other.flags &&
         original_url == other.original_url;
}

bool OfflinePageItem::IsExpired() const {
  return creation_time < expiration_time;
}

}  // namespace offline_pages
