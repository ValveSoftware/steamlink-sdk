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
                                           const appcache::AppCacheInfo& info) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnCacheSelected(info);
}

void AppCacheFrontendImpl::OnStatusChanged(const std::vector<int>& host_ids,
                                           appcache::AppCacheStatus status) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnStatusChanged(status);
  }
}

void AppCacheFrontendImpl::OnEventRaised(const std::vector<int>& host_ids,
                                         appcache::AppCacheEventID event_id) {
  DCHECK(event_id !=
         appcache::APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK(event_id != appcache::APPCACHE_ERROR_EVENT); // See OnErrorEventRaised.
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
    const appcache::AppCacheErrorDetails& details) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnErrorEventRaised(details);
  }
}

void AppCacheFrontendImpl::OnLogMessage(int host_id,
                                        appcache::AppCacheLogLevel log_level,
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
COMPILE_ASSERT((int)WebApplicationCacheHost::Uncached ==
               (int)appcache::APPCACHE_STATUS_UNCACHED, Uncached);
COMPILE_ASSERT((int)WebApplicationCacheHost::Idle ==
               (int)appcache::APPCACHE_STATUS_IDLE, Idle);
COMPILE_ASSERT((int)WebApplicationCacheHost::Checking ==
               (int)appcache::APPCACHE_STATUS_CHECKING, Checking);
COMPILE_ASSERT((int)WebApplicationCacheHost::Downloading ==
               (int)appcache::APPCACHE_STATUS_DOWNLOADING, Downloading);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReady ==
               (int)appcache::APPCACHE_STATUS_UPDATE_READY, UpdateReady);
COMPILE_ASSERT((int)WebApplicationCacheHost::Obsolete ==
               (int)appcache::APPCACHE_STATUS_OBSOLETE, Obsolete);

COMPILE_ASSERT((int)WebApplicationCacheHost::CheckingEvent ==
               (int)appcache::APPCACHE_CHECKING_EVENT, CheckingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ErrorEvent ==
               (int)appcache::APPCACHE_ERROR_EVENT, ErrorEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::NoUpdateEvent ==
               (int)appcache::APPCACHE_NO_UPDATE_EVENT, NoUpdateEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::DownloadingEvent ==
               (int)appcache::APPCACHE_DOWNLOADING_EVENT, DownloadingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ProgressEvent ==
               (int)appcache::APPCACHE_PROGRESS_EVENT, ProgressEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReadyEvent ==
               (int)appcache::APPCACHE_UPDATE_READY_EVENT, UpdateReadyEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::CachedEvent ==
               (int)appcache::APPCACHE_CACHED_EVENT, CachedEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ObsoleteEvent ==
               (int)appcache::APPCACHE_OBSOLETE_EVENT, ObsoleteEvent);

COMPILE_ASSERT((int)WebConsoleMessage::LevelDebug ==
               (int)appcache::APPCACHE_LOG_DEBUG, LevelDebug);
COMPILE_ASSERT((int)WebConsoleMessage::LevelLog ==
               (int)appcache::APPCACHE_LOG_INFO, LevelLog);
COMPILE_ASSERT((int)WebConsoleMessage::LevelWarning ==
               (int)appcache::APPCACHE_LOG_WARNING, LevelWarning);
COMPILE_ASSERT((int)WebConsoleMessage::LevelError ==
               (int)appcache::APPCACHE_LOG_ERROR, LevelError);

COMPILE_ASSERT((int)WebApplicationCacheHost::ManifestError ==
                   (int)appcache::APPCACHE_MANIFEST_ERROR,
               ManifestError);
COMPILE_ASSERT((int)WebApplicationCacheHost::SignatureError ==
                   (int)appcache::APPCACHE_SIGNATURE_ERROR,
               SignatureError);
COMPILE_ASSERT((int)WebApplicationCacheHost::ResourceError ==
                   (int)appcache::APPCACHE_RESOURCE_ERROR,
               ResourceError);
COMPILE_ASSERT((int)WebApplicationCacheHost::ChangedError ==
                   (int)appcache::APPCACHE_CHANGED_ERROR,
               ChangedError);
COMPILE_ASSERT((int)WebApplicationCacheHost::AbortError ==
                   (int)appcache::APPCACHE_ABORT_ERROR,
               AbortError);
COMPILE_ASSERT((int)WebApplicationCacheHost::QuotaError ==
                   (int)appcache::APPCACHE_QUOTA_ERROR,
               QuotaError);
COMPILE_ASSERT((int)WebApplicationCacheHost::PolicyError ==
                   (int)appcache::APPCACHE_POLICY_ERROR,
               PolicyError);
COMPILE_ASSERT((int)WebApplicationCacheHost::UnknownError ==
                   (int)appcache::APPCACHE_UNKNOWN_ERROR,
               UnknownError);

}  // namespace content
