// Copyright (c) 2011-2013 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <errno.h>

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversions.h"
#include "chromium_logger.h"
#include "env_chromium_stdio.h"

#if defined(OS_WIN)
#include <io.h>
#include "base/win/win_util.h"
#endif

#if defined(OS_POSIX)
#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#endif

using namespace leveldb;

namespace leveldb_env {

namespace {

#if (defined(OS_POSIX) && !defined(OS_LINUX)) || defined(OS_WIN)
// The following are glibc-specific

size_t fread_unlocked(void* ptr, size_t size, size_t n, FILE* file) {
  return fread(ptr, size, n, file);
}

size_t fwrite_unlocked(const void* ptr, size_t size, size_t n, FILE* file) {
  return fwrite(ptr, size, n, file);
}

int fflush_unlocked(FILE* file) { return fflush(file); }

#if !defined(OS_ANDROID)
int fdatasync(int fildes) {
#if defined(OS_WIN)
  return _commit(fildes);
#else
  return HANDLE_EINTR(fsync(fildes));
#endif
}
#endif

#endif

// Wide-char safe fopen wrapper.
FILE* fopen_internal(const char* fname, const char* mode) {
#if defined(OS_WIN)
  return _wfopen(base::UTF8ToUTF16(fname).c_str(),
                 base::ASCIIToUTF16(mode).c_str());
#else
  return fopen(fname, mode);
#endif
}

class ChromiumSequentialFile : public SequentialFile {
 private:
  std::string filename_;
  FILE* file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumSequentialFile(const std::string& fname,
                         FILE* f,
                         const UMALogger* uma_logger)
      : filename_(fname), file_(f), uma_logger_(uma_logger) {}
  virtual ~ChromiumSequentialFile() { fclose(file_); }

  virtual Status Read(size_t n, Slice* result, char* scratch) {
    Status s;
    size_t r = fread_unlocked(scratch, 1, n, file_);
    *result = Slice(scratch, r);
    if (r < n) {
      if (feof(file_)) {
        // We leave status as ok if we hit the end of the file
      } else {
        // A partial read with an error: return a non-ok status
        s = MakeIOError(filename_, strerror(errno), kSequentialFileRead, errno);
        uma_logger_->RecordErrorAt(kSequentialFileRead);
      }
    }
    return s;
  }

  virtual Status Skip(uint64_t n) {
    if (fseek(file_, n, SEEK_CUR)) {
      int saved_errno = errno;
      uma_logger_->RecordErrorAt(kSequentialFileSkip);
      return MakeIOError(
          filename_, strerror(saved_errno), kSequentialFileSkip, saved_errno);
    }
    return Status::OK();
  }
};

class ChromiumRandomAccessFile : public RandomAccessFile {
 private:
  std::string filename_;
  mutable ::base::File file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumRandomAccessFile(const std::string& fname,
                           ::base::File file,
                           const UMALogger* uma_logger)
      : filename_(fname), file_(file.Pass()), uma_logger_(uma_logger) {}
  virtual ~ChromiumRandomAccessFile() {}

  virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch)
      const {
    Status s;
    int r = file_.Read(offset, scratch, n);
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      s = MakeIOError(
          filename_, "Could not perform read", kRandomAccessFileRead);
      uma_logger_->RecordErrorAt(kRandomAccessFileRead);
    }
    return s;
  }
};

}  // unnamed namespace

ChromiumWritableFile::ChromiumWritableFile(const std::string& fname,
                                           FILE* f,
                                           const UMALogger* uma_logger,
                                           WriteTracker* tracker,
                                           bool make_backup)
    : filename_(fname),
      file_(f),
      uma_logger_(uma_logger),
      tracker_(tracker),
      file_type_(kOther),
      make_backup_(make_backup) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(fname);
  if (FilePathToString(path.BaseName()).find("MANIFEST") == 0)
    file_type_ = kManifest;
  else if (ChromiumEnv::HasTableExtension(path))
    file_type_ = kTable;
  if (file_type_ != kManifest)
    tracker_->DidCreateNewFile(filename_);
  parent_dir_ = FilePathToString(ChromiumEnv::CreateFilePath(fname).DirName());
}

ChromiumWritableFile::~ChromiumWritableFile() {
  if (file_ != NULL) {
    // Ignoring any potential errors
    fclose(file_);
  }
}

Status ChromiumWritableFile::SyncParent() {
  Status s;
#if !defined(OS_WIN)
  TRACE_EVENT0("leveldb", "SyncParent");

  int parent_fd = HANDLE_EINTR(open(parent_dir_.c_str(), O_RDONLY));
  if (parent_fd < 0) {
    int saved_errno = errno;
    return MakeIOError(
        parent_dir_, strerror(saved_errno), kSyncParent, saved_errno);
  }
  if (HANDLE_EINTR(fsync(parent_fd)) != 0) {
    int saved_errno = errno;
    s = MakeIOError(
        parent_dir_, strerror(saved_errno), kSyncParent, saved_errno);
  };
  close(parent_fd);
#endif
  return s;
}

Status ChromiumWritableFile::Append(const Slice& data) {
  if (file_type_ == kManifest && tracker_->DoesDirNeedSync(filename_)) {
    Status s = SyncParent();
    if (!s.ok())
      return s;
    tracker_->DidSyncDir(filename_);
  }

  size_t r = fwrite_unlocked(data.data(), 1, data.size(), file_);
  if (r != data.size()) {
    int saved_errno = errno;
    uma_logger_->RecordOSError(kWritableFileAppend, saved_errno);
    return MakeIOError(
        filename_, strerror(saved_errno), kWritableFileAppend, saved_errno);
  }
  return Status::OK();
}

Status ChromiumWritableFile::Close() {
  Status result;
  if (fclose(file_) != 0) {
    result = MakeIOError(filename_, strerror(errno), kWritableFileClose, errno);
    uma_logger_->RecordErrorAt(kWritableFileClose);
  }
  file_ = NULL;
  return result;
}

Status ChromiumWritableFile::Flush() {
  Status result;
  if (HANDLE_EINTR(fflush_unlocked(file_))) {
    int saved_errno = errno;
    result = MakeIOError(
        filename_, strerror(saved_errno), kWritableFileFlush, saved_errno);
    uma_logger_->RecordOSError(kWritableFileFlush, saved_errno);
  }
  return result;
}

Status ChromiumWritableFile::Sync() {
  TRACE_EVENT0("leveldb", "ChromiumEnvStdio::Sync");
  Status result;
  int error = 0;

  if (HANDLE_EINTR(fflush_unlocked(file_)))
    error = errno;
  // Sync even if fflush gave an error; perhaps the data actually got out,
  // even though something went wrong.
  if (fdatasync(fileno(file_)) && !error)
    error = errno;
  // Report the first error we found.
  if (error) {
    result = MakeIOError(filename_, strerror(error), kWritableFileSync, error);
    uma_logger_->RecordErrorAt(kWritableFileSync);
  } else if (make_backup_ && file_type_ == kTable) {
    bool success = ChromiumEnv::MakeBackup(filename_);
    uma_logger_->RecordBackupResult(success);
  }
  return result;
}

ChromiumEnvStdio::ChromiumEnvStdio() {}

ChromiumEnvStdio::~ChromiumEnvStdio() {}

Status ChromiumEnvStdio::NewSequentialFile(const std::string& fname,
                                           SequentialFile** result) {
  FILE* f = fopen_internal(fname.c_str(), "rb");
  if (f == NULL) {
    *result = NULL;
    int saved_errno = errno;
    RecordOSError(kNewSequentialFile, saved_errno);
    return MakeIOError(
        fname, strerror(saved_errno), kNewSequentialFile, saved_errno);
  } else {
    *result = new ChromiumSequentialFile(fname, f, this);
    return Status::OK();
  }
}

void ChromiumEnvStdio::RecordOpenFilesLimit(const std::string& type) {
#if defined(OS_POSIX)
  struct rlimit nofile;
  if (getrlimit(RLIMIT_NOFILE, &nofile))
    return;
  GetMaxFDHistogram(type)->Add(nofile.rlim_cur);
#endif
}

Status ChromiumEnvStdio::NewRandomAccessFile(const std::string& fname,
                                             RandomAccessFile** result) {
  int flags = ::base::File::FLAG_READ | ::base::File::FLAG_OPEN;
  ::base::File file(ChromiumEnv::CreateFilePath(fname), flags);
  if (file.IsValid()) {
    *result = new ChromiumRandomAccessFile(fname, file.Pass(), this);
    RecordOpenFilesLimit("Success");
    return Status::OK();
  }
  ::base::File::Error error_code = file.error_details();
  if (error_code == ::base::File::FILE_ERROR_TOO_MANY_OPENED)
    RecordOpenFilesLimit("TooManyOpened");
  else
    RecordOpenFilesLimit("OtherError");
  *result = NULL;
  RecordOSError(kNewRandomAccessFile, error_code);
  return MakeIOError(fname,
                     FileErrorString(error_code),
                     kNewRandomAccessFile,
                     error_code);
}

Status ChromiumEnvStdio::NewWritableFile(const std::string& fname,
                                         WritableFile** result) {
  *result = NULL;
  FILE* f = fopen_internal(fname.c_str(), "wb");
  if (f == NULL) {
    int saved_errno = errno;
    RecordErrorAt(kNewWritableFile);
    return MakeIOError(
        fname, strerror(saved_errno), kNewWritableFile, saved_errno);
  } else {
    *result = new ChromiumWritableFile(fname, f, this, this, make_backup_);
    return Status::OK();
  }
}

#if defined(OS_WIN)
base::File::Error ChromiumEnvStdio::GetDirectoryEntries(
    const base::FilePath& dir_param,
    std::vector<base::FilePath>* result) const {
  result->clear();
  base::FilePath dir_filepath = dir_param.Append(FILE_PATH_LITERAL("*"));
  WIN32_FIND_DATA find_data;
  HANDLE find_handle = FindFirstFile(dir_filepath.value().c_str(), &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    DWORD last_error = GetLastError();
    if (last_error == ERROR_FILE_NOT_FOUND)
      return base::File::FILE_OK;
    return base::File::OSErrorToFileError(last_error);
  }
  do {
    base::FilePath filepath(find_data.cFileName);
    base::FilePath::StringType basename = filepath.BaseName().value();
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
}
#else
base::File::Error ChromiumEnvStdio::GetDirectoryEntries(
    const base::FilePath& dir_filepath,
    std::vector<base::FilePath>* result) const {
  const std::string dir_string = FilePathToString(dir_filepath);
  result->clear();
  DIR* dir = opendir(dir_string.c_str());
  if (!dir)
    return base::File::OSErrorToFileError(errno);
  struct dirent dent_buf;
  struct dirent* dent;
  int readdir_result;
  while ((readdir_result = readdir_r(dir, &dent_buf, &dent)) == 0 && dent) {
    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;
    result->push_back(ChromiumEnv::CreateFilePath(dent->d_name));
  }
  int saved_errno = errno;
  closedir(dir);
  if (readdir_result != 0)
    return base::File::OSErrorToFileError(saved_errno);
  return base::File::FILE_OK;
}
#endif

Status ChromiumEnvStdio::NewLogger(const std::string& fname, Logger** result) {
  FILE* f = fopen_internal(fname.c_str(), "w");
  if (f == NULL) {
    *result = NULL;
    int saved_errno = errno;
    RecordOSError(kNewLogger, saved_errno);
    return MakeIOError(fname, strerror(saved_errno), kNewLogger, saved_errno);
  } else {
    *result = new ChromiumLogger(f);
    return Status::OK();
  }
}

}  // namespace leveldb_env
