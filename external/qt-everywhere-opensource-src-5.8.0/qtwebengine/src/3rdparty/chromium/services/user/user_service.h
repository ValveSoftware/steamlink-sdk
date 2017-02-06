// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_USER_USER_SERVICE_IMPL_H_
#define SERVICES_USER_USER_SERVICE_IMPL_H_

#include "base/files/file_path.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/connection.h"
#include "services/user/public/interfaces/user_service.mojom.h"

namespace filesystem {
class LockTable;
}

namespace user_service {

// A service which serves directories to callers.
class UserService : public mojom::UserService {
 public:
  UserService(const base::FilePath& base_user_dir,
              const scoped_refptr<filesystem::LockTable>& lock_table);
  ~UserService() override;

  // Overridden from mojom::UserService:
  void GetDirectory(filesystem::mojom::DirectoryRequest request,
                    const GetDirectoryCallback& callback) override;
  void GetSubDirectory(const mojo::String& sub_directory_path,
                       filesystem::mojom::DirectoryRequest request,
                       const GetSubDirectoryCallback& callback) override;

 private:
  scoped_refptr<filesystem::LockTable> lock_table_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(UserService);
};

}  // namespace user_service

#endif  // SERVICES_USER_USER_SERVICE_IMPL_H_
