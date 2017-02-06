// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_database.h"

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
  Flush();
}

void PrecacheDatabase::ClearHistory() {
  if (!IsDatabaseAccessible()) {
    // Do nothing if unable to access the database.
    return;
  }

  buffered_writes_.push_back(base::Bind(
      &PrecacheURLTable::DeleteAll, base::Unretained(&precache_url_table_)));
  Flush();
}

void PrecacheDatabase::RecordURLPrefetch(const GURL& url,
                                         const base::TimeDelta& latency,
                                         const base::Time& fetch_time,
                                         const net::HttpResponseInfo& info,
                                         int64_t size) {
  UMA_HISTOGRAM_TIMES("Precache.Latency.Prefetch", latency);

  if (!IsDatabaseAccessible()) {
    // Don't track anything if unable to access the database.
    return;
  }

  if (buffered_urls_.find(url.spec()) != buffered_urls_.end()) {
    // If the URL for this fetch is in the write buffer, then flush the write
    // buffer.
    Flush();
  }

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

  if (info.was_cached && !precache_url_table_.HasURL(url)) {
    // Since the precache came from the cache, and there's no entry in the URL
    // table for the URL, this means that the resource was already in the cache
    // because of user browsing. Therefore, this precache won't be considered as
    // precache-motivated since it had no significant effect (besides a possible
    // revalidation and a change in the cache LRU priority).
    return;
  }

  if (!info.was_cached) {
    // The precache only counts as overhead if it was downloaded over the
    // network.
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedPrecacheMotivated",
                         static_cast<base::HistogramBase::Sample>(size));
  }

  // Use the URL table to keep track of URLs that are in the cache thanks to
  // precaching. If a row for the URL already exists, than update the timestamp
  // to |fetch_time|.
  buffered_writes_.push_back(
      base::Bind(&PrecacheURLTable::AddURL,
                 base::Unretained(&precache_url_table_), url, fetch_time));
  buffered_urls_.insert(url.spec());
  MaybePostFlush();
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

  if (buffered_urls_.find(url.spec()) != buffered_urls_.end()) {
    // If the URL for this fetch is in the write buffer, then flush the write
    // buffer.
    Flush();
  }

  if (info.was_cached && !precache_url_table_.HasURL(url)) {
    // Ignore cache hits that precache can't take credit for.
    return;
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
  } else {  // info.was_cached.
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
  }

  // Since the resource has been fetched during user browsing, remove any record
  // of that URL having been precached from the URL table, if any exists.
  // The current fetch would have put this resource in the cache regardless of
  // whether or not it was previously precached, so delete any record of that
  // URL having been precached from the URL table.
  buffered_writes_.push_back(
      base::Bind(&PrecacheURLTable::DeleteURL,
                 base::Unretained(&precache_url_table_), url));
  buffered_urls_.insert(url.spec());
  MaybePostFlush();
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

base::WeakPtr<PrecacheDatabase> PrecacheDatabase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace precache
