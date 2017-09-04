// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_DIRECTORY_IMPL_H_
#define COMPONENTS_FILESYSTEM_DIRECTORY_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/filesystem/shared_temp_dir.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace filesystem {

class LockTable;

class DirectoryImpl : public mojom::Directory {
 public:
  // Set |temp_dir| only if there's a temporary directory that should be deleted
  // when this object is destroyed.
  DirectoryImpl(base::FilePath directory_path,
                scoped_refptr<SharedTempDir> temp_dir,
                scoped_refptr<LockTable> lock_table);
  ~DirectoryImpl() override;

  // |Directory| implementation:
  void Read(const ReadCallback& callback) override;
  void OpenFile(const std::string& path,
                mojom::FileRequest file,
                uint32_t open_flags,
                const OpenFileCallback& callback) override;
  void OpenFileHandle(const std::string& path,
                      uint32_t open_flags,
                      const OpenFileHandleCallback& callback) override;
  void OpenFileHandles(std::vector<mojom::FileOpenDetailsPtr> details,
                       const OpenFileHandlesCallback& callback) override;
  void OpenDirectory(const std::string& path,
                     mojom::DirectoryRequest directory,
                     uint32_t open_flags,
                     const OpenDirectoryCallback& callback) override;
  void Rename(const std::string& path,
              const std::string& new_path,
              const RenameCallback& callback) override;
  void Delete(const std::string& path,
              uint32_t delete_flags,
              const DeleteCallback& callback) override;
  void Exists(const std::string& path, const ExistsCallback& callback) override;
  void IsWritable(const std::string& path,
                  const IsWritableCallback& callback) override;
  void Flush(const FlushCallback& callback) override;
  void StatFile(const std::string& path,
                const StatFileCallback& callback) override;
  void Clone(mojom::DirectoryRequest directory) override;
  void ReadEntireFile(const std::string& path,
                      const ReadEntireFileCallback& callback) override;
  void WriteFile(const std::string& path,
                 const std::vector<uint8_t>& data,
                 const WriteFileCallback& callback) override;

 private:
  mojo::ScopedHandle OpenFileHandleImpl(const std::string& raw_path,
                                        uint32_t open_flags,
                                        mojom::FileError* error);

  base::FilePath directory_path_;
  scoped_refptr<SharedTempDir> temp_dir_;
  scoped_refptr<LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_DIRECTORY_IMPL_H_
