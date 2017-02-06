// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_frontend_proxy.h"

#include "content/common/appcache_messages.h"

namespace content {

AppCacheFrontendProxy::AppCacheFrontendProxy(IPC::Sender* sender)
    : sender_(sender) {
}

void AppCacheFrontendProxy::OnCacheSelected(
    int host_id, const AppCacheInfo& info) {
  sender_->Send(new AppCacheMsg_CacheSelected(host_id, info));
}

void AppCacheFrontendProxy::OnStatusChanged(const std::vector<int>& host_ids,
                                            AppCacheStatus status) {
  sender_->Send(new AppCacheMsg_StatusChanged(host_ids, status));
}

void AppCacheFrontendProxy::OnEventRaised(const std::vector<int>& host_ids,
                                          AppCacheEventID event_id) {
  DCHECK_NE(APPCACHE_PROGRESS_EVENT,
      event_id);  // See OnProgressEventRaised.
  sender_->Send(new AppCacheMsg_EventRaised(host_ids, event_id));
}

void AppCacheFrontendProxy::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url, int num_total, int num_complete) {
  sender_->Send(new AppCacheMsg_ProgressEventRaised(
      host_ids, url, num_total, num_complete));
}

void AppCacheFrontendProxy::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const AppCacheErrorDetails& details) {
  sender_->Send(new AppCacheMsg_ErrorEventRaised(host_ids, details));
}

void AppCacheFrontendProxy::OnLogMessage(int host_id,
                                         AppCacheLogLevel log_level,
                                         const std::string& message) {
  sender_->Send(new AppCacheMsg_LogMessage(host_id, log_level, message));
}

void AppCacheFrontendProxy::OnContentBlocked(int host_id,
                                             const GURL& manifest_url) {
  sender_->Send(new AppCacheMsg_ContentBlocked(host_id, manifest_url));
}

}  // namespace content
