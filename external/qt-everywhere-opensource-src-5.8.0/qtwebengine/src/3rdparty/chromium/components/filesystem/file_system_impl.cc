// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/lock_table.h"
#include "services/shell/public/cpp/connection.h"
#include "url/gurl.h"

namespace filesystem {

FileSystemImpl::FileSystemImpl(shell::Connection* connection,
                               mojom::FileSystemRequest request,
                               base::FilePath persistent_dir,
                               scoped_refptr<LockTable> lock_table)
    : remote_application_name_(connection->GetRemoteIdentity().name()),
      binding_(this, std::move(request)),
      lock_table_(std::move(lock_table)),
      persistent_dir_(persistent_dir) {}

FileSystemImpl::~FileSystemImpl() {
}

void FileSystemImpl::OpenTempDirectory(
    mojo::InterfaceRequest<mojom::Directory> directory,
    const OpenTempDirectoryCallback& callback) {
  // Set only if the |DirectoryImpl| will own a temporary directory.
  std::unique_ptr<base::ScopedTempDir> temp_dir(new base::ScopedTempDir);
  CHECK(temp_dir->CreateUniqueTempDir());

  base::FilePath path = temp_dir->path();
  scoped_refptr<SharedTempDir> shared_temp_dir =
      new SharedTempDir(std::move(temp_dir));
  new DirectoryImpl(
      std::move(directory), path, std::move(shared_temp_dir), lock_table_);
  callback.Run(mojom::FileError::OK);
}

void FileSystemImpl::OpenPersistentFileSystem(
    mojo::InterfaceRequest<mojom::Directory> directory,
    const OpenPersistentFileSystemCallback& callback) {
  std::unique_ptr<base::ScopedTempDir> temp_dir;
  base::FilePath path = persistent_dir_;
  if (!base::PathExists(path))
    base::CreateDirectory(path);

  scoped_refptr<SharedTempDir> shared_temp_dir =
      new SharedTempDir(std::move(temp_dir));

  new DirectoryImpl(
      std::move(directory), path, std::move(shared_temp_dir), lock_table_);
  callback.Run(mojom::FileError::OK);
}

}  // namespace filesystem
