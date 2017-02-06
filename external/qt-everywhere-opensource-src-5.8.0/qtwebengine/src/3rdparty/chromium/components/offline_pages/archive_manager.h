// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_ARCHIVE_MANAGER_H_
#define COMPONENTS_OFFLINE_PAGES_ARCHIVE_MANAGER_H_

#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

// Class that manages all archive files for offline pages. They are all stored
// in a specific archive directory.
// All tasks are performed using |task_runner_|.
class ArchiveManager {
 public:
  struct StorageStats {
    int64_t free_disk_space;
    int64_t total_archives_size;
  };

  ArchiveManager(const base::FilePath& archives_dir,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  virtual ~ArchiveManager();

  // Creates archives directory if one does not exist yet;
  virtual void EnsureArchivesDirCreated(const base::Closure& callback);

  // Checks whether an archive with specified |archive_path| exists.
  virtual void ExistsArchive(const base::FilePath& archive_path,
                             const base::Callback<void(bool)>& callback);

  // Deletes an archive with specified |archive_path|.
  // It is considered successful to attempt to delete a file that does not
  // exist.
  virtual void DeleteArchive(const base::FilePath& archive_path,
                             const base::Callback<void(bool)>& callback);

  // Deletes multiple archives that are specified in |archive_paths|.
  // It is considered successful to attempt to delete a file that does not
  // exist.
  virtual void DeleteMultipleArchives(
      const std::vector<base::FilePath>& archive_paths,
      const base::Callback<void(bool)>& callback);

  // Lists all archive files in the archive directory.
  virtual void GetAllArchives(
      const base::Callback<void(const std::set<base::FilePath>&)>& callback)
      const;

  // Gets stats about archive storage, i.e. total archive sizes and free disk
  // space.
  virtual void GetStorageStats(
      const base::Callback<void(const StorageStats& storage_sizes)>& callback)
      const;

 protected:
  ArchiveManager();

 private:
  // Path under which all of the managed archives should be stored.
  base::FilePath archives_dir_;
  // Task runner for running file operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_ARCHIVE_MANAGER_H_
