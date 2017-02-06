// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_frontend_proxy.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {
class ChromeAppCacheService;

// Handles appcache related messages sent to the main browser process from
// its child processes. There is a distinct host for each child process.
// Messages are handled on the IO thread. The RenderProcessHostImpl creates
// an instance and delegates calls to it.
class AppCacheDispatcherHost : public BrowserMessageFilter {
 public:
  AppCacheDispatcherHost(ChromeAppCacheService* appcache_service,
                         int process_id);

  // BrowserIOMessageFilter implementation
  void OnChannelConnected(int32_t peer_pid) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~AppCacheDispatcherHost() override;

 private:
  // IPC message handlers
  void OnRegisterHost(int host_id);
  void OnUnregisterHost(int host_id);
  void OnSetSpawningHostId(int host_id, int spawning_host_id);
  void OnSelectCache(int host_id,
                     const GURL& document_url,
                     int64_t cache_document_was_loaded_from,
                     const GURL& opt_manifest_url);
  void OnSelectCacheForWorker(int host_id, int parent_process_id,
                              int parent_host_id);
  void OnSelectCacheForSharedWorker(int host_id, int64_t appcache_id);
  void OnMarkAsForeignEntry(int host_id,
                            const GURL& document_url,
                            int64_t cache_document_was_loaded_from);
  void OnGetStatus(int host_id, IPC::Message* reply_msg);
  void OnStartUpdate(int host_id, IPC::Message* reply_msg);
  void OnSwapCache(int host_id, IPC::Message* reply_msg);
  void OnGetResourceList(
      int host_id,
      std::vector<AppCacheResourceInfo>* resource_infos);
  void GetStatusCallback(AppCacheStatus status, void* param);
  void StartUpdateCallback(bool result, void* param);
  void SwapCacheCallback(bool result, void* param);


  scoped_refptr<ChromeAppCacheService> appcache_service_;
  AppCacheFrontendProxy frontend_proxy_;
  AppCacheBackendImpl backend_impl_;

  content::GetStatusCallback get_status_callback_;
  content::StartUpdateCallback start_update_callback_;
  content::SwapCacheCallback swap_cache_callback_;
  std::unique_ptr<IPC::Message> pending_reply_msg_;

  // The corresponding ChildProcessHost object's id().
  int process_id_;

  base::WeakPtrFactory<AppCacheDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_DISPATCHER_HOST_H_
