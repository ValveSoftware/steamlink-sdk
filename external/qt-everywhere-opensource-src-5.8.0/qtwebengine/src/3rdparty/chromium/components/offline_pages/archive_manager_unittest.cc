// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/archive_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const base::FilePath::CharType kMissingArchivePath[] = FILE_PATH_LITERAL(
    "missing_archive.path");
}  // namespace

enum class CallbackStatus {
  NOT_CALLED,
  CALLED_FALSE,
  CALLED_TRUE,
};

class ArchiveManagerTest : public testing::Test {
 public:
  ArchiveManagerTest();
  void SetUp() override;

  void PumpLoop();
  void ResetResults();

  void ResetManager(const base::FilePath& file_path);
  void Callback(bool result);
  void GetAllArchivesCallback(const std::set<base::FilePath>& archive_paths);
  void GetStorageStatsCallback(
      const ArchiveManager::StorageStats& storage_sizes);

  ArchiveManager* manager() { return manager_.get(); }
  const base::FilePath& temp_path() const { return temp_dir_.path(); }
  CallbackStatus callback_status() const { return callback_status_; }
  const std::set<base::FilePath>& last_archive_paths() const {
    return last_archvie_paths_;
  }
  ArchiveManager::StorageStats last_storage_sizes() const {
    return last_storage_sizes_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<ArchiveManager> manager_;
  CallbackStatus callback_status_;
  std::set<base::FilePath> last_archvie_paths_;
  ArchiveManager::StorageStats last_storage_sizes_;
};

ArchiveManagerTest::ArchiveManagerTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      callback_status_(CallbackStatus::NOT_CALLED),
      last_storage_sizes_({0, 0}) {}

void ArchiveManagerTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ResetManager(temp_dir_.path());
}

void ArchiveManagerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void ArchiveManagerTest::ResetResults() {
  callback_status_ = CallbackStatus::NOT_CALLED;
  last_archvie_paths_.clear();
}

void ArchiveManagerTest::ResetManager(const base::FilePath& file_path) {
  manager_.reset(
      new ArchiveManager(file_path, base::ThreadTaskRunnerHandle::Get()));
}

void ArchiveManagerTest::Callback(bool result) {
  callback_status_ =
      result ? CallbackStatus::CALLED_TRUE : CallbackStatus::CALLED_FALSE;
}

void ArchiveManagerTest::GetAllArchivesCallback(
    const std::set<base::FilePath>& archive_paths) {
  last_archvie_paths_ = archive_paths;
}

void ArchiveManagerTest::GetStorageStatsCallback(
    const ArchiveManager::StorageStats& storage_sizes) {
  last_storage_sizes_ = storage_sizes;
}

TEST_F(ArchiveManagerTest, EnsureArchivesDirCreated) {
  base::FilePath archive_dir =
      temp_path().Append(FILE_PATH_LITERAL("test_path"));
  ResetManager(archive_dir);
  EXPECT_FALSE(base::PathExists(archive_dir));

  // Ensure archives dir exists, when it doesn't.
  manager()->EnsureArchivesDirCreated(
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this), true));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_TRUE(base::PathExists(archive_dir));

  // Try again when the file already exists.
  ResetResults();
  manager()->EnsureArchivesDirCreated(
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this), true));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_TRUE(base::PathExists(archive_dir));
}

TEST_F(ArchiveManagerTest, ExistsArchive) {
  base::FilePath archive_path = temp_path().Append(kMissingArchivePath);
  manager()->ExistsArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_FALSE, callback_status());

  ResetResults();
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path));

  manager()->ExistsArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
}

TEST_F(ArchiveManagerTest, DeleteMultipleArchives) {
  base::FilePath archive_path_1;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_1));
  base::FilePath archive_path_2;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_2));
  base::FilePath archive_path_3;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_3));

  std::vector<base::FilePath> archive_paths = {archive_path_1, archive_path_2};

  manager()->DeleteMultipleArchives(
      archive_paths,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));
  EXPECT_TRUE(base::PathExists(archive_path_3));
}

TEST_F(ArchiveManagerTest, DeleteMultipleArchivesSomeDoNotExist) {
  base::FilePath archive_path_1 = temp_path().Append(kMissingArchivePath);
  base::FilePath archive_path_2;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_2));
  base::FilePath archive_path_3;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_3));

  std::vector<base::FilePath> archive_paths = {archive_path_1, archive_path_2};

  EXPECT_FALSE(base::PathExists(archive_path_1));

  manager()->DeleteMultipleArchives(
      archive_paths,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));
  EXPECT_TRUE(base::PathExists(archive_path_3));
}

TEST_F(ArchiveManagerTest, DeleteMultipleArchivesNoneExist) {
  base::FilePath archive_path_1 = temp_path().Append(kMissingArchivePath);
  base::FilePath archive_path_2 = temp_path().Append(FILE_PATH_LITERAL(
      "other_missing_file.mhtml"));
  base::FilePath archive_path_3;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_3));

  std::vector<base::FilePath> archive_paths = {archive_path_1, archive_path_2};

  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));

  manager()->DeleteMultipleArchives(
      archive_paths,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));
  EXPECT_TRUE(base::PathExists(archive_path_3));
}

TEST_F(ArchiveManagerTest, DeleteArchive) {
  base::FilePath archive_path;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path));

  manager()->DeleteArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path));
}

TEST_F(ArchiveManagerTest, DeleteArchiveThatDoesNotExist) {
  base::FilePath archive_path = temp_path().Append(kMissingArchivePath);
  EXPECT_FALSE(base::PathExists(archive_path));

  manager()->DeleteArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path));
}

TEST_F(ArchiveManagerTest, GetAllArchives) {
  base::FilePath archive_path_1;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_1));
  base::FilePath archive_path_2;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_2));
  base::FilePath archive_path_3;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_3));
  std::vector<base::FilePath> expected_paths{archive_path_1, archive_path_2,
                                             archive_path_3};
  std::sort(expected_paths.begin(), expected_paths.end());

  manager()->GetAllArchives(base::Bind(
      &ArchiveManagerTest::GetAllArchivesCallback, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(3UL, last_archive_paths().size());
  std::vector<base::FilePath> actual_paths(last_archive_paths().begin(),
                                           last_archive_paths().end());
  // Comparing one to one works because last_archive_paths set is sorted.
  // Because some windows bots provide abbreviated path (e.g. chrome-bot becomes
  // CHROME~2), this test only focuses on file name.
  EXPECT_EQ(expected_paths[0].BaseName(), actual_paths[0].BaseName());
  EXPECT_EQ(expected_paths[1].BaseName(), actual_paths[1].BaseName());
  EXPECT_EQ(expected_paths[2].BaseName(), actual_paths[2].BaseName());
}

TEST_F(ArchiveManagerTest, GetStorageStats) {
  base::FilePath archive_path_1;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_1));
  base::FilePath archive_path_2;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_path(), &archive_path_2));

  manager()->GetStorageStats(base::Bind(
      &ArchiveManagerTest::GetStorageStatsCallback, base::Unretained(this)));
  PumpLoop();
  EXPECT_GT(last_storage_sizes().free_disk_space, 0);
  EXPECT_EQ(last_storage_sizes().total_archives_size,
            base::ComputeDirectorySize(temp_path()));
}

}  // namespace offline_pages
