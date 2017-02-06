// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/appcache/appcache_dispatcher.h"

#include "content/common/appcache_messages.h"

namespace content {

AppCacheDispatcher::AppCacheDispatcher(
    IPC::Sender* sender,
    AppCacheFrontend* frontend)
    : backend_proxy_(sender),
      frontend_(frontend) {}

AppCacheDispatcher::~AppCacheDispatcher() {}

bool AppCacheDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppCacheDispatcher, msg)
    IPC_MESSAGE_HANDLER(AppCacheMsg_CacheSelected, OnCacheSelected)
    IPC_MESSAGE_HANDLER(AppCacheMsg_StatusChanged, OnStatusChanged)
    IPC_MESSAGE_HANDLER(AppCacheMsg_EventRaised, OnEventRaised)
    IPC_MESSAGE_HANDLER(AppCacheMsg_ProgressEventRaised, OnProgressEventRaised)
    IPC_MESSAGE_HANDLER(AppCacheMsg_ErrorEventRaised, OnErrorEventRaised)
    IPC_MESSAGE_HANDLER(AppCacheMsg_LogMessage, OnLogMessage)
    IPC_MESSAGE_HANDLER(AppCacheMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AppCacheDispatcher::OnCacheSelected(
    int host_id, const AppCacheInfo& info) {
  frontend_->OnCacheSelected(host_id, info);
}

void AppCacheDispatcher::OnStatusChanged(const std::vector<int>& host_ids,
                                         AppCacheStatus status) {
  frontend_->OnStatusChanged(host_ids, status);
}

void AppCacheDispatcher::OnEventRaised(const std::vector<int>& host_ids,
                                       AppCacheEventID event_id) {
  frontend_->OnEventRaised(host_ids, event_id);
}

void AppCacheDispatcher::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url, int num_total, int num_complete) {
  frontend_->OnProgressEventRaised(host_ids, url, num_total, num_complete);
}

void AppCacheDispatcher::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const AppCacheErrorDetails& details) {
  frontend_->OnErrorEventRaised(host_ids, details);
}

void AppCacheDispatcher::OnLogMessage(
    int host_id, int log_level, const std::string& message) {
  frontend_->OnLogMessage(
      host_id, static_cast<AppCacheLogLevel>(log_level), message);
}

void AppCacheDispatcher::OnContentBlocked(int host_id,
                                          const GURL& manifest_url) {
  frontend_->OnContentBlocked(host_id, manifest_url);
}

}  // namespace content
