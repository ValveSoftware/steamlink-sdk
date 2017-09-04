// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_

#include <memory>

#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

namespace browsing_data {

class HistoryCounter : public browsing_data::BrowsingDataCounter,
                       public syncer::SyncServiceObserver {
 public:
  typedef base::Callback<history::WebHistoryService*()>
      GetUpdatedWebHistoryServiceCallback;

  class HistoryResult : public FinishedResult {
   public:
    HistoryResult(const HistoryCounter* source,
                  ResultInt value,
                  bool has_synced_visits);
    ~HistoryResult() override;

    bool has_synced_visits() const { return has_synced_visits_; }

   private:
    bool has_synced_visits_;
  };

  explicit HistoryCounter(history::HistoryService* history_service,
                          const GetUpdatedWebHistoryServiceCallback& callback,
                          syncer::SyncService* sync_service);
  ~HistoryCounter() override;

  void OnInitialized() override;

  // Whether there are counting tasks in progress. Only used for testing.
  bool HasTrackedTasks();

  const char* GetPrefName() const override;

 private:
  void Count() override;

  void OnGetLocalHistoryCount(history::HistoryCountResult result);
  void OnGetWebHistoryCount(history::WebHistoryService::Request* request,
                            const base::DictionaryValue* result);
  void OnWebHistoryTimeout();
  void MergeResults();

  // SyncServiceObserver implementation.
  void OnStateChanged() override;

  history::HistoryService* history_service_;

  GetUpdatedWebHistoryServiceCallback web_history_service_callback_;

  syncer::SyncService* sync_service_;

  bool has_synced_visits_;

  bool local_counting_finished_;
  bool web_counting_finished_;

  base::CancelableTaskTracker cancelable_task_tracker_;
  std::unique_ptr<history::WebHistoryService::Request> web_history_request_;
  base::OneShotTimer web_history_timeout_;

  base::ThreadChecker thread_checker_;

  BrowsingDataCounter::ResultInt local_result_;

  bool history_sync_enabled_;

  base::WeakPtrFactory<HistoryCounter> weak_ptr_factory_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
