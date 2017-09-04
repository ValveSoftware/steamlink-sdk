// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager() {}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
  DCHECK(!v4_get_hash_protocol_manager_);
}

bool SafeBrowsingDatabaseManager::CancelApiCheck(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ApiCheckSet::iterator it = FindClientApiCheck(client);
  if (it != api_checks_.end()) {
    api_checks_.erase(it);
    return true;
  }
  NOTREACHED();
  return false;
}

bool SafeBrowsingDatabaseManager::CheckApiBlacklistUrl(const GURL& url,
                                                       Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(v4_get_hash_protocol_manager_);

  // Make sure we can check this url.
  if (!(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    return true;
  }

  // There can only be one in-progress check for the same client at a time.
  DCHECK(FindClientApiCheck(client) == api_checks_.end());

  std::unique_ptr<SafeBrowsingApiCheck> check(
      new SafeBrowsingApiCheck(url, client));
  api_checks_.insert(check.get());
  v4_get_hash_protocol_manager_->GetFullHashesWithApis(
      url, base::Bind(&SafeBrowsingDatabaseManager::OnThreatMetadataResponse,
                      base::Unretained(this), base::Passed(std::move(check))));

  return false;
}

SafeBrowsingDatabaseManager::ApiCheckSet::iterator
SafeBrowsingDatabaseManager::FindClientApiCheck(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (ApiCheckSet::iterator it = api_checks_.begin(); it != api_checks_.end();
       ++it) {
    if ((*it)->client() == client) {
      return it;
    }
  }
  return api_checks_.end();
}

StoresToCheck SafeBrowsingDatabaseManager::GetStoresForFullHashRequests() {
  return StoresToCheck({GetChromeUrlApiId()});
}

void SafeBrowsingDatabaseManager::OnThreatMetadataResponse(
    std::unique_ptr<SafeBrowsingApiCheck> check,
    const ThreatMetadata& md) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(check);

  // If the check is not in |api_checks_| then the request was cancelled by the
  // client.
  ApiCheckSet::iterator it = api_checks_.find(check.get());
  if (it == api_checks_.end())
    return;

  check->client()->OnCheckApiBlacklistUrlResult(check->url(), md);
  api_checks_.erase(it);
}

void SafeBrowsingDatabaseManager::StartOnIOThread(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  v4_get_hash_protocol_manager_ = V4GetHashProtocolManager::Create(
      request_context_getter, GetStoresForFullHashRequests(), config);
}

// |shutdown| not used. Destroys the v4 protocol managers. This may be called
// multiple times during the life of the DatabaseManager.
// Must be called on IO thread.
void SafeBrowsingDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Delete pending checks, calling back any clients with empty metadata.
  for (const SafeBrowsingApiCheck* check : api_checks_) {
    if (check->client()) {
      check->client()->OnCheckApiBlacklistUrlResult(check->url(),
                                                    ThreatMetadata());
    }
  }

  // This cancels all in-flight GetHash requests.
  v4_get_hash_protocol_manager_.reset();
}

SafeBrowsingDatabaseManager::SafeBrowsingApiCheck::SafeBrowsingApiCheck(
    const GURL& url,
    Client* client)
    : url_(url), client_(client) {}

SafeBrowsingDatabaseManager::SafeBrowsingApiCheck::~SafeBrowsingApiCheck() {}

}  // namespace safe_browsing
