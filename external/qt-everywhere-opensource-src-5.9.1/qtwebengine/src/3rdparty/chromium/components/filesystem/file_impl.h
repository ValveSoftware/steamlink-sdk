// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_IMPL_H_
#define COMPONENTS_FILESYSTEM_FILE_IMPL_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace base {
class FilePath;
}

namespace filesystem {

class LockTable;
class SharedTempDir;

class FileImpl : public mojom::File {
 public:
  FileImpl(const base::FilePath& path,
           uint32_t flags,
           scoped_refptr<SharedTempDir> temp_dir,
           scoped_refptr<LockTable> lock_table);
  FileImpl(const base::FilePath& path,
           base::File file,
           scoped_refptr<SharedTempDir> temp_dir,
           scoped_refptr<LockTable> lock_table);
  ~FileImpl() override;

  // Returns whether the underlying file handle is valid.
  bool IsValid() const;

  // Attempts to perform the native operating system's locking operations on
  // the internal mojom::File handle
  base::File::Error RawLockFile();
  base::File::Error RawUnlockFile();

  const base::FilePath& path() const { return path_; }

  // |File| implementation:
  void Close(const CloseCallback& callback) override;
  void Read(uint32_t num_bytes_to_read,
            int64_t offset,
            mojom::Whence whence,
            const ReadCallback& callback) override;
  void Write(const std::vector<uint8_t>& bytes_to_write,
             int64_t offset,
             mojom::Whence whence,
             const WriteCallback& callback) override;
  void Tell(const TellCallback& callback) override;
  void Seek(int64_t offset,
            mojom::Whence whence,
            const SeekCallback& callback) override;
  void Stat(const StatCallback& callback) override;
  void Truncate(int64_t size, const TruncateCallback& callback) override;
  void Touch(mojom::TimespecOrNowPtr atime,
             mojom::TimespecOrNowPtr mtime,
             const TouchCallback& callback) override;
  void Dup(mojom::FileRequest file, const DupCallback& callback) override;
  void Flush(const FlushCallback& callback) override;
  void Lock(const LockCallback& callback) override;
  void Unlock(const UnlockCallback& callback) override;
  void AsHandle(const AsHandleCallback& callback) override;

 private:
  base::File file_;
  base::FilePath path_;
  scoped_refptr<SharedTempDir> temp_dir_;
  scoped_refptr<LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(FileImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_IMPL_H_
