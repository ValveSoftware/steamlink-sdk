// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/history_counter.h"

#include <limits.h>
#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"
#include "components/browsing_data/core/pref_names.h"

namespace {
static const int64_t kWebHistoryTimeoutSeconds = 10;
}

namespace browsing_data {

HistoryCounter::HistoryCounter(
    history::HistoryService* history_service,
    const GetUpdatedWebHistoryServiceCallback& callback,
    syncer::SyncService* sync_service)
    : history_service_(history_service),
      web_history_service_callback_(callback),
      sync_service_(sync_service),
      has_synced_visits_(false),
      local_counting_finished_(false),
      web_counting_finished_(false),
      history_sync_enabled_(false),
      weak_ptr_factory_(this) {}

HistoryCounter::~HistoryCounter() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

void HistoryCounter::OnInitialized() {
  if (sync_service_)
    sync_service_->AddObserver(this);
  history_sync_enabled_ = !!web_history_service_callback_.Run();
}

bool HistoryCounter::HasTrackedTasks() {
  return cancelable_task_tracker_.HasTrackedTasks();
}

const char* HistoryCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteBrowsingHistory;
}

void HistoryCounter::Count() {
  // Reset the state.
  cancelable_task_tracker_.TryCancelAll();
  web_history_request_.reset();
  has_synced_visits_ = false;

  // Count the locally stored items.
  local_counting_finished_ = false;

  history_service_->GetHistoryCount(
      GetPeriodStart(), base::Time::Max(),
      base::Bind(&HistoryCounter::OnGetLocalHistoryCount,
                 weak_ptr_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);

  // If the history sync is enabled, test if there is at least one synced item.
  history::WebHistoryService* web_history =
      web_history_service_callback_.Run();

  if (!web_history) {
    web_counting_finished_ = true;
    return;
  }

  web_counting_finished_ = false;

  web_history_timeout_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kWebHistoryTimeoutSeconds), this,
      &HistoryCounter::OnWebHistoryTimeout);

  history::QueryOptions options;
  options.max_count = 1;
  options.begin_time = GetPeriodStart();
  options.end_time = base::Time::Max();
  web_history_request_ = web_history->QueryHistory(
      base::string16(), options,
      base::Bind(&HistoryCounter::OnGetWebHistoryCount,
                 weak_ptr_factory_.GetWeakPtr()));

  // TODO(msramek): Include web history count when there is an API for it.
}

void HistoryCounter::OnGetLocalHistoryCount(
    history::HistoryCountResult result) {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for |MergeResults|.
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!result.success) {
    LOG(ERROR) << "Failed to count the local history.";
    return;
  }

  local_result_ = result.count;
  local_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::OnGetWebHistoryCount(
    history::WebHistoryService::Request* request,
    const base::DictionaryValue* result) {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for |MergeResults|.
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the timeout for this request already fired, ignore the result.
  if (!web_history_timeout_.IsRunning())
    return;

  web_history_timeout_.Stop();

  // If the query failed, err on the safe side and inform the user that they
  // may have history items stored in Sync. Otherwise, we expect at least one
  // entry in the "event" list.
  const base::ListValue* events;
  has_synced_visits_ =
      !result || (result->GetList("event", &events) && !events->empty());
  web_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::OnWebHistoryTimeout() {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for |MergeResults|.
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the query timed out, err on the safe side and inform the user that they
  // may have history items stored in Sync.
  web_history_request_.reset();
  has_synced_visits_ = true;
  web_counting_finished_ = true;
  MergeResults();
}

void HistoryCounter::MergeResults() {
  if (!local_counting_finished_ || !web_counting_finished_)
    return;

  ReportResult(
      base::MakeUnique<HistoryResult>(this, local_result_, has_synced_visits_));
}

HistoryCounter::HistoryResult::HistoryResult(const HistoryCounter* source,
                                             ResultInt value,
                                             bool has_synced_visits)
    : FinishedResult(source, value), has_synced_visits_(has_synced_visits) {}

HistoryCounter::HistoryResult::~HistoryResult() {}

void HistoryCounter::OnStateChanged() {
  bool history_sync_enabled_new_state = !!web_history_service_callback_.Run();

  // If the history sync was just enabled or disabled, restart the counter
  // so that we update the result accordingly.
  if (history_sync_enabled_ != history_sync_enabled_new_state) {
    history_sync_enabled_ = history_sync_enabled_new_state;
    Restart();
  }
}

}  // namespace browsing_data
