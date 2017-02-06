// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_JSON_PREF_STORE_H_
#define COMPONENTS_PREFS_JSON_PREF_STORE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "components/prefs/base_prefs_export.h"
#include "components/prefs/persistent_pref_store.h"

class PrefFilter;

namespace base {
class Clock;
class DictionaryValue;
class FilePath;
class HistogramBase;
class JsonPrefStoreLossyWriteTest;
class SequencedTaskRunner;
class SequencedWorkerPool;
class Value;
FORWARD_DECLARE_TEST(JsonPrefStoreTest, WriteCountHistogramTestBasic);
FORWARD_DECLARE_TEST(JsonPrefStoreTest, WriteCountHistogramTestSinglePeriod);
FORWARD_DECLARE_TEST(JsonPrefStoreTest, WriteCountHistogramTestMultiplePeriods);
FORWARD_DECLARE_TEST(JsonPrefStoreTest, WriteCountHistogramTestPeriodWithGaps);
}

// A writable PrefStore implementation that is used for user preferences.
class COMPONENTS_PREFS_EXPORT JsonPrefStore
    : public PersistentPrefStore,
      public base::ImportantFileWriter::DataSerializer,
      public base::SupportsWeakPtr<JsonPrefStore>,
      public base::NonThreadSafe {
 public:
  struct ReadResult;

  // Returns instance of SequencedTaskRunner which guarantees that file
  // operations on the same file will be executed in sequenced order.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForFile(
      const base::FilePath& pref_filename,
      base::SequencedWorkerPool* worker_pool);

  // Same as the constructor below with no alternate filename.
  JsonPrefStore(
      const base::FilePath& pref_filename,
      const scoped_refptr<base::SequencedTaskRunner>& sequenced_task_runner,
      std::unique_ptr<PrefFilter> pref_filter);

  // |sequenced_task_runner| must be a shutdown-blocking task runner, ideally
  // created by the GetTaskRunnerForFile() method above.
  // |pref_filename| is the path to the file to read prefs from.
  // |pref_alternate_filename| is the path to an alternate file which the
  // desired prefs may have previously been written to. If |pref_filename|
  // doesn't exist and |pref_alternate_filename| does, |pref_alternate_filename|
  // will be moved to |pref_filename| before the read occurs.
  JsonPrefStore(
      const base::FilePath& pref_filename,
      const base::FilePath& pref_alternate_filename,
      const scoped_refptr<base::SequencedTaskRunner>& sequenced_task_runner,
      std::unique_ptr<PrefFilter> pref_filter);

  // PrefStore overrides:
  bool GetValue(const std::string& key,
                const base::Value** result) const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  // PersistentPrefStore overrides:
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        std::unique_ptr<base::Value> value,
                        uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  // Note this method may be asynchronous if this instance has a |pref_filter_|
  // in which case it will return PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE.
  // See details in pref_filter.h.
  PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void CommitPendingWrite() override;
  void SchedulePendingLossyWrites() override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;

  // Just like RemoveValue(), but doesn't notify observers. Used when doing some
  // cleanup that shouldn't otherwise alert observers.
  void RemoveValueSilently(const std::string& key, uint32_t flags);

  // Registers |on_next_successful_write| to be called once, on the next
  // successful write event of |writer_|.
  void RegisterOnNextSuccessfulWriteCallback(
      const base::Closure& on_next_successful_write);

  void ClearMutableValues() override;

 private:
  // Represents a histogram for recording the number of writes to the pref file
  // that occur every kHistogramWriteReportIntervalInMins minutes.
  class COMPONENTS_PREFS_EXPORT WriteCountHistogram {
   public:
    static const int32_t kHistogramWriteReportIntervalMins;

    WriteCountHistogram(const base::TimeDelta& commit_interval,
                        const base::FilePath& path);
    // Constructor for testing. |clock| is a test Clock that is used to retrieve
    // the time.
    WriteCountHistogram(const base::TimeDelta& commit_interval,
                        const base::FilePath& path,
                        std::unique_ptr<base::Clock> clock);
    ~WriteCountHistogram();

    // Record that a write has occured.
    void RecordWriteOccured();

    // Reports writes (that have not yet been reported) in all of the recorded
    // intervals that have elapsed up until current time.
    void ReportOutstandingWrites();

    base::HistogramBase* GetHistogram();

   private:
    // The minimum interval at which writes can occur.
    const base::TimeDelta commit_interval_;

    // The path to the file.
    const base::FilePath path_;

    // Clock which is used to retrieve the current time.
    std::unique_ptr<base::Clock> clock_;

    // The interval at which to report write counts.
    const base::TimeDelta report_interval_;

    // The time at which the last histogram value was reported for the number
    // of write counts.
    base::Time last_report_time_;

    // The number of writes that have occured since the last write count was
    // reported.
    uint32_t writes_since_last_report_;

    DISALLOW_COPY_AND_ASSIGN(WriteCountHistogram);
  };

  FRIEND_TEST_ALL_PREFIXES(base::JsonPrefStoreTest,
                           WriteCountHistogramTestBasic);
  FRIEND_TEST_ALL_PREFIXES(base::JsonPrefStoreTest,
                           WriteCountHistogramTestSinglePeriod);
  FRIEND_TEST_ALL_PREFIXES(base::JsonPrefStoreTest,
                           WriteCountHistogramTestMultiplePeriods);
  FRIEND_TEST_ALL_PREFIXES(base::JsonPrefStoreTest,
                           WriteCountHistogramTestPeriodWithGaps);
  friend class base::JsonPrefStoreLossyWriteTest;

  ~JsonPrefStore() override;

  // This method is called after the JSON file has been read.  It then hands
  // |value| (or an empty dictionary in some read error cases) to the
  // |pref_filter| if one is set. It also gives a callback pointing at
  // FinalizeFileRead() to that |pref_filter_| which is then responsible for
  // invoking it when done. If there is no |pref_filter_|, FinalizeFileRead()
  // is invoked directly.
  void OnFileRead(std::unique_ptr<ReadResult> read_result);

  // ImportantFileWriter::DataSerializer overrides:
  bool SerializeData(std::string* output) override;

  // This method is called after the JSON file has been read and the result has
  // potentially been intercepted and modified by |pref_filter_|.
  // |initialization_successful| is pre-determined by OnFileRead() and should
  // be used when reporting OnInitializationCompleted().
  // |schedule_write| indicates whether a write should be immediately scheduled
  // (typically because the |pref_filter_| has already altered the |prefs|) --
  // this will be ignored if this store is read-only.
  void FinalizeFileRead(bool initialization_successful,
                        std::unique_ptr<base::DictionaryValue> prefs,
                        bool schedule_write);

  // Schedule a write with the file writer as long as |flags| doesn't contain
  // WriteablePrefStore::LOSSY_PREF_WRITE_FLAG.
  void ScheduleWrite(uint32_t flags);

  const base::FilePath path_;
  const base::FilePath alternate_path_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  std::unique_ptr<base::DictionaryValue> prefs_;

  bool read_only_;

  // Helper for safely writing pref data.
  base::ImportantFileWriter writer_;

  std::unique_ptr<PrefFilter> pref_filter_;
  base::ObserverList<PrefStore::Observer, true> observers_;

  std::unique_ptr<ReadErrorDelegate> error_delegate_;

  bool initialized_;
  bool filtering_in_progress_;
  bool pending_lossy_write_;
  PrefReadError read_error_;

  std::set<std::string> keys_need_empty_value_;

  WriteCountHistogram write_count_histogram_;

  DISALLOW_COPY_AND_ASSIGN(JsonPrefStore);
};

#endif  // COMPONENTS_PREFS_JSON_PREF_STORE_H_
