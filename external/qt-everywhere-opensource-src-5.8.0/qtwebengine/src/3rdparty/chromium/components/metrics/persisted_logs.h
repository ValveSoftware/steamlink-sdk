// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PERSISTED_LOGS_H_
#define COMPONENTS_METRICS_PERSISTED_LOGS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"

class PrefService;

namespace metrics {

// Maintains a list of unsent logs that are written and restored from disk.
class PersistedLogs {
 public:
  // Used to produce a histogram that keeps track of the status of recalling
  // persisted per logs.
  enum LogReadStatus {
    RECALL_SUCCESS,         // We were able to correctly recall a persisted log.
    LIST_EMPTY,             // Attempting to recall from an empty list.
    LIST_SIZE_MISSING,      // Failed to recover list size using GetAsInteger().
    LIST_SIZE_TOO_SMALL,    // Too few elements in the list (less than 3).
    LIST_SIZE_CORRUPTION,   // List size is not as expected.
    LOG_STRING_CORRUPTION,  // Failed to recover log string using GetAsString().
    CHECKSUM_CORRUPTION,    // Failed to verify checksum.
    CHECKSUM_STRING_CORRUPTION,  // Failed to recover checksum string using
                                 // GetAsString().
    DECODE_FAIL,            // Failed to decode log.
    DEPRECATED_XML_PROTO_MISMATCH,  // The XML and protobuf logs have
                                    // inconsistent data.
    END_RECALL_STATUS       // Number of bins to use to create the histogram.
  };

  // Constructs a PersistedLogs that stores data in |local_state| under the
  // preference |pref_name|.
  // Calling code is responsible for ensuring that the lifetime of |local_state|
  // is longer than the lifetime of PersistedLogs.
  //
  // When saving logs to disk, stores either the first |min_log_count| logs, or
  // at least |min_log_bytes| bytes of logs, whichever is greater.
  //
  // If the optional |max_log_size| parameter is non-zero, all logs larger than
  // that limit will be skipped when writing to disk.
  PersistedLogs(PrefService* local_state,
                const char* pref_name,
                size_t min_log_count,
                size_t min_log_bytes,
                size_t max_log_size);
  ~PersistedLogs();

  // Write list to storage.
  void SerializeLogs() const;

  // Reads the list from the preference.
  LogReadStatus DeserializeLogs();

  // Adds a log to the list.
  void StoreLog(const std::string& log_data);

  // Stages the most recent log.  The staged_log will remain the same even if
  // additional logs are added.
  void StageLog();

  // Remove the staged log.
  void DiscardStagedLog();

  // True if a log has been staged.
  bool has_staged_log() const { return staged_log_index_ != -1; }

  // Returns the element in the front of the list.
  const std::string& staged_log() const {
    DCHECK(has_staged_log());
    return list_[staged_log_index_].compressed_log_data;
  }

  // Returns the element in the front of the list.
  const std::string& staged_log_hash() const {
    DCHECK(has_staged_log());
    return list_[staged_log_index_].hash;
  }

  // The number of elements currently stored.
  size_t size() const { return list_.size(); }

  // True if there are no stored logs.
  bool empty() const { return list_.empty(); }

 private:
  // Writes the list to the ListValue.
  void WriteLogsToPrefList(base::ListValue* list) const;

  // Reads the list from the ListValue.
  LogReadStatus ReadLogsFromPrefList(const base::ListValue& list);

  // A weak pointer to the PrefService object to read and write the preference
  // from.  Calling code should ensure this object continues to exist for the
  // lifetime of the PersistedLogs object.
  PrefService* local_state_;

  // The name of the preference to serialize logs to/from.
  const char* pref_name_;

  // We will keep at least this |min_log_count_| logs or |min_log_bytes_| bytes
  // of logs, whichever is greater, when writing to disk.  These apply after
  // skipping logs greater than |max_log_size_|.
  const size_t min_log_count_;
  const size_t min_log_bytes_;

  // Logs greater than this size will not be written to disk.
  const size_t max_log_size_;

  struct LogHashPair {
    // Initializes the members based on uncompressed |log_data|.
    void Init(const std::string& log_data);

    // Compressed log data - a serialized protobuf that's been gzipped.
    std::string compressed_log_data;

    // The SHA1 hash of log, stored to catch errors from memory corruption.
    std::string hash;
  };
  // A list of all of the stored logs, stored with SHA1 hashes to check for
  // corruption while they are stored in memory.
  std::vector<LogHashPair> list_;

  // The index and type of the log staged for upload. If nothing has been
  // staged, the index will be -1.
  int staged_log_index_;

  DISALLOW_COPY_AND_ASSIGN(PersistedLogs);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PERSISTED_LOGS_H_
