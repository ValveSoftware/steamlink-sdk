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
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace filesystem {

class LockTable;

class DirectoryImpl : public mojom::Directory {
 public:
  // Set |temp_dir| only if there's a temporary directory that should be deleted
  // when this object is destroyed.
  DirectoryImpl(mojo::InterfaceRequest<mojom::Directory> request,
                base::FilePath directory_path,
                scoped_refptr<SharedTempDir> temp_dir,
                scoped_refptr<LockTable> lock_table);
  ~DirectoryImpl() override;

  void set_connection_error_handler(const base::Closure& error_handler) {
    binding_.set_connection_error_handler(error_handler);
  }

  // |Directory| implementation:
  void Read(const ReadCallback& callback) override;
  void OpenFile(const mojo::String& path,
                mojo::InterfaceRequest<mojom::File> file,
                uint32_t open_flags,
                const OpenFileCallback& callback) override;
  void OpenFileHandle(const mojo::String& path,
                      uint32_t open_flags,
                      const OpenFileHandleCallback& callback) override;
  void OpenFileHandles(mojo::Array<mojom::FileOpenDetailsPtr> details,
                       const OpenFileHandlesCallback& callback) override;
  void OpenDirectory(const mojo::String& path,
                     mojo::InterfaceRequest<mojom::Directory> directory,
                     uint32_t open_flags,
                     const OpenDirectoryCallback& callback) override;
  void Rename(const mojo::String& path,
              const mojo::String& new_path,
              const RenameCallback& callback) override;
  void Delete(const mojo::String& path,
              uint32_t delete_flags,
              const DeleteCallback& callback) override;
  void Exists(const mojo::String& path,
              const ExistsCallback& callback) override;
  void IsWritable(const mojo::String& path,
                  const IsWritableCallback& callback) override;
  void Flush(const FlushCallback& callback) override;
  void StatFile(const mojo::String& path,
                const StatFileCallback& callback) override;
  void Clone(mojo::InterfaceRequest<mojom::Directory> directory) override;
  void ReadEntireFile(const mojo::String& path,
                      const ReadEntireFileCallback& callback) override;
  void WriteFile(const mojo::String& path,
                 mojo::Array<uint8_t> data,
                 const WriteFileCallback& callback) override;

 private:
  mojo::ScopedHandle OpenFileHandleImpl(const mojo::String& raw_path,
                                        uint32_t open_flags,
                                        mojom::FileError* error);

  mojo::StrongBinding<mojom::Directory> binding_;
  base::FilePath directory_path_;
  scoped_refptr<SharedTempDir> temp_dir_;
  scoped_refptr<LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_DIRECTORY_IMPL_H_
