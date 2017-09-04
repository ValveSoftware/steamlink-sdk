// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_database.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_constants.h"
#include "components/precache/core/proto/unfinished_work.pb.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "sql/connection.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace {

// The number of days old that an entry in the precache URL table can be before
// it is considered "old" and is removed from the table.
const int kPrecacheHistoryExpiryPeriodDays = 60;

const int kSecondsInMinute = 60;
const int kSecondsInHour = kSecondsInMinute * 60;

}  // namespace

namespace precache {

PrecacheDatabase::PrecacheDatabase()
    : is_flush_posted_(false), weak_factory_(this) {
  // A PrecacheDatabase can be constructed on any thread.
  thread_checker_.DetachFromThread();
}

PrecacheDatabase::~PrecacheDatabase() {
  // The destructor must not run on the UI thread, as it may trigger IO
  // operations via sql::Connection's destructor.
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool PrecacheDatabase::Init(const base::FilePath& db_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!db_);  // Init must only be called once.

  db_.reset(new sql::Connection());
  db_->set_histogram_tag("Precache");

  if (!db_->Open(db_path)) {
    // Don't initialize the URL table if unable to access
    // the database.
    return false;
  }

  if (!precache_url_table_.Init(db_.get()) ||
      !precache_referrer_host_table_.Init(db_.get()) ||
      !precache_session_table_.Init(db_.get())) {
    // Raze and close the database connection to indicate that it's not usable,
    // and so that the database will be created anew next time, in case it's
    // corrupted.
    db_->RazeAndClose();
    return false;
  }
  return true;
}

void PrecacheDatabase::DeleteExpiredPrecacheHistory(
    const base::Time& current_time) {
  if (!IsDatabaseAccessible()) {
    // Do nothing if unable to access the database.
    return;
  }

  // Delete old precache history that has expired.
  base::Time delete_end = current_time - base::TimeDelta::FromDays(
                                             kPrecacheHistoryExpiryPeriodDays);
  buffered_writes_.push_back(
      base::Bind(&PrecacheURLTable::DeleteAllPrecachedBefore,
                 base::Unretained(&precache_url_table_), delete_end));
  buffered_writes_.push_back(
      base::Bind(&PrecacheReferrerHostTable::DeleteAllEntriesBefore,
                 base::Unretained(&precache_referrer_host_table_), delete_end));
  Flush();
}

void PrecacheDatabase::ClearHistory() {
  if (!IsDatabaseAccessible()) {
    // Do nothing if unable to access the database.
    return;
  }

  buffered_writes_.push_back(base::Bind(
      &PrecacheURLTable::DeleteAll, base::Unretained(&precache_url_table_)));
  buffered_writes_.push_back(
      base::Bind(&PrecacheReferrerHostTable::DeleteAll,
                 base::Unretained(&precache_referrer_host_table_)));
  Flush();
}

void PrecacheDatabase::SetLastPrecacheTimestamp(const base::Time& time) {
  last_precache_timestamp_ = time;

  if (!IsDatabaseAccessible()) {
    // Do nothing if unable to access the database.
    return;
  }

  buffered_writes_.push_back(
      base::Bind(&PrecacheSessionTable::SetLastPrecacheTimestamp,
                 base::Unretained(&precache_session_table_), time));
  MaybePostFlush();
}

base::Time PrecacheDatabase::GetLastPrecacheTimestamp() {
  if (last_precache_timestamp_.is_null() && IsDatabaseAccessible()) {
    last_precache_timestamp_ =
        precache_session_table_.GetLastPrecacheTimestamp();
  }
  return last_precache_timestamp_;
}

PrecacheReferrerHostEntry PrecacheDatabase::GetReferrerHost(
    const std::string& referrer_host) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return precache_referrer_host_table_.GetReferrerHost(referrer_host);
}

void PrecacheDatabase::GetURLListForReferrerHost(
    int64_t referrer_host_id,
    std::vector<GURL>* used_urls,
    std::vector<GURL>* downloaded_urls) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(PrecacheReferrerHostEntry::kInvalidId, referrer_host_id);

  // Flush any pending writes to the URL and referrer host tables.
  Flush();

  precache_url_table_.GetURLListForReferrerHost(referrer_host_id, used_urls,
                                                downloaded_urls);
  precache_url_table_.SetDownloadReported(referrer_host_id);
}

void PrecacheDatabase::RecordURLPrefetchMetrics(
    const net::HttpResponseInfo& info,
    const base::TimeDelta& latency) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UMA_HISTOGRAM_TIMES("Precache.Latency.Prefetch", latency);
  UMA_HISTOGRAM_ENUMERATION("Precache.CacheStatus.Prefetch",
                            info.cache_entry_status,
                            net::HttpResponseInfo::CacheEntryStatus::ENTRY_MAX);

  DCHECK(info.headers) << "The headers are required to get the freshness.";
  if (info.headers) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Precache.Freshness.Prefetch",
        info.headers->GetFreshnessLifetimes(info.response_time)
            .freshness.InSeconds(),
        base::TimeDelta::FromMinutes(5).InSeconds() /* min */,
        base::TimeDelta::FromDays(356).InSeconds() /* max */,
        100 /* bucket_count */);
  }
}

void PrecacheDatabase::RecordURLPrefetch(const GURL& url,
                                         const std::string& referrer_host,
                                         const base::Time& fetch_time,
                                         bool was_cached,
                                         int64_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsDatabaseAccessible()) {
    // Don't track anything if unable to access the database.
    return;
  }

  if (buffered_urls_.find(url.spec()) != buffered_urls_.end()) {
    // If the URL for this fetch is in the write buffer, then flush the write
    // buffer.
    Flush();
  }

  if (!was_cached) {
    // The precache only counts as overhead if it was downloaded over the
    // network.
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedPrecacheMotivated",
                         static_cast<base::HistogramBase::Sample>(size));
  }

  // Use the URL table to keep track of URLs. URLs that are fetched via network
  // or already in the cache due to prior precaching are recorded as
  // precache-motivated. URLs that came from the cache and not recorded as
  // precached previously, were already in the cache because of user browsing.
  // Therefore, this precache will not be considered as precache-motivated,
  // since it had no significant effect (besides a possible revalidation and a
  // change in the cache LRU priority). If a row for the URL already exists,
  // then the timestamp is updated.
  const PrecacheURLInfo info = precache_url_table_.GetURLInfo(url);
  bool is_download_reported = info.is_download_reported;
  if (info.is_precached && !was_cached) {
    is_download_reported = false;
  }
  buffered_writes_.push_back(
      base::Bind(&PrecacheDatabase::RecordURLPrefetchInternal, GetWeakPtr(),
                 url, referrer_host, !was_cached || info.is_precached,
                 fetch_time, is_download_reported));
  buffered_urls_.insert(url.spec());
  MaybePostFlush();
}

void PrecacheDatabase::RecordURLPrefetchInternal(
    const GURL& url,
    const std::string& referrer_host,
    bool is_precached,
    const base::Time& fetch_time,
    bool is_download_reported) {
  int64_t referrer_host_id =
      precache_referrer_host_table_.GetReferrerHost(referrer_host).id;
  if (referrer_host_id == PrecacheReferrerHostEntry::kInvalidId) {
    referrer_host_id = precache_referrer_host_table_.UpdateReferrerHost(
        referrer_host, 0, fetch_time);
  }
  DCHECK_NE(referrer_host_id, PrecacheReferrerHostEntry::kInvalidId);
  precache_url_table_.AddURL(url, referrer_host_id, is_precached, fetch_time,
                             is_download_reported);
}

void PrecacheDatabase::RecordURLNonPrefetch(const GURL& url,
                                            const base::TimeDelta& latency,
                                            const base::Time& fetch_time,
                                            const net::HttpResponseInfo& info,
                                            int64_t size,
                                            int host_rank,
                                            bool is_connection_cellular) {
  UMA_HISTOGRAM_TIMES("Precache.Latency.NonPrefetch", latency);
  UMA_HISTOGRAM_ENUMERATION("Precache.CacheStatus.NonPrefetch",
                            info.cache_entry_status,
                            net::HttpResponseInfo::CacheEntryStatus::ENTRY_MAX);

  if (host_rank != history::kMaxTopHosts) {
    // The resource was loaded on a page that could have been affected by
    // precaching.
    UMA_HISTOGRAM_TIMES("Precache.Latency.NonPrefetch.TopHosts", latency);
  } else {
    // The resource was loaded on a page that could NOT have been affected by
    // precaching.
    UMA_HISTOGRAM_TIMES("Precache.Latency.NonPrefetch.NonTopHosts", latency);
  }

  if (!IsDatabaseAccessible()) {
    // Don't track anything if unable to access the database.
    return;
  }

  RecordTimeSinceLastPrecache(fetch_time);

  if (buffered_urls_.find(url.spec()) != buffered_urls_.end()) {
    // If the URL for this fetch is in the write buffer, then flush the write
    // buffer.
    Flush();
  }

  const PrecacheURLInfo url_info = precache_url_table_.GetURLInfo(url);

  if (url_info.was_precached) {
    UMA_HISTOGRAM_ENUMERATION(
        "Precache.CacheStatus.NonPrefetch.FromPrecache",
        info.cache_entry_status,
        net::HttpResponseInfo::CacheEntryStatus::ENTRY_MAX);
  }

  base::HistogramBase::Sample size_sample =
      static_cast<base::HistogramBase::Sample>(size);
  if (!info.was_cached) {
    // The fetch was served over the network during user browsing, so count it
    // as downloaded non-precache bytes.
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedNonPrecache", size_sample);
    if (is_connection_cellular) {
      UMA_HISTOGRAM_COUNTS("Precache.DownloadedNonPrecache.Cellular",
                           size_sample);
    }
    // Since the resource has been fetched during user browsing, mark the URL as
    // used in the precache URL table, if any exists. The current fetch would
    // have put this resource in the cache regardless of whether or not it was
    // previously precached, so mark the URL as used.
    buffered_writes_.push_back(
        base::Bind(&PrecacheURLTable::SetURLAsNotPrecached,
                   base::Unretained(&precache_url_table_), url));
    buffered_urls_.insert(url.spec());
    MaybePostFlush();
  } else if (/* info.was_cached && */ url_info.is_precached &&
             !url_info.was_used) {
    // The fetch was served from the cache, and since there's an entry for this
    // URL in the URL table, this means that the resource was served from the
    // cache only because precaching put it there. Thus, precaching was helpful,
    // so count the fetch as saved bytes.
    UMA_HISTOGRAM_COUNTS("Precache.Saved", size_sample);
    if (is_connection_cellular) {
      UMA_HISTOGRAM_COUNTS("Precache.Saved.Cellular", size_sample);
    }

    DCHECK(info.headers) << "The headers are required to get the freshness.";
    if (info.headers) {
      // TODO(jamartin): Maybe report stale_while_validate as well.
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Precache.Saved.Freshness",
          info.headers->GetFreshnessLifetimes(info.response_time)
              .freshness.InSeconds(),
          base::TimeDelta::FromMinutes(5).InSeconds() /* min */,
          base::TimeDelta::FromDays(356).InSeconds() /* max */,
          100 /* bucket_count */);
    }

    buffered_writes_.push_back(
        base::Bind(&PrecacheURLTable::SetPrecachedURLAsUsed,
                   base::Unretained(&precache_url_table_), url));
    buffered_urls_.insert(url.spec());
    MaybePostFlush();
  }
}

void PrecacheDatabase::UpdatePrecacheReferrerHost(
    const std::string& hostname,
    int64_t manifest_id,
    const base::Time& fetch_time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsDatabaseAccessible()) {
    // Don't track anything if unable to access the database.
    return;
  }

  buffered_writes_.push_back(
      base::Bind(&PrecacheDatabase::UpdatePrecacheReferrerHostInternal,
                 GetWeakPtr(), hostname, manifest_id, fetch_time));
  MaybePostFlush();
}

void PrecacheDatabase::UpdatePrecacheReferrerHostInternal(
    const std::string& hostname,
    int64_t manifest_id,
    const base::Time& fetch_time) {
  int64_t referrer_host_id = precache_referrer_host_table_.UpdateReferrerHost(
      hostname, manifest_id, fetch_time);

  if (referrer_host_id != PrecacheReferrerHostEntry::kInvalidId) {
    precache_url_table_.ClearAllForReferrerHost(referrer_host_id);
  }
}

void PrecacheDatabase::RecordTimeSinceLastPrecache(
    const base::Time& fetch_time) {
  const base::Time& last_precache_timestamp = GetLastPrecacheTimestamp();
  // It could still be null if the DB was not accessible.
  if (!last_precache_timestamp.is_null()) {
    // This is the timespan (in seconds) between the last call to
    // PrecacheManager::StartPrecaching and the fetch time of a non-precache
    // URL. Please note that the session started by that call to
    // PrecacheManager::StartPrecaching may not have precached this particular
    // URL or even any URL for that matter.
    // The range was estimated to have the 95 percentile within the last bounded
    // bucket.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Precache.TimeSinceLastPrecache",
        (fetch_time - last_precache_timestamp).InSeconds(), kSecondsInMinute,
        kSecondsInHour * 36, 100);
  }
}

bool PrecacheDatabase::IsDatabaseAccessible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(db_);

  return db_->is_open();
}

void PrecacheDatabase::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (buffered_writes_.empty()) {
    // Do nothing if there's nothing to flush.
    DCHECK(buffered_urls_.empty());
    return;
  }

  if (IsDatabaseAccessible()) {
    sql::Transaction transaction(db_.get());
    if (transaction.Begin()) {
      for (std::vector<base::Closure>::const_iterator it =
               buffered_writes_.begin();
           it != buffered_writes_.end(); ++it) {
        it->Run();
      }

      transaction.Commit();
    }
  }

  // Clear the buffer, even if the database is inaccessible or unable to begin a
  // transaction.
  buffered_writes_.clear();
  buffered_urls_.clear();
}

void PrecacheDatabase::PostedFlush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_flush_posted_);
  is_flush_posted_ = false;
  Flush();
}

void PrecacheDatabase::MaybePostFlush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffered_writes_.empty() || is_flush_posted_) {
    // There's no point in posting a flush if there's nothing to be flushed or
    // if a flush has already been posted.
    return;
  }

  // Post a delayed task to flush the buffer in 1 second, so that multiple
  // database writes can be buffered up and flushed together in the same
  // transaction.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&PrecacheDatabase::PostedFlush,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
  is_flush_posted_ = true;
}

std::unique_ptr<PrecacheUnfinishedWork>
PrecacheDatabase::GetUnfinishedWork() {
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work =
      precache_session_table_.GetUnfinishedWork();
  precache_session_table_.DeleteUnfinishedWork();
  return unfinished_work;
}

void PrecacheDatabase::SaveUnfinishedWork(
    std::unique_ptr<PrecacheUnfinishedWork> unfinished_work) {
  precache_session_table_.SaveUnfinishedWork(
      std::move(unfinished_work));
}

void PrecacheDatabase::DeleteUnfinishedWork() {
  precache_session_table_.DeleteUnfinishedWork();
}

void PrecacheDatabase::SaveQuota(const PrecacheQuota& quota) {
  precache_session_table_.SaveQuota(quota);
}

PrecacheQuota PrecacheDatabase::GetQuota() {
  return precache_session_table_.GetQuota();
}

base::WeakPtr<PrecacheDatabase> PrecacheDatabase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace precache
