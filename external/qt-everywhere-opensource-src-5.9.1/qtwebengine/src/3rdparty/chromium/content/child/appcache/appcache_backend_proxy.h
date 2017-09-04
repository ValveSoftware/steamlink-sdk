// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_APPCACHE_APPCACHE_BACKEND_PROXY_H_
#define CONTENT_CHILD_APPCACHE_APPCACHE_BACKEND_PROXY_H_

#include <stdint.h>

#include <vector>

#include "content/common/appcache_interfaces.h"
#include "ipc/ipc_sender.h"

namespace content {

// Sends appcache related messages to the main process.
class AppCacheBackendProxy : public AppCacheBackend {
 public:
  explicit AppCacheBackendProxy(IPC::Sender* sender) : sender_(sender) {}

  IPC::Sender* sender() const { return sender_; }

  // AppCacheBackend methods
  void RegisterHost(int host_id) override;
  void UnregisterHost(int host_id) override;
  void SetSpawningHostId(int host_id, int spawning_host_id) override;
  void SelectCache(int host_id,
                   const GURL& document_url,
                   const int64_t cache_document_was_loaded_from,
                   const GURL& manifest_url) override;
  void SelectCacheForWorker(int host_id,
                            int parent_process_id,
                            int parent_host_id) override;
  void SelectCacheForSharedWorker(int host_id, int64_t appcache_id) override;
  void MarkAsForeignEntry(int host_id,
                          const GURL& document_url,
                          int64_t cache_document_was_loaded_from) override;
  AppCacheStatus GetStatus(int host_id) override;
  bool StartUpdate(int host_id) override;
  bool SwapCache(int host_id) override;
  void GetResourceList(
      int host_id,
      std::vector<AppCacheResourceInfo>* resource_infos) override;

 private:
  IPC::Sender* sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_APPCACHE_APPCACHE_BACKEND_PROXY_H_
