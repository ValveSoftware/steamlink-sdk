// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/public/cpp/prefs/filesystem_json_pref_store.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/prefs/pref_filter.h"
#include "mojo/common/common_type_converters.h"

namespace filesystem {

// Result returned from internal read tasks.
struct FilesystemJsonPrefStore::ReadResult {
 public:
  ReadResult();
  ~ReadResult();

  std::unique_ptr<base::Value> value;
  PrefReadError error;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadResult);
};

FilesystemJsonPrefStore::ReadResult::ReadResult()
    : error(PersistentPrefStore::PREF_READ_ERROR_NONE) {}

FilesystemJsonPrefStore::ReadResult::~ReadResult() {}

namespace {

PersistentPrefStore::PrefReadError HandleReadErrors(const base::Value* value) {
  if (!value->IsType(base::Value::TYPE_DICTIONARY))
    return PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE;
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

}  // namespace

FilesystemJsonPrefStore::FilesystemJsonPrefStore(
    const std::string& pref_filename,
    filesystem::mojom::FileSystemPtr filesystem,
    std::unique_ptr<PrefFilter> pref_filter)
    : path_(pref_filename),
      binding_(this),
      filesystem_(std::move(filesystem)),
      prefs_(new base::DictionaryValue()),
      read_only_(false),
      pref_filter_(std::move(pref_filter)),
      initialized_(false),
      filtering_in_progress_(false),
      pending_lossy_write_(false),
      read_error_(PREF_READ_ERROR_NONE) {
  DCHECK(!path_.empty());
}

bool FilesystemJsonPrefStore::GetValue(const std::string& key,
                                       const base::Value** result) const {
  DCHECK(CalledOnValidThread());

  base::Value* tmp = nullptr;
  if (!prefs_->Get(key, &tmp))
    return false;

  if (result)
    *result = tmp;
  return true;
}

void FilesystemJsonPrefStore::AddObserver(PrefStore::Observer* observer) {
  DCHECK(CalledOnValidThread());

  observers_.AddObserver(observer);
}

void FilesystemJsonPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  DCHECK(CalledOnValidThread());

  observers_.RemoveObserver(observer);
}

bool FilesystemJsonPrefStore::HasObservers() const {
  DCHECK(CalledOnValidThread());

  return observers_.might_have_observers();
}

bool FilesystemJsonPrefStore::IsInitializationComplete() const {
  DCHECK(CalledOnValidThread());

  return initialized_;
}

bool FilesystemJsonPrefStore::GetMutableValue(const std::string& key,
                                              base::Value** result) {
  DCHECK(CalledOnValidThread());

  return prefs_->Get(key, result);
}

void FilesystemJsonPrefStore::SetValue(const std::string& key,
                                       std::unique_ptr<base::Value> value,
                                       uint32_t flags) {
  DCHECK(CalledOnValidThread());

  DCHECK(value);
  base::Value* old_value = nullptr;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    prefs_->Set(key, std::move(value));
    ReportValueChanged(key, flags);
  }
}

void FilesystemJsonPrefStore::SetValueSilently(
    const std::string& key,
    std::unique_ptr<base::Value> value,
    uint32_t flags) {
  DCHECK(CalledOnValidThread());

  DCHECK(value);
  base::Value* old_value = nullptr;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    prefs_->Set(key, std::move(value));
    ScheduleWrite(flags);
  }
}

void FilesystemJsonPrefStore::RemoveValue(const std::string& key,
                                          uint32_t flags) {
  DCHECK(CalledOnValidThread());

  if (prefs_->RemovePath(key, nullptr))
    ReportValueChanged(key, flags);
}

void FilesystemJsonPrefStore::RemoveValueSilently(const std::string& key,
                                                  uint32_t flags) {
  DCHECK(CalledOnValidThread());

  prefs_->RemovePath(key, nullptr);
  ScheduleWrite(flags);
}

void FilesystemJsonPrefStore::ClearMutableValues() {
  NOTIMPLEMENTED();
}

bool FilesystemJsonPrefStore::ReadOnly() const {
  DCHECK(CalledOnValidThread());

  return read_only_;
}

PersistentPrefStore::PrefReadError FilesystemJsonPrefStore::GetReadError()
    const {
  DCHECK(CalledOnValidThread());

  return read_error_;
}

PersistentPrefStore::PrefReadError FilesystemJsonPrefStore::ReadPrefs() {
  NOTREACHED();
  // TODO(erg): Synchronously reading files makes no sense in a mojo world and
  // should be removed from the API.
  return PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE;
}

void FilesystemJsonPrefStore::ReadPrefsAsync(
    ReadErrorDelegate* error_delegate) {
  DCHECK(CalledOnValidThread());

  initialized_ = false;
  error_delegate_.reset(error_delegate);

  if (!directory_) {
    OpenFilesystem(
        Bind(&FilesystemJsonPrefStore::OnPreferencesReadStart, AsWeakPtr()));
  } else {
    OnPreferencesReadStart();
  }
}

void FilesystemJsonPrefStore::CommitPendingWrite() {
  DCHECK(CalledOnValidThread());

  // TODO(erg): This is another one of those cases where we have problems
  // because of mismatch between the models used in the pref service versus
  // here. Most of the time, CommitPendingWrite() is called from
  // PrefService. However, in JSONPrefStore, we also call this method on
  // shutdown and thus need to synchronously write. But in mojo:filesystem,
  // everything is done asynchronously. So we're sort of stuck until we can
  // change the interface, which we'll do in the longer term.

  SchedulePendingLossyWrites();
}

void FilesystemJsonPrefStore::SchedulePendingLossyWrites() {
  // This method is misnamed for the sake of the interface. Given that writing
  // is already asynchronous in this new world, "sheduleing" a pending write
  // just starts the asynchronous process.
  if (pending_lossy_write_)
    PerformWrite();
}

void FilesystemJsonPrefStore::ReportValueChanged(const std::string& key,
                                                 uint32_t flags) {
  DCHECK(CalledOnValidThread());

  if (pref_filter_)
    pref_filter_->FilterUpdate(key);

  FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));

  ScheduleWrite(flags);
}

void FilesystemJsonPrefStore::OnFileSystemShutdown() {}

void FilesystemJsonPrefStore::OnFileRead(
    std::unique_ptr<ReadResult> read_result) {
  DCHECK(CalledOnValidThread());

  DCHECK(read_result);

  std::unique_ptr<base::DictionaryValue> unfiltered_prefs(
      new base::DictionaryValue);

  read_error_ = read_result->error;

  switch (read_error_) {
    case PREF_READ_ERROR_ACCESS_DENIED:
    case PREF_READ_ERROR_FILE_OTHER:
    case PREF_READ_ERROR_FILE_LOCKED:
    case PREF_READ_ERROR_JSON_TYPE:
    case PREF_READ_ERROR_FILE_NOT_SPECIFIED:
      read_only_ = true;
      break;
    case PREF_READ_ERROR_NONE:
      DCHECK(read_result->value.get());
      unfiltered_prefs.reset(
          static_cast<base::DictionaryValue*>(read_result->value.release()));
      break;
    case PREF_READ_ERROR_NO_FILE:
    // If the file just doesn't exist, maybe this is first run.  In any case
    // there's no harm in writing out default prefs in this case.
    case PREF_READ_ERROR_JSON_PARSE:
    case PREF_READ_ERROR_JSON_REPEAT:
      break;
    case PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE:
    // This is a special error code to be returned by ReadPrefs when it
    // can't complete synchronously, it should never be returned by the read
    // operation itself.
    case PREF_READ_ERROR_MAX_ENUM:
      NOTREACHED();
      break;
  }

  if (pref_filter_) {
    filtering_in_progress_ = true;
    const PrefFilter::PostFilterOnLoadCallback post_filter_on_load_callback(
        base::Bind(&FilesystemJsonPrefStore::FinalizeFileRead, AsWeakPtr()));
    pref_filter_->FilterOnLoad(post_filter_on_load_callback,
                               std::move(unfiltered_prefs));
  } else {
    FinalizeFileRead(std::move(unfiltered_prefs), false);
  }
}

FilesystemJsonPrefStore::~FilesystemJsonPrefStore() {
  // TODO(erg): Now that writing is asynchronous, we can't really async write
  // prefs at shutdown. See comment in CommitPendingWrite().
}

void FilesystemJsonPrefStore::FinalizeFileRead(
    std::unique_ptr<base::DictionaryValue> prefs,
    bool schedule_write) {
  DCHECK(CalledOnValidThread());

  filtering_in_progress_ = false;

  prefs_ = std::move(prefs);

  initialized_ = true;

  if (schedule_write)
    ScheduleWrite(DEFAULT_PREF_WRITE_FLAGS);

  if (error_delegate_ && read_error_ != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(read_error_);

  FOR_EACH_OBSERVER(PrefStore::Observer, observers_,
                    OnInitializationCompleted(true));

  return;
}

void FilesystemJsonPrefStore::ScheduleWrite(uint32_t flags) {
  if (read_only_)
    return;

  if (flags & LOSSY_PREF_WRITE_FLAG)
    pending_lossy_write_ = true;
  else
    PerformWrite();
}

void FilesystemJsonPrefStore::PerformWrite() {
  if (!directory_) {
    OpenFilesystem(
        Bind(&FilesystemJsonPrefStore::OnTempFileWriteStart, AsWeakPtr()));
  } else {
    OnTempFileWriteStart();
  }
}

void FilesystemJsonPrefStore::OpenFilesystem(base::Closure callback) {
  filesystem::mojom::FileSystemClientPtr client;
  binding_.Bind(GetProxy(&client));

  filesystem_->OpenFileSystem(
      "origin", GetProxy(&directory_), std::move(client),
      base::Bind(&FilesystemJsonPrefStore::OnOpenFilesystem, AsWeakPtr(),
                 callback));
}

void FilesystemJsonPrefStore::OnOpenFilesystem(base::Closure callback,
                                               mojom::FileError err) {
  if (err != mojom::FileError::OK) {
    // Do real error checking.
    NOTIMPLEMENTED();
    return;
  }

  callback.Run();
}

void FilesystemJsonPrefStore::OnTempFileWriteStart() {
  // Calculate what we want to write, and then write to the temporary file.
  pending_lossy_write_ = false;

  if (pref_filter_)
    pref_filter_->FilterSerializeData(prefs_.get());

  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(false);
  serializer.Serialize(*prefs_);

  directory_->WriteFile(
      "tmp",
      mojo::Array<uint8_t>::From(output),
      Bind(&FilesystemJsonPrefStore::OnTempFileWrite, AsWeakPtr()));
}

void FilesystemJsonPrefStore::OnTempFileWrite(mojom::FileError err) {
  // TODO(erg): Error handling. The JsonPrefStore code assumes that writing the
  // file can never fail.
  CHECK_EQ(mojom::FileError::OK, err);

  directory_->Rename(
      "tmp", path_,
      Bind(&FilesystemJsonPrefStore::OnTempFileRenamed, AsWeakPtr()));
}

void FilesystemJsonPrefStore::OnTempFileRenamed(mojom::FileError err) {}

void FilesystemJsonPrefStore::OnPreferencesReadStart() {
  directory_->ReadEntireFile(
      path_,
      Bind(&FilesystemJsonPrefStore::OnPreferencesFileRead, AsWeakPtr()));
}

void FilesystemJsonPrefStore::OnPreferencesFileRead(
    mojom::FileError err,
    mojo::Array<uint8_t> contents) {
  std::unique_ptr<FilesystemJsonPrefStore::ReadResult> read_result(
      new FilesystemJsonPrefStore::ReadResult);
  // TODO(erg): Needs even better error handling.
  switch (err) {
    case mojom::FileError::IN_USE:
    case mojom::FileError::ACCESS_DENIED:
    case mojom::FileError::NOT_A_FILE: {
      read_only_ = true;
      break;
    }
    case mojom::FileError::FAILED:
    case mojom::FileError::NOT_FOUND: {
      // If the file just doesn't exist, maybe this is the first run. Just
      // don't pass a value.
      read_result->error = PREF_READ_ERROR_NO_FILE;
      break;
    }
    default: {
      int error_code;
      std::string error_msg;
      JSONStringValueDeserializer deserializer(base::StringPiece(
          reinterpret_cast<char*>(&contents.front()), contents.size()));
      read_result->value = deserializer.Deserialize(&error_code, &error_msg);
      read_result->error = HandleReadErrors(read_result->value.get());
    }
  }

  OnFileRead(std::move(read_result));
}

}  // namespace filesystem
