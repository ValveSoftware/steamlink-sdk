// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_registry.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "storage/browser/blob/blob_entry.h"
#include "url/gurl.h"

namespace storage {

namespace {
// We can't use GURL directly for these hash fragment manipulations
// since it doesn't have specific knowlege of the BlobURL format. GURL
// treats BlobURLs as if they were PathURLs which don't support hash
// fragments.

bool BlobUrlHasRef(const GURL& url) {
  return url.spec().find('#') != std::string::npos;
}

GURL ClearBlobUrlRef(const GURL& url) {
  size_t hash_pos = url.spec().find('#');
  if (hash_pos == std::string::npos)
    return url;
  return GURL(url.spec().substr(0, hash_pos));
}

}  // namespace


BlobStorageRegistry::BlobStorageRegistry() {}

BlobStorageRegistry::~BlobStorageRegistry() {
  // Note: We don't bother calling the construction complete callbacks, as we
  // are only being destructed at the end of the life of the browser process.
  // So it shouldn't matter.
}

BlobEntry* BlobStorageRegistry::CreateEntry(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition) {
  DCHECK(!ContainsKey(blob_map_, uuid));
  std::unique_ptr<BlobEntry> entry(
      new BlobEntry(content_type, content_disposition));
  BlobEntry* entry_ptr = entry.get();
  blob_map_.add(uuid, std::move(entry));
  return entry_ptr;
}

bool BlobStorageRegistry::DeleteEntry(const std::string& uuid) {
  return blob_map_.erase(uuid) == 1;
}

bool BlobStorageRegistry::HasEntry(const std::string& uuid) const {
  return blob_map_.find(uuid) != blob_map_.end();
}

BlobEntry* BlobStorageRegistry::GetEntry(const std::string& uuid) {
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return nullptr;
  return found->second;
}

const BlobEntry* BlobStorageRegistry::GetEntry(const std::string& uuid) const {
  return const_cast<BlobStorageRegistry*>(this)->GetEntry(uuid);
}

bool BlobStorageRegistry::CreateUrlMapping(const GURL& blob_url,
                                           const std::string& uuid) {
  DCHECK(!BlobUrlHasRef(blob_url));
  if (blob_map_.find(uuid) == blob_map_.end() || IsURLMapped(blob_url))
    return false;
  url_to_uuid_[blob_url] = uuid;
  return true;
}

bool BlobStorageRegistry::DeleteURLMapping(const GURL& blob_url,
                                           std::string* uuid) {
  DCHECK(!BlobUrlHasRef(blob_url));
  URLMap::iterator found = url_to_uuid_.find(blob_url);
  if (found == url_to_uuid_.end())
    return false;
  if (uuid)
    uuid->assign(found->second);
  url_to_uuid_.erase(found);
  return true;
}

bool BlobStorageRegistry::IsURLMapped(const GURL& blob_url) const {
  return base::ContainsKey(url_to_uuid_, blob_url);
}

BlobEntry* BlobStorageRegistry::GetEntryFromURL(const GURL& url,
                                                std::string* uuid) {
  URLMap::iterator found =
      url_to_uuid_.find(BlobUrlHasRef(url) ? ClearBlobUrlRef(url) : url);
  if (found == url_to_uuid_.end())
    return nullptr;
  BlobEntry* entry = GetEntry(found->second);
  if (entry && uuid)
    uuid->assign(found->second);
  return entry;
}

}  // namespace storage
