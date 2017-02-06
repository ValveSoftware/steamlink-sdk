// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persisted_logs.h"

#include <string>

#include "base/base64.h"
#include "base/md5.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/timer/elapsed_timer.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

PersistedLogs::LogReadStatus MakeRecallStatusHistogram(
    PersistedLogs::LogReadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PrefService.PersistentLogRecallProtobufs",
                            status, PersistedLogs::END_RECALL_STATUS);
  return status;
}

// Reads the value at |index| from |list_value| as a string and Base64-decodes
// it into |result|. Returns true on success.
bool ReadBase64String(const base::ListValue& list_value,
                      size_t index,
                      std::string* result) {
  std::string base64_result;
  if (!list_value.GetString(index, &base64_result))
    return false;
  return base::Base64Decode(base64_result, result);
}

// Base64-encodes |str| and appends the result to |list_value|.
void AppendBase64String(const std::string& str, base::ListValue* list_value) {
  std::string base64_str;
  base::Base64Encode(str, &base64_str);
  list_value->AppendString(base64_str);
}

}  // namespace

void PersistedLogs::LogHashPair::Init(const std::string& log_data) {
  DCHECK(!log_data.empty());

  if (!compression::GzipCompress(log_data, &compressed_log_data)) {
    NOTREACHED();
    return;
  }

  UMA_HISTOGRAM_PERCENTAGE(
      "UMA.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_log_data.size() / log_data.size()));

  hash = base::SHA1HashString(log_data);
}

PersistedLogs::PersistedLogs(PrefService* local_state,
                             const char* pref_name,
                             size_t min_log_count,
                             size_t min_log_bytes,
                             size_t max_log_size)
    : local_state_(local_state),
      pref_name_(pref_name),
      min_log_count_(min_log_count),
      min_log_bytes_(min_log_bytes),
      max_log_size_(max_log_size != 0 ? max_log_size : static_cast<size_t>(-1)),
      staged_log_index_(-1) {
  DCHECK(local_state_);
  // One of the limit arguments must be non-zero.
  DCHECK(min_log_count_ > 0 || min_log_bytes_ > 0);
}

PersistedLogs::~PersistedLogs() {}

void PersistedLogs::SerializeLogs() const {
  ListPrefUpdate update(local_state_, pref_name_);
  WriteLogsToPrefList(update.Get());
}

PersistedLogs::LogReadStatus PersistedLogs::DeserializeLogs() {
  return ReadLogsFromPrefList(*local_state_->GetList(pref_name_));
}

void PersistedLogs::StoreLog(const std::string& log_data) {
  list_.push_back(LogHashPair());
  list_.back().Init(log_data);
}

void PersistedLogs::StageLog() {
  // CHECK, rather than DCHECK, because swap()ing with an empty list causes
  // hard-to-identify crashes much later.
  CHECK(!list_.empty());
  DCHECK(!has_staged_log());
  staged_log_index_ = list_.size() - 1;
  DCHECK(has_staged_log());
}

void PersistedLogs::DiscardStagedLog() {
  DCHECK(has_staged_log());
  DCHECK_LT(static_cast<size_t>(staged_log_index_), list_.size());
  list_.erase(list_.begin() + staged_log_index_);
  staged_log_index_ = -1;
}

void PersistedLogs::WriteLogsToPrefList(base::ListValue* list_value) const {
  list_value->Clear();

  // Keep the most recent logs which are smaller than |max_log_size_|.
  // We keep at least |min_log_bytes_| and |min_log_count_| of logs before
  // discarding older logs.
  size_t start = list_.size();
  size_t saved_log_count = 0;
  size_t bytes_used = 0;
  for (; start > 0; --start) {
    size_t log_size = list_[start - 1].compressed_log_data.length();
    if (bytes_used >= min_log_bytes_ &&
        saved_log_count >= min_log_count_) {
      break;
    }
    // Oversized logs won't be persisted, so don't count them.
    if (log_size > max_log_size_)
      continue;
    bytes_used += log_size;
    ++saved_log_count;
  }
  int dropped_logs_num = start - 1;

  for (size_t i = start; i < list_.size(); ++i) {
    size_t log_size = list_[i].compressed_log_data.length();
    if (log_size > max_log_size_) {
      UMA_HISTOGRAM_COUNTS("UMA.Large Accumulated Log Not Persisted",
                           static_cast<int>(log_size));
      dropped_logs_num++;
      continue;
    }
    AppendBase64String(list_[i].compressed_log_data, list_value);
    AppendBase64String(list_[i].hash, list_value);
  }
  if (dropped_logs_num > 0)
    UMA_HISTOGRAM_COUNTS("UMA.UnsentLogs.Dropped", dropped_logs_num);
}

PersistedLogs::LogReadStatus PersistedLogs::ReadLogsFromPrefList(
    const base::ListValue& list_value) {
  if (list_value.empty())
    return MakeRecallStatusHistogram(LIST_EMPTY);

  // For each log, there's two entries in the list (the data and the hash).
  DCHECK_EQ(0U, list_value.GetSize() % 2);
  const size_t log_count = list_value.GetSize() / 2;

  // Resize |list_| ahead of time, so that values can be decoded directly into
  // the elements of the list.
  DCHECK(list_.empty());
  list_.resize(log_count);

  for (size_t i = 0; i < log_count; ++i) {
    if (!ReadBase64String(list_value, i * 2, &list_[i].compressed_log_data) ||
        !ReadBase64String(list_value, i * 2 + 1, &list_[i].hash)) {
      list_.clear();
      return MakeRecallStatusHistogram(LOG_STRING_CORRUPTION);
    }
  }

  return MakeRecallStatusHistogram(RECALL_SUCCESS);
}

}  // namespace metrics
