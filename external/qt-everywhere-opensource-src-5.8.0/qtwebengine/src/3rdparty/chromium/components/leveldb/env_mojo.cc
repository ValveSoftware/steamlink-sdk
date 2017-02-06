// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/env_mojo.h"

#include <errno.h>

#include <memory>

#include "base/trace_event/trace_event.h"
#include "third_party/leveldatabase/chromium_logger.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {

namespace {

const base::FilePath::CharType table_extension[] = FILE_PATH_LITERAL(".ldb");

base::File::Error LastFileError() {
#if defined(OS_WIN)
  return base::File::OSErrorToFileError(GetLastError());
#else
  return base::File::OSErrorToFileError(errno);
#endif
}

Status FilesystemErrorToStatus(filesystem::mojom::FileError error,
                               const std::string& filename,
                               leveldb_env::MethodID method) {
  if (error == filesystem::mojom::FileError::OK)
    return Status::OK();

  std::string err_str =
      base::File::ErrorToString(base::File::Error(static_cast<int>(error)));

  char buf[512];
  snprintf(buf, sizeof(buf), "%s (MojoFSError: %d::%s)", err_str.c_str(),
           method, MethodIDToString(method));
  return Status::IOError(filename, buf);
}

class MojoFileLock : public FileLock {
 public:
  MojoFileLock(LevelDBMojoProxy::OpaqueLock* lock, const std::string& name)
      : fname_(name), lock_(lock) {}
  ~MojoFileLock() override { DCHECK(!lock_); }

  const std::string& name() const { return fname_; }

  LevelDBMojoProxy::OpaqueLock* TakeLock() {
    LevelDBMojoProxy::OpaqueLock* to_return = lock_;
    lock_ = nullptr;
    return to_return;
  }

 private:
  std::string fname_;
  LevelDBMojoProxy::OpaqueLock* lock_;
};

class MojoSequentialFile : public leveldb::SequentialFile {
 public:
  MojoSequentialFile(const std::string& fname, base::File f)
      : filename_(fname), file_(std::move(f)) {}
  ~MojoSequentialFile() override {}

  Status Read(size_t n, Slice* result, char* scratch) override {
    int bytes_read = file_.ReadAtCurrentPosNoBestEffort(
        scratch,
        static_cast<int>(n));
    if (bytes_read == -1) {
      base::File::Error error = LastFileError();
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         leveldb_env::kSequentialFileRead, error);
    } else {
      *result = Slice(scratch, bytes_read);
      return Status::OK();
    }
  }

  Status Skip(uint64_t n) override {
    if (file_.Seek(base::File::FROM_CURRENT, n) == -1) {
      base::File::Error error = LastFileError();
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         leveldb_env::kSequentialFileSkip, error);
    } else {
      return Status::OK();
    }
  }

 private:
  std::string filename_;
  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(MojoSequentialFile);
};

class MojoRandomAccessFile : public leveldb::RandomAccessFile {
 public:
  MojoRandomAccessFile(const std::string& fname, base::File file)
      : filename_(fname), file_(std::move(file)) {}
  ~MojoRandomAccessFile() override {}

  Status Read(uint64_t offset,
              size_t n,
              Slice* result,
              char* scratch) const override {
    Status s;
    int r = file_.Read(offset, scratch, static_cast<int>(n));
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      s = MakeIOError(filename_, "Could not perform read",
                      leveldb_env::kRandomAccessFileRead);
    }
    return s;
  }

 private:
  std::string filename_;
  mutable base::File file_;

  DISALLOW_COPY_AND_ASSIGN(MojoRandomAccessFile);
};

class MojoWritableFile : public leveldb::WritableFile {
 public:
  MojoWritableFile(LevelDBMojoProxy::OpaqueDir* dir,
                   const std::string& fname,
                   base::File f,
                   scoped_refptr<LevelDBMojoProxy> thread)
      : filename_(fname),
        file_(std::move(f)),
        file_type_(kOther),
        dir_(dir),
        thread_(thread) {
    base::FilePath path = base::FilePath::FromUTF8Unsafe(fname);
    if (path.BaseName().AsUTF8Unsafe().find("MANIFEST") == 0)
      file_type_ = kManifest;
    else if (path.MatchesExtension(table_extension))
      file_type_ = kTable;
    parent_dir_ =
        base::FilePath::FromUTF8Unsafe(fname).DirName().AsUTF8Unsafe();
  }

  ~MojoWritableFile() override {}

  leveldb::Status Append(const leveldb::Slice& data) override {
    size_t bytes_written = file_.WriteAtCurrentPos(
        data.data(), static_cast<int>(data.size()));
    if (bytes_written != data.size()) {
      base::File::Error error = LastFileError();
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         leveldb_env::kWritableFileAppend, error);
    }

    return Status::OK();
  }

  leveldb::Status Close() override {
    file_.Close();
    return Status::OK();
  }

  leveldb::Status Flush() override {
    // base::File doesn't do buffered I/O (i.e. POSIX FILE streams) so nothing
    // to
    // flush.
    return Status::OK();
  }

  leveldb::Status Sync() override {
    TRACE_EVENT0("leveldb", "MojoWritableFile::Sync");

    if (!file_.Flush()) {
      base::File::Error error = LastFileError();
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         leveldb_env::kWritableFileSync, error);
    }

    // leveldb's implicit contract for Sync() is that if this instance is for a
    // manifest file then the directory is also sync'ed. See leveldb's
    // env_posix.cc.
    if (file_type_ == kManifest)
      return SyncParent();

    return Status::OK();
  }

 private:
  enum Type { kManifest, kTable, kOther };

  leveldb::Status SyncParent() {
    filesystem::mojom::FileError error =
        thread_->SyncDirectory(dir_, parent_dir_);
    return error == filesystem::mojom::FileError::OK
               ? Status::OK()
               : Status::IOError(filename_,
                                 base::File::ErrorToString(base::File::Error(
                                     static_cast<int>(error))));
  }

  std::string filename_;
  base::File file_;
  Type file_type_;
  LevelDBMojoProxy::OpaqueDir* dir_;
  std::string parent_dir_;
  scoped_refptr<LevelDBMojoProxy> thread_;

  DISALLOW_COPY_AND_ASSIGN(MojoWritableFile);
};

}  // namespace

MojoEnv::MojoEnv(scoped_refptr<LevelDBMojoProxy> file_thread,
                 LevelDBMojoProxy::OpaqueDir* dir)
    : thread_(file_thread), dir_(dir) {}

MojoEnv::~MojoEnv() {
  thread_->UnregisterDirectory(dir_);
}

Status MojoEnv::NewSequentialFile(const std::string& fname,
                                  SequentialFile** result) {
  TRACE_EVENT1("leveldb", "MojoEnv::NewSequentialFile", "fname", fname);
  base::File f = thread_->OpenFileHandle(
      dir_, mojo::String::From(fname),
      filesystem::mojom::kFlagOpen | filesystem::mojom::kFlagRead);
  if (!f.IsValid()) {
    *result = nullptr;
    return MakeIOError(fname, "Unable to create sequential file",
                       leveldb_env::kNewSequentialFile, f.error_details());
  }

  *result = new MojoSequentialFile(fname, std::move(f));
  return Status::OK();
}

Status MojoEnv::NewRandomAccessFile(const std::string& fname,
                                    RandomAccessFile** result) {
  TRACE_EVENT1("leveldb", "MojoEnv::NewRandomAccessFile", "fname", fname);
  base::File f = thread_->OpenFileHandle(
      dir_, mojo::String::From(fname),
      filesystem::mojom::kFlagRead | filesystem::mojom::kFlagOpen);
  if (!f.IsValid()) {
    *result = nullptr;
    base::File::Error error_code = f.error_details();
    return MakeIOError(fname, FileErrorString(error_code),
                       leveldb_env::kNewRandomAccessFile, error_code);
  }

  *result = new MojoRandomAccessFile(fname, std::move(f));
  return Status::OK();
}

Status MojoEnv::NewWritableFile(const std::string& fname,
                                WritableFile** result) {
  TRACE_EVENT1("leveldb", "MojoEnv::NewWritableFile", "fname", fname);
  base::File f = thread_->OpenFileHandle(
      dir_, mojo::String::From(fname),
      filesystem::mojom::kCreateAlways | filesystem::mojom::kFlagWrite);
  if (!f.IsValid()) {
    *result = nullptr;
    return MakeIOError(fname, "Unable to create writable file",
                       leveldb_env::kNewWritableFile, f.error_details());
  }

  *result = new MojoWritableFile(dir_, fname, std::move(f), thread_);
  return Status::OK();
}

Status MojoEnv::NewAppendableFile(const std::string& fname,
                                  WritableFile** result) {
  TRACE_EVENT1("leveldb", "MojoEnv::NewAppendableFile", "fname", fname);
  base::File f = thread_->OpenFileHandle(
      dir_, mojo::String::From(fname),
      filesystem::mojom::kFlagOpenAlways | filesystem::mojom::kFlagAppend);
  if (!f.IsValid()) {
    *result = nullptr;
    return MakeIOError(fname, "Unable to create appendable file",
                       leveldb_env::kNewAppendableFile, f.error_details());
  }

  *result = new MojoWritableFile(dir_, fname, std::move(f), thread_);
  return Status::OK();
}

bool MojoEnv::FileExists(const std::string& fname) {
  TRACE_EVENT1("leveldb", "MojoEnv::FileExists", "fname", fname);
  return thread_->FileExists(dir_, fname);
}

Status MojoEnv::GetChildren(const std::string& path,
                            std::vector<std::string>* result) {
  TRACE_EVENT1("leveldb", "MojoEnv::GetChildren", "path", path);
  return FilesystemErrorToStatus(thread_->GetChildren(dir_, path, result), path,
                                 leveldb_env::kGetChildren);
}

Status MojoEnv::DeleteFile(const std::string& fname) {
  TRACE_EVENT1("leveldb", "MojoEnv::DeleteFile", "fname", fname);
  return FilesystemErrorToStatus(thread_->Delete(dir_, fname, 0), fname,
                                 leveldb_env::kDeleteFile);
}

Status MojoEnv::CreateDir(const std::string& dirname) {
  TRACE_EVENT1("leveldb", "MojoEnv::CreateDir", "dirname", dirname);
  return FilesystemErrorToStatus(thread_->CreateDir(dir_, dirname), dirname,
                                 leveldb_env::kCreateDir);
}

Status MojoEnv::DeleteDir(const std::string& dirname) {
  TRACE_EVENT1("leveldb", "MojoEnv::DeleteDir", "dirname", dirname);
  return FilesystemErrorToStatus(
      thread_->Delete(dir_, dirname, filesystem::mojom::kDeleteFlagRecursive),
      dirname, leveldb_env::kDeleteDir);
}

Status MojoEnv::GetFileSize(const std::string& fname, uint64_t* file_size) {
  TRACE_EVENT1("leveldb", "MojoEnv::GetFileSize", "fname", fname);
  return FilesystemErrorToStatus(thread_->GetFileSize(dir_, fname, file_size),
                                 fname,
                                 leveldb_env::kGetFileSize);
}

Status MojoEnv::RenameFile(const std::string& src, const std::string& target) {
  TRACE_EVENT2("leveldb", "MojoEnv::RenameFile", "src", src, "target", target);
  return FilesystemErrorToStatus(thread_->RenameFile(dir_, src, target), src,
                                 leveldb_env::kRenameFile);
}

Status MojoEnv::LockFile(const std::string& fname, FileLock** lock) {
  TRACE_EVENT1("leveldb", "MojoEnv::LockFile", "fname", fname);

  std::pair<filesystem::mojom::FileError, LevelDBMojoProxy::OpaqueLock*> p =
      thread_->LockFile(dir_, mojo::String::From(fname));

  if (p.second)
    *lock = new MojoFileLock(p.second, fname);

  return FilesystemErrorToStatus(p.first, fname, leveldb_env::kLockFile);
}

Status MojoEnv::UnlockFile(FileLock* lock) {
  MojoFileLock* my_lock = reinterpret_cast<MojoFileLock*>(lock);

  std::string fname = my_lock ? my_lock->name() : "(invalid)";
  TRACE_EVENT1("leveldb", "MojoEnv::UnlockFile", "fname", fname);

  filesystem::mojom::FileError err = thread_->UnlockFile(my_lock->TakeLock());
  delete my_lock;
  return FilesystemErrorToStatus(err, fname, leveldb_env::kUnlockFile);
}

Status MojoEnv::GetTestDirectory(std::string* path) {
  // TODO(erg): This method is actually only used from the test harness in
  // leveldb. And when we go and port that test stuff to a mojo apptest, we
  // probably won't use it since the mojo filesystem actually handles temporary
  // filesystems just fine.
  NOTREACHED();
  return Status::OK();
}

Status MojoEnv::NewLogger(const std::string& fname, Logger** result) {
  TRACE_EVENT1("leveldb", "MojoEnv::NewLogger", "fname", fname);
  std::unique_ptr<base::File> f(new base::File(thread_->OpenFileHandle(
      dir_, mojo::String::From(fname),
      filesystem::mojom::kCreateAlways | filesystem::mojom::kFlagWrite)));
  if (!f->IsValid()) {
    *result = NULL;
    return MakeIOError(fname, "Unable to create log file",
                       leveldb_env::kNewLogger, f->error_details());
  } else {
    *result = new leveldb::ChromiumLogger(f.release());
    return Status::OK();
  }
}

}  // namespace leveldb
