// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/archive_manager.h"

namespace offline_pages {

namespace {

using StorageStatsCallback =
    base::Callback<void(const ArchiveManager::StorageStats& storage_stats)>;

void EnsureArchivesDirCreatedImpl(const base::FilePath& archives_dir) {
  CHECK(base::CreateDirectory(archives_dir));
}

void ExistsArchiveImpl(const base::FilePath& file_path,
                       scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const base::Callback<void(bool)>& callback) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(callback, base::PathExists(file_path)));
}

void DeleteArchivesImpl(const std::vector<base::FilePath>& file_paths,
                        scoped_refptr<base::SequencedTaskRunner> task_runner,
                        const base::Callback<void(bool)>& callback) {
  bool result = false;
  for (const auto& file_path : file_paths) {
    // Make sure delete happens on the left of || so that it is always executed.
    result = base::DeleteFile(file_path, false) || result;
  }
  task_runner->PostTask(FROM_HERE, base::Bind(callback, result));
}

void GetAllArchivesImpl(
    const base::FilePath& archive_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Callback<void(const std::set<base::FilePath>&)>& callback) {
  std::set<base::FilePath> archive_paths;
  base::FileEnumerator file_enumerator(archive_dir, false,
                                       base::FileEnumerator::FILES);
  for (base::FilePath archive_path = file_enumerator.Next();
       !archive_path.empty(); archive_path = file_enumerator.Next()) {
    archive_paths.insert(archive_path);
  }
  task_runner->PostTask(FROM_HERE, base::Bind(callback, archive_paths));
}

void GetStorageStatsImpl(const base::FilePath& archive_dir,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         const StorageStatsCallback& callback) {
  ArchiveManager::StorageStats storage_stats;
  storage_stats.free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(archive_dir);
  storage_stats.total_archives_size = base::ComputeDirectorySize(archive_dir);
  task_runner->PostTask(FROM_HERE, base::Bind(callback, storage_stats));
}

}  // namespace

// protected and used for testing.
ArchiveManager::ArchiveManager() {}

ArchiveManager::ArchiveManager(
    const base::FilePath& archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : archives_dir_(archives_dir), task_runner_(task_runner) {}

ArchiveManager::~ArchiveManager() {}

void ArchiveManager::EnsureArchivesDirCreated(const base::Closure& callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(EnsureArchivesDirCreatedImpl, archives_dir_),
      callback);
}

void ArchiveManager::ExistsArchive(const base::FilePath& archive_path,
                                   const base::Callback<void(bool)>& callback) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(ExistsArchiveImpl, archive_path,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void ArchiveManager::DeleteArchive(const base::FilePath& archive_path,
                                   const base::Callback<void(bool)>& callback) {
  std::vector<base::FilePath> archive_paths = {archive_path};
  DeleteMultipleArchives(archive_paths, callback);
}

void ArchiveManager::DeleteMultipleArchives(
    const std::vector<base::FilePath>& archive_paths,
    const base::Callback<void(bool)>& callback) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(DeleteArchivesImpl, archive_paths,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void ArchiveManager::GetAllArchives(
    const base::Callback<void(const std::set<base::FilePath>&)>& callback)
    const {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(GetAllArchivesImpl, archives_dir_,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void ArchiveManager::GetStorageStats(
    const StorageStatsCallback& callback) const {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(GetStorageStatsImpl, archives_dir_,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

}  // namespace offline_pages
