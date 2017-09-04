// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

class GURL;

namespace storage {
class BlobDataHandle;
class BlobEntry;
class ShareableBlobDataItem;

// This class stores the blob data in the various states of construction, as
// well as URL mappings to blob uuids.
// Implementation notes:
// * When removing a uuid registration, we do not check for URL mappings to that
//   uuid. The user must keep track of these.
class STORAGE_EXPORT BlobStorageRegistry {
 public:
  BlobStorageRegistry();
  ~BlobStorageRegistry();

  // Creates the blob entry with a refcount of 1 and a state of PENDING. If
  // the blob is already in use, we return null.
  BlobEntry* CreateEntry(const std::string& uuid,
                         const std::string& content_type,
                         const std::string& content_disposition);

  // Removes the blob entry with the given uuid. This does not unmap any
  // URLs that are pointing to this uuid. Returns if the entry existed.
  bool DeleteEntry(const std::string& uuid);

  bool HasEntry(const std::string& uuid) const;

  // Gets the blob entry for the given uuid. Returns nullptr if the entry
  // does not exist.
  BlobEntry* GetEntry(const std::string& uuid);
  const BlobEntry* GetEntry(const std::string& uuid) const;

  // Creates a url mapping from blob uuid to the given url. Returns false if
  // the uuid isn't mapped to an entry or if there already is a map for the URL.
  bool CreateUrlMapping(const GURL& url, const std::string& uuid);

  // Removes the given URL mapping. Optionally populates a uuid string of the
  // removed entry uuid. Returns false if the url isn't mapped.
  bool DeleteURLMapping(const GURL& url, std::string* uuid);

  // Returns if the url is mapped to a blob uuid.
  bool IsURLMapped(const GURL& blob_url) const;

  // Returns the entry from the given url, and optionally populates the uuid for
  // that entry. Returns a nullptr if the mapping or entry doesn't exist.
  BlobEntry* GetEntryFromURL(const GURL& url, std::string* uuid);

  size_t blob_count() const { return blob_map_.size(); }
  size_t url_count() const { return url_to_uuid_.size(); }

 private:
  friend class ViewBlobInternalsJob;
  using BlobMap =
      base::ScopedPtrHashMap<std::string, std::unique_ptr<BlobEntry>>;
  using URLMap = std::map<GURL, std::string>;

  BlobMap blob_map_;
  URLMap url_to_uuid_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageRegistry);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
