// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/json_pref_store.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_filter.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"

namespace {

// Some extensions we'll tack on to copies of the Preferences files.
const base::FilePath::CharType* kBadExtension = FILE_PATH_LITERAL("bad");

// Differentiates file loading between origin thread and passed
// (aka file) thread.
class FileThreadDeserializer
    : public base::RefCountedThreadSafe<FileThreadDeserializer> {
 public:
  FileThreadDeserializer(JsonPrefStore* delegate,
                         base::SequencedTaskRunner* sequenced_task_runner)
      : no_dir_(false),
        error_(PersistentPrefStore::PREF_READ_ERROR_NONE),
        delegate_(delegate),
        sequenced_task_runner_(sequenced_task_runner),
        origin_loop_proxy_(base::MessageLoopProxy::current()) {
  }

  void Start(const base::FilePath& path,
             const base::FilePath& alternate_path) {
    DCHECK(origin_loop_proxy_->BelongsToCurrentThread());
    // TODO(gab): This should use PostTaskAndReplyWithResult instead of using
    // the |error_| member to pass data across tasks.
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FileThreadDeserializer::ReadFileAndReport,
                   this, path, alternate_path));
  }

  // Deserializes JSON on the sequenced task runner.
  void ReadFileAndReport(const base::FilePath& path,
                         const base::FilePath& alternate_path) {
    DCHECK(sequenced_task_runner_->RunsTasksOnCurrentThread());

    value_.reset(DoReading(path, alternate_path, &error_, &no_dir_));

    origin_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&FileThreadDeserializer::ReportOnOriginThread, this));
  }

  // Reports deserialization result on the origin thread.
  void ReportOnOriginThread() {
    DCHECK(origin_loop_proxy_->BelongsToCurrentThread());
    delegate_->OnFileRead(value_.Pass(), error_, no_dir_);
  }

  static base::Value* DoReading(const base::FilePath& path,
                                const base::FilePath& alternate_path,
                                PersistentPrefStore::PrefReadError* error,
                                bool* no_dir) {
    if (!base::PathExists(path) && !alternate_path.empty() &&
        base::PathExists(alternate_path)) {
      base::Move(alternate_path, path);
    }

    int error_code;
    std::string error_msg;
    JSONFileValueSerializer serializer(path);
    base::Value* value = serializer.Deserialize(&error_code, &error_msg);
    HandleErrors(value, path, error_code, error_msg, error);
    *no_dir = !base::PathExists(path.DirName());
    return value;
  }

  static void HandleErrors(const base::Value* value,
                           const base::FilePath& path,
                           int error_code,
                           const std::string& error_msg,
                           PersistentPrefStore::PrefReadError* error);

 private:
  friend class base::RefCountedThreadSafe<FileThreadDeserializer>;
  ~FileThreadDeserializer() {}

  bool no_dir_;
  PersistentPrefStore::PrefReadError error_;
  scoped_ptr<base::Value> value_;
  const scoped_refptr<JsonPrefStore> delegate_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  const scoped_refptr<base::MessageLoopProxy> origin_loop_proxy_;
};

// static
void FileThreadDeserializer::HandleErrors(
    const base::Value* value,
    const base::FilePath& path,
    int error_code,
    const std::string& error_msg,
    PersistentPrefStore::PrefReadError* error) {
  *error = PersistentPrefStore::PREF_READ_ERROR_NONE;
  if (!value) {
    DVLOG(1) << "Error while loading JSON file: " << error_msg
             << ", file: " << path.value();
    switch (error_code) {
      case JSONFileValueSerializer::JSON_ACCESS_DENIED:
        *error = PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED;
        break;
      case JSONFileValueSerializer::JSON_CANNOT_READ_FILE:
        *error = PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER;
        break;
      case JSONFileValueSerializer::JSON_FILE_LOCKED:
        *error = PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED;
        break;
      case JSONFileValueSerializer::JSON_NO_SUCH_FILE:
        *error = PersistentPrefStore::PREF_READ_ERROR_NO_FILE;
        break;
      default:
        *error = PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
        // JSON errors indicate file corruption of some sort.
        // Since the file is corrupt, move it to the side and continue with
        // empty preferences.  This will result in them losing their settings.
        // We keep the old file for possible support and debugging assistance
        // as well as to detect if they're seeing these errors repeatedly.
        // TODO(erikkay) Instead, use the last known good file.
        base::FilePath bad = path.ReplaceExtension(kBadExtension);

        // If they've ever had a parse error before, put them in another bucket.
        // TODO(erikkay) if we keep this error checking for very long, we may
        // want to differentiate between recent and long ago errors.
        if (base::PathExists(bad))
          *error = PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT;
        base::Move(path, bad);
        break;
    }
  } else if (!value->IsType(base::Value::TYPE_DICTIONARY)) {
    *error = PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE;
  }
}

}  // namespace

scoped_refptr<base::SequencedTaskRunner> JsonPrefStore::GetTaskRunnerForFile(
    const base::FilePath& filename,
    base::SequencedWorkerPool* worker_pool) {
  std::string token("json_pref_store-");
  token.append(filename.AsUTF8Unsafe());
  return worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
      worker_pool->GetNamedSequenceToken(token),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

JsonPrefStore::JsonPrefStore(const base::FilePath& filename,
                             base::SequencedTaskRunner* sequenced_task_runner,
                             scoped_ptr<PrefFilter> pref_filter)
    : path_(filename),
      sequenced_task_runner_(sequenced_task_runner),
      prefs_(new base::DictionaryValue()),
      read_only_(false),
      writer_(filename, sequenced_task_runner),
      pref_filter_(pref_filter.Pass()),
      initialized_(false),
      filtering_in_progress_(false),
      read_error_(PREF_READ_ERROR_NONE) {
}

JsonPrefStore::JsonPrefStore(const base::FilePath& filename,
                             const base::FilePath& alternate_filename,
                             base::SequencedTaskRunner* sequenced_task_runner,
                             scoped_ptr<PrefFilter> pref_filter)
    : path_(filename),
      alternate_path_(alternate_filename),
      sequenced_task_runner_(sequenced_task_runner),
      prefs_(new base::DictionaryValue()),
      read_only_(false),
      writer_(filename, sequenced_task_runner),
      pref_filter_(pref_filter.Pass()),
      initialized_(false),
      filtering_in_progress_(false),
      read_error_(PREF_READ_ERROR_NONE) {
}

bool JsonPrefStore::GetValue(const std::string& key,
                             const base::Value** result) const {
  base::Value* tmp = NULL;
  if (!prefs_->Get(key, &tmp))
    return false;

  if (result)
    *result = tmp;
  return true;
}

void JsonPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void JsonPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool JsonPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool JsonPrefStore::IsInitializationComplete() const {
  return initialized_;
}

bool JsonPrefStore::GetMutableValue(const std::string& key,
                                    base::Value** result) {
  return prefs_->Get(key, result);
}

void JsonPrefStore::SetValue(const std::string& key, base::Value* value) {
  DCHECK(value);
  scoped_ptr<base::Value> new_value(value);
  base::Value* old_value = NULL;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    prefs_->Set(key, new_value.release());
    ReportValueChanged(key);
  }
}

void JsonPrefStore::SetValueSilently(const std::string& key,
                                     base::Value* value) {
  DCHECK(value);
  scoped_ptr<base::Value> new_value(value);
  base::Value* old_value = NULL;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    prefs_->Set(key, new_value.release());
    if (!read_only_)
      writer_.ScheduleWrite(this);
  }
}

void JsonPrefStore::RemoveValue(const std::string& key) {
  if (prefs_->RemovePath(key, NULL))
    ReportValueChanged(key);
}

void JsonPrefStore::RemoveValueSilently(const std::string& key) {
  prefs_->RemovePath(key, NULL);
  if (!read_only_)
    writer_.ScheduleWrite(this);
}

bool JsonPrefStore::ReadOnly() const {
  return read_only_;
}

PersistentPrefStore::PrefReadError JsonPrefStore::GetReadError() const {
  return read_error_;
}

PersistentPrefStore::PrefReadError JsonPrefStore::ReadPrefs() {
  if (path_.empty()) {
    OnFileRead(
        scoped_ptr<base::Value>(), PREF_READ_ERROR_FILE_NOT_SPECIFIED, false);
    return PREF_READ_ERROR_FILE_NOT_SPECIFIED;
  }

  PrefReadError error;
  bool no_dir;
  scoped_ptr<base::Value> value(
      FileThreadDeserializer::DoReading(path_, alternate_path_, &error,
                                        &no_dir));
  OnFileRead(value.Pass(), error, no_dir);
  return filtering_in_progress_ ? PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE :
                                  error;
}

void JsonPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  initialized_ = false;
  error_delegate_.reset(error_delegate);
  if (path_.empty()) {
    OnFileRead(
        scoped_ptr<base::Value>(), PREF_READ_ERROR_FILE_NOT_SPECIFIED, false);
    return;
  }

  // Start async reading of the preferences file. It will delete itself
  // in the end.
  scoped_refptr<FileThreadDeserializer> deserializer(
      new FileThreadDeserializer(this, sequenced_task_runner_.get()));
  deserializer->Start(path_, alternate_path_);
}

void JsonPrefStore::CommitPendingWrite() {
  if (writer_.HasPendingWrite() && !read_only_)
    writer_.DoScheduledWrite();
}

void JsonPrefStore::ReportValueChanged(const std::string& key) {
  if (pref_filter_)
    pref_filter_->FilterUpdate(key);

  FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));

  if (!read_only_)
    writer_.ScheduleWrite(this);
}

void JsonPrefStore::RegisterOnNextSuccessfulWriteCallback(
    const base::Closure& on_next_successful_write) {
  writer_.RegisterOnNextSuccessfulWriteCallback(on_next_successful_write);
}

void JsonPrefStore::OnFileRead(scoped_ptr<base::Value> value,
                               PersistentPrefStore::PrefReadError error,
                               bool no_dir) {
  scoped_ptr<base::DictionaryValue> unfiltered_prefs(new base::DictionaryValue);

  read_error_ = error;

  bool initialization_successful = !no_dir;

  if (initialization_successful) {
    switch (read_error_) {
      case PREF_READ_ERROR_ACCESS_DENIED:
      case PREF_READ_ERROR_FILE_OTHER:
      case PREF_READ_ERROR_FILE_LOCKED:
      case PREF_READ_ERROR_JSON_TYPE:
      case PREF_READ_ERROR_FILE_NOT_SPECIFIED:
        read_only_ = true;
        break;
      case PREF_READ_ERROR_NONE:
        DCHECK(value.get());
        unfiltered_prefs.reset(
            static_cast<base::DictionaryValue*>(value.release()));
        break;
      case PREF_READ_ERROR_NO_FILE:
        // If the file just doesn't exist, maybe this is first run.  In any case
        // there's no harm in writing out default prefs in this case.
        break;
      case PREF_READ_ERROR_JSON_PARSE:
      case PREF_READ_ERROR_JSON_REPEAT:
        break;
      case PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE:
        // This is a special error code to be returned by ReadPrefs when it
        // can't complete synchronously, it should never be returned by the read
        // operation itself.
        NOTREACHED();
        break;
      case PREF_READ_ERROR_LEVELDB_IO:
      case PREF_READ_ERROR_LEVELDB_CORRUPTION_READ_ONLY:
      case PREF_READ_ERROR_LEVELDB_CORRUPTION:
        // These are specific to LevelDBPrefStore.
        NOTREACHED();
      case PREF_READ_ERROR_MAX_ENUM:
        NOTREACHED();
        break;
    }
  }

  if (pref_filter_) {
    filtering_in_progress_ = true;
    const PrefFilter::PostFilterOnLoadCallback post_filter_on_load_callback(
        base::Bind(
            &JsonPrefStore::FinalizeFileRead, this, initialization_successful));
    pref_filter_->FilterOnLoad(post_filter_on_load_callback,
                               unfiltered_prefs.Pass());
  } else {
    FinalizeFileRead(initialization_successful, unfiltered_prefs.Pass(), false);
  }
}

JsonPrefStore::~JsonPrefStore() {
  CommitPendingWrite();
}

bool JsonPrefStore::SerializeData(std::string* output) {
  if (pref_filter_)
    pref_filter_->FilterSerializeData(prefs_.get());

  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*prefs_);
}

void JsonPrefStore::FinalizeFileRead(bool initialization_successful,
                                     scoped_ptr<base::DictionaryValue> prefs,
                                     bool schedule_write) {
  filtering_in_progress_ = false;

  if (!initialization_successful) {
    FOR_EACH_OBSERVER(PrefStore::Observer,
                      observers_,
                      OnInitializationCompleted(false));
    return;
  }

  prefs_ = prefs.Pass();

  initialized_ = true;

  if (schedule_write && !read_only_)
    writer_.ScheduleWrite(this);

  if (error_delegate_ && read_error_ != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(read_error_);

  FOR_EACH_OBSERVER(PrefStore::Observer,
                    observers_,
                    OnInitializationCompleted(true));

  return;
}
