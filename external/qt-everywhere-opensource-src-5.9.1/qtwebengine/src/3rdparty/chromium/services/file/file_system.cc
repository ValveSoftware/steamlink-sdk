// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/file/file_system.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connection.h"

namespace file {

FileSystem::FileSystem(const base::FilePath& base_user_dir,
                       const scoped_refptr<filesystem::LockTable>& lock_table)
    : lock_table_(lock_table), path_(base_user_dir) {
  base::CreateDirectory(path_);
}

FileSystem::~FileSystem() {}

void FileSystem::GetDirectory(filesystem::mojom::DirectoryRequest request,
                              const GetDirectoryCallback& callback) {
  mojo::MakeStrongBinding(
      base::MakeUnique<filesystem::DirectoryImpl>(
          path_, scoped_refptr<filesystem::SharedTempDir>(), lock_table_),
      std::move(request));
  callback.Run();
}

void FileSystem::GetSubDirectory(const std::string& sub_directory_path,
                                 filesystem::mojom::DirectoryRequest request,
                                 const GetSubDirectoryCallback& callback) {
  // Ensure that we've made |subdirectory| recursively under our user dir.
  base::FilePath subdir = path_.Append(
#if defined(OS_WIN)
      base::UTF8ToWide(sub_directory_path));
#else
      sub_directory_path);
#endif
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(subdir, &error)) {
    callback.Run(static_cast<filesystem::mojom::FileError>(error));
    return;
  }

  mojo::MakeStrongBinding(
      base::MakeUnique<filesystem::DirectoryImpl>(
          subdir, scoped_refptr<filesystem::SharedTempDir>(), lock_table_),
      std::move(request));
  callback.Run(filesystem::mojom::FileError::OK);
}

}  // namespace file
