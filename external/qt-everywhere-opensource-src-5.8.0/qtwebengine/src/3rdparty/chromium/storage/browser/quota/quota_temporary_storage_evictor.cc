// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_temporary_storage_evictor.h"

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/gurl.h"

#define UMA_HISTOGRAM_MBYTES(name, sample)          \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                      \
      (name), static_cast<int>((sample) / kMBytes), \
      1, 10 * 1024 * 1024 /* 10TB */, 100)

#define UMA_HISTOGRAM_MINUTES(name, sample) \
  UMA_HISTOGRAM_CUSTOM_TIMES(             \
      (name), (sample),                   \
      base::TimeDelta::FromMinutes(1),    \
      base::TimeDelta::FromDays(1), 50)

namespace {
const int64_t kMBytes = 1024 * 1024;
const double kUsageRatioToStartEviction = 0.7;
const int kThresholdOfErrorsToStopEviction = 5;
const int kHistogramReportIntervalMinutes = 60;
const double kMustRemainAvailableRatio = 0.1;
const int64_t kDefaultMustRemainAvailableSpace = 1024 * kMBytes;
const double kDiskSpaceShortageAllowanceRatio = 0.5;
}

namespace storage {

const int QuotaTemporaryStorageEvictor::
    kMinAvailableToStartEvictionNotSpecified = -1;

QuotaTemporaryStorageEvictor::EvictionRoundStatistics::EvictionRoundStatistics()
    : in_round(false),
      is_initialized(false),
      usage_overage_at_round(-1),
      diskspace_shortage_at_round(-1),
      usage_on_beginning_of_round(-1),
      usage_on_end_of_round(-1),
      num_evicted_origins_in_round(0) {
}

QuotaTemporaryStorageEvictor::QuotaTemporaryStorageEvictor(
    QuotaEvictionHandler* quota_eviction_handler,
    int64_t interval_ms)
    : min_available_to_start_eviction_(
          kMinAvailableToStartEvictionNotSpecified),
      quota_eviction_handler_(quota_eviction_handler),
      interval_ms_(interval_ms),
      repeated_eviction_(true),
      weak_factory_(this) {
  DCHECK(quota_eviction_handler);
}

QuotaTemporaryStorageEvictor::~QuotaTemporaryStorageEvictor() {
}

void QuotaTemporaryStorageEvictor::GetStatistics(
    std::map<std::string, int64_t>* statistics) {
  DCHECK(statistics);

  (*statistics)["errors-on-evicting-origin"] =
      statistics_.num_errors_on_evicting_origin;
  (*statistics)["errors-on-getting-usage-and-quota"] =
      statistics_.num_errors_on_getting_usage_and_quota;
  (*statistics)["evicted-origins"] =
      statistics_.num_evicted_origins;
  (*statistics)["eviction-rounds"] =
      statistics_.num_eviction_rounds;
  (*statistics)["skipped-eviction-rounds"] =
      statistics_.num_skipped_eviction_rounds;
}

void QuotaTemporaryStorageEvictor::ReportPerRoundHistogram() {
  DCHECK(round_statistics_.in_round);
  DCHECK(round_statistics_.is_initialized);

  base::Time now = base::Time::Now();
  UMA_HISTOGRAM_TIMES("Quota.TimeSpentToAEvictionRound",
                      now - round_statistics_.start_time);
  if (!time_of_end_of_last_round_.is_null())
    UMA_HISTOGRAM_MINUTES("Quota.TimeDeltaOfEvictionRounds",
                          now - time_of_end_of_last_round_);
  UMA_HISTOGRAM_MBYTES("Quota.UsageOverageOfTemporaryGlobalStorage",
                       round_statistics_.usage_overage_at_round);
  UMA_HISTOGRAM_MBYTES("Quota.DiskspaceShortage",
                       round_statistics_.diskspace_shortage_at_round);
  UMA_HISTOGRAM_MBYTES("Quota.EvictedBytesPerRound",
                       round_statistics_.usage_on_beginning_of_round -
                       round_statistics_.usage_on_end_of_round);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfEvictedOriginsPerRound",
                       round_statistics_.num_evicted_origins_in_round);
}

void QuotaTemporaryStorageEvictor::ReportPerHourHistogram() {
  Statistics stats_in_hour(statistics_);
  stats_in_hour.subtract_assign(previous_statistics_);
  previous_statistics_ = statistics_;

  UMA_HISTOGRAM_COUNTS("Quota.ErrorsOnEvictingOriginPerHour",
                       stats_in_hour.num_errors_on_evicting_origin);
  UMA_HISTOGRAM_COUNTS("Quota.ErrorsOnGettingUsageAndQuotaPerHour",
                       stats_in_hour.num_errors_on_getting_usage_and_quota);
  UMA_HISTOGRAM_COUNTS("Quota.EvictedOriginsPerHour",
                       stats_in_hour.num_evicted_origins);
  UMA_HISTOGRAM_COUNTS("Quota.EvictionRoundsPerHour",
                       stats_in_hour.num_eviction_rounds);
  UMA_HISTOGRAM_COUNTS("Quota.SkippedEvictionRoundsPerHour",
                       stats_in_hour.num_skipped_eviction_rounds);
}

void QuotaTemporaryStorageEvictor::OnEvictionRoundStarted() {
  if (round_statistics_.in_round)
    return;
  round_statistics_.in_round = true;
  round_statistics_.start_time = base::Time::Now();
  ++statistics_.num_eviction_rounds;
}

void QuotaTemporaryStorageEvictor::OnEvictionRoundFinished() {
  in_progress_eviction_origins_.clear();

  // Check if skipped round
  if (round_statistics_.num_evicted_origins_in_round) {
    ReportPerRoundHistogram();
    time_of_end_of_last_nonskipped_round_ = base::Time::Now();
  } else {
    ++statistics_.num_skipped_eviction_rounds;
  }
  // Reset stats for next round.
  round_statistics_ = EvictionRoundStatistics();
}

void QuotaTemporaryStorageEvictor::Start() {
  DCHECK(CalledOnValidThread());
  StartEvictionTimerWithDelay(0);

  if (histogram_timer_.IsRunning())
    return;

  histogram_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kHistogramReportIntervalMinutes),
      this, &QuotaTemporaryStorageEvictor::ReportPerHourHistogram);
}

void QuotaTemporaryStorageEvictor::StartEvictionTimerWithDelay(int delay_ms) {
  if (eviction_timer_.IsRunning())
    return;
  eviction_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms),
                        this, &QuotaTemporaryStorageEvictor::ConsiderEviction);
}

void QuotaTemporaryStorageEvictor::ConsiderEviction() {
  OnEvictionRoundStarted();

  if (min_available_to_start_eviction_ ==
      kMinAvailableToStartEvictionNotSpecified) {
    quota_eviction_handler_->AsyncGetVolumeInfo(
        base::Bind(&QuotaTemporaryStorageEvictor::OnGotVolumeInfo,
                   weak_factory_.GetWeakPtr()));
  } else {
    quota_eviction_handler_->GetUsageAndQuotaForEviction(
        base::Bind(&QuotaTemporaryStorageEvictor::OnGotUsageAndQuotaForEviction,
                   weak_factory_.GetWeakPtr(),
                   min_available_to_start_eviction_));
  }
}

void QuotaTemporaryStorageEvictor::OnGotVolumeInfo(
    bool success, uint64_t available_space, uint64_t total_size) {
  // Compute how much to keep free as a function of total disk size.
  int64_t must_remain_available_space = success ?
      static_cast<int64_t>(total_size * kMustRemainAvailableRatio) :
      kDefaultMustRemainAvailableSpace;

  quota_eviction_handler_->GetUsageAndQuotaForEviction(
      base::Bind(&QuotaTemporaryStorageEvictor::OnGotUsageAndQuotaForEviction,
                 weak_factory_.GetWeakPtr(), must_remain_available_space));
}

void QuotaTemporaryStorageEvictor::OnGotUsageAndQuotaForEviction(
    int64_t must_remain_available_space,
    QuotaStatusCode status,
    const UsageAndQuota& qau) {
  DCHECK(CalledOnValidThread());

  int64_t usage = qau.global_limited_usage;
  DCHECK_GE(usage, 0);

  if (status != kQuotaStatusOk)
    ++statistics_.num_errors_on_getting_usage_and_quota;

  int64_t usage_overage = std::max(
      static_cast<int64_t>(0),
      usage - static_cast<int64_t>(qau.quota * kUsageRatioToStartEviction));

  int64_t diskspace_shortage = std::max(
      static_cast<int64_t>(0),
      must_remain_available_space - qau.available_disk_space);

  // If we're using so little that freeing all of it wouldn't help,
  // don't let the low space condition cause us to delete it all.
  if (usage < static_cast<int64_t>(diskspace_shortage *
                                   kDiskSpaceShortageAllowanceRatio)) {
    diskspace_shortage = 0;
  }

  if (!round_statistics_.is_initialized) {
    round_statistics_.usage_overage_at_round = usage_overage;
    round_statistics_.diskspace_shortage_at_round = diskspace_shortage;
    round_statistics_.usage_on_beginning_of_round = usage;
    round_statistics_.is_initialized = true;
  }
  round_statistics_.usage_on_end_of_round = usage;

  int64_t amount_to_evict = std::max(usage_overage, diskspace_shortage);
  if (status == kQuotaStatusOk && amount_to_evict > 0) {
    // Space is getting tight. Get the least recently used origin and continue.
    // TODO(michaeln): if the reason for eviction is low physical disk space,
    // make 'unlimited' origins subject to eviction too.
    quota_eviction_handler_->GetEvictionOrigin(
        kStorageTypeTemporary, in_progress_eviction_origins_, qau.quota,
        base::Bind(&QuotaTemporaryStorageEvictor::OnGotEvictionOrigin,
                   weak_factory_.GetWeakPtr()));
  } else {
    if (repeated_eviction_) {
      // No action required, sleep for a while and check again later.
      if (statistics_.num_errors_on_getting_usage_and_quota <
          kThresholdOfErrorsToStopEviction) {
        StartEvictionTimerWithDelay(interval_ms_);
      } else {
        // TODO(dmikurube): Try restarting eviction after a while.
        LOG(WARNING) << "Stopped eviction of temporary storage due to errors "
                        "in GetUsageAndQuotaForEviction.";
      }
    }
    OnEvictionRoundFinished();
  }

  // TODO(dmikurube): Add error handling for the case status != kQuotaStatusOk.
}

void QuotaTemporaryStorageEvictor::OnGotEvictionOrigin(const GURL& origin) {
  DCHECK(CalledOnValidThread());

  if (origin.is_empty()) {
    if (repeated_eviction_)
      StartEvictionTimerWithDelay(interval_ms_);
    OnEvictionRoundFinished();
    return;
  }

  in_progress_eviction_origins_.insert(origin);

  quota_eviction_handler_->EvictOriginData(origin, kStorageTypeTemporary,
      base::Bind(
          &QuotaTemporaryStorageEvictor::OnEvictionComplete,
          weak_factory_.GetWeakPtr()));
}

void QuotaTemporaryStorageEvictor::OnEvictionComplete(
    QuotaStatusCode status) {
  DCHECK(CalledOnValidThread());

  // Just calling ConsiderEviction() or StartEvictionTimerWithDelay() here is
  // ok.  No need to deal with the case that all of the Delete operations fail
  // for a certain origin.  It doesn't result in trying to evict the same
  // origin permanently.  The evictor skips origins which had deletion errors
  // a few times.

  if (status == kQuotaStatusOk) {
    ++statistics_.num_evicted_origins;
    ++round_statistics_.num_evicted_origins_in_round;
    // We many need to get rid of more space so reconsider immediately.
    ConsiderEviction();
  } else {
    ++statistics_.num_errors_on_evicting_origin;
    if (repeated_eviction_) {
      // Sleep for a while and retry again until we see too many errors.
      StartEvictionTimerWithDelay(interval_ms_);
    }
    OnEvictionRoundFinished();
  }
}

}  // namespace storage
