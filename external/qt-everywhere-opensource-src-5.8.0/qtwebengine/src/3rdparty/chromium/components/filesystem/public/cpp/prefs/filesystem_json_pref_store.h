// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_PUBLIC_CPP_PREFS_FILESYSTEM_JSON_PREF_STORE_H_
#define COMPONENTS_FILESYSTEM_PUBLIC_CPP_PREFS_FILESYSTEM_JSON_PREF_STORE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/filesystem/public/interfaces/file.mojom.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "components/prefs/base_prefs_export.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "mojo/public/cpp/bindings/binding.h"

class PrefFilter;

namespace base {
class Clock;
class DictionaryValue;
class FilePath;
class JsonPrefStoreLossyWriteTest;
class Value;
}

namespace filesystem {

// A forked, hack'n'slashed copy of base::JsonPrefStore which writes its
// preference data to the mojo filesystem instead of the real
// filesystem. Unlike base::JsonPrefStore, this class can safely be used inside
// a sandboxed process.
//
// In the long run, we'll want to replace the current PrefService code with
// something very different, especially since this component hard punts on all
// the hard things that the preference service does (enterprise management,
// parental controls, extension integration, etc.) and its interface is really
// not optimal for a mojoified world--there are a few places where we assume
// that writing to disk is synchronous...but it no longer is!
//
// Removing this class is a part of crbug.com/580652.
class FilesystemJsonPrefStore
    : public PersistentPrefStore,
      public filesystem::mojom::FileSystemClient,
      public base::SupportsWeakPtr<FilesystemJsonPrefStore>,
      public base::NonThreadSafe {
 public:
  struct ReadResult;

  FilesystemJsonPrefStore(const std::string& pref_filename,
                          filesystem::mojom::FileSystemPtr filesystem,
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

  // FileSystemClient overrides:
  void OnFileSystemShutdown() override;

  // Just like RemoveValue(), but doesn't notify observers. Used when doing some
  // cleanup that shouldn't otherwise alert observers.
  void RemoveValueSilently(const std::string& key, uint32_t flags);

  void ClearMutableValues() override;

 private:
  friend class base::JsonPrefStoreLossyWriteTest;

  ~FilesystemJsonPrefStore() override;

  // This method is called after the JSON file has been read.  It then hands
  // |value| (or an empty dictionary in some read error cases) to the
  // |pref_filter| if one is set. It also gives a callback pointing at
  // FinalizeFileRead() to that |pref_filter_| which is then responsible for
  // invoking it when done. If there is no |pref_filter_|, FinalizeFileRead()
  // is invoked directly.
  void OnFileRead(std::unique_ptr<ReadResult> read_result);

  // This method is called after the JSON file has been read and the result has
  // potentially been intercepted and modified by |pref_filter_|.
  // |schedule_write| indicates whether a write should be immediately scheduled
  // (typically because the |pref_filter_| has already altered the |prefs|) --
  // this will be ignored if this store is read-only.
  void FinalizeFileRead(std::unique_ptr<base::DictionaryValue> prefs,
                        bool schedule_write);

  // Schedule a write with the file writer as long as |flags| doesn't contain
  // WriteablePrefStore::LOSSY_PREF_WRITE_FLAG.
  void ScheduleWrite(uint32_t flags);

  // Actually performs a write. Unlike the //base version of this class, we
  // don't use the ImportantFileWriter and instead write using the mojo
  // filesystem API.
  void PerformWrite();

  // Opens the filesystem and calls |callback| when completed, whether
  // successfully or unsuccessfully.
  void OpenFilesystem(base::Closure callback);

  // Callback method which verifies that there were no errors on opening the
  // filesystem, and if there aren't, invokes the passed in callback.
  void OnOpenFilesystem(base::Closure callback, mojom::FileError err);

  // Asynchronous implementation details of PerformWrite().
  void OnTempFileWriteStart();
  void OnTempFileWrite(mojom::FileError err);
  void OnTempFileRenamed(mojom::FileError err);

  // Asynchronous implementation details of ReadPrefsAsync().
  void OnPreferencesReadStart();
  void OnPreferencesFileRead(mojom::FileError err,
                             mojo::Array<uint8_t> contents);

  const std::string path_;
  mojo::Binding<filesystem::mojom::FileSystemClient> binding_;
  filesystem::mojom::FileSystemPtr filesystem_;

  // |directory_| is only bound after the first attempt to access the
  // |filesystem. See OpenFilesystem().
  mojom::DirectoryPtr directory_;

  std::unique_ptr<base::DictionaryValue> prefs_;

  bool read_only_;

  std::unique_ptr<PrefFilter> pref_filter_;
  base::ObserverList<PrefStore::Observer, true> observers_;

  std::unique_ptr<ReadErrorDelegate> error_delegate_;

  bool initialized_;
  bool filtering_in_progress_;
  bool pending_lossy_write_;
  PrefReadError read_error_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemJsonPrefStore);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_PUBLIC_CPP_PREFS_FILESYSTEM_JSON_PREF_STORE_H_
