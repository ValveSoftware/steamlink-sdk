// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "third_party/leveldatabase/env_chromium.h"

#include <memory>
#include <utility>

#if defined(OS_POSIX)
#include <dirent.h>
#include <sys/types.h>
#endif

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/process/process_metrics.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/leveldatabase/chromium_logger.h"
#include "third_party/re2/src/re2/re2.h"

using base::FilePath;
using leveldb::FileLock;
using leveldb::Slice;
using leveldb::Status;

namespace leveldb_env {

namespace {

const FilePath::CharType backup_table_extension[] = FILE_PATH_LITERAL(".bak");
const FilePath::CharType table_extension[] = FILE_PATH_LITERAL(".ldb");

static const FilePath::CharType kLevelDBTestDirectoryPrefix[] =
    FILE_PATH_LITERAL("leveldb-test-");

static base::File::Error LastFileError() {
#if defined(OS_WIN)
  return base::File::OSErrorToFileError(GetLastError());
#else
  return base::File::OSErrorToFileError(errno);
#endif
}

// Making direct platform in lieu of using base::FileEnumerator because the
// latter can fail quietly without return an error result.
static base::File::Error GetDirectoryEntries(const FilePath& dir_param,
                                             std::vector<FilePath>* result) {
  base::ThreadRestrictions::AssertIOAllowed();
  result->clear();
#if defined(OS_WIN)
  FilePath dir_filepath = dir_param.Append(FILE_PATH_LITERAL("*"));
  WIN32_FIND_DATA find_data;
  HANDLE find_handle = FindFirstFile(dir_filepath.value().c_str(), &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    DWORD last_error = GetLastError();
    if (last_error == ERROR_FILE_NOT_FOUND)
      return base::File::FILE_OK;
    return base::File::OSErrorToFileError(last_error);
  }
  do {
    FilePath filepath(find_data.cFileName);
    FilePath::StringType basename = filepath.BaseName().value();
    if (basename == FILE_PATH_LITERAL(".") ||
        basename == FILE_PATH_LITERAL(".."))
      continue;
    result->push_back(filepath.BaseName());
  } while (FindNextFile(find_handle, &find_data));
  DWORD last_error = GetLastError();
  base::File::Error return_value = base::File::FILE_OK;
  if (last_error != ERROR_NO_MORE_FILES)
    return_value = base::File::OSErrorToFileError(last_error);
  FindClose(find_handle);
  return return_value;
#else
  const std::string dir_string = dir_param.AsUTF8Unsafe();
  DIR* dir = opendir(dir_string.c_str());
  if (!dir)
    return base::File::OSErrorToFileError(errno);
  struct dirent dent_buf;
  struct dirent* dent;
  int readdir_result;
  while ((readdir_result = readdir_r(dir, &dent_buf, &dent)) == 0 && dent) {
    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;
    result->push_back(FilePath::FromUTF8Unsafe(dent->d_name));
  }
  int saved_errno = errno;
  closedir(dir);
  if (readdir_result != 0)
    return base::File::OSErrorToFileError(saved_errno);
  return base::File::FILE_OK;
#endif
}

class ChromiumFileLock : public FileLock {
 public:
  base::File file_;
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
        DCHECK_LT(last_error_, 0);
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

class ChromiumSequentialFile : public leveldb::SequentialFile {
 public:
  ChromiumSequentialFile(const std::string& fname,
                         base::File* f,
                         const UMALogger* uma_logger)
      : filename_(fname), file_(f), uma_logger_(uma_logger) {}
  virtual ~ChromiumSequentialFile() {}

  Status Read(size_t n, Slice* result, char* scratch) override {
    int bytes_read = file_->ReadAtCurrentPosNoBestEffort(scratch, n);
    if (bytes_read == -1) {
      base::File::Error error = LastFileError();
      uma_logger_->RecordErrorAt(kSequentialFileRead);
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         kSequentialFileRead, error);
    } else {
      *result = Slice(scratch, bytes_read);
      return Status::OK();
    }
  }

  Status Skip(uint64_t n) override {
    if (file_->Seek(base::File::FROM_CURRENT, n) == -1) {
      base::File::Error error = LastFileError();
      uma_logger_->RecordErrorAt(kSequentialFileSkip);
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         kSequentialFileSkip, error);
    } else {
      return Status::OK();
    }
  }

 private:
  std::string filename_;
  std::unique_ptr<base::File> file_;
  const UMALogger* uma_logger_;
};

class ChromiumRandomAccessFile : public leveldb::RandomAccessFile {
 public:
  ChromiumRandomAccessFile(const std::string& fname,
                           base::File file,
                           const UMALogger* uma_logger)
      : filename_(fname), file_(std::move(file)), uma_logger_(uma_logger) {}
  virtual ~ChromiumRandomAccessFile() {}

  Status Read(uint64_t offset,
              size_t n,
              Slice* result,
              char* scratch) const override {
    Status s;
    int r = file_.Read(offset, scratch, n);
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      s = MakeIOError(filename_, "Could not perform read",
                      kRandomAccessFileRead);
      uma_logger_->RecordErrorAt(kRandomAccessFileRead);
    }
    return s;
  }

 private:
  std::string filename_;
  mutable base::File file_;
  const UMALogger* uma_logger_;
};

class ChromiumWritableFile : public leveldb::WritableFile {
 public:
  ChromiumWritableFile(const std::string& fname,
                       base::File* f,
                       const UMALogger* uma_logger,
                       bool make_backup);
  virtual ~ChromiumWritableFile() {}
  leveldb::Status Append(const leveldb::Slice& data) override;
  leveldb::Status Close() override;
  leveldb::Status Flush() override;
  leveldb::Status Sync() override;

 private:
  enum Type { kManifest, kTable, kOther };
  leveldb::Status SyncParent();

  std::string filename_;
  std::unique_ptr<base::File> file_;
  const UMALogger* uma_logger_;
  Type file_type_;
  std::string parent_dir_;
  bool make_backup_;
};

ChromiumWritableFile::ChromiumWritableFile(const std::string& fname,
                                           base::File* f,
                                           const UMALogger* uma_logger,
                                           bool make_backup)
    : filename_(fname),
      file_(f),
      uma_logger_(uma_logger),
      file_type_(kOther),
      make_backup_(make_backup) {
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  if (path.BaseName().AsUTF8Unsafe().find("MANIFEST") == 0)
    file_type_ = kManifest;
  else if (path.MatchesExtension(table_extension))
    file_type_ = kTable;
  parent_dir_ = FilePath::FromUTF8Unsafe(fname).DirName().AsUTF8Unsafe();
}

Status ChromiumWritableFile::SyncParent() {
  TRACE_EVENT0("leveldb", "SyncParent");
#if defined(OS_POSIX)
  FilePath path = FilePath::FromUTF8Unsafe(parent_dir_);
  base::File f(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!f.IsValid()) {
    return MakeIOError(parent_dir_, "Unable to open directory", kSyncParent,
                       f.error_details());
  }
  if (!f.Flush()) {
    base::File::Error error = LastFileError();
    return MakeIOError(parent_dir_, base::File::ErrorToString(error),
                       kSyncParent, error);
  }
#endif
  return Status::OK();
}

Status ChromiumWritableFile::Append(const Slice& data) {
  int bytes_written = file_->WriteAtCurrentPos(data.data(), data.size());
  if (bytes_written != data.size()) {
    base::File::Error error = LastFileError();
    uma_logger_->RecordOSError(kWritableFileAppend, error);
    return MakeIOError(filename_, base::File::ErrorToString(error),
                       kWritableFileAppend, error);
  }

  return Status::OK();
}

Status ChromiumWritableFile::Close() {
  file_->Close();
  return Status::OK();
}

Status ChromiumWritableFile::Flush() {
  // base::File doesn't do buffered I/O (i.e. POSIX FILE streams) so nothing to
  // flush.
  return Status::OK();
}

Status ChromiumWritableFile::Sync() {
  TRACE_EVENT0("leveldb", "WritableFile::Sync");

  if (!file_->Flush()) {
    base::File::Error error = LastFileError();
    uma_logger_->RecordErrorAt(kWritableFileSync);
    return MakeIOError(filename_, base::File::ErrorToString(error),
                       kWritableFileSync, error);
  }

  if (make_backup_ && file_type_ == kTable)
    uma_logger_->RecordBackupResult(ChromiumEnv::MakeBackup(filename_));

  // leveldb's implicit contract for Sync() is that if this instance is for a
  // manifest file then the directory is also sync'ed. See leveldb's
  // env_posix.cc.
  if (file_type_ == kManifest)
    return SyncParent();

  return Status::OK();
}

base::LazyInstance<ChromiumEnv>::Leaky default_env = LAZY_INSTANCE_INITIALIZER;

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
    case kNewAppendableFile:
      return "NewAppendableFile";
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
                   const std::string& message,
                   MethodID method,
                   base::File::Error error) {
  DCHECK_LT(error, 0);
  char buf[512];
  snprintf(buf, sizeof(buf), "%s (ChromeMethodBFE: %d::%s::%d)",
           message.c_str(), method, MethodIDToString(method), -error);
  return Status::IOError(filename, buf);
}

Status MakeIOError(Slice filename,
                   const std::string& message,
                   MethodID method) {
  char buf[512];
  snprintf(buf, sizeof(buf), "%s (ChromeMethodOnly: %d::%s)", message.c_str(),
           method, MethodIDToString(method));
  return Status::IOError(filename, buf);
}

ErrorParsingResult ParseMethodAndError(const leveldb::Status& status,
                                       MethodID* method_param,
                                       base::File::Error* error) {
  const std::string status_string = status.ToString();
  int method;
  if (RE2::PartialMatch(status_string.c_str(), "ChromeMethodOnly: (\\d+)",
                        &method)) {
    *method_param = static_cast<MethodID>(method);
    return METHOD_ONLY;
  }
  int parsed_error;
  if (RE2::PartialMatch(status_string.c_str(),
                        "ChromeMethodBFE: (\\d+)::.*::(\\d+)", &method,
                        &parsed_error)) {
    *method_param = static_cast<MethodID>(method);
    *error = static_cast<base::File::Error>(-parsed_error);
    DCHECK_LT(*error, base::File::FILE_OK);
    DCHECK_GT(*error, base::File::FILE_ERROR_MAX);
    return METHOD_AND_BFE;
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
  base::File::Error error = base::File::FILE_OK;
  leveldb_env::ErrorParsingResult result =
      leveldb_env::ParseMethodAndError(status, &method, &error);
  return (result == leveldb_env::METHOD_AND_BFE &&
          static_cast<base::File::Error>(error) ==
              base::File::FILE_ERROR_NO_SPACE);
}

bool ChromiumEnv::MakeBackup(const std::string& fname) {
  FilePath original_table_name = FilePath::FromUTF8Unsafe(fname);
  FilePath backup_table_name =
      original_table_name.ReplaceExtension(backup_table_extension);
  return base::CopyFile(original_table_name, backup_table_name);
}

ChromiumEnv::ChromiumEnv()
    : ChromiumEnv("LevelDBEnv", false /* make_backup */) {}

ChromiumEnv::ChromiumEnv(const std::string& name, bool make_backup)
    : kMaxRetryTimeMillis(1000),
      name_(name),
      make_backup_(make_backup),
      bgsignal_(&mu_),
      started_bgthread_(false) {
  uma_ioerror_base_name_ = name_ + ".IOError.BFE";
}

ChromiumEnv::~ChromiumEnv() {
  // In chromium, ChromiumEnv is leaked. It'd be nice to add NOTREACHED here to
  // ensure that behavior isn't accidentally changed, but there's an instance in
  // a unit test that is deleted.
}

bool ChromiumEnv::FileExists(const std::string& fname) {
  return base::PathExists(FilePath::FromUTF8Unsafe(fname));
}

const char* ChromiumEnv::FileErrorString(base::File::Error error) {
  switch (error) {
    case base::File::FILE_ERROR_FAILED:
      return "No further details.";
    case base::File::FILE_ERROR_IN_USE:
      return "File currently in use.";
    case base::File::FILE_ERROR_EXISTS:
      return "File already exists.";
    case base::File::FILE_ERROR_NOT_FOUND:
      return "File not found.";
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return "Access denied.";
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return "Too many files open.";
    case base::File::FILE_ERROR_NO_MEMORY:
      return "Out of memory.";
    case base::File::FILE_ERROR_NO_SPACE:
      return "No space left on drive.";
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return "Not a directory.";
    case base::File::FILE_ERROR_INVALID_OPERATION:
      return "Invalid operation.";
    case base::File::FILE_ERROR_SECURITY:
      return "Security error.";
    case base::File::FILE_ERROR_ABORT:
      return "File operation aborted.";
    case base::File::FILE_ERROR_NOT_A_FILE:
      return "The supplied path was not a file.";
    case base::File::FILE_ERROR_NOT_EMPTY:
      return "The file was not empty.";
    case base::File::FILE_ERROR_INVALID_URL:
      return "Invalid URL.";
    case base::File::FILE_ERROR_IO:
      return "OS or hardware error.";
    case base::File::FILE_OK:
      return "OK.";
    case base::File::FILE_ERROR_MAX:
      NOTREACHED();
  }
  NOTIMPLEMENTED();
  return "Unknown error.";
}

FilePath ChromiumEnv::RestoreFromBackup(const FilePath& base_name) {
  FilePath table_name = base_name.AddExtension(table_extension);
  bool result = base::CopyFile(base_name.AddExtension(backup_table_extension),
                               table_name);
  std::string uma_name(name_);
  uma_name.append(".TableRestore");
  base::BooleanHistogram::FactoryGet(
      uma_name, base::Histogram::kUmaTargetedHistogramFlag)->AddBoolean(result);
  return table_name;
}

void ChromiumEnv::RestoreIfNecessary(const std::string& dir,
                                     std::vector<std::string>* dir_entries) {
  std::set<FilePath> tables_found;
  std::set<FilePath> backups_found;
  for (const std::string& entry : *dir_entries) {
    FilePath current = FilePath::FromUTF8Unsafe(entry);
    if (current.MatchesExtension(table_extension))
      tables_found.insert(current.RemoveExtension());
    if (current.MatchesExtension(backup_table_extension))
      backups_found.insert(current.RemoveExtension());
  }
  std::set<FilePath> backups_only =
      base::STLSetDifference<std::set<FilePath>>(backups_found, tables_found);

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
  FilePath dir_path = FilePath::FromUTF8Unsafe(dir);
  for (const FilePath& backup : backups_only) {
    FilePath restored_table_name = RestoreFromBackup(dir_path.Append(backup));
    dir_entries->push_back(restored_table_name.BaseName().AsUTF8Unsafe());
  }
}

Status ChromiumEnv::GetChildren(const std::string& dir,
                                std::vector<std::string>* result) {
  std::vector<FilePath> entries;
  base::File::Error error =
      GetDirectoryEntries(FilePath::FromUTF8Unsafe(dir), &entries);
  if (error != base::File::FILE_OK) {
    RecordOSError(kGetChildren, error);
    return MakeIOError(dir, "Could not open/read directory", kGetChildren,
                       error);
  }

  result->clear();
  for (const auto& entry : entries)
    result->push_back(entry.BaseName().AsUTF8Unsafe());

  if (make_backup_)
    RestoreIfNecessary(dir, result);

  return Status::OK();
}

Status ChromiumEnv::DeleteFile(const std::string& fname) {
  Status result;
  FilePath fname_filepath = FilePath::FromUTF8Unsafe(fname);
  // TODO(jorlow): Should we assert this is a file?
  if (!base::DeleteFile(fname_filepath, false)) {
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
    if (base::CreateDirectoryAndGetError(FilePath::FromUTF8Unsafe(name),
                                         &error))
      return result;
  } while (retrier.ShouldKeepTrying(error));
  result = MakeIOError(name, "Could not create directory.", kCreateDir, error);
  RecordOSError(kCreateDir, error);
  return result;
}

Status ChromiumEnv::DeleteDir(const std::string& name) {
  Status result;
  // TODO(jorlow): Should we assert this is a directory?
  if (!base::DeleteFile(FilePath::FromUTF8Unsafe(name), false)) {
    result = MakeIOError(name, "Could not delete directory.", kDeleteDir);
    RecordErrorAt(kDeleteDir);
  }
  return result;
}

Status ChromiumEnv::GetFileSize(const std::string& fname, uint64_t* size) {
  Status s;
  int64_t signed_size;
  if (!base::GetFileSize(FilePath::FromUTF8Unsafe(fname), &signed_size)) {
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
  FilePath src_file_path = FilePath::FromUTF8Unsafe(src);
  if (!base::PathExists(src_file_path))
    return result;
  FilePath destination = FilePath::FromUTF8Unsafe(dst);

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
  int flags = base::File::FLAG_OPEN_ALWAYS |
              base::File::FLAG_READ |
              base::File::FLAG_WRITE;
  base::File::Error error_code;
  base::File file;
  Retrier retrier(kLockFile, this);
  do {
    file.Initialize(FilePath::FromUTF8Unsafe(fname), flags);
    if (!file.IsValid())
      error_code = file.error_details();
  } while (!file.IsValid() && retrier.ShouldKeepTrying(error_code));

  if (!file.IsValid()) {
    if (error_code == base::File::FILE_ERROR_NOT_FOUND) {
      FilePath parent = FilePath::FromUTF8Unsafe(fname).DirName();
      FilePath last_parent;
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
  } while (error_code != base::File::FILE_OK &&
           retrier.ShouldKeepTrying(error_code));

  if (error_code != base::File::FILE_OK) {
    locks_.Remove(fname);
    result = MakeIOError(fname, FileErrorString(error_code), kLockFile,
                         error_code);
    RecordOSError(kLockFile, error_code);
    return result;
  }

  ChromiumFileLock* my_lock = new ChromiumFileLock;
  my_lock->file_ = std::move(file);
  my_lock->name_ = fname;
  *lock = my_lock;
  return result;
}

Status ChromiumEnv::UnlockFile(FileLock* lock) {
  ChromiumFileLock* my_lock = reinterpret_cast<ChromiumFileLock*>(lock);
  Status result;

  base::File::Error error_code = my_lock->file_.Unlock();
  if (error_code != base::File::FILE_OK) {
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
  *path = test_directory_.AsUTF8Unsafe();
  mu_.Release();
  return Status::OK();
}

Status ChromiumEnv::NewLogger(const std::string& fname,
                              leveldb::Logger** result) {
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  std::unique_ptr<base::File> f(new base::File(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
  if (!f->IsValid()) {
    *result = NULL;
    RecordOSError(kNewLogger, f->error_details());
    return MakeIOError(fname, "Unable to create log file", kNewLogger,
                       f->error_details());
  } else {
    *result = new leveldb::ChromiumLogger(f.release());
    return Status::OK();
  }
}

Status ChromiumEnv::NewSequentialFile(const std::string& fname,
                                      leveldb::SequentialFile** result) {
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  std::unique_ptr<base::File> f(
      new base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  if (!f->IsValid()) {
    *result = NULL;
    RecordOSError(kNewSequentialFile, f->error_details());
    return MakeIOError(fname, "Unable to create sequential file",
                       kNewSequentialFile, f->error_details());
  } else {
    *result = new ChromiumSequentialFile(fname, f.release(), this);
    return Status::OK();
  }
}

void ChromiumEnv::RecordOpenFilesLimit(const std::string& type) {
#if defined(OS_POSIX)
  GetMaxFDHistogram(type)->Add(base::GetMaxFds());
#elif defined(OS_WIN)
// Windows is only limited by available memory
#else
#error "Need to determine limit to open files for this OS"
#endif
}

Status ChromiumEnv::NewRandomAccessFile(const std::string& fname,
                                        leveldb::RandomAccessFile** result) {
  int flags = base::File::FLAG_READ | base::File::FLAG_OPEN;
  base::File file(FilePath::FromUTF8Unsafe(fname), flags);
  if (file.IsValid()) {
    *result = new ChromiumRandomAccessFile(fname, std::move(file), this);
    RecordOpenFilesLimit("Success");
    return Status::OK();
  }
  base::File::Error error_code = file.error_details();
  if (error_code == base::File::FILE_ERROR_TOO_MANY_OPENED)
    RecordOpenFilesLimit("TooManyOpened");
  else
    RecordOpenFilesLimit("OtherError");
  *result = NULL;
  RecordOSError(kNewRandomAccessFile, error_code);
  return MakeIOError(fname, FileErrorString(error_code), kNewRandomAccessFile,
                     error_code);
}

Status ChromiumEnv::NewWritableFile(const std::string& fname,
                                    leveldb::WritableFile** result) {
  *result = NULL;
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  std::unique_ptr<base::File> f(new base::File(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
  if (!f->IsValid()) {
    RecordErrorAt(kNewWritableFile);
    return MakeIOError(fname, "Unable to create writable file",
                       kNewWritableFile, f->error_details());
  } else {
    *result = new ChromiumWritableFile(fname, f.release(), this, make_backup_);
    return Status::OK();
  }
}

Status ChromiumEnv::NewAppendableFile(const std::string& fname,
                                      leveldb::WritableFile** result) {
  *result = NULL;
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  std::unique_ptr<base::File> f(new base::File(
      path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND));
  if (!f->IsValid()) {
    RecordErrorAt(kNewAppendableFile);
    return MakeIOError(fname, "Unable to create appendable file",
                       kNewAppendableFile, f->error_details());
  }
  *result = new ChromiumWritableFile(fname, f.release(), this, make_backup_);
  return Status::OK();
}

uint64_t ChromiumEnv::NowMicros() {
  return base::TimeTicks::Now().ToInternalValue();
}

void ChromiumEnv::SleepForMicroseconds(int micros) {
  // Round up to the next millisecond.
  base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(micros));
}

void ChromiumEnv::RecordErrorAt(MethodID method) const {
  GetMethodIOErrorHistogram()->Add(method);
}

void ChromiumEnv::RecordLockFileAncestors(int num_missing_ancestors) const {
  GetLockFileAncestorHistogram()->Add(num_missing_ancestors);
}

void ChromiumEnv::RecordOSError(MethodID method,
                                base::File::Error error) const {
  DCHECK_LT(error, 0);
  RecordErrorAt(method);
  GetOSErrorHistogram(method, -base::File::FILE_ERROR_MAX)->Add(-error);
}

void ChromiumEnv::RecordBackupResult(bool result) const {
  std::string uma_name(name_);
  uma_name.append(".TableBackup");
  base::BooleanHistogram::FactoryGet(
      uma_name, base::Histogram::kUmaTargetedHistogramFlag)->AddBoolean(result);
}

base::HistogramBase* ChromiumEnv::GetOSErrorHistogram(MethodID method,
                                                      int limit) const {
  std::string uma_name;
  base::StringAppendF(&uma_name, "%s.%s", uma_ioerror_base_name_.c_str(),
                      MethodIDToString(method));
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

class Thread : public base::PlatformThread::Delegate {
 public:
  Thread(void (*function)(void* arg), void* arg)
      : function_(function), arg_(arg) {
    base::PlatformThreadHandle handle;
    bool success = base::PlatformThread::Create(0, this, &handle);
    DCHECK(success);
  }
  virtual ~Thread() {}
  void ThreadMain() override {
    (*function_)(arg_);
    delete this;
  }

 private:
  void (*function_)(void* arg);
  void* arg_;
};

void ChromiumEnv::Schedule(ScheduleFunc* function, void* arg) {
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
  new Thread(function, arg);  // Will self-delete.
}

LevelDBStatusValue GetLevelDBStatusUMAValue(const leveldb::Status& s) {
  if (s.ok())
    return LEVELDB_STATUS_OK;
  if (s.IsNotFound())
    return LEVELDB_STATUS_NOT_FOUND;
  if (s.IsCorruption())
    return LEVELDB_STATUS_CORRUPTION;
  if (s.IsNotSupportedError())
    return LEVELDB_STATUS_NOT_SUPPORTED;
  if (s.IsIOError())
    return LEVELDB_STATUS_IO_ERROR;
  // TODO(cmumford): IsInvalidArgument() was just added to leveldb. Use this
  // function once that change goes to the public repository.
  return LEVELDB_STATUS_INVALID_ARGUMENT;
}

}  // namespace leveldb_env

namespace leveldb {

Env* Env::Default() {
  return leveldb_env::default_env.Pointer();
}

}  // namespace leveldb
