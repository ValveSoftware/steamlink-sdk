// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_
#define COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "components/filesystem/shared_temp_dir.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace base {
class FilePath;
}

namespace shell {
class Connection;
}

namespace filesystem {
class FileSystemApp;

class LockTable;

// The base implementation of FileSystemImpl.
class FileSystemImpl : public mojom::FileSystem {
 public:
  // |persistent_dir| is the directory served to callers of
  // |OpenPersistentFileSystem().
  FileSystemImpl(shell::Connection* connection,
                 mojo::InterfaceRequest<mojom::FileSystem> request,
                 base::FilePath persistent_dir,
                 scoped_refptr<LockTable> lock_table);
  ~FileSystemImpl() override;

  // |Files| implementation:
  void OpenTempDirectory(mojo::InterfaceRequest<mojom::Directory> directory,
                         const OpenTempDirectoryCallback& callback) override;
  void OpenPersistentFileSystem(
      mojo::InterfaceRequest<mojom::Directory> directory,
      const OpenPersistentFileSystemCallback& callback) override;

 private:
  const std::string remote_application_name_;
  mojo::StrongBinding<mojom::FileSystem> binding_;
  scoped_refptr<LockTable> lock_table_;

  base::FilePath persistent_dir_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_
