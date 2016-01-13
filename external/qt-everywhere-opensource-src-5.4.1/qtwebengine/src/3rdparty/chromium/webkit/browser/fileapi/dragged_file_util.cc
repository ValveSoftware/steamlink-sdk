// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/dragged_file_util.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace fileapi {

typedef IsolatedContext::MountPointInfo FileInfo;

namespace {

// Simply enumerate each path from a given fileinfo set.
// Used to enumerate top-level paths of an isolated filesystem.
class SetFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  explicit SetFileEnumerator(const std::vector<FileInfo>& files)
      : files_(files) {
    file_iter_ = files_.begin();
  }
  virtual ~SetFileEnumerator() {}

  // AbstractFileEnumerator overrides.
  virtual base::FilePath Next() OVERRIDE {
    if (file_iter_ == files_.end())
      return base::FilePath();
    base::FilePath platform_file = (file_iter_++)->path;
    NativeFileUtil::GetFileInfo(platform_file, &file_info_);
    return platform_file;
  }
  virtual int64 Size() OVERRIDE { return file_info_.size; }
  virtual bool IsDirectory() OVERRIDE { return file_info_.is_directory; }
  virtual base::Time LastModifiedTime() OVERRIDE {
    return file_info_.last_modified;
  }

 private:
  std::vector<FileInfo> files_;
  std::vector<FileInfo>::const_iterator file_iter_;
  base::File::Info file_info_;
};

}  // namespace

//-------------------------------------------------------------------------

DraggedFileUtil::DraggedFileUtil() {}

base::File::Error DraggedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  DCHECK(file_info);
  std::string filesystem_id;
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // The root directory case.
    // For now we leave three time fields (modified/accessed/creation time)
    // NULL as it is not really clear what to be set for this virtual directory.
    // TODO(kinuko): Maybe we want to set the time when this filesystem is
    // created (i.e. when the files/directories are dropped).
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->size = 0;
    return base::File::FILE_OK;
  }
  base::File::Error error =
      NativeFileUtil::GetFileInfo(url.path(), file_info);
  if (base::IsLink(url.path()) && !base::FilePath().IsParent(url.path())) {
    // Don't follow symlinks unless it's the one that are selected by the user.
    return base::File::FILE_ERROR_NOT_FOUND;
  }
  if (error == base::File::FILE_OK)
    *platform_path = url.path();
  return error;
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
    DraggedFileUtil::CreateFileEnumerator(
        FileSystemOperationContext* context,
        const FileSystemURL& root) {
  DCHECK(root.is_valid());
  if (!root.path().empty())
    return LocalFileUtil::CreateFileEnumerator(context, root);

  // Root path case.
  std::vector<FileInfo> toplevels;
  IsolatedContext::GetInstance()->GetDraggedFileInfo(
      root.filesystem_id(), &toplevels);
  return scoped_ptr<AbstractFileEnumerator>(new SetFileEnumerator(toplevels));
}

}  // namespace fileapi
