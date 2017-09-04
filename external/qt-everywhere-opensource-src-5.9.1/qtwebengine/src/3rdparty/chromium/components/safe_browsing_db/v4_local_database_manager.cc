// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file should not be build on Android but is currently getting built.
// TODO(vakh): Fix that: http://crbug.com/621647

#include "components/safe_browsing_db/v4_local_database_manager.h"

#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing_db/v4_feature_list.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

using content::BrowserThread;
using base::TimeTicks;

namespace safe_browsing {

namespace {

const ThreatSeverity kLeastSeverity =
    std::numeric_limits<ThreatSeverity>::max();

ListInfos GetListInfos() {
  // NOTE(vakh): When adding a store here, add the corresponding store-specific
  // histograms also.
  // NOTE(vakh): Delete file "AnyIpMalware.store". It has been renamed to
  // "IpMalware.store". If it exists, it should be 75 bytes long.
  // The first argument to ListInfo specifies whether to sync hash prefixes for
  // that list. This can be false for two reasons:
  // - The server doesn't support that list yet. Once the server adds support
  //   for it, it can be changed to true.
  // - The list doesn't have hash prefixes to match. All requests lead to full
  //   hash checks. For instance: GetChromeUrlApiId()

#if defined(GOOGLE_CHROME_BUILD)
  const bool kSyncOnlyOnChromeBuilds = true;
#else
  const bool kSyncOnlyOnChromeBuilds = false;
#endif
  const bool kSyncAlways = true;
  const bool kSyncNever = false;
  return ListInfos({
      ListInfo(kSyncOnlyOnChromeBuilds, "CertCsdDownloadWhitelist.store",
               GetCertCsdDownloadWhitelistId(), SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncOnlyOnChromeBuilds, "ChromeFilenameClientIncident.store",
               GetChromeFilenameClientIncidentId(), SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncAlways, "IpMalware.store", GetIpMalwareId(),
               SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncOnlyOnChromeBuilds, "UrlCsdDownloadWhitelist.store",
               GetUrlCsdDownloadWhitelistId(), SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncOnlyOnChromeBuilds, "UrlCsdWhitelist.store",
               GetUrlCsdWhitelistId(), SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncAlways, "UrlSoceng.store", GetUrlSocEngId(),
               SB_THREAT_TYPE_URL_PHISHING),
      ListInfo(kSyncAlways, "UrlMalware.store", GetUrlMalwareId(),
               SB_THREAT_TYPE_URL_MALWARE),
      ListInfo(kSyncAlways, "UrlUws.store", GetUrlUwsId(),
               SB_THREAT_TYPE_URL_UNWANTED),
      ListInfo(kSyncAlways, "UrlMalBin.store", GetUrlMalBinId(),
               SB_THREAT_TYPE_BINARY_MALWARE_URL),
      ListInfo(kSyncAlways, "ChromeExtMalware.store", GetChromeExtMalwareId(),
               SB_THREAT_TYPE_EXTENSION),
      ListInfo(kSyncOnlyOnChromeBuilds, "ChromeUrlClientIncident.store",
               GetChromeUrlClientIncidentId(),
               SB_THREAT_TYPE_BLACKLISTED_RESOURCE),
      ListInfo(kSyncNever, "", GetChromeUrlApiId(), SB_THREAT_TYPE_API_ABUSE),
  });
}

// Returns the severity information about a given SafeBrowsing list. The lowest
// value is 0, which represents the most severe list.
ThreatSeverity GetThreatSeverity(const ListIdentifier& list_id) {
  switch (list_id.threat_type()) {
    case MALWARE_THREAT:
    case SOCIAL_ENGINEERING_PUBLIC:
    case MALICIOUS_BINARY:
      return 0;
    case UNWANTED_SOFTWARE:
      return 1;
    case API_ABUSE:
      return 2;
    default:
      NOTREACHED() << "Unexpected ThreatType encountered: "
                   << list_id.threat_type();
      return kLeastSeverity;
  }
}

}  // namespace

V4LocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::vector<GURL>& urls)
    : client(client),
      client_callback_type(client_callback_type),
      result_threat_type(SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check),
      urls(urls) {
  for (const auto& url : urls) {
    V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  }
}

V4LocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::set<FullHash>& full_hashes_set)
    : client(client),
      client_callback_type(client_callback_type),
      result_threat_type(SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check) {
  full_hashes.assign(full_hashes_set.begin(), full_hashes_set.end());
}

V4LocalDatabaseManager::PendingCheck::~PendingCheck() {}

// static
scoped_refptr<V4LocalDatabaseManager> V4LocalDatabaseManager::Create(
    const base::FilePath& base_path) {
  if (!V4FeatureList::IsLocalDatabaseManagerEnabled()) {
    return nullptr;
  }

  return make_scoped_refptr(new V4LocalDatabaseManager(base_path));
}

V4LocalDatabaseManager::V4LocalDatabaseManager(const base::FilePath& base_path)
    : base_path_(base_path),
      enabled_(false),
      list_infos_(GetListInfos()),
      weak_factory_(this) {
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());

  DVLOG(1) << "V4LocalDatabaseManager::V4LocalDatabaseManager: "
           << "base_path_: " << base_path_.AsUTF8Unsafe();
}

V4LocalDatabaseManager::~V4LocalDatabaseManager() {
  DCHECK(!enabled_);
}

//
// Start: SafeBrowsingDatabaseManager implementation
//

void V4LocalDatabaseManager::CancelCheck(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(enabled_);

  auto pending_it = std::find_if(
      std::begin(pending_checks_), std::end(pending_checks_),
      [client](const PendingCheck* check) { return check->client == client; });
  if (pending_it != pending_checks_.end()) {
    pending_checks_.erase(pending_it);
  }

  auto queued_it =
      std::find_if(std::begin(queued_checks_), std::end(queued_checks_),
                   [&client](const std::unique_ptr<PendingCheck>& check) {
                     return check->client == client;
                   });
  if (queued_it != queued_checks_.end()) {
    queued_checks_.erase(queued_it);
  }
}

bool V4LocalDatabaseManager::CanCheckResourceType(
    content::ResourceType resource_type) const {
  // We check all types since most checks are fast.
  return true;
}

bool V4LocalDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(url::kHttpsScheme) || url.SchemeIs(url::kHttpScheme) ||
         url.SchemeIs(url::kFtpScheme);
}

bool V4LocalDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool V4LocalDatabaseManager::CheckBrowseUrl(const GURL& url, Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!enabled_ || !CanCheckUrl(url)) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = base::MakeUnique<PendingCheck>(
      client, ClientCallbackType::CHECK_BROWSE_URL,
      StoresToCheck({GetUrlMalwareId(), GetUrlSocEngId(), GetUrlUwsId()}),
      std::vector<GURL>(1, url));

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!enabled_ || url_chain.empty()) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = base::MakeUnique<PendingCheck>(
      client, ClientCallbackType::CHECK_DOWNLOAD_URLS,
      StoresToCheck({GetUrlMalBinId()}), url_chain);

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckExtensionIDs(
    const std::set<FullHash>& extension_ids,
    Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!enabled_) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = base::MakeUnique<PendingCheck>(
      client, ClientCallbackType::CHECK_EXTENSION_IDS,
      StoresToCheck({GetChromeExtMalwareId()}), extension_ids);

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckResourceUrl(const GURL& url, Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StoresToCheck stores_to_check({GetChromeUrlClientIncidentId()});

  if (!CanCheckUrl(url) || !AreStoresAvailableNow(stores_to_check)) {
    // Fail open: Mark resource as safe immediately.
    // TODO(nparker): This should queue the request if the DB isn't yet
    // loaded, and later decide if this store is available.
    // Currently this is the only store that requires full-hash-checks
    // AND isn't supported on Chromium, so it's unique.
    return true;
  }

  std::unique_ptr<PendingCheck> check = base::MakeUnique<PendingCheck>(
      client, ClientCallbackType::CHECK_RESOURCE_URL, stores_to_check,
      std::vector<GURL>(1, url));

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::MatchCsdWhitelistUrl(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StoresToCheck stores_to_check({GetUrlCsdWhitelistId()});
  if (!AreStoresAvailableNow(stores_to_check)) {
    // Fail open: Whitelist everything. Otherwise we may run the
    // CSD phishing/malware detector on popular domains and generate
    // undue load on the client and server. This has the effect of disabling
    // CSD phishing/malware detection until the store is first synced.
    return true;
  }

  return HandleUrlSynchronously(url, stores_to_check);
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StoresToCheck stores_to_check({GetCertCsdDownloadWhitelistId()});
  if (!AreStoresAvailableNow(stores_to_check)) {
    // Fail close: Whitelist nothing. This may generate download-protection
    // pings for whitelisted binaries, but that's fine.
    return false;
  }

  return HandleHashSynchronously(str, stores_to_check);
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistUrl(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StoresToCheck stores_to_check({GetUrlCsdDownloadWhitelistId()});
  if (!AreStoresAvailableNow(stores_to_check)) {
    // Fail close: Whitelist nothing. This may generate download-protection
    // pings for whitelisted domains, but that's fine.
    return false;
  }

  return HandleUrlSynchronously(url, stores_to_check);
}

bool V4LocalDatabaseManager::MatchMalwareIP(const std::string& ip_address) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!enabled_ || !v4_database_) {
    return false;
  }

  FullHash hashed_encoded_ip;
  if (!V4ProtocolManagerUtil::IPAddressToEncodedIPV6Hash(ip_address,
                                                         &hashed_encoded_ip)) {
    return false;
  }

  return HandleHashSynchronously(hashed_encoded_ip,
                                 StoresToCheck({GetIpMalwareId()}));
}

bool V4LocalDatabaseManager::MatchModuleWhitelistString(
    const std::string& str) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StoresToCheck stores_to_check({GetChromeFilenameClientIncidentId()});
  if (!AreStoresAvailableNow(stores_to_check)) {
    // Fail open: Whitelist everything.  This has the effect of marking
    // all DLLs as safe until the DB is synced and loaded.
    return true;
  }

  // str is the module's filename.  Convert to hash.
  FullHash hash = crypto::SHA256HashString(str);
  return HandleHashSynchronously(hash, stores_to_check);
}

ThreatSource V4LocalDatabaseManager::GetThreatSource() const {
  return ThreatSource::LOCAL_PVER4;
}

bool V4LocalDatabaseManager::IsCsdWhitelistKillSwitchOn() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return false;
}

bool V4LocalDatabaseManager::IsDownloadProtectionEnabled() const {
  // TODO(vakh): Investigate the possibility of using a command line switch for
  // this instead.
  return true;
}

bool V4LocalDatabaseManager::IsMalwareKillSwitchOn() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return false;
}

bool V4LocalDatabaseManager::IsSupported() const {
  return true;
}

void V4LocalDatabaseManager::StartOnIOThread(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnIOThread(request_context_getter, config);

  db_updated_callback_ = base::Bind(&V4LocalDatabaseManager::DatabaseUpdated,
                                    weak_factory_.GetWeakPtr());

  SetupUpdateProtocolManager(request_context_getter, config);
  SetupDatabase();

  enabled_ = true;
}

void V4LocalDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  enabled_ = false;

  pending_checks_.clear();

  RespondSafeToQueuedChecks();

  // Delete the V4Database. Any pending writes to disk are completed.
  // This operation happens on the task_runner on which v4_database_ operates
  // and doesn't block the IO thread.
  V4Database::Destroy(std::move(v4_database_));

  // Delete the V4UpdateProtocolManager.
  // This cancels any in-flight update request.
  v4_update_protocol_manager_.reset();

  db_updated_callback_.Reset();

  SafeBrowsingDatabaseManager::StopOnIOThread(shutdown);
}

//
// End: SafeBrowsingDatabaseManager implementation
//

void V4LocalDatabaseManager::DatabaseReadyForChecks(
    std::unique_ptr<V4Database> v4_database) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The following check is needed because it is possible that by the time the
  // database is ready, StopOnIOThread has been called.
  if (enabled_) {
    v4_database_ = std::move(v4_database);

    v4_database_->RecordFileSizeHistograms();

    // The consistency of the stores read from the disk needs to verified. Post
    // that task on the task runner. It calls |DatabaseReadyForUpdates|
    // callback with the stores to reset, if any, and then we can schedule the
    // database updates.
    v4_database_->VerifyChecksum(
        base::Bind(&V4LocalDatabaseManager::DatabaseReadyForUpdates,
                   weak_factory_.GetWeakPtr()));

    ProcessQueuedChecks();
  } else {
    // Schedule the deletion of v4_database off IO thread.
    V4Database::Destroy(std::move(v4_database));
  }
}

void V4LocalDatabaseManager::DatabaseReadyForUpdates(
    const std::vector<ListIdentifier>& stores_to_reset) {
  if (enabled_) {
    v4_database_->ResetStores(stores_to_reset);

    // The database is ready to process updates. Schedule them now.
    v4_update_protocol_manager_->ScheduleNextUpdate(
        v4_database_->GetStoreStateMap());
  }
}

void V4LocalDatabaseManager::DatabaseUpdated() {
  if (enabled_) {
    v4_database_->RecordFileSizeHistograms();
    v4_update_protocol_manager_->ScheduleNextUpdate(
        v4_database_->GetStoreStateMap());
  }
}

bool V4LocalDatabaseManager::GetPrefixMatches(
    const std::unique_ptr<PendingCheck>& check,
    FullHashToStoreAndHashPrefixesMap* full_hash_to_store_and_hash_prefixes) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(enabled_);
  DCHECK(v4_database_);

  const base::TimeTicks before = TimeTicks::Now();
  if (check->client_callback_type == ClientCallbackType::CHECK_BROWSE_URL ||
      check->client_callback_type == ClientCallbackType::CHECK_DOWNLOAD_URLS ||
      check->client_callback_type == ClientCallbackType::CHECK_RESOURCE_URL ||
      check->client_callback_type == ClientCallbackType::CHECK_EXTENSION_IDS ||
      check->client_callback_type == ClientCallbackType::CHECK_OTHER) {
    DCHECK(!check->full_hashes.empty());

    full_hash_to_store_and_hash_prefixes->clear();
    for (const auto& full_hash : check->full_hashes) {
      StoreAndHashPrefixes matched_store_and_hash_prefixes;
      v4_database_->GetStoresMatchingFullHash(full_hash, check->stores_to_check,
                                              &matched_store_and_hash_prefixes);
      if (!matched_store_and_hash_prefixes.empty()) {
        (*full_hash_to_store_and_hash_prefixes)[full_hash] =
            matched_store_and_hash_prefixes;
      }
    }
  } else {
    NOTREACHED() << "Unexpected client_callback_type encountered.";
  }

  // TODO(vakh): Only log SafeBrowsing.V4GetPrefixMatches.Time once PVer3 code
  // is removed.
  // NOTE(vakh): This doesn't distinguish which stores it's searching through.
  // However, the vast majority of the entries in this histogram will be from
  // searching the three CHECK_BROWSE_URL stores.
  base::TimeDelta diff = TimeTicks::Now() - before;
  UMA_HISTOGRAM_TIMES("SB2.FilterCheck", diff);
  UMA_HISTOGRAM_CUSTOM_TIMES("SafeBrowsing.V4GetPrefixMatches.Time", diff,
                             base::TimeDelta::FromMicroseconds(20),
                             base::TimeDelta::FromSeconds(1), 50);
  return !full_hash_to_store_and_hash_prefixes->empty();
}

void V4LocalDatabaseManager::GetSeverestThreatTypeAndMetadata(
    SBThreatType* result_threat_type,
    ThreatMetadata* metadata,
    FullHash* matching_full_hash,
    const std::vector<FullHashInfo>& full_hash_infos) {
  DCHECK(result_threat_type);
  DCHECK(metadata);
  DCHECK(matching_full_hash);

  ThreatSeverity most_severe_yet = kLeastSeverity;
  for (const FullHashInfo& fhi : full_hash_infos) {
    ThreatSeverity severity = GetThreatSeverity(fhi.list_id);
    if (severity < most_severe_yet) {
      most_severe_yet = severity;
      *result_threat_type = GetSBThreatTypeForList(fhi.list_id);
      *metadata = fhi.metadata;
      *matching_full_hash = fhi.full_hash;
    }
  }
}

StoresToCheck V4LocalDatabaseManager::GetStoresForFullHashRequests() {
  StoresToCheck stores_for_full_hash;
  for (auto it : list_infos_) {
    stores_for_full_hash.insert(it.list_id());
  }
  return stores_for_full_hash;
}

// Returns the SBThreatType corresponding to a given SafeBrowsing list.
SBThreatType V4LocalDatabaseManager::GetSBThreatTypeForList(
    const ListIdentifier& list_id) {
  auto it = std::find_if(
      std::begin(list_infos_), std::end(list_infos_),
      [&list_id](ListInfo const& li) { return li.list_id() == list_id; });
  DCHECK(list_infos_.end() != it);
  DCHECK_NE(SB_THREAT_TYPE_SAFE, it->sb_threat_type());
  DCHECK_NE(SB_THREAT_TYPE_UNUSED, it->sb_threat_type());
  return it->sb_threat_type();
}

bool V4LocalDatabaseManager::HandleCheck(std::unique_ptr<PendingCheck> check) {
  if (!v4_database_) {
    queued_checks_.push_back(std::move(check));
    return false;
  }

  FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;
  if (!GetPrefixMatches(check, &full_hash_to_store_and_hash_prefixes)) {
    return true;
  }

  // Add check to pending_checks_ before scheduling PerformFullHashCheck so that
  // even if the client calls CancelCheck before PerformFullHashCheck gets
  // called, the check can be found in pending_checks_.
  pending_checks_.insert(check.get());

  // Post on the IO thread to enforce async behavior.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&V4LocalDatabaseManager::PerformFullHashCheck, this,
                 base::Passed(std::move(check)),
                 full_hash_to_store_and_hash_prefixes));

  return false;
}

bool V4LocalDatabaseManager::HandleHashSynchronously(
    const FullHash& hash,
    const StoresToCheck& stores_to_check) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::set<FullHash> hashes{hash};
  std::unique_ptr<PendingCheck> check = base::MakeUnique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check, hashes);

  FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;
  return GetPrefixMatches(check, &full_hash_to_store_and_hash_prefixes);
}

bool V4LocalDatabaseManager::HandleUrlSynchronously(
    const GURL& url,
    const StoresToCheck& stores_to_check) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<PendingCheck> check = base::MakeUnique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check,
      std::vector<GURL>(1, url));

  FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;
  return GetPrefixMatches(check, &full_hash_to_store_and_hash_prefixes);
}

void V4LocalDatabaseManager::OnFullHashResponse(
    std::unique_ptr<PendingCheck> check,
    const std::vector<FullHashInfo>& full_hash_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!enabled_) {
    DCHECK(pending_checks_.empty());
    return;
  }

  const auto it = pending_checks_.find(check.get());
  if (it == pending_checks_.end()) {
    // The check has since been cancelled.
    return;
  }

  // Find out the most severe threat, if any, to report to the client.
  GetSeverestThreatTypeAndMetadata(&check->result_threat_type,
                                   &check->url_metadata,
                                   &check->matching_full_hash, full_hash_infos);
  pending_checks_.erase(it);
  RespondToClient(std::move(check));
}

void V4LocalDatabaseManager::PerformFullHashCheck(
    std::unique_ptr<PendingCheck> check,
    const FullHashToStoreAndHashPrefixesMap&
        full_hash_to_store_and_hash_prefixes) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(enabled_);
  DCHECK(!full_hash_to_store_and_hash_prefixes.empty());

  v4_get_hash_protocol_manager_->GetFullHashes(
      full_hash_to_store_and_hash_prefixes,
      base::Bind(&V4LocalDatabaseManager::OnFullHashResponse,
                 weak_factory_.GetWeakPtr(), base::Passed(std::move(check))));
}

void V4LocalDatabaseManager::ProcessQueuedChecks() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Steal the queue to protect against reentrant CancelCheck() calls.
  QueuedChecks checks;
  checks.swap(queued_checks_);

  for (auto& it : checks) {
    FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes;
    if (!GetPrefixMatches(it, &full_hash_to_store_and_hash_prefixes)) {
      RespondToClient(std::move(it));
    } else {
      pending_checks_.insert(it.get());
      PerformFullHashCheck(std::move(it), full_hash_to_store_and_hash_prefixes);
    }
  }
}

void V4LocalDatabaseManager::RespondSafeToQueuedChecks() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Steal the queue to protect against reentrant CancelCheck() calls.
  QueuedChecks checks;
  checks.swap(queued_checks_);

  for (std::unique_ptr<PendingCheck>& it : checks) {
    RespondToClient(std::move(it));
  }
}

void V4LocalDatabaseManager::RespondToClient(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(check.get());

  switch (check->client_callback_type) {
    case ClientCallbackType::CHECK_BROWSE_URL:
      DCHECK_EQ(1u, check->urls.size());
      check->client->OnCheckBrowseUrlResult(
          check->urls[0], check->result_threat_type, check->url_metadata);
      break;

    case ClientCallbackType::CHECK_DOWNLOAD_URLS:
      check->client->OnCheckDownloadUrlResult(check->urls,
                                              check->result_threat_type);
      break;

    case ClientCallbackType::CHECK_RESOURCE_URL:
      DCHECK_EQ(1u, check->urls.size());
      check->client->OnCheckResourceUrlResult(
          check->urls[0], check->result_threat_type, check->matching_full_hash);
      break;

    case ClientCallbackType::CHECK_EXTENSION_IDS: {
      const std::set<FullHash> extension_ids(check->full_hashes.begin(),
                                             check->full_hashes.end());
      check->client->OnCheckExtensionsResult(extension_ids);
      break;
    }

    case ClientCallbackType::CHECK_OTHER:
      NOTREACHED() << "Unexpected client_callback_type encountered";
  }
}

void V4LocalDatabaseManager::SetupDatabase() {
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only get a new task runner if there isn't one already. If the service has
  // previously been started and stopped, a task runner could already exist.
  if (!task_runner_) {
    base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
    task_runner_ = pool->GetSequencedTaskRunnerWithShutdownBehavior(
        pool->GetSequenceToken(), base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  }

  // Do not create the database on the IO thread since this may be an expensive
  // operation. Instead, do that on the task_runner and when the new database
  // has been created, swap it out on the IO thread.
  NewDatabaseReadyCallback db_ready_callback =
      base::Bind(&V4LocalDatabaseManager::DatabaseReadyForChecks,
                 weak_factory_.GetWeakPtr());
  V4Database::Create(task_runner_, base_path_, list_infos_, db_ready_callback);
}

void V4LocalDatabaseManager::SetupUpdateProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  V4UpdateCallback callback =
      base::Bind(&V4LocalDatabaseManager::UpdateRequestCompleted,
                 weak_factory_.GetWeakPtr());

  v4_update_protocol_manager_ =
      V4UpdateProtocolManager::Create(request_context_getter, config, callback);
}

void V4LocalDatabaseManager::UpdateRequestCompleted(
    std::unique_ptr<ParsedServerResponse> parsed_server_response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            db_updated_callback_);
}

bool V4LocalDatabaseManager::AreStoresAvailableNow(
    const StoresToCheck& stores_to_check) const {
  return enabled_ && v4_database_ &&
         v4_database_->AreStoresAvailable(stores_to_check);
}

}  // namespace safe_browsing
