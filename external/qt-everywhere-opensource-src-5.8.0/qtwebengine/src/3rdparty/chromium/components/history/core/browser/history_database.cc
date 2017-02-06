// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_database.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/url_utils.h"
#include "sql/statement.h"
#include "sql/transaction.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

namespace history {

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// our database without *too* many bad effects.
const int kCurrentVersionNumber = 32;
const int kCompatibleVersionNumber = 16;
const char kEarlyExpirationThresholdKey[] = "early_expiration_threshold";
const int kMaxHostsInMemory = 10000;

}  // namespace

HistoryDatabase::HistoryDatabase(
    DownloadInterruptReason download_interrupt_reason_none,
    DownloadInterruptReason download_interrupt_reason_crash)
    : DownloadDatabase(download_interrupt_reason_none,
                       download_interrupt_reason_crash) {
}

HistoryDatabase::~HistoryDatabase() {
}

sql::InitStatus HistoryDatabase::Init(const base::FilePath& history_name) {
  db_.set_histogram_tag("History");

  // Set the exceptional sqlite error handler.
  db_.set_error_callback(error_callback_);

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // Set the cache size. The page size, plus a little extra, times this
  // value, tells us how much memory the cache will use maximum.
  // 1000 * 4kB = 4MB
  // TODO(brettw) scale this value to the amount of available memory.
  db_.set_cache_size(1000);

  // Note that we don't set exclusive locking here. That's done by
  // BeginExclusiveMode below which is called later (we have to be in shared
  // mode to start out for the in-memory backend to read the data).

  if (!db_.Open(history_name))
    return sql::INIT_FAILURE;

  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return sql::INIT_FAILURE;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Exclude the history file from backups.
  base::mac::SetFileBackupExclusion(history_name);
#endif

  // Prime the cache.
  db_.Preload();

  // Create the tables and indices.
  // NOTE: If you add something here, also add it to
  //       RecreateAllButStarAndURLTables.
  if (!meta_table_.Init(&db_, GetCurrentVersion(), kCompatibleVersionNumber))
    return sql::INIT_FAILURE;
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable() || !InitDownloadTable() ||
      !InitSegmentTables())
    return sql::INIT_FAILURE;
  CreateMainURLIndex();
  CreateKeywordSearchTermsIndices();

  // TODO(benjhayden) Remove at some point.
  meta_table_.DeleteKey("next_download_id");

  // Version check.
  sql::InitStatus version_status = EnsureCurrentVersion();
  if (version_status != sql::INIT_OK)
    return version_status;

  return committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

void HistoryDatabase::ComputeDatabaseMetrics(
    const base::FilePath& history_name) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  int64_t file_size = 0;
  if (!base::GetFileSize(history_name, &file_size))
    return;
  int file_mb = static_cast<int>(file_size / (1024 * 1024));
  UMA_HISTOGRAM_MEMORY_MB("History.DatabaseFileMB", file_mb);

  sql::Statement url_count(db_.GetUniqueStatement("SELECT count(*) FROM urls"));
  if (!url_count.Step())
    return;
  UMA_HISTOGRAM_COUNTS("History.URLTableCount", url_count.ColumnInt(0));

  sql::Statement visit_count(db_.GetUniqueStatement(
      "SELECT count(*) FROM visits"));
  if (!visit_count.Step())
    return;
  UMA_HISTOGRAM_COUNTS("History.VisitTableCount", visit_count.ColumnInt(0));

  base::Time one_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);
  sql::Statement weekly_visit_sql(db_.GetUniqueStatement(
      "SELECT count(*) FROM visits WHERE visit_time > ?"));
  weekly_visit_sql.BindInt64(0, one_week_ago.ToInternalValue());
  int weekly_visit_count = 0;
  if (weekly_visit_sql.Step())
    weekly_visit_count = weekly_visit_sql.ColumnInt(0);
  UMA_HISTOGRAM_COUNTS("History.WeeklyVisitCount", weekly_visit_count);

  base::Time one_month_ago = base::Time::Now() - base::TimeDelta::FromDays(30);
  sql::Statement monthly_visit_sql(db_.GetUniqueStatement(
      "SELECT count(*) FROM visits WHERE visit_time > ? AND visit_time <= ?"));
  monthly_visit_sql.BindInt64(0, one_month_ago.ToInternalValue());
  monthly_visit_sql.BindInt64(1, one_week_ago.ToInternalValue());
  int older_visit_count = 0;
  if (monthly_visit_sql.Step())
    older_visit_count = monthly_visit_sql.ColumnInt(0);
  UMA_HISTOGRAM_COUNTS("History.MonthlyVisitCount",
                       older_visit_count + weekly_visit_count);

  UMA_HISTOGRAM_TIMES("History.DatabaseBasicMetricsTime",
                      base::TimeTicks::Now() - start_time);

  // Compute the advanced metrics even less often, pending timing data showing
  // that's not necessary.
  if (base::RandInt(1, 3) == 3) {
    start_time = base::TimeTicks::Now();

    // Collect all URLs visited within the last month.
    sql::Statement url_sql(db_.GetUniqueStatement(
        "SELECT url, last_visit_time FROM urls WHERE last_visit_time > ?"));
    url_sql.BindInt64(0, one_month_ago.ToInternalValue());

    // Count URLs (which will always be unique) and unique hosts within the last
    // week and last month.
    int week_url_count = 0;
    int month_url_count = 0;
    std::set<std::string> week_hosts;
    std::set<std::string> month_hosts;
    while (url_sql.Step()) {
      GURL url(url_sql.ColumnString(0));
      base::Time visit_time =
          base::Time::FromInternalValue(url_sql.ColumnInt64(1));
      ++month_url_count;
      month_hosts.insert(url.host());
      if (visit_time > one_week_ago) {
        ++week_url_count;
        week_hosts.insert(url.host());
      }
    }
    UMA_HISTOGRAM_COUNTS("History.WeeklyURLCount", week_url_count);
    UMA_HISTOGRAM_COUNTS_10000("History.WeeklyHostCount",
                               static_cast<int>(week_hosts.size()));
    UMA_HISTOGRAM_COUNTS("History.MonthlyURLCount", month_url_count);
    UMA_HISTOGRAM_COUNTS_10000("History.MonthlyHostCount",
                               static_cast<int>(month_hosts.size()));
    UMA_HISTOGRAM_TIMES("History.DatabaseAdvancedMetricsTime",
                        base::TimeTicks::Now() - start_time);
  }
}

TopHostsList HistoryDatabase::TopHosts(size_t num_hosts) {
  base::Time one_month_ago =
      std::max(base::Time::Now() - base::TimeDelta::FromDays(30), base::Time());

  sql::Statement url_sql(db_.GetUniqueStatement(
      "SELECT url, visit_count FROM urls WHERE last_visit_time > ?"));
  url_sql.BindInt64(0, one_month_ago.ToInternalValue());

  // Collect a map from host to visit count.
  base::hash_map<std::string, int> host_count;
  while (url_sql.Step()) {
    GURL url(url_sql.ColumnString(0));
    if (!(url.is_valid() && (url.SchemeIsHTTPOrHTTPS() || url.SchemeIs("ftp"))))
      continue;

    int64_t visit_count = url_sql.ColumnInt64(1);
    host_count[HostForTopHosts(url)] += visit_count;

    // kMaxHostsInMemory is well above typical values for
    // History.MonthlyHostCount, but here to guard against unbounded memory
    // growth in the event of an atypical history.
    if (host_count.size() >= kMaxHostsInMemory)
      break;
  }

  // Collect the top 100 hosts by visit count, into the range
  // [top_hosts.begin(), middle).
  typedef std::vector<std::pair<int, std::string>> IntermediateList;
  IntermediateList top_hosts;
  for (const auto& it : host_count)
    top_hosts.push_back(std::make_pair(-it.second, it.first));
  IntermediateList::size_type middle_index =
      std::min(num_hosts, top_hosts.size());
  auto middle = std::min(top_hosts.end(), top_hosts.begin() + middle_index);
  std::partial_sort(top_hosts.begin(), middle, top_hosts.end());

  TopHostsList hosts;
  for (IntermediateList::const_iterator it = top_hosts.begin(); it != middle;
       ++it)
    hosts.push_back(std::make_pair(it->second, -it->first));
  return hosts;
}

void HistoryDatabase::BeginExclusiveMode() {
  // We can't use set_exclusive_locking() since that only has an effect before
  // the DB is opened.
  ignore_result(db_.Execute("PRAGMA locking_mode=EXCLUSIVE"));
}

// static
int HistoryDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

void HistoryDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void HistoryDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void HistoryDatabase::RollbackTransaction() {
  db_.RollbackTransaction();
}

bool HistoryDatabase::RecreateAllTablesButURL() {
  if (!DropVisitTable())
    return false;
  if (!InitVisitTable())
    return false;

  if (!DropKeywordSearchTermsTable())
    return false;
  if (!InitKeywordSearchTermsTable())
    return false;

  if (!DropSegmentTables())
    return false;
  if (!InitSegmentTables())
    return false;

  CreateKeywordSearchTermsIndices();
  return true;
}

void HistoryDatabase::Vacuum() {
  DCHECK_EQ(0, db_.transaction_nesting()) <<
      "Can not have a transaction when vacuuming.";
  ignore_result(db_.Execute("VACUUM"));
}

void HistoryDatabase::TrimMemory(bool aggressively) {
  db_.TrimMemory(aggressively);
}

bool HistoryDatabase::Raze() {
  return db_.Raze();
}

bool HistoryDatabase::SetSegmentID(VisitID visit_id, SegmentID segment_id) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE visits SET segment_id = ? WHERE id = ?"));
  s.BindInt64(0, segment_id);
  s.BindInt64(1, visit_id);
  DCHECK(db_.GetLastChangeCount() == 1);

  return s.Run();
}

SegmentID HistoryDatabase::GetSegmentID(VisitID visit_id) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT segment_id FROM visits WHERE id = ?"));
  s.BindInt64(0, visit_id);

  if (s.Step()) {
    if (s.ColumnType(0) == sql::COLUMN_TYPE_NULL)
      return 0;
    else
      return s.ColumnInt64(0);
  }
  return 0;
}

base::Time HistoryDatabase::GetEarlyExpirationThreshold() {
  if (!cached_early_expiration_threshold_.is_null())
    return cached_early_expiration_threshold_;

  int64_t threshold;
  if (!meta_table_.GetValue(kEarlyExpirationThresholdKey, &threshold)) {
    // Set to a very early non-zero time, so it's before all history, but not
    // zero to avoid re-retrieval.
    threshold = 1L;
  }

  cached_early_expiration_threshold_ = base::Time::FromInternalValue(threshold);
  return cached_early_expiration_threshold_;
}

void HistoryDatabase::UpdateEarlyExpirationThreshold(base::Time threshold) {
  meta_table_.SetValue(kEarlyExpirationThresholdKey,
                       threshold.ToInternalValue());
  cached_early_expiration_threshold_ = threshold;
}

sql::Connection& HistoryDatabase::GetDB() {
  return db_;
}

// Migration -------------------------------------------------------------------

sql::InitStatus HistoryDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "History database is too new.";
    return sql::INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();

  // Put migration code here

  if (cur_version == 15) {
    if (!db_.Execute("DROP TABLE starred") || !DropStarredIDFromURLs()) {
      LOG(WARNING) << "Unable to update history database to version 16.";
      return sql::INIT_FAILURE;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  if (cur_version == 16) {
#if !defined(OS_WIN)
    // In this version we bring the time format on Mac & Linux in sync with the
    // Windows version so that profiles can be moved between computers.
    MigrateTimeEpoch();
#endif
    // On all platforms we bump the version number, so on Windows this
    // migration is a NOP. We keep the compatible version at 16 since things
    // will basically still work, just history will be in the future if an
    // old version reads it.
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 17) {
    // Version 17 was for thumbnails to top sites migration. We ended up
    // disabling it though, so 17->18 does nothing.
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 18) {
    // This is the version prior to adding url_source column. We need to
    // migrate the database.
    cur_version = 19;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 19) {
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
    // This was the thumbnail migration.  Obsolete.
  }

  if (cur_version == 20) {
    // This is the version prior to adding the visit_duration field in visits
    // database. We need to migrate the database.
    if (!MigrateVisitsWithoutDuration()) {
      LOG(WARNING) << "Unable to update history database to version 21.";
      return sql::INIT_FAILURE;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 21) {
    // The android_urls table's data schemal was changed in version 21.
#if defined(OS_ANDROID)
    if (!MigrateToVersion22()) {
      LOG(WARNING) << "Unable to migrate the android_urls table to version 22";
    }
#endif
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 22) {
    if (!MigrateDownloadsState()) {
      LOG(WARNING) << "Unable to fix invalid downloads state values";
      // Invalid state values may cause crashes.
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 23) {
    if (!MigrateDownloadsReasonPathsAndDangerType()) {
      LOG(WARNING) << "Unable to upgrade download interrupt reason and paths";
      // Invalid state values may cause crashes.
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 24) {
    if (!MigratePresentationIndex()) {
      LOG(WARNING) << "Unable to migrate history to version 25";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 25) {
    if (!MigrateReferrer()) {
      LOG(WARNING) << "Unable to migrate history to version 26";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 26) {
    if (!MigrateDownloadedByExtension()) {
      LOG(WARNING) << "Unable to migrate history to version 27";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 27) {
    if (!MigrateDownloadValidators()) {
      LOG(WARNING) << "Unable to migrate history to version 28";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 28) {
    if (!MigrateMimeType()) {
      LOG(WARNING) << "Unable to migrate history to version 29";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 29) {
    if (!MigrateHashHttpMethodAndGenerateGuids()) {
      LOG(WARNING) << "Unable to migrate history to version 30";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 30) {
    if (!MigrateDownloadTabUrl()) {
      LOG(WARNING) << "Unable to migrate history to version 31";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 31) {
    if (!MigrateDownloadSiteInstanceUrl()) {
      LOG(WARNING) << "Unable to migrate history to version 32";
      return sql::INIT_FAILURE;
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < GetCurrentVersion()) <<
         "History database version " << cur_version << " is too old to handle.";

  return sql::INIT_OK;
}

#if !defined(OS_WIN)
void HistoryDatabase::MigrateTimeEpoch() {
  // Update all the times in the URLs and visits table in the main database.
  ignore_result(db_.Execute(
      "UPDATE urls "
      "SET last_visit_time = last_visit_time + 11644473600000000 "
      "WHERE id IN (SELECT id FROM urls WHERE last_visit_time > 0);"));
  ignore_result(db_.Execute(
      "UPDATE visits "
      "SET visit_time = visit_time + 11644473600000000 "
      "WHERE id IN (SELECT id FROM visits WHERE visit_time > 0);"));
  ignore_result(db_.Execute(
      "UPDATE segment_usage "
      "SET time_slot = time_slot + 11644473600000000 "
      "WHERE id IN (SELECT id FROM segment_usage WHERE time_slot > 0);"));
}
#endif

}  // namespace history
