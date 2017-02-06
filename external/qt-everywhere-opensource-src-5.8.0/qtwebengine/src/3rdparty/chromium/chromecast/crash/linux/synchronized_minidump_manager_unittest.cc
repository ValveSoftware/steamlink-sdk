// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/synchronized_minidump_manager.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>  // perror
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <time.h>

#include <fstream>
#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/process/launch.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "chromecast/base/scoped_temp_file.h"
#include "chromecast/crash/linux/crash_testing_utils.h"
#include "chromecast/crash/linux/dump_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

const char kLockfileName[] = "lockfile";
const char kMetadataName[] = "metadata";
const char kMinidumpSubdir[] = "minidumps";

// A trivial implementation of SynchronizedMinidumpManager, which does no work
// to the minidump and exposes its protected members for testing. This simply
// adds an entry to the lockfile.
class SynchronizedMinidumpManagerSimple : public SynchronizedMinidumpManager {
 public:
  SynchronizedMinidumpManagerSimple()
      : SynchronizedMinidumpManager(),
        work_done_(false),
        add_entry_return_code_(-1),
        lockfile_path_(dump_path_.Append(kLockfileName).value()) {}
  ~SynchronizedMinidumpManagerSimple() override {}

  void SetDumpInfoToWrite(std::unique_ptr<DumpInfo> dump_info) {
    dump_info_ = std::move(dump_info);
  }

  int DoWorkLocked() { return AcquireLockAndDoWork(); }

  // SynchronizedMinidumpManager implementation:
  int DoWork() override {
    if (dump_info_)
      add_entry_return_code_ = AddEntryToLockFile(*dump_info_);
    work_done_ = true;
    return 0;
  }

  // Accessors for testing.
  bool HasDumps() { return SynchronizedMinidumpManager::HasDumps(); }
  const std::string& dump_path() { return dump_path_.value(); }
  const std::string& lockfile_path() { return lockfile_path_; }
  bool work_done() { return work_done_; }
  int add_entry_return_code() { return add_entry_return_code_; }

 private:
  bool work_done_;
  int add_entry_return_code_;
  std::string lockfile_path_;
  std::unique_ptr<DumpInfo> dump_info_;
};

void DoWorkLockedTask(SynchronizedMinidumpManagerSimple* manager) {
  manager->DoWorkLocked();
}

// Simple SynchronizedMinidumpManager consumer. Checks if a dump can be uploaded
// then removes it from the lockfile.
class FakeSynchronizedMinidumpUploader : public SynchronizedMinidumpManager {
 public:
  FakeSynchronizedMinidumpUploader()
      : SynchronizedMinidumpManager(), can_upload_return_val_(false) {}
  ~FakeSynchronizedMinidumpUploader() override {}

  int DoWorkLocked() { return AcquireLockAndDoWork(); }

  // SynchronizedMinidumpManager implementation:
  int DoWork() override {
    can_upload_return_val_ = CanUploadDump();

    if (RemoveEntryFromLockFile(0) < 0)
      return -1;

    if (IncrementNumDumpsInCurrentPeriod() < 0)
      return -1;

    return 0;
  }

  // Accessors for testing.
  bool HasDumps() { return SynchronizedMinidumpManager::HasDumps(); }
  bool can_upload_return_val() { return can_upload_return_val_; }

 private:
  bool can_upload_return_val_;
};

class SleepySynchronizedMinidumpManagerSimple
    : public SynchronizedMinidumpManagerSimple {
 public:
  SleepySynchronizedMinidumpManagerSimple(int sleep_duration_ms)
      : SynchronizedMinidumpManagerSimple(),
        sleep_duration_ms_(sleep_duration_ms) {}
  ~SleepySynchronizedMinidumpManagerSimple() override {}

  // SynchronizedMinidumpManager implementation:
  int DoWork() override {
    // The lock has been acquired. Fall asleep for |kSleepDurationMs|, then
    // write the file.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(sleep_duration_ms_));
    return SynchronizedMinidumpManagerSimple::DoWork();
  }

 private:
  const int sleep_duration_ms_;
};

class SynchronizedMinidumpManagerTest : public testing::Test {
 public:
  SynchronizedMinidumpManagerTest() {}
  ~SynchronizedMinidumpManagerTest() override {}

  void SetUp() override {
    // Set up a temporary directory which will be used as our fake home dir.
    ASSERT_TRUE(fake_home_dir_.CreateUniqueTempDir());
    path_override_.reset(
        new base::ScopedPathOverride(base::DIR_HOME, fake_home_dir_.path()));

    minidump_dir_ = fake_home_dir_.path().Append(kMinidumpSubdir);
    lockfile_ = minidump_dir_.Append(kLockfileName);
    metadata_ = minidump_dir_.Append(kMetadataName);

    // Create a minidump directory.
    ASSERT_TRUE(base::CreateDirectory(minidump_dir_));
    ASSERT_TRUE(base::IsDirectoryEmpty(minidump_dir_));

    // Create a lockfile in that directory.
    base::File lockfile(
        lockfile_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(lockfile.IsValid());
  }

 protected:
  base::FilePath minidump_dir_;   // Path the the minidump directory.
  base::FilePath lockfile_;       // Path to the lockfile in |minidump_dir_|.
  base::FilePath metadata_;       // Path to the metadata in |minidump_dir_|.

 private:
  base::ScopedTempDir fake_home_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
};

// Have |producer| generate |num_dumps| while checking there are no errors.
void produce_dumps(SynchronizedMinidumpManagerSimple& producer, int num_dumps) {
  for (int i = 0; i < num_dumps; ++i) {
    ASSERT_EQ(0, producer.DoWorkLocked());
    ASSERT_EQ(0, producer.add_entry_return_code());
  }
}

// Have |consumer| remove and process |num_dumps| while checking there are no
// errors.
void consume_dumps(FakeSynchronizedMinidumpUploader& consumer, int num_dumps) {
  for (int i = 0; i < num_dumps; ++i) {
    ASSERT_EQ(0, consumer.DoWorkLocked());
    ASSERT_TRUE(consumer.can_upload_return_val());
  }
}

}  // namespace

TEST_F(SynchronizedMinidumpManagerTest, FilePathsAreCorrect) {
  SynchronizedMinidumpManagerSimple manager;

  // Verify file paths for directory and lock file.
  ASSERT_EQ(minidump_dir_.value(), manager.dump_path());
  ASSERT_EQ(lockfile_.value(), manager.lockfile_path());
}

TEST_F(SynchronizedMinidumpManagerTest, AcquireLockOnNonExistentDirectory) {
  // The directory was created in SetUp(). Delete it and its contents.
  ASSERT_TRUE(base::DeleteFile(minidump_dir_, true));
  ASSERT_FALSE(base::PathExists(minidump_dir_));

  SynchronizedMinidumpManagerSimple manager;
  ASSERT_EQ(0, manager.DoWorkLocked());
  ASSERT_TRUE(manager.work_done());

  // Verify the directory and the lockfile both exist.
  ASSERT_TRUE(base::DirectoryExists(minidump_dir_));
  ASSERT_TRUE(base::PathExists(lockfile_));
}

TEST_F(SynchronizedMinidumpManagerTest, AcquireLockOnExistingEmptyDirectory) {
  // The lockfile was created in SetUp(). Delete it.
  ASSERT_TRUE(base::DeleteFile(lockfile_, false));
  ASSERT_FALSE(base::PathExists(lockfile_));

  SynchronizedMinidumpManagerSimple manager;
  ASSERT_EQ(0, manager.DoWorkLocked());
  ASSERT_TRUE(manager.work_done());

  // Verify the directory and the lockfile both exist.
  ASSERT_TRUE(base::DirectoryExists(minidump_dir_));
  ASSERT_TRUE(base::PathExists(lockfile_));
}

TEST_F(SynchronizedMinidumpManagerTest,
       AcquireLockOnExistingDirectoryWithLockfile) {
  SynchronizedMinidumpManagerSimple manager;
  ASSERT_EQ(0, manager.DoWorkLocked());
  ASSERT_TRUE(manager.work_done());

  // Verify the directory and the lockfile both exist.
  ASSERT_TRUE(base::DirectoryExists(minidump_dir_));
  ASSERT_TRUE(base::PathExists(lockfile_));
}

TEST_F(SynchronizedMinidumpManagerTest,
       AddEntryToLockFile_FailsWithInvalidEntry) {
  // Create invalid dump info value
  base::DictionaryValue val;

  // Test that the manager tried to log the entry and failed.
  SynchronizedMinidumpManagerSimple manager;
  manager.SetDumpInfoToWrite(base::WrapUnique(new DumpInfo(&val)));
  ASSERT_EQ(0, manager.DoWorkLocked());
  ASSERT_EQ(-1, manager.add_entry_return_code());

  // Verify the lockfile is untouched.
  ScopedVector<DumpInfo> dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  ASSERT_EQ(0u, dumps.size());
}

TEST_F(SynchronizedMinidumpManagerTest,
       AddEntryToLockFile_SucceedsWithValidEntries) {
  // Sample parameters.
  time_t now = time(0);
  MinidumpParams params;
  params.process_name = "process";

  // Write the first entry.
  SynchronizedMinidumpManagerSimple manager;
  manager.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump1", "log1", now, params)));
  ASSERT_EQ(0, manager.DoWorkLocked());
  ASSERT_EQ(0, manager.add_entry_return_code());

  // Test that the manager was successful in logging the entry.
  ScopedVector<DumpInfo> dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  ASSERT_EQ(1u, dumps.size());

  // Write the second entry.
  manager.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump2", "log2", now, params)));
  ASSERT_EQ(0, manager.DoWorkLocked());
  ASSERT_EQ(0, manager.add_entry_return_code());

  // Test that the second entry is also valid.
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  ASSERT_EQ(2u, dumps.size());
}

TEST_F(SynchronizedMinidumpManagerTest,
       AcquireLockFile_FailsWhenNonBlockingAndFileLocked) {
  ASSERT_TRUE(CreateFiles(lockfile_.value(), metadata_.value()));
  // Lock the lockfile here. Note that the Chromium base::File tools permit
  // multiple locks on the same process to succeed, so we must use POSIX system
  // calls to accomplish this.
  int fd = open(lockfile_.value().c_str(), O_RDWR | O_CREAT, 0660);
  ASSERT_GE(fd, 0);
  ASSERT_EQ(0, flock(fd, LOCK_EX));

  SynchronizedMinidumpManagerSimple manager;
  manager.set_non_blocking(true);
  ASSERT_EQ(-1, manager.DoWorkLocked());
  ASSERT_FALSE(manager.work_done());

  // Test that the manager was not able to log the crash dump.
  ScopedVector<DumpInfo> dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  ASSERT_EQ(0u, dumps.size());
}

TEST_F(SynchronizedMinidumpManagerTest,
       AcquireLockFile_WaitsForOtherThreadWhenBlocking) {
  // Create some parameters for a minidump.
  time_t now = time(0);
  MinidumpParams params;
  params.process_name = "process";

  // Create a manager that grabs the lock then sleeps. Post a DoWork task to
  // another thread. |sleepy_manager| will grab the lock and hold it for
  // |sleep_time_ms|. It will then write a dump and release the lock.
  const int sleep_time_ms = 100;
  SleepySynchronizedMinidumpManagerSimple sleepy_manager(sleep_time_ms);
  sleepy_manager.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump", "log", now, params)));
  base::Thread sleepy_thread("sleepy");
  sleepy_thread.Start();
  sleepy_thread.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DoWorkLockedTask, base::Unretained(&sleepy_manager)));

  // Meanwhile, this thread should wait brielfy to allow the other thread to
  // grab the lock.
  const int concurrency_delay = 50;
  base::PlatformThread::Sleep(
      base::TimeDelta::FromMilliseconds(concurrency_delay));

  // |sleepy_manager| has the lock by now, but has not released it. Attempt to
  // grab it. DoWorkLocked() should block until |manager| has a chance to write
  // the dump.
  SynchronizedMinidumpManagerSimple manager;
  manager.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump", "log", now, params)));
  manager.set_non_blocking(false);

  EXPECT_EQ(0, manager.DoWorkLocked());
  EXPECT_EQ(0, manager.add_entry_return_code());
  EXPECT_TRUE(manager.work_done());

  // Check that the other manager was also successful.
  EXPECT_EQ(0, sleepy_manager.add_entry_return_code());
  EXPECT_TRUE(sleepy_manager.work_done());

  // Test that both entries were logged.
  ScopedVector<DumpInfo> dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  EXPECT_EQ(2u, dumps.size());
}

// TODO(slan): These tests are passing but forking them is creating duplicates
// of all tests in this thread. Figure out how to lock the file more cleanly
// from another process.
TEST_F(SynchronizedMinidumpManagerTest,
       DISABLED_AcquireLockFile_FailsWhenNonBlockingAndLockedFromOtherProcess) {
  // Fork the process.
  pid_t pid = base::ForkWithFlags(0u, nullptr, nullptr);
  if (pid != 0) {
    // The child process should instantiate a manager which immediately grabs
    // the lock, and falls aleep for some period of time, then writes a dump,
    // and finally releases the lock.
    SleepySynchronizedMinidumpManagerSimple sleepy_manager(100);
    ASSERT_EQ(0, sleepy_manager.DoWorkLocked());
    ASSERT_TRUE(sleepy_manager.work_done());
    return;
  }

  // Meanwhile, this process should wait brielfy to allow the other thread to
  // grab the lock.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));

  SynchronizedMinidumpManagerSimple manager;
  manager.set_non_blocking(true);
  ASSERT_EQ(-1, manager.DoWorkLocked());
  ASSERT_FALSE(manager.work_done());

  // Test that the manager was not able to log the crash dump.
  ScopedVector<DumpInfo> dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  ASSERT_EQ(0u, dumps.size());
}

// TODO(slan): These tests are passing but forking them is creating duplicates
// of all tests in this thread. Figure out how to lock the file more cleanly
// from another process.
TEST_F(SynchronizedMinidumpManagerTest,
       DISABLED_AcquireLockFile_WaitsForOtherProcessWhenBlocking) {
  // Create some parameters for a minidump.
  time_t now = time(0);
  MinidumpParams params;
  params.process_name = "process";

  // Fork the process.
  pid_t pid = base::ForkWithFlags(0u, nullptr, nullptr);
  if (pid != 0) {
    // The child process should instantiate a manager which immediately grabs
    // the lock, and falls aleep for some period of time, then writes a dump,
    // and finally releases the lock.
    SleepySynchronizedMinidumpManagerSimple sleepy_manager(100);
    sleepy_manager.SetDumpInfoToWrite(
        base::WrapUnique(new DumpInfo("dump", "log", now, params)));
    ASSERT_EQ(0, sleepy_manager.DoWorkLocked());
    ASSERT_TRUE(sleepy_manager.work_done());
    return;
  }

  // Meanwhile, this process should wait brielfy to allow the other thread to
  // grab the lock.
  const int concurrency_delay = 50;
  base::PlatformThread::Sleep(
      base::TimeDelta::FromMilliseconds(concurrency_delay));

  // |sleepy_manager| has the lock by now, but has not released it. Attempt to
  // grab it. DoWorkLocked() should block until |manager| has a chance to write
  // the dump.
  SynchronizedMinidumpManagerSimple manager;
  manager.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump", "log", now, params)));
  manager.set_non_blocking(false);

  EXPECT_EQ(0, manager.DoWorkLocked());
  EXPECT_EQ(0, manager.add_entry_return_code());
  EXPECT_TRUE(manager.work_done());

  // Test that both entries were logged.
  ScopedVector<DumpInfo> dumps;
  ASSERT_TRUE(FetchDumps(lockfile_.value(), &dumps));
  EXPECT_EQ(2u, dumps.size());
}

TEST_F(SynchronizedMinidumpManagerTest,
       Upload_SucceedsWhenDumpLimitsNotExceeded) {
  // Sample parameters.
  time_t now = time(0);
  MinidumpParams params;
  params.process_name = "process";

  FakeSynchronizedMinidumpUploader uploader;
  SynchronizedMinidumpManagerSimple producer;
  producer.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump1", "log1", now, params)));

  const int max_dumps = SynchronizedMinidumpManager::kRatelimitPeriodMaxDumps;
  produce_dumps(producer, max_dumps);
  consume_dumps(uploader, max_dumps);
}

TEST_F(SynchronizedMinidumpManagerTest, Upload_FailsWhenTooManyRecentDumps) {
  // Sample parameters.
  time_t now = time(0);
  MinidumpParams params;
  params.process_name = "process";

  FakeSynchronizedMinidumpUploader uploader;
  SynchronizedMinidumpManagerSimple producer;
  producer.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump1", "log1", now, params)));

  const int max_dumps = SynchronizedMinidumpManager::kRatelimitPeriodMaxDumps;
  produce_dumps(producer, max_dumps + 1);
  consume_dumps(uploader, max_dumps);

  // Should fail with too many dumps
  ASSERT_EQ(0, uploader.DoWorkLocked());
  ASSERT_FALSE(uploader.can_upload_return_val());
}

TEST_F(SynchronizedMinidumpManagerTest, UploadSucceedsAfterRateLimitPeriodEnd) {
  // Sample parameters.
  time_t now = time(0);
  MinidumpParams params;
  params.process_name = "process";

  FakeSynchronizedMinidumpUploader uploader;
  SynchronizedMinidumpManagerSimple producer;
  producer.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump1", "log1", now, params)));

  const int iters = 3;
  const int max_dumps = SynchronizedMinidumpManager::kRatelimitPeriodMaxDumps;

  for (int i = 0; i < iters; ++i) {
    produce_dumps(producer, max_dumps + 1);
    consume_dumps(uploader, max_dumps);

    // Should fail with too many dumps
    ASSERT_EQ(0, uploader.DoWorkLocked());
    ASSERT_FALSE(uploader.can_upload_return_val());

    int64_t period = SynchronizedMinidumpManager::kRatelimitPeriodSeconds;

    // Half period shouldn't trigger reset
    produce_dumps(producer, 1);
    SetRatelimitPeriodStart(metadata_.value(), now - period / 2);
    ASSERT_EQ(0, uploader.DoWorkLocked());
    ASSERT_FALSE(uploader.can_upload_return_val());

    // Set period starting time to trigger a reset
    SetRatelimitPeriodStart(metadata_.value(), now - period);
  }

  produce_dumps(producer, 1);
  consume_dumps(uploader, 1);
}

TEST_F(SynchronizedMinidumpManagerTest, HasDumpsWithoutDumps) {
  FakeSynchronizedMinidumpUploader uploader;
  ASSERT_FALSE(uploader.HasDumps());
}

TEST_F(SynchronizedMinidumpManagerTest, HasDumpsWithDumps) {
  // Sample parameters.
  time_t now = time(0);
  MinidumpParams params;
  params.process_name = "process";

  SynchronizedMinidumpManagerSimple producer;
  FakeSynchronizedMinidumpUploader uploader;

  producer.SetDumpInfoToWrite(
      base::WrapUnique(new DumpInfo("dump1", "log1", now, params)));

  const int kNumDumps = 3;
  for (int i = 0; i < kNumDumps; ++i) {
    produce_dumps(producer, 1);
    ASSERT_TRUE(uploader.HasDumps());
  }

  for (int i = 0; i < kNumDumps; ++i) {
    ASSERT_TRUE(uploader.HasDumps());
    consume_dumps(uploader, 1);
  }

  ASSERT_FALSE(uploader.HasDumps());
}

TEST_F(SynchronizedMinidumpManagerTest, HasDumpsNotInLockFile) {
  SynchronizedMinidumpManagerSimple manager;
  ASSERT_FALSE(manager.HasDumps());

  // Create file in dump path.
  const base::FilePath path =
      base::FilePath(manager.dump_path()).Append("hello123");
  const char kFileContents[] = "foobar";
  ASSERT_EQ(static_cast<int>(sizeof(kFileContents)),
            WriteFile(path, kFileContents, sizeof(kFileContents)));

  ASSERT_TRUE(manager.HasDumps());
}

}  // namespace chromecast
