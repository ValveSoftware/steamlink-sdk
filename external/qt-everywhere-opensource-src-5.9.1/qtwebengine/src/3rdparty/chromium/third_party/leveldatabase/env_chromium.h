// Copyright (c) 2013 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_H_
#define THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_H_

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "leveldb/env.h"
#include "port/port_chromium.h"
#include "util/mutexlock.h"

namespace leveldb_env {

// These entries map to values in tools/metrics/histograms/histograms.xml. New
// values should be appended at the end.
enum MethodID {
  kSequentialFileRead,
  kSequentialFileSkip,
  kRandomAccessFileRead,
  kWritableFileAppend,
  kWritableFileClose,
  kWritableFileFlush,
  kWritableFileSync,
  kNewSequentialFile,
  kNewRandomAccessFile,
  kNewWritableFile,
  kDeleteFile,
  kCreateDir,
  kDeleteDir,
  kGetFileSize,
  kRenameFile,
  kLockFile,
  kUnlockFile,
  kGetTestDirectory,
  kNewLogger,
  kSyncParent,
  kGetChildren,
  kNewAppendableFile,
  kNumEntries
};

// leveldb::Status::Code values are mapped to these values for UMA logging.
// Do not change/delete these values as you will break reporting for older
// copies of Chrome. Only add new values to the end.
enum LevelDBStatusValue {
  LEVELDB_STATUS_OK = 0,
  LEVELDB_STATUS_NOT_FOUND,
  LEVELDB_STATUS_CORRUPTION,
  LEVELDB_STATUS_NOT_SUPPORTED,
  LEVELDB_STATUS_INVALID_ARGUMENT,
  LEVELDB_STATUS_IO_ERROR,
  LEVELDB_STATUS_MAX
};

LevelDBStatusValue GetLevelDBStatusUMAValue(const leveldb::Status& s);

// The default value for leveldb::Options::reuse_logs. Currently log reuse is an
// experimental feature in leveldb. More info at:
// https://github.com/google/leveldb/commit/251ebf5dc70129ad3
#if defined(OS_CHROMEOS)
// Reusing logs on Chrome OS resulted in an unacceptably high leveldb corruption
// rate (at least for Indexed DB). More info at https://crbug.com/460568
const bool kDefaultLogReuseOptionValue = false;
#else
const bool kDefaultLogReuseOptionValue = true;
#endif

const char* MethodIDToString(MethodID method);

leveldb::Status MakeIOError(leveldb::Slice filename,
                            const std::string& message,
                            MethodID method,
                            base::File::Error error);
leveldb::Status MakeIOError(leveldb::Slice filename,
                            const std::string& message,
                            MethodID method);

enum ErrorParsingResult {
  METHOD_ONLY,
  METHOD_AND_BFE,
  NONE,
};

ErrorParsingResult ParseMethodAndError(const leveldb::Status& status,
                                       MethodID* method,
                                       base::File::Error* error);
int GetCorruptionCode(const leveldb::Status& status);
int GetNumCorruptionCodes();
std::string GetCorruptionMessage(const leveldb::Status& status);
bool IndicatesDiskFull(const leveldb::Status& status);

// Determine the appropriate leveldb write buffer size to use. The default size
// (4MB) may result in a log file too large to be compacted given the available
// storage space. This function will return smaller values for smaller disks,
// and the default leveldb value for larger disks.
//
// |disk_space| is the logical partition size (in bytes), and *not* available
// space. A value of -1 will return leveldb's default write buffer size.
extern size_t WriteBufferSize(int64_t disk_space);

class UMALogger {
 public:
  virtual void RecordErrorAt(MethodID method) const = 0;
  virtual void RecordOSError(MethodID method,
                             base::File::Error error) const = 0;
};

class RetrierProvider {
 public:
  virtual int MaxRetryTimeMillis() const = 0;
  virtual base::HistogramBase* GetRetryTimeHistogram(MethodID method) const = 0;
  virtual base::HistogramBase* GetRecoveredFromErrorHistogram(
      MethodID method) const = 0;
};

class ChromiumEnv : public leveldb::Env,
                    public UMALogger,
                    public RetrierProvider {
 public:
  ChromiumEnv();

  typedef void(ScheduleFunc)(void*);

  virtual ~ChromiumEnv();

  virtual bool FileExists(const std::string& fname);
  virtual leveldb::Status GetChildren(const std::string& dir,
                                      std::vector<std::string>* result);
  virtual leveldb::Status DeleteFile(const std::string& fname);
  virtual leveldb::Status CreateDir(const std::string& name);
  virtual leveldb::Status DeleteDir(const std::string& name);
  virtual leveldb::Status GetFileSize(const std::string& fname, uint64_t* size);
  virtual leveldb::Status RenameFile(const std::string& src,
                                     const std::string& dst);
  virtual leveldb::Status LockFile(const std::string& fname,
                                   leveldb::FileLock** lock);
  virtual leveldb::Status UnlockFile(leveldb::FileLock* lock);
  virtual void Schedule(ScheduleFunc*, void* arg);
  virtual void StartThread(void (*function)(void* arg), void* arg);
  virtual leveldb::Status GetTestDirectory(std::string* path);
  virtual uint64_t NowMicros();
  virtual void SleepForMicroseconds(int micros);
  virtual leveldb::Status NewSequentialFile(const std::string& fname,
                                            leveldb::SequentialFile** result);
  virtual leveldb::Status NewRandomAccessFile(
      const std::string& fname,
      leveldb::RandomAccessFile** result);
  virtual leveldb::Status NewWritableFile(const std::string& fname,
                                          leveldb::WritableFile** result);
  virtual leveldb::Status NewAppendableFile(const std::string& fname,
                                            leveldb::WritableFile** result);
  virtual leveldb::Status NewLogger(const std::string& fname,
                                    leveldb::Logger** result);

 protected:
  explicit ChromiumEnv(const std::string& name);

  static const char* FileErrorString(base::File::Error error);

 private:
  virtual void RecordErrorAt(MethodID method) const;
  virtual void RecordOSError(MethodID method,
                             base::File::Error error) const;
  void RecordOpenFilesLimit(const std::string& type);
  base::HistogramBase* GetMaxFDHistogram(const std::string& type) const;
  base::HistogramBase* GetOSErrorHistogram(MethodID method, int limit) const;
  void DeleteBackupFiles(const base::FilePath& dir);

  // File locks may not be exclusive within a process (e.g. on POSIX). Track
  // locks held by the ChromiumEnv to prevent access within the process.
  class LockTable {
   public:
    bool Insert(const std::string& fname) {
      leveldb::MutexLock l(&mu_);
      return locked_files_.insert(fname).second;
    }
    bool Remove(const std::string& fname) {
      leveldb::MutexLock l(&mu_);
      return locked_files_.erase(fname) == 1;
    }
   private:
    leveldb::port::Mutex mu_;
    std::set<std::string> locked_files_;
  };

  const int kMaxRetryTimeMillis;
  // BGThread() is the body of the background thread
  void BGThread();
  static void BGThreadWrapper(void* arg) {
    reinterpret_cast<ChromiumEnv*>(arg)->BGThread();
  }

  void RecordLockFileAncestors(int num_missing_ancestors) const;
  base::HistogramBase* GetMethodIOErrorHistogram() const;
  base::HistogramBase* GetLockFileAncestorHistogram() const;

  // RetrierProvider implementation.
  virtual int MaxRetryTimeMillis() const { return kMaxRetryTimeMillis; }
  virtual base::HistogramBase* GetRetryTimeHistogram(MethodID method) const;
  virtual base::HistogramBase* GetRecoveredFromErrorHistogram(
      MethodID method) const;

  base::FilePath test_directory_;

  std::string name_;
  std::string uma_ioerror_base_name_;

  base::Lock mu_;
  base::ConditionVariable bgsignal_;
  bool started_bgthread_;

  // Entry per Schedule() call
  struct BGItem {
    void* arg;
    void (*function)(void*);
  };
  typedef std::deque<BGItem> BGQueue;
  BGQueue queue_;
  LockTable locks_;
};

}  // namespace leveldb_env

#endif  // THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_H_
