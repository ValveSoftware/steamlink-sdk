// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/appcache/appcache_frontend_impl.h"

#include "base/logging.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"

using blink::WebApplicationCacheHost;
using blink::WebConsoleMessage;

namespace content {

// Inline helper to keep the lines shorter and unwrapped.
inline WebApplicationCacheHostImpl* GetHost(int id) {
  return WebApplicationCacheHostImpl::FromId(id);
}

void AppCacheFrontendImpl::OnCacheSelected(int host_id,
                                           const AppCacheInfo& info) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnCacheSelected(info);
}

void AppCacheFrontendImpl::OnStatusChanged(const std::vector<int>& host_ids,
                                           AppCacheStatus status) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnStatusChanged(status);
  }
}

void AppCacheFrontendImpl::OnEventRaised(const std::vector<int>& host_ids,
                                         AppCacheEventID event_id) {
  DCHECK(event_id !=
         APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK(event_id != APPCACHE_ERROR_EVENT); // See OnErrorEventRaised.
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnEventRaised(event_id);
  }
}

void AppCacheFrontendImpl::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url,
    int num_total,
    int num_complete) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnProgressEventRaised(url, num_total, num_complete);
  }
}

void AppCacheFrontendImpl::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const AppCacheErrorDetails& details) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnErrorEventRaised(details);
  }
}

void AppCacheFrontendImpl::OnLogMessage(int host_id,
                                        AppCacheLogLevel log_level,
                                        const std::string& message) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnLogMessage(log_level, message);
}

void AppCacheFrontendImpl::OnContentBlocked(int host_id,
                                            const GURL& manifest_url) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnContentBlocked(manifest_url);
}

// Ensure that enum values never get out of sync with the
// ones declared for use within the WebKit api

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatched enum: " #a)

STATIC_ASSERT_ENUM(WebApplicationCacheHost::Uncached, APPCACHE_STATUS_UNCACHED);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Idle, APPCACHE_STATUS_IDLE);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Checking, APPCACHE_STATUS_CHECKING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Downloading,
                   APPCACHE_STATUS_DOWNLOADING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::UpdateReady,
                   APPCACHE_STATUS_UPDATE_READY);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::Obsolete, APPCACHE_STATUS_OBSOLETE);

STATIC_ASSERT_ENUM(WebApplicationCacheHost::CheckingEvent,
                   APPCACHE_CHECKING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ErrorEvent, APPCACHE_ERROR_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::NoUpdateEvent,
                   APPCACHE_NO_UPDATE_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::DownloadingEvent,
                   APPCACHE_DOWNLOADING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ProgressEvent,
                   APPCACHE_PROGRESS_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::UpdateReadyEvent,
                   APPCACHE_UPDATE_READY_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::CachedEvent, APPCACHE_CACHED_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ObsoleteEvent,
                   APPCACHE_OBSOLETE_EVENT);

STATIC_ASSERT_ENUM(WebApplicationCacheHost::ManifestError,
                   APPCACHE_MANIFEST_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::SignatureError,
                   APPCACHE_SIGNATURE_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ResourceError,
                   APPCACHE_RESOURCE_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::ChangedError,
                   APPCACHE_CHANGED_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::AbortError, APPCACHE_ABORT_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::QuotaError, APPCACHE_QUOTA_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::PolicyError, APPCACHE_POLICY_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::UnknownError,
                   APPCACHE_UNKNOWN_ERROR);

STATIC_ASSERT_ENUM(WebConsoleMessage::LevelDebug, APPCACHE_LOG_DEBUG);
STATIC_ASSERT_ENUM(WebConsoleMessage::LevelLog, APPCACHE_LOG_INFO);
STATIC_ASSERT_ENUM(WebConsoleMessage::LevelWarning, APPCACHE_LOG_WARNING);
STATIC_ASSERT_ENUM(WebConsoleMessage::LevelError, APPCACHE_LOG_ERROR);

}  // namespace content
