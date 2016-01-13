// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_JSON_PREF_STORE_H_
#define BASE_PREFS_JSON_PREF_STORE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/prefs/base_prefs_export.h"
#include "base/prefs/persistent_pref_store.h"

class PrefFilter;

namespace base {
class DictionaryValue;
class FilePath;
class SequencedTaskRunner;
class SequencedWorkerPool;
class Value;
}


// A writable PrefStore implementation that is used for user preferences.
class BASE_PREFS_EXPORT JsonPrefStore
    : public PersistentPrefStore,
      public base::ImportantFileWriter::DataSerializer,
      public base::SupportsWeakPtr<JsonPrefStore> {
 public:
  // Returns instance of SequencedTaskRunner which guarantees that file
  // operations on the same file will be executed in sequenced order.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForFile(
      const base::FilePath& pref_filename,
      base::SequencedWorkerPool* worker_pool);

  // Same as the constructor below with no alternate filename.
  JsonPrefStore(const base::FilePath& pref_filename,
                base::SequencedTaskRunner* sequenced_task_runner,
                scoped_ptr<PrefFilter> pref_filter);

  // |sequenced_task_runner| must be a shutdown-blocking task runner, ideally
  // created by the GetTaskRunnerForFile() method above.
  // |pref_filename| is the path to the file to read prefs from.
  // |pref_alternate_filename| is the path to an alternate file which the
  // desired prefs may have previously been written to. If |pref_filename|
  // doesn't exist and |pref_alternate_filename| does, |pref_alternate_filename|
  // will be moved to |pref_filename| before the read occurs.
  JsonPrefStore(const base::FilePath& pref_filename,
                const base::FilePath& pref_alternate_filename,
                base::SequencedTaskRunner* sequenced_task_runner,
                scoped_ptr<PrefFilter> pref_filter);

  // PrefStore overrides:
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const OVERRIDE;
  virtual void AddObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual bool HasObservers() const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;

  // PersistentPrefStore overrides:
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) OVERRIDE;
  virtual void SetValue(const std::string& key, base::Value* value) OVERRIDE;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) OVERRIDE;
  virtual void RemoveValue(const std::string& key) OVERRIDE;
  virtual bool ReadOnly() const OVERRIDE;
  virtual PrefReadError GetReadError() const OVERRIDE;
  // Note this method may be asynchronous if this instance has a |pref_filter_|
  // in which case it will return PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE.
  // See details in pref_filter.h.
  virtual PrefReadError ReadPrefs() OVERRIDE;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) OVERRIDE;
  virtual void CommitPendingWrite() OVERRIDE;
  virtual void ReportValueChanged(const std::string& key) OVERRIDE;

  // Just like RemoveValue(), but doesn't notify observers. Used when doing some
  // cleanup that shouldn't otherwise alert observers.
  void RemoveValueSilently(const std::string& key);

  // Registers |on_next_successful_write| to be called once, on the next
  // successful write event of |writer_|.
  void RegisterOnNextSuccessfulWriteCallback(
      const base::Closure& on_next_successful_write);

  // This method is called after the JSON file has been read.  It then hands
  // |value| (or an empty dictionary in some read error cases) to the
  // |pref_filter| if one is set. It also gives a callback pointing at
  // FinalizeFileRead() to that |pref_filter_| which is then responsible for
  // invoking it when done. If there is no |pref_filter_|, FinalizeFileRead()
  // is invoked directly.
  // Note, this method is used with asynchronous file reading, so this class
  // exposes it only for the internal needs (read: do not call it manually).
  // TODO(gab): Move this method to the private section and hand a callback to
  // it to FileThreadDeserializer rather than exposing this public method and
  // giving a JsonPrefStore* to FileThreadDeserializer.
  void OnFileRead(scoped_ptr<base::Value> value,
                  PrefReadError error,
                  bool no_dir);

 private:
  virtual ~JsonPrefStore();

  // ImportantFileWriter::DataSerializer overrides:
  virtual bool SerializeData(std::string* output) OVERRIDE;

  // This method is called after the JSON file has been read and the result has
  // potentially been intercepted and modified by |pref_filter_|.
  // |initialization_successful| is pre-determined by OnFileRead() and should
  // be used when reporting OnInitializationCompleted().
  // |schedule_write| indicates whether a write should be immediately scheduled
  // (typically because the |pref_filter_| has already altered the |prefs|) --
  // this will be ignored if this store is read-only.
  void FinalizeFileRead(bool initialization_successful,
                        scoped_ptr<base::DictionaryValue> prefs,
                        bool schedule_write);

  const base::FilePath path_;
  const base::FilePath alternate_path_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  scoped_ptr<base::DictionaryValue> prefs_;

  bool read_only_;

  // Helper for safely writing pref data.
  base::ImportantFileWriter writer_;

  scoped_ptr<PrefFilter> pref_filter_;
  ObserverList<PrefStore::Observer, true> observers_;

  scoped_ptr<ReadErrorDelegate> error_delegate_;

  bool initialized_;
  bool filtering_in_progress_;
  PrefReadError read_error_;

  std::set<std::string> keys_need_empty_value_;

  DISALLOW_COPY_AND_ASSIGN(JsonPrefStore);
};

#endif  // BASE_PREFS_JSON_PREF_STORE_H_
