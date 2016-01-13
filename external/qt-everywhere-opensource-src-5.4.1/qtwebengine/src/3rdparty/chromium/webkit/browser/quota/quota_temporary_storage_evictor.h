// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
#define WEBKIT_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/quota/quota_types.h"

class GURL;

namespace content {
class QuotaTemporaryStorageEvictorTest;
}

namespace quota {

class QuotaEvictionHandler;
struct UsageAndQuota;

class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE QuotaTemporaryStorageEvictor
    : public base::NonThreadSafe {
 public:
  struct Statistics {
    Statistics()
        : num_errors_on_evicting_origin(0),
          num_errors_on_getting_usage_and_quota(0),
          num_evicted_origins(0),
          num_eviction_rounds(0),
          num_skipped_eviction_rounds(0) {}
    int64 num_errors_on_evicting_origin;
    int64 num_errors_on_getting_usage_and_quota;
    int64 num_evicted_origins;
    int64 num_eviction_rounds;
    int64 num_skipped_eviction_rounds;

    void subtract_assign(const Statistics& rhs) {
      num_errors_on_evicting_origin -= rhs.num_errors_on_evicting_origin;
      num_errors_on_getting_usage_and_quota -=
          rhs.num_errors_on_getting_usage_and_quota;
      num_evicted_origins -= rhs.num_evicted_origins;
      num_eviction_rounds -= rhs.num_eviction_rounds;
      num_skipped_eviction_rounds -= rhs.num_skipped_eviction_rounds;
    }
  };

  struct EvictionRoundStatistics {
    EvictionRoundStatistics();

    bool in_round;
    bool is_initialized;

    base::Time start_time;
    int64 usage_overage_at_round;
    int64 diskspace_shortage_at_round;

    int64 usage_on_beginning_of_round;
    int64 usage_on_end_of_round;
    int64 num_evicted_origins_in_round;
  };

  QuotaTemporaryStorageEvictor(
      QuotaEvictionHandler* quota_eviction_handler,
      int64 interval_ms);
  virtual ~QuotaTemporaryStorageEvictor();

  void GetStatistics(std::map<std::string, int64>* statistics);
  void ReportPerRoundHistogram();
  void ReportPerHourHistogram();
  void Start();

  int64 min_available_disk_space_to_start_eviction() {
    return min_available_disk_space_to_start_eviction_;
  }
  void reset_min_available_disk_space_to_start_eviction() {
    min_available_disk_space_to_start_eviction_ =
        kMinAvailableDiskSpaceToStartEvictionNotSpecified;
  }
  void set_min_available_disk_space_to_start_eviction(int64 value) {
    min_available_disk_space_to_start_eviction_ = value;
  }

 private:
  friend class content::QuotaTemporaryStorageEvictorTest;

  void StartEvictionTimerWithDelay(int delay_ms);
  void ConsiderEviction();
  void OnGotUsageAndQuotaForEviction(
      QuotaStatusCode status,
      const UsageAndQuota& quota_and_usage);
  void OnGotLRUOrigin(const GURL& origin);
  void OnEvictionComplete(QuotaStatusCode status);

  void OnEvictionRoundStarted();
  void OnEvictionRoundFinished();

  // This is only used for tests.
  void set_repeated_eviction(bool repeated_eviction) {
    repeated_eviction_ = repeated_eviction;
  }

  static const int kMinAvailableDiskSpaceToStartEvictionNotSpecified;

  int64 min_available_disk_space_to_start_eviction_;

  // Not owned; quota_eviction_handler owns us.
  QuotaEvictionHandler* quota_eviction_handler_;

  Statistics statistics_;
  Statistics previous_statistics_;
  EvictionRoundStatistics round_statistics_;
  base::Time time_of_end_of_last_nonskipped_round_;
  base::Time time_of_end_of_last_round_;

  int64 interval_ms_;
  bool repeated_eviction_;

  base::OneShotTimer<QuotaTemporaryStorageEvictor> eviction_timer_;
  base::RepeatingTimer<QuotaTemporaryStorageEvictor> histogram_timer_;
  base::WeakPtrFactory<QuotaTemporaryStorageEvictor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaTemporaryStorageEvictor);
};

}  // namespace quota

#endif  // WEBKIT_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
