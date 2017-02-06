// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/storage_handler.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "base/strings/string_split.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"

namespace content {
namespace devtools {
namespace storage {

namespace {
static const char kAppCache[] = "appcache";
static const char kCookies[] = "cookies";
static const char kFileSystems[] = "filesystems";
static const char kIndexedDB[] = "indexeddb";
static const char kLocalStorage[] = "local_storage";
static const char kShaderCache[] = "shader_cache";
static const char kWebSQL[] = "websql";
static const char kWebRTCIdentity[] = "webrdc_identity";
static const char kServiceWorkers[] = "service_workers";
static const char kCacheStorage[] = "cache_storage";
static const char kAll[] = "all";
}

typedef DevToolsProtocolClient::Response Response;

StorageHandler::StorageHandler()
    : host_(nullptr) {
}

StorageHandler::~StorageHandler() = default;

void StorageHandler::SetRenderFrameHost(RenderFrameHost* host) {
  host_ = host;
}

Response StorageHandler::ClearDataForOrigin(
    const std::string& origin,
    const std::string& storage_types) {
  if (!host_)
    return Response::InternalError("Not connected to host");

  StoragePartition* partition = host_->GetProcess()->GetStoragePartition();
  std::vector<std::string> types = base::SplitString(
      storage_types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::unordered_set<std::string> set(types.begin(), types.end());
  uint32_t remove_mask = 0;
  if (set.count(kAppCache))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  if (set.count(kCookies))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  if (set.count(kFileSystems))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (set.count(kIndexedDB))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  if (set.count(kLocalStorage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (set.count(kShaderCache))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  if (set.count(kWebSQL))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  if (set.count(kWebRTCIdentity))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBRTC_IDENTITY;
  if (set.count(kServiceWorkers))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  if (set.count(kCacheStorage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  if (set.count(kAll))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_ALL;

  if (!remove_mask)
    return Response::InvalidParams("No valid storage type specified");

  partition->ClearDataForOrigin(
      remove_mask,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(origin),
      partition->GetURLRequestContext(),
      base::Bind(&base::DoNothing));
  return Response::OK();
}

}  // namespace storage
}  // namespace devtools
}  // namespace content
