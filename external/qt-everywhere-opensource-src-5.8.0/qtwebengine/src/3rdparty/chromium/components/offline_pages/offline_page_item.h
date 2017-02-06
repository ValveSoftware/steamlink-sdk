// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ITEM_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ITEM_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace offline_pages {

struct ClientId {
  // The namespace for the id (of course 'namespace' is a reserved word, so...)
  std::string name_space;
  // The id in the client's namespace.  Opaque to us.
  std::string id;

  ClientId();
  ClientId(std::string name_space, std::string id);

  bool operator==(const ClientId& client_id) const;
};

// Metadata of the offline page.
struct OfflinePageItem {
 public:
  // Note that this should match with Flags enum in offline_pages.proto.
  enum Flags {
    NO_FLAG = 0,
    MARKED_FOR_DELETION = 0x1,
  };

  OfflinePageItem();
  OfflinePageItem(const GURL& url,
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size);
  OfflinePageItem(const GURL& url,
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size,
                  const base::Time& creation_time);
  OfflinePageItem(const OfflinePageItem& other);
  ~OfflinePageItem();

  // Gets a URL of the file under |file_path|.
  GURL GetOfflineURL() const;

  // Returns whether the offline page is expired.
  bool IsExpired() const;

  // The URL of the page.
  GURL url;
  // The primary key/ID for this page in offline pages internal database.
  int64_t offline_id;

  // The Client ID (external) related to the offline page. This is opaque
  // to our system, but useful for users of offline pages who want to map
  // their ids to our saved pages.
  ClientId client_id;

  // Version of the offline page item.
  int version;
  // The file path to the archive with a local copy of the page.
  base::FilePath file_path;
  // The size of the offline copy.
  int64_t file_size;
  // The time when the offline archive was created.
  base::Time creation_time;
  // The time when the offline archive was last accessed.
  base::Time last_access_time;
  // The time when the offline page was expired.
  base::Time expiration_time;
  // Number of times that the offline archive has been accessed.
  int access_count;
  // Flags about the state and behavior of the offline page.
  Flags flags;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ITEM_H_
