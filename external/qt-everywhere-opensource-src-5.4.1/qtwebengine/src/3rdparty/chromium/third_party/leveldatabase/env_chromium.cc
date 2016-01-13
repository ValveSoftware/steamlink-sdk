// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "env_chromium_stdio.h"
#include "third_party/re2/re2/re2.h"

#if defined(OS_WIN)
#include <io.h>
#include "base/command_line.h"
#include "base/win/win_util.h"
#include "env_chromium_win.h"
#endif

using namespace leveldb;

namespace leveldb_env {

namespace {

const base::FilePath::CharType backup_table_extension[] =
    FILE_PATH_LITERAL(".bak");
const base::FilePath::CharType table_extension[] = FILE_PATH_LITERAL(".ldb");

static const base::FilePath::CharType kLevelDBTestDirectoryPrefix[]
    = FILE_PATH_LITERAL("leveldb-test-");

class ChromiumFileLock : public FileLock {
 public:
  ::base::File file_;
  std::string name_;
};

class Retrier {
 public:
  Retrier(MethodID method, RetrierProvider* provider)
      : start_(base::TimeTicks::Now()),
        limit_(start_ + base::TimeDelta::FromMilliseconds(
                            provider->MaxRetryTimeMillis())),
        last_(start_),
        time_to_sleep_(base::TimeDelta::FromMilliseconds(10)),
        success_(true),
        method_(method),
        last_error_(base::File::FILE_OK),
        provider_(provider) {}
  ~Retrier() {
    if (success_) {
      provider_->GetRetryTimeHistogram(method_)->AddTime(last_ - start_);
      if (last_error_ != base::File::FILE_OK) {
        DCHECK(last_error_ < 0);
        provider_->GetRecoveredFromErrorHistogram(method_)->Add(-last_error_);
      }
    }
  }
  bool ShouldKeepTrying(base::File::Error last_error) {
    DCHECK_NE(last_error, base::File::FILE_OK);
    last_error_ = last_error;
    if (last_ < limit_) {
      base::PlatformThread::Sleep(time_to_sleep_);
      last_ = base::TimeTicks::Now();
      return true;
    }
    success_ = false;
    return false;
  }

 private:
  base::TimeTicks start_;
  base::TimeTicks limit_;
  base::TimeTicks last_;
  base::TimeDelta time_to_sleep_;
  bool success_;
  MethodID method_;
  base::File::Error last_error_;
  RetrierProvider* provider_;
};

class IDBEnvStdio : public ChromiumEnvStdio {
 public:
  IDBEnvStdio() : ChromiumEnvStdio() {
    name_ = "LevelDBEnv.IDB";
    make_backup_ = true;
  }
};

#if defined(OS_WIN)
class IDBEnvWin : public ChromiumEnvWin {
 public:
  IDBEnvWin() : ChromiumEnvWin() {
    name_ = "LevelDBEnv.IDB";
    make_backup_ = true;
  }
};
#endif

#if defined(OS_WIN)
::base::LazyInstance<IDBEnvWin>::Leaky idb_env =
    LAZY_INSTANCE_INITIALIZER;
#else
::base::LazyInstance<IDBEnvStdio>::Leaky idb_env =
    LAZY_INSTANCE_INITIALIZER;
#endif

::base::LazyInstance<ChromiumEnvStdio>::Leaky default_env =
    LAZY_INSTANCE_INITIALIZER;

}  // unnamed namespace

const char* MethodIDToString(MethodID method) {
  switch (method) {
    case kSequentialFileRead:
      return "SequentialFileRead";
    case kSequentialFileSkip:
      return "SequentialFileSkip";
    case kRandomAccessFileRead:
      return "RandomAccessFileRead";
    case kWritableFileAppend:
      return "WritableFileAppend";
    case kWritableFileClose:
      return "WritableFileClose";
    case kWritableFileFlush:
      return "WritableFileFlush";
    case kWritableFileSync:
      return "WritableFileSync";
    case kNewSequentialFile:
      return "NewSequentialFile";
    case kNewRandomAccessFile:
      return "NewRandomAccessFile";
    case kNewWritableFile:
      return "NewWritableFile";
    case kDeleteFile:
      return "DeleteFile";
    case kCreateDir:
      return "CreateDir";
    case kDeleteDir:
      return "DeleteDir";
    case kGetFileSize:
      return "GetFileSize";
    case kRenameFile:
      return "RenameFile";
    case kLockFile:
      return "LockFile";
    case kUnlockFile:
      return "UnlockFile";
    case kGetTestDirectory:
      return "GetTestDirectory";
    case kNewLogger:
      return "NewLogger";
    case kSyncParent:
      return "SyncParent";
    case kGetChildren:
      return "GetChildren";
    case kNumEntries:
      NOTREACHED();
      return "kNumEntries";
  }
  NOTREACHED();
  return "Unknown";
}

Status MakeIOError(Slice filename,
                   const char* message,
                   MethodID method,
                   int saved_errno) {
  char buf[512];
  snprintf(buf,
           sizeof(buf),
           "%s (ChromeMethodErrno: %d::%s::%d)",
           message,
           method,
           MethodIDToString(method),
           saved_errno);
  return Status::IOError(filename, buf);
}

Status MakeIOError(Slice filename,
                   const char* message,
                   MethodID method,
                   base::File::Error error) {
  DCHECK(error < 0);
  char buf[512];
  snprintf(buf,
           sizeof(buf),
           "%s (ChromeMethodPFE: %d::%s::%d)",
           message,
           method,
           MethodIDToString(method),
           -error);
  return Status::IOError(filename, buf);
}

Status MakeIOError(Slice filename, const char* message, MethodID method) {
  char buf[512];
  snprintf(buf,
           sizeof(buf),
           "%s (ChromeMethodOnly: %d::%s)",
           message,
           method,
           MethodIDToString(method));
  return Status::IOError(filename, buf);
}

ErrorParsingResult ParseMethodAndError(const char* string,
                                       MethodID* method_param,
                                       int* error) {
  int method;
  if (RE2::PartialMatch(string, "ChromeMethodOnly: (\\d+)", &method)) {
    *method_param = static_cast<MethodID>(method);
    return METHOD_ONLY;
  }
  if (RE2::PartialMatch(
          string, "ChromeMethodPFE: (\\d+)::.*::(\\d+)", &method, error)) {
    *error = -*error;
    *method_param = static_cast<MethodID>(method);
    return METHOD_AND_PFE;
  }
  if (RE2::PartialMatch(
          string, "ChromeMethodErrno: (\\d+)::.*::(\\d+)", &method, error)) {
    *method_param = static_cast<MethodID>(method);
    return METHOD_AND_ERRNO;
  }
  return NONE;
}

// Keep in sync with LevelDBCorruptionTypes in histograms.xml. Also, don't
// change the order because indices into this array have been recorded in uma
// histograms.
const char* patterns[] = {
  "missing files",
  "log record too small",
  "corrupted internal key",
  "partial record",
  "missing start of fragmented record",
  "error in middle of record",
  "unknown record type",
  "truncated record at end",
  "bad record length",
  "VersionEdit",
  "FileReader invoked with unexpected value",
  "corrupted key",
  "CURRENT file does not end with newline",
  "no meta-nextfile entry",
  "no meta-lognumber entry",
  "no last-sequence-number entry",
  "malformed WriteBatch",
  "bad WriteBatch Put",
  "bad WriteBatch Delete",
  "unknown WriteBatch tag",
  "WriteBatch has wrong count",
  "bad entry in block",
  "bad block contents",
  "bad block handle",
  "truncated block read",
  "block checksum mismatch",
  "checksum mismatch",
  "corrupted compressed block contents",
  "bad block type",
  "bad magic number",
  "file is too short",
};

// Returns 1-based index into the above array or 0 if nothing matches.
int GetCorruptionCode(const leveldb::Status& status) {
  DCHECK(!status.IsIOError());
  DCHECK(!status.ok());
  const int kOtherError = 0;
  int error = kOtherError;
  const std::string& str_error = status.ToString();
  const size_t kNumPatterns = arraysize(patterns);
  for (size_t i = 0; i < kNumPatterns; ++i) {
    if (str_error.find(patterns[i]) != std::string::npos) {
      error = i + 1;
      break;
    }
  }
  return error;
}

int GetNumCorruptionCodes() {
  // + 1 for the "other" error that is returned when a corruption message
  // doesn't match any of the patterns.
  return arraysize(patterns) + 1;
}

std::string GetCorruptionMessage(const leveldb::Status& status) {
  int code = GetCorruptionCode(status);
  if (code == 0)
    return "Unknown corruption";
  return patterns[code - 1];
}

bool IndicatesDiskFull(const leveldb::Status& status) {
  if (status.ok())
    return false;
  leveldb_env::MethodID method;
  int error = -1;
  leveldb_env::ErrorParsingResult result = leveldb_env::ParseMethodAndError(
      status.ToString().c_str(), &method, &error);
  return (result == leveldb_env::METHOD_AND_PFE &&
          static_cast<base::File::Error>(error) ==
              base::File::FILE_ERROR_NO_SPACE) ||
         (result == leveldb_env::METHOD_AND_ERRNO && error == ENOSPC);
}

bool IsIOError(const leveldb::Status& status) {
  leveldb_env::MethodID method;
  int error = -1;
  leveldb_env::ErrorParsingResult result = leveldb_env::ParseMethodAndError(
      status.ToString().c_str(), &method, &error);
  return result != leveldb_env::NONE;
}

bool IsCorruption(const leveldb::Status& status) {
  // LevelDB returns InvalidArgument when an sst file is truncated but there is
  // no IsInvalidArgument() accessor defined.
  return status.IsCorruption() || (!status.ok() && !IsIOError(status));
}

std::string FilePathToString(const base::FilePath& file_path) {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(file_path.value());
#else
  return file_path.value();
#endif
}

base::FilePath ChromiumEnv::CreateFilePath(const std::string& file_path) {
#if defined(OS_WIN)
  return base::FilePath(base::UTF8ToUTF16(file_path));
#else
  return base::FilePath(file_path);
#endif
}

bool ChromiumEnv::MakeBackup(const std::string& fname) {
  base::FilePath original_table_name = CreateFilePath(fname);
  base::FilePath backup_table_name =
      original_table_name.ReplaceExtension(backup_table_extension);
  return base::CopyFile(original_table_name, backup_table_name);
}

bool ChromiumEnv::HasTableExtension(const base::FilePath& path)
{
  return path.MatchesExtension(table_extension);
}

ChromiumEnv::ChromiumEnv()
    : name_("LevelDBEnv"),
      make_backup_(false),
      bgsignal_(&mu_),
      started_bgthread_(false),
      kMaxRetryTimeMillis(1000) {
}

ChromiumEnv::~ChromiumEnv() {
  // In chromium, ChromiumEnv is leaked. It'd be nice to add NOTREACHED here to
  // ensure that behavior isn't accidentally changed, but there's an instance in
  // a unit test that is deleted.
}

bool ChromiumEnv::FileExists(const std::string& fname) {
  return ::base::PathExists(CreateFilePath(fname));
}

const char* ChromiumEnv::FileErrorString(::base::File::Error error) {
  switch (error) {
    case ::base::File::FILE_ERROR_FAILED:
      return "No further details.";
    case ::base::File::FILE_ERROR_IN_USE:
      return "File currently in use.";
    case ::base::File::FILE_ERROR_EXISTS:
      return "File already exists.";
    case ::base::File::FILE_ERROR_NOT_FOUND:
      return "File not found.";
    case ::base::File::FILE_ERROR_ACCESS_DENIED:
      return "Access denied.";
    case ::base::File::FILE_ERROR_TOO_MANY_OPENED:
      return "Too many files open.";
    case ::base::File::FILE_ERROR_NO_MEMORY:
      return "Out of memory.";
    case ::base::File::FILE_ERROR_NO_SPACE:
      return "No space left on drive.";
    case ::base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return "Not a directory.";
    case ::base::File::FILE_ERROR_INVALID_OPERATION:
      return "Invalid operation.";
    case ::base::File::FILE_ERROR_SECURITY:
      return "Security error.";
    case ::base::File::FILE_ERROR_ABORT:
      return "File operation aborted.";
    case ::base::File::FILE_ERROR_NOT_A_FILE:
      return "The supplied path was not a file.";
    case ::base::File::FILE_ERROR_NOT_EMPTY:
      return "The file was not empty.";
    case ::base::File::FILE_ERROR_INVALID_URL:
      return "Invalid URL.";
    case ::base::File::FILE_ERROR_IO:
      return "OS or hardware error.";
    case ::base::File::FILE_OK:
      return "OK.";
    case ::base::File::FILE_ERROR_MAX:
      NOTREACHED();
  }
  NOTIMPLEMENTED();
  return "Unknown error.";
}

base::FilePath ChromiumEnv::RestoreFromBackup(const base::FilePath& base_name) {
  base::FilePath table_name =
      base_name.AddExtension(table_extension);
  bool result = base::CopyFile(base_name.AddExtension(backup_table_extension),
                               table_name);
  std::string uma_name(name_);
  uma_name.append(".TableRestore");
  base::BooleanHistogram::FactoryGet(
      uma_name, base::Histogram::kUmaTargetedHistogramFlag)->AddBoolean(result);
  return table_name;
}

void ChromiumEnv::RestoreIfNecessary(const std::string& dir,
                                     std::vector<std::string>* result) {
  std::set<base::FilePath> tables_found;
  std::set<base::FilePath> backups_found;
  for (std::vector<std::string>::iterator it = result->begin();
       it != result->end();
       ++it) {
    base::FilePath current = CreateFilePath(*it);
    if (current.MatchesExtension(table_extension))
      tables_found.insert(current.RemoveExtension());
    if (current.MatchesExtension(backup_table_extension))
      backups_found.insert(current.RemoveExtension());
  }
  std::set<base::FilePath> backups_only;
  std::set_difference(backups_found.begin(),
                      backups_found.end(),
                      tables_found.begin(),
                      tables_found.end(),
                      std::inserter(backups_only, backups_only.begin()));
  if (backups_only.size()) {
    std::string uma_name(name_);
    uma_name.append(".MissingFiles");
    int num_missing_files =
        backups_only.size() > INT_MAX ? INT_MAX : backups_only.size();
    base::Histogram::FactoryGet(uma_name,
                                1 /*min*/,
                                100 /*max*/,
                                8 /*num_buckets*/,
                                base::Histogram::kUmaTargetedHistogramFlag)
        ->Add(num_missing_files);
  }
  base::FilePath dir_filepath = base::FilePath::FromUTF8Unsafe(dir);
  for (std::set<base::FilePath>::iterator it = backups_only.begin();
       it != backups_only.end();
       ++it) {
    base::FilePath restored_table_name =
        RestoreFromBackup(dir_filepath.Append(*it));
    result->push_back(FilePathToString(restored_table_name.BaseName()));
  }
}

Status ChromiumEnv::GetChildren(const std::string& dir_string,
                                std::vector<std::string>* result) {
  std::vector<base::FilePath> entries;
  base::File::Error error =
      GetDirectoryEntries(CreateFilePath(dir_string), &entries);
  if (error != base::File::FILE_OK) {
    RecordOSError(kGetChildren, error);
    return MakeIOError(
        dir_string, "Could not open/read directory", kGetChildren, error);
  }
  result->clear();
  for (std::vector<base::FilePath>::iterator it = entries.begin();
       it != entries.end();
       ++it) {
    result->push_back(FilePathToString(*it));
  }

  if (make_backup_)
    RestoreIfNecessary(dir_string, result);
  return Status::OK();
}

Status ChromiumEnv::DeleteFile(const std::string& fname) {
  Status result;
  base::FilePath fname_filepath = CreateFilePath(fname);
  // TODO(jorlow): Should we assert this is a file?
  if (!::base::DeleteFile(fname_filepath, false)) {
    result = MakeIOError(fname, "Could not delete file.", kDeleteFile);
    RecordErrorAt(kDeleteFile);
  }
  if (make_backup_ && fname_filepath.MatchesExtension(table_extension)) {
    base::DeleteFile(fname_filepath.ReplaceExtension(backup_table_extension),
                     false);
  }
  return result;
}

Status ChromiumEnv::CreateDir(const std::string& name) {
  Status result;
  base::File::Error error = base::File::FILE_OK;
  Retrier retrier(kCreateDir, this);
  do {
    if (base::CreateDirectoryAndGetError(CreateFilePath(name), &error))
      return result;
  } while (retrier.ShouldKeepTrying(error));
  result = MakeIOError(name, "Could not create directory.", kCreateDir, error);
  RecordOSError(kCreateDir, error);
  return result;
}

Status ChromiumEnv::DeleteDir(const std::string& name) {
  Status result;
  // TODO(jorlow): Should we assert this is a directory?
  if (!::base::DeleteFile(CreateFilePath(name), false)) {
    result = MakeIOError(name, "Could not delete directory.", kDeleteDir);
    RecordErrorAt(kDeleteDir);
  }
  return result;
}

Status ChromiumEnv::GetFileSize(const std::string& fname, uint64_t* size) {
  Status s;
  int64_t signed_size;
  if (!::base::GetFileSize(CreateFilePath(fname), &signed_size)) {
    *size = 0;
    s = MakeIOError(fname, "Could not determine file size.", kGetFileSize);
    RecordErrorAt(kGetFileSize);
  } else {
    *size = static_cast<uint64_t>(signed_size);
  }
  return s;
}

Status ChromiumEnv::RenameFile(const std::string& src, const std::string& dst) {
  Status result;
  base::FilePath src_file_path = CreateFilePath(src);
  if (!::base::PathExists(src_file_path))
    return result;
  base::FilePath destination = CreateFilePath(dst);

  Retrier retrier(kRenameFile, this);
  base::File::Error error = base::File::FILE_OK;
  do {
    if (base::ReplaceFile(src_file_path, destination, &error))
      return result;
  } while (retrier.ShouldKeepTrying(error));

  DCHECK(error != base::File::FILE_OK);
  RecordOSError(kRenameFile, error);
  char buf[100];
  snprintf(buf,
           sizeof(buf),
           "Could not rename file: %s",
           FileErrorString(error));
  return MakeIOError(src, buf, kRenameFile, error);
}

Status ChromiumEnv::LockFile(const std::string& fname, FileLock** lock) {
  *lock = NULL;
  Status result;
  int flags = ::base::File::FLAG_OPEN_ALWAYS |
              ::base::File::FLAG_READ |
              ::base::File::FLAG_WRITE;
  ::base::File::Error error_code;
  ::base::File file;
  Retrier retrier(kLockFile, this);
  do {
    file.Initialize(CreateFilePath(fname), flags);
    if (!file.IsValid())
      error_code = file.error_details();
  } while (!file.IsValid() && retrier.ShouldKeepTrying(error_code));

  if (!file.IsValid()) {
    if (error_code == ::base::File::FILE_ERROR_NOT_FOUND) {
      ::base::FilePath parent = CreateFilePath(fname).DirName();
      ::base::FilePath last_parent;
      int num_missing_ancestors = 0;
      do {
        if (base::DirectoryExists(parent))
          break;
        ++num_missing_ancestors;
        last_parent = parent;
        parent = parent.DirName();
      } while (parent != last_parent);
      RecordLockFileAncestors(num_missing_ancestors);
    }

    result = MakeIOError(fname, FileErrorString(error_code), kLockFile,
                         error_code);
    RecordOSError(kLockFile, error_code);
    return result;
  }

  if (!locks_.Insert(fname)) {
    result = MakeIOError(fname, "Lock file already locked.", kLockFile);
    return result;
  }

  Retrier lock_retrier = Retrier(kLockFile, this);
  do {
    error_code = file.Lock();
  } while (error_code != ::base::File::FILE_OK &&
           retrier.ShouldKeepTrying(error_code));

  if (error_code != ::base::File::FILE_OK) {
    locks_.Remove(fname);
    result = MakeIOError(fname, FileErrorString(error_code), kLockFile,
                         error_code);
    RecordOSError(kLockFile, error_code);
    return result;
  }

  ChromiumFileLock* my_lock = new ChromiumFileLock;
  my_lock->file_ = file.Pass();
  my_lock->name_ = fname;
  *lock = my_lock;
  return result;
}

Status ChromiumEnv::UnlockFile(FileLock* lock) {
  ChromiumFileLock* my_lock = reinterpret_cast<ChromiumFileLock*>(lock);
  Status result;

  ::base::File::Error error_code = my_lock->file_.Unlock();
  if (error_code != ::base::File::FILE_OK) {
    result =
        MakeIOError(my_lock->name_, "Could not unlock lock file.", kUnlockFile);
    RecordOSError(kUnlockFile, error_code);
  }
  bool removed = locks_.Remove(my_lock->name_);
  DCHECK(removed);
  delete my_lock;
  return result;
}

Status ChromiumEnv::GetTestDirectory(std::string* path) {
  mu_.Acquire();
  if (test_directory_.empty()) {
    if (!base::CreateNewTempDirectory(kLevelDBTestDirectoryPrefix,
                                      &test_directory_)) {
      mu_.Release();
      RecordErrorAt(kGetTestDirectory);
      return MakeIOError(
          "Could not create temp directory.", "", kGetTestDirectory);
    }
  }
  *path = FilePathToString(test_directory_);
  mu_.Release();
  return Status::OK();
}

uint64_t ChromiumEnv::NowMicros() {
  return ::base::TimeTicks::Now().ToInternalValue();
}

void ChromiumEnv::SleepForMicroseconds(int micros) {
  // Round up to the next millisecond.
  ::base::PlatformThread::Sleep(::base::TimeDelta::FromMicroseconds(micros));
}

void ChromiumEnv::RecordErrorAt(MethodID method) const {
  GetMethodIOErrorHistogram()->Add(method);
}

void ChromiumEnv::RecordLockFileAncestors(int num_missing_ancestors) const {
  GetLockFileAncestorHistogram()->Add(num_missing_ancestors);
}

void ChromiumEnv::RecordOSError(MethodID method,
                                base::File::Error error) const {
  DCHECK(error < 0);
  RecordErrorAt(method);
  GetOSErrorHistogram(method, -base::File::FILE_ERROR_MAX)->Add(-error);
}

void ChromiumEnv::RecordOSError(MethodID method, int error) const {
  DCHECK(error > 0);
  RecordErrorAt(method);
  GetOSErrorHistogram(method, ERANGE + 1)->Add(error);
}

void ChromiumEnv::RecordBackupResult(bool result) const {
  std::string uma_name(name_);
  uma_name.append(".TableBackup");
  base::BooleanHistogram::FactoryGet(
      uma_name, base::Histogram::kUmaTargetedHistogramFlag)->AddBoolean(result);
}

base::HistogramBase* ChromiumEnv::GetOSErrorHistogram(MethodID method,
                                                      int limit) const {
  std::string uma_name(name_);
  // TODO(dgrogan): This is probably not the best way to concatenate strings.
  uma_name.append(".IOError.").append(MethodIDToString(method));
  return base::LinearHistogram::FactoryGet(uma_name, 1, limit, limit + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetMethodIOErrorHistogram() const {
  std::string uma_name(name_);
  uma_name.append(".IOError");
  return base::LinearHistogram::FactoryGet(uma_name, 1, kNumEntries,
      kNumEntries + 1, base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetMaxFDHistogram(
    const std::string& type) const {
  std::string uma_name(name_);
  uma_name.append(".MaxFDs.").append(type);
  // These numbers make each bucket twice as large as the previous bucket.
  const int kFirstEntry = 1;
  const int kLastEntry = 65536;
  const int kNumBuckets = 18;
  return base::Histogram::FactoryGet(
      uma_name, kFirstEntry, kLastEntry, kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetLockFileAncestorHistogram() const {
  std::string uma_name(name_);
  uma_name.append(".LockFileAncestorsNotFound");
  const int kMin = 1;
  const int kMax = 10;
  const int kNumBuckets = 11;
  return base::LinearHistogram::FactoryGet(
      uma_name, kMin, kMax, kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetRetryTimeHistogram(MethodID method) const {
  std::string uma_name(name_);
  // TODO(dgrogan): This is probably not the best way to concatenate strings.
  uma_name.append(".TimeUntilSuccessFor").append(MethodIDToString(method));

  const int kBucketSizeMillis = 25;
  // Add 2, 1 for each of the buckets <1 and >max.
  const int kNumBuckets = kMaxRetryTimeMillis / kBucketSizeMillis + 2;
  return base::Histogram::FactoryTimeGet(
      uma_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(kMaxRetryTimeMillis + 1),
      kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetRecoveredFromErrorHistogram(
    MethodID method) const {
  std::string uma_name(name_);
  uma_name.append(".RetryRecoveredFromErrorIn")
      .append(MethodIDToString(method));
  return base::LinearHistogram::FactoryGet(uma_name, 1, kNumEntries,
      kNumEntries + 1, base::Histogram::kUmaTargetedHistogramFlag);
}

class Thread : public ::base::PlatformThread::Delegate {
 public:
  Thread(void (*function)(void* arg), void* arg)
      : function_(function), arg_(arg) {
    ::base::PlatformThreadHandle handle;
    bool success = ::base::PlatformThread::Create(0, this, &handle);
    DCHECK(success);
  }
  virtual ~Thread() {}
  virtual void ThreadMain() {
    (*function_)(arg_);
    delete this;
  }

 private:
  void (*function_)(void* arg);
  void* arg_;
};

void ChromiumEnv::Schedule(void (*function)(void*), void* arg) {
  mu_.Acquire();

  // Start background thread if necessary
  if (!started_bgthread_) {
    started_bgthread_ = true;
    StartThread(&ChromiumEnv::BGThreadWrapper, this);
  }

  // If the queue is currently empty, the background thread may currently be
  // waiting.
  if (queue_.empty()) {
    bgsignal_.Signal();
  }

  // Add to priority queue
  queue_.push_back(BGItem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  mu_.Release();
}

void ChromiumEnv::BGThread() {
  base::PlatformThread::SetName(name_.c_str());

  while (true) {
    // Wait until there is an item that is ready to run
    mu_.Acquire();
    while (queue_.empty()) {
      bgsignal_.Wait();
    }

    void (*function)(void*) = queue_.front().function;
    void* arg = queue_.front().arg;
    queue_.pop_front();

    mu_.Release();
    TRACE_EVENT0("leveldb", "ChromiumEnv::BGThread-Task");
    (*function)(arg);
  }
}

void ChromiumEnv::StartThread(void (*function)(void* arg), void* arg) {
  new Thread(function, arg); // Will self-delete.
}

static std::string GetDirName(const std::string& filename) {
  base::FilePath file = base::FilePath::FromUTF8Unsafe(filename);
  return FilePathToString(file.DirName());
}

void ChromiumEnv::DidCreateNewFile(const std::string& filename) {
  base::AutoLock auto_lock(map_lock_);
  needs_sync_map_[GetDirName(filename)] = true;
}

bool ChromiumEnv::DoesDirNeedSync(const std::string& filename) {
  base::AutoLock auto_lock(map_lock_);
  return needs_sync_map_.find(GetDirName(filename)) != needs_sync_map_.end();
}

void ChromiumEnv::DidSyncDir(const std::string& filename) {
  base::AutoLock auto_lock(map_lock_);
  needs_sync_map_.erase(GetDirName(filename));
}

}  // namespace leveldb_env

namespace leveldb {

Env* IDBEnv() {
  return leveldb_env::idb_env.Pointer();
}

Env* Env::Default() {
  return leveldb_env::default_env.Pointer();
}

}  // namespace leveldb

