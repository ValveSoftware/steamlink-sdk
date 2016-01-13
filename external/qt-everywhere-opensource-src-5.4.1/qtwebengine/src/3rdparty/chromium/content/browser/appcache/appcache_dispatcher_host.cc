// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_dispatcher_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/common/appcache_messages.h"
#include "content/public/browser/user_metrics.h"

namespace content {

AppCacheDispatcherHost::AppCacheDispatcherHost(
    ChromeAppCacheService* appcache_service,
    int process_id)
    : BrowserMessageFilter(AppCacheMsgStart),
      appcache_service_(appcache_service),
      frontend_proxy_(this),
      process_id_(process_id) {
}

void AppCacheDispatcherHost::OnChannelConnected(int32 peer_pid) {
  if (appcache_service_.get()) {
    backend_impl_.Initialize(
        appcache_service_.get(), &frontend_proxy_, process_id_);
    get_status_callback_ =
        base::Bind(&AppCacheDispatcherHost::GetStatusCallback,
                   base::Unretained(this));
    start_update_callback_ =
        base::Bind(&AppCacheDispatcherHost::StartUpdateCallback,
                   base::Unretained(this));
    swap_cache_callback_ =
        base::Bind(&AppCacheDispatcherHost::SwapCacheCallback,
                   base::Unretained(this));
  }
}

bool AppCacheDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppCacheDispatcherHost, message)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_RegisterHost, OnRegisterHost)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_UnregisterHost, OnUnregisterHost)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_SetSpawningHostId, OnSetSpawningHostId)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_GetResourceList, OnGetResourceList)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_SelectCache, OnSelectCache)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_SelectCacheForWorker,
                        OnSelectCacheForWorker)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_SelectCacheForSharedWorker,
                        OnSelectCacheForSharedWorker)
    IPC_MESSAGE_HANDLER(AppCacheHostMsg_MarkAsForeignEntry,
                        OnMarkAsForeignEntry)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheHostMsg_GetStatus, OnGetStatus)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheHostMsg_StartUpdate, OnStartUpdate)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AppCacheHostMsg_SwapCache, OnSwapCache)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

AppCacheDispatcherHost::~AppCacheDispatcherHost() {}

void AppCacheDispatcherHost::BadMessageReceived() {
  RecordAction(base::UserMetricsAction("BadMessageTerminate_ACDH"));
  BrowserMessageFilter::BadMessageReceived();
}

void AppCacheDispatcherHost::OnRegisterHost(int host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.RegisterHost(host_id)) {
      BadMessageReceived();
    }
  }
}

void AppCacheDispatcherHost::OnUnregisterHost(int host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.UnregisterHost(host_id)) {
      BadMessageReceived();
    }
  }
}

void AppCacheDispatcherHost::OnSetSpawningHostId(
    int host_id, int spawning_host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.SetSpawningHostId(host_id, spawning_host_id))
      BadMessageReceived();
  }
}

void AppCacheDispatcherHost::OnSelectCache(
    int host_id, const GURL& document_url,
    int64 cache_document_was_loaded_from,
    const GURL& opt_manifest_url) {
  if (appcache_service_.get()) {
    if (!backend_impl_.SelectCache(host_id,
                                   document_url,
                                   cache_document_was_loaded_from,
                                   opt_manifest_url)) {
      BadMessageReceived();
    }
  } else {
    frontend_proxy_.OnCacheSelected(host_id, appcache::AppCacheInfo());
  }
}

void AppCacheDispatcherHost::OnSelectCacheForWorker(
    int host_id, int parent_process_id, int parent_host_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.SelectCacheForWorker(
            host_id, parent_process_id, parent_host_id)) {
      BadMessageReceived();
    }
  } else {
    frontend_proxy_.OnCacheSelected(host_id, appcache::AppCacheInfo());
  }
}

void AppCacheDispatcherHost::OnSelectCacheForSharedWorker(
    int host_id, int64 appcache_id) {
  if (appcache_service_.get()) {
    if (!backend_impl_.SelectCacheForSharedWorker(host_id, appcache_id))
      BadMessageReceived();
  } else {
    frontend_proxy_.OnCacheSelected(host_id, appcache::AppCacheInfo());
  }
}

void AppCacheDispatcherHost::OnMarkAsForeignEntry(
    int host_id, const GURL& document_url,
    int64 cache_document_was_loaded_from) {
  if (appcache_service_.get()) {
    if (!backend_impl_.MarkAsForeignEntry(
            host_id, document_url, cache_document_was_loaded_from)) {
      BadMessageReceived();
    }
  }
}

void AppCacheDispatcherHost::OnGetResourceList(
    int host_id, std::vector<appcache::AppCacheResourceInfo>* params) {
  if (appcache_service_.get())
    backend_impl_.GetResourceList(host_id, params);
}

void AppCacheDispatcherHost::OnGetStatus(int host_id, IPC::Message* reply_msg) {
  if (pending_reply_msg_) {
    BadMessageReceived();
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.GetStatusWithCallback(
            host_id, get_status_callback_, reply_msg)) {
      BadMessageReceived();
    }
    return;
  }

  GetStatusCallback(appcache::APPCACHE_STATUS_UNCACHED, reply_msg);
}

void AppCacheDispatcherHost::OnStartUpdate(int host_id,
                                           IPC::Message* reply_msg) {
  if (pending_reply_msg_) {
    BadMessageReceived();
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.StartUpdateWithCallback(
            host_id, start_update_callback_, reply_msg)) {
      BadMessageReceived();
    }
    return;
  }

  StartUpdateCallback(false, reply_msg);
}

void AppCacheDispatcherHost::OnSwapCache(int host_id, IPC::Message* reply_msg) {
  if (pending_reply_msg_) {
    BadMessageReceived();
    delete reply_msg;
    return;
  }

  pending_reply_msg_.reset(reply_msg);
  if (appcache_service_.get()) {
    if (!backend_impl_.SwapCacheWithCallback(
            host_id, swap_cache_callback_, reply_msg)) {
      BadMessageReceived();
    }
    return;
  }

  SwapCacheCallback(false, reply_msg);
}

void AppCacheDispatcherHost::GetStatusCallback(
    appcache::AppCacheStatus status, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK_EQ(pending_reply_msg_.get(), reply_msg);
  AppCacheHostMsg_GetStatus::WriteReplyParams(reply_msg, status);
  Send(pending_reply_msg_.release());
}

void AppCacheDispatcherHost::StartUpdateCallback(bool result, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK_EQ(pending_reply_msg_.get(), reply_msg);
  AppCacheHostMsg_StartUpdate::WriteReplyParams(reply_msg, result);
  Send(pending_reply_msg_.release());
}

void AppCacheDispatcherHost::SwapCacheCallback(bool result, void* param) {
  IPC::Message* reply_msg = reinterpret_cast<IPC::Message*>(param);
  DCHECK_EQ(pending_reply_msg_.get(), reply_msg);
  AppCacheHostMsg_SwapCache::WriteReplyParams(reply_msg, result);
  Send(pending_reply_msg_.release());
}

}  // namespace content
