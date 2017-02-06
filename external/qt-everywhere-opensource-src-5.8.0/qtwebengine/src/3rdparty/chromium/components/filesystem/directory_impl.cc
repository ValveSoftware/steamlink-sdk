// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/directory_impl.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/filesystem/file_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/filesystem/util.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace filesystem {

DirectoryImpl::DirectoryImpl(mojo::InterfaceRequest<mojom::Directory> request,
                             base::FilePath directory_path,
                             scoped_refptr<SharedTempDir> temp_dir,
                             scoped_refptr<LockTable> lock_table)
    : binding_(this, std::move(request)),
      directory_path_(directory_path),
      temp_dir_(std::move(temp_dir)),
      lock_table_(std::move(lock_table)) {}

DirectoryImpl::~DirectoryImpl() {}

void DirectoryImpl::Read(const ReadCallback& callback) {
  mojo::Array<mojom::DirectoryEntryPtr> entries;
  base::FileEnumerator directory_enumerator(
      directory_path_, false,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  for (base::FilePath name = directory_enumerator.Next(); !name.empty();
       name = directory_enumerator.Next()) {
    base::FileEnumerator::FileInfo info = directory_enumerator.GetInfo();
    mojom::DirectoryEntryPtr entry = mojom::DirectoryEntry::New();
    entry->type = info.IsDirectory() ? mojom::FsFileType::DIRECTORY
                                     : mojom::FsFileType::REGULAR_FILE;
    entry->name = info.GetName().AsUTF8Unsafe();
    entries.push_back(std::move(entry));
  }

  callback.Run(mojom::FileError::OK, std::move(entries));
}

// TODO(erg): Consider adding an implementation of Stat()/Touch() to the
// directory, too. Right now, the base::File abstractions do not really deal
// with directories properly, so these are broken for now.

// TODO(vtl): Move the implementation to a thread pool.
void DirectoryImpl::OpenFile(const mojo::String& raw_path,
                             mojo::InterfaceRequest<mojom::File> file,
                             uint32_t open_flags,
                             const OpenFileCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error);
    return;
  }

  if (base::DirectoryExists(path)) {
    // We must not return directories as files. In the file abstraction, we can
    // fetch raw file descriptors over mojo pipes, and passing a file
    // descriptor to a directory is a sandbox escape on Windows.
    callback.Run(mojom::FileError::NOT_A_FILE);
    return;
  }

  base::File base_file(path, open_flags);
  if (!base_file.IsValid()) {
    callback.Run(GetError(base_file));
    return;
  }

  if (file.is_pending()) {
    new FileImpl(std::move(file), path, std::move(base_file), temp_dir_,
                 lock_table_);
  }
  callback.Run(mojom::FileError::OK);
}

void DirectoryImpl::OpenFileHandle(const mojo::String& raw_path,
                                   uint32_t open_flags,
                                   const OpenFileHandleCallback& callback) {
  mojom::FileError error = mojom::FileError::OK;
  mojo::ScopedHandle handle = OpenFileHandleImpl(raw_path, open_flags, &error);
  callback.Run(error, std::move(handle));
}

void DirectoryImpl::OpenFileHandles(
    mojo::Array<mojom::FileOpenDetailsPtr> details,
    const OpenFileHandlesCallback& callback) {
  mojo::Array<mojom::FileOpenResultPtr> results(
      mojo::Array<mojom::FileOpenResultPtr>::New(details.size()));
  size_t i = 0;
  for (const auto& detail : details) {
    mojom::FileOpenResultPtr result(mojom::FileOpenResult::New());
    result->path = detail->path;
    result->file_handle =
        OpenFileHandleImpl(detail->path, detail->open_flags, &result->error);
    results[i++] = std::move(result);
  }
  callback.Run(std::move(results));
}

void DirectoryImpl::OpenDirectory(
    const mojo::String& raw_path,
    mojo::InterfaceRequest<mojom::Directory> directory,
    uint32_t open_flags,
    const OpenDirectoryCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error);
    return;
  }

  if (!base::DirectoryExists(path)) {
    if (base::PathExists(path)) {
      callback.Run(mojom::FileError::NOT_A_DIRECTORY);
      return;
    }

    if (!(open_flags & mojom::kFlagOpenAlways ||
          open_flags & mojom::kFlagCreate)) {
      // The directory doesn't exist, and we weren't passed parameters to
      // create it.
      callback.Run(mojom::FileError::NOT_FOUND);
      return;
    }

    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(path, &error)) {
      callback.Run(static_cast<filesystem::mojom::FileError>(error));
      return;
    }
  }

  if (directory.is_pending())
    new DirectoryImpl(std::move(directory), path, temp_dir_, lock_table_);
  callback.Run(mojom::FileError::OK);
}

void DirectoryImpl::Rename(const mojo::String& raw_old_path,
                           const mojo::String& raw_new_path,
                           const RenameCallback& callback) {
  base::FilePath old_path;
  mojom::FileError error =
      ValidatePath(raw_old_path, directory_path_, &old_path);
  if (error != mojom::FileError::OK) {
    callback.Run(error);
    return;
  }

  base::FilePath new_path;
  error = ValidatePath(raw_new_path, directory_path_, &new_path);
  if (error != mojom::FileError::OK) {
    callback.Run(error);
    return;
  }

  if (!base::Move(old_path, new_path)) {
    callback.Run(mojom::FileError::FAILED);
    return;
  }

  callback.Run(mojom::FileError::OK);
}

void DirectoryImpl::Delete(const mojo::String& raw_path,
                           uint32_t delete_flags,
                           const DeleteCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error);
    return;
  }

  bool recursive = delete_flags & mojom::kDeleteFlagRecursive;
  if (!base::DeleteFile(path, recursive)) {
    callback.Run(mojom::FileError::FAILED);
    return;
  }

  callback.Run(mojom::FileError::OK);
}

void DirectoryImpl::Exists(const mojo::String& raw_path,
                           const ExistsCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error, false);
    return;
  }

  bool exists = base::PathExists(path);
  callback.Run(mojom::FileError::OK, exists);
}

void DirectoryImpl::IsWritable(const mojo::String& raw_path,
                               const IsWritableCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error, false);
    return;
  }

  callback.Run(mojom::FileError::OK, base::PathIsWritable(path));
}

void DirectoryImpl::Flush(const FlushCallback& callback) {
  base::File file(directory_path_, base::File::FLAG_READ);
  if (!file.IsValid()) {
    callback.Run(GetError(file));
    return;
  }

  if (!file.Flush()) {
    callback.Run(mojom::FileError::FAILED);
    return;
  }

  callback.Run(mojom::FileError::OK);
}

void DirectoryImpl::StatFile(const mojo::String& raw_path,
                             const StatFileCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error, nullptr);
    return;
  }

  base::File base_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!base_file.IsValid()) {
    callback.Run(GetError(base_file), nullptr);
    return;
  }

  base::File::Info info;
  if (!base_file.GetInfo(&info)) {
    callback.Run(mojom::FileError::FAILED, nullptr);
    return;
  }

  callback.Run(mojom::FileError::OK, MakeFileInformation(info));
}

void DirectoryImpl::Clone(mojo::InterfaceRequest<mojom::Directory> directory) {
  if (directory.is_pending()) {
    new DirectoryImpl(std::move(directory), directory_path_,
                      temp_dir_, lock_table_);
  }
}

void DirectoryImpl::ReadEntireFile(const mojo::String& raw_path,
                                   const ReadEntireFileCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error, mojo::Array<uint8_t>());
    return;
  }

  if (base::DirectoryExists(path)) {
    callback.Run(mojom::FileError::NOT_A_FILE, mojo::Array<uint8_t>());
    return;
  }

  base::File base_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!base_file.IsValid()) {
    callback.Run(GetError(base_file), mojo::Array<uint8_t>());
    return;
  }

  std::string contents;
  const int kBufferSize = 1 << 16;
  std::unique_ptr<char[]> buf(new char[kBufferSize]);
  int len;
  while ((len = base_file.ReadAtCurrentPos(buf.get(), kBufferSize)) > 0)
    contents.append(buf.get(), len);

  callback.Run(mojom::FileError::OK, mojo::Array<uint8_t>::From(contents));
}

void DirectoryImpl::WriteFile(const mojo::String& raw_path,
                              mojo::Array<uint8_t> data,
                              const WriteFileCallback& callback) {
  base::FilePath path;
  mojom::FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != mojom::FileError::OK) {
    callback.Run(error);
    return;
  }

  if (base::DirectoryExists(path)) {
    callback.Run(mojom::FileError::NOT_A_FILE);
    return;
  }

  base::File base_file(path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!base_file.IsValid()) {
    callback.Run(GetError(base_file));
    return;
  }

  // If we're given empty data, we don't write and just truncate the file.
  if (data.size()) {
    const int data_size = static_cast<int>(data.size());
    if (base_file.Write(0, reinterpret_cast<char*>(&data.front()),
                        data_size) == -1) {
      callback.Run(GetError(base_file));
      return;
    }
  }

  callback.Run(mojom::FileError::OK);
}

mojo::ScopedHandle DirectoryImpl::OpenFileHandleImpl(
    const mojo::String& raw_path,
    uint32_t open_flags,
    mojom::FileError* error) {
  base::FilePath path;
  *error = ValidatePath(raw_path, directory_path_, &path);
  if (*error != mojom::FileError::OK)
    return mojo::ScopedHandle();

  if (base::DirectoryExists(path)) {
    // We must not return directories as files. In the file abstraction, we
    // can fetch raw file descriptors over mojo pipes, and passing a file
    // descriptor to a directory is a sandbox escape on Windows.
    *error = mojom::FileError::NOT_A_FILE;
    return mojo::ScopedHandle();
  }

  base::File base_file(path, open_flags);
  if (!base_file.IsValid()) {
    *error = GetError(base_file);
    return mojo::ScopedHandle();
  }

  *error = mojom::FileError::OK;
  return mojo::WrapPlatformFile(base_file.TakePlatformFile());
}

}  // namespace filesystem
