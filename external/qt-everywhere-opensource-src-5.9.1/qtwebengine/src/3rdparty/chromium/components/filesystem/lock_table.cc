// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/lock_table.h"

#include "components/filesystem/file_impl.h"

namespace filesystem {

LockTable::LockTable() {}

LockTable::~LockTable() {}

base::File::Error LockTable::LockFile(FileImpl* file) {
  DCHECK(file->IsValid());
  DCHECK(file->path().IsAbsolute());

  auto it = locked_files_.find(file->path());
  if (it != locked_files_.end()) {
    // We're already locked; that's an error condition on Windows.
    return base::File::FILE_ERROR_FAILED;
  }

  base::File::Error lock_err = file->RawLockFile();
  if (lock_err != base::File::FILE_OK) {
    // Locking failed for some reason.
    return lock_err;
  }

  locked_files_.insert(file->path());
  return base::File::FILE_OK;
}

base::File::Error LockTable::UnlockFile(FileImpl* file) {
  auto it = locked_files_.find(file->path());
  if (it != locked_files_.end()) {
    base::File::Error lock_err = file->RawUnlockFile();
    if (lock_err != base::File::FILE_OK) {
      // TODO(erg): When can we fail to release a lock?
      NOTREACHED();
      return lock_err;
    }

    locked_files_.erase(it);
  }

  return base::File::FILE_OK;
}

void LockTable::RemoveFromLockTable(const base::FilePath& path) {
  auto it = locked_files_.find(path);
  if (it != locked_files_.end())
    locked_files_.erase(it);
}

}  // namespace filesystem
