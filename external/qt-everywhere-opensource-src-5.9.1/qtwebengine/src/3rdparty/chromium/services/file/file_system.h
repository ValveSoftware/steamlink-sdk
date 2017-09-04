// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILE_FILE_SYSTEM_H_
#define SERVICES_FILE_FILE_SYSTEM_H_

#include "base/files/file_path.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/file/public/interfaces/file_system.mojom.h"
#include "services/service_manager/public/cpp/connection.h"

namespace filesystem {
class LockTable;
}

namespace file {

// A service which serves directories to callers.
class FileSystem : public mojom::FileSystem {
 public:
  FileSystem(const base::FilePath& base_user_dir,
             const scoped_refptr<filesystem::LockTable>& lock_table);
  ~FileSystem() override;

  // Overridden from mojom::FileSystem:
  void GetDirectory(filesystem::mojom::DirectoryRequest request,
                    const GetDirectoryCallback& callback) override;
  void GetSubDirectory(const std::string& sub_directory_path,
                       filesystem::mojom::DirectoryRequest request,
                       const GetSubDirectoryCallback& callback) override;

 private:
  scoped_refptr<filesystem::LockTable> lock_table_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

}  // namespace file

#endif  // SERVICES_FILE_FILE_SYSTEM_H_
