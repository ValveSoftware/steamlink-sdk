// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_ASYNC_FILE_UTIL_H_
#define WEBKIT_BROWSER_FILEAPI_ASYNC_FILE_UTIL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/directory_entry.h"

namespace base {
class Time;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace fileapi {

class FileSystemOperationContext;
class FileSystemURL;

// An interface which provides filesystem-specific file operations for
// FileSystemOperationImpl.
//
// Each filesystem which needs to be dispatched from FileSystemOperationImpl
// must implement this interface or a synchronous version of interface:
// FileSystemFileUtil.
//
// As far as an instance of this class is owned by a FileSystemBackend
// (which is owned by FileSystemContext), it's guaranteed that this instance's
// alive while FileSystemOperationContext given to each operation is kept
// alive. (Note that this instance might be freed on different thread
// from the thread it is created.)
//
// It is NOT valid to give null callback to this class, and implementors
// can assume that they don't get any null callbacks.
//
class AsyncFileUtil {
 public:
  typedef base::Callback<void(base::File::Error result)> StatusCallback;

  // |on_close_callback| will be called after the |file| is closed in the
  // child process. |on_close_callback|.is_null() can be true, if no operation
  // is needed on closing the file.
  typedef base::Callback<
      void(base::File file,
           const base::Closure& on_close_callback)> CreateOrOpenCallback;

  typedef base::Callback<
      void(base::File::Error result,
           bool created)> EnsureFileExistsCallback;

  typedef base::Callback<
      void(base::File::Error result,
           const base::File::Info& file_info)> GetFileInfoCallback;

  typedef std::vector<DirectoryEntry> EntryList;
  typedef base::Callback<
      void(base::File::Error result,
           const EntryList& file_list,
           bool has_more)> ReadDirectoryCallback;

  typedef base::Callback<
      void(base::File::Error result,
           const base::File::Info& file_info,
           const base::FilePath& platform_path,
           const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref)>
      CreateSnapshotFileCallback;


  typedef base::Callback<void(int64 size)> CopyFileProgressCallback;

  typedef FileSystemOperation::CopyOrMoveOption CopyOrMoveOption;

  // Creates an AsyncFileUtil instance which performs file operations on
  // local native file system. The created instance assumes
  // FileSystemURL::path() has the target platform path.
  WEBKIT_STORAGE_BROWSER_EXPORT static AsyncFileUtil*
      CreateForLocalFileSystem();

  AsyncFileUtil() {}
  virtual ~AsyncFileUtil() {}

  // Creates or opens a file with the given flags.
  // If File::FLAG_CREATE is set in |file_flags| it always tries to create
  // a new file at the given |url| and calls back with
  // File::FILE_ERROR_FILE_EXISTS if the |url| already exists.
  //
  // FileSystemOperationImpl::OpenFile calls this.
  // This is used only by Pepper/NaCl File API.
  //
  virtual void CreateOrOpen(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback) = 0;

  // Ensures that the given |url| exist.  This creates a empty new file
  // at |url| if the |url| does not exist.
  //
  // FileSystemOperationImpl::CreateFile calls this.
  //
  // This reports following error code via |callback|:
  // - File::FILE_OK and created==true if a file has not existed and
  //   is created at |url|.
  // - File::FILE_OK and created==false if the file already exists.
  // - Other error code (with created=false) if a file hasn't existed yet
  //   and there was an error while creating a new file.
  //
  virtual void EnsureFileExists(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const EnsureFileExistsCallback& callback) = 0;

  // Creates directory at given url.
  //
  // FileSystemOperationImpl::CreateDirectory calls this.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if the |url|'s parent directory
  //   does not exist and |recursive| is false.
  // - File::FILE_ERROR_EXISTS if a directory already exists at |url|
  //   and |exclusive| is true.
  // - File::FILE_ERROR_EXISTS if a file already exists at |url|
  //   (regardless of |exclusive| value).
  // - Other error code if it failed to create a directory.
  //
  virtual void CreateDirectory(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback) = 0;

  // Retrieves the information about a file.
  //
  // FileSystemOperationImpl::GetMetadata calls this.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if the file doesn't exist.
  // - Other error code if there was an error while retrieving the file info.
  //
  virtual void GetFileInfo(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const GetFileInfoCallback& callback) = 0;

  // Reads contents of a directory at |path|.
  //
  // FileSystemOperationImpl::ReadDirectory calls this.
  //
  // Note that the |name| field of each entry in |file_list|
  // returned by |callback| should have a base file name
  // of the entry relative to the directory, but not an absolute path.
  //
  // (E.g. if ReadDirectory is called for a directory
  // 'path/to/dir' and the directory has entries 'a' and 'b',
  // the returned |file_list| should include entries whose names
  // are 'a' and 'b', but not '/path/to/dir/a' and '/path/to/dir/b'.)
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if the target directory doesn't exist.
  // - File::FILE_ERROR_NOT_A_DIRECTORY if an entry exists at |url| but
  //   is a file (not a directory).
  //
  virtual void ReadDirectory(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const ReadDirectoryCallback& callback) = 0;

  // Modifies timestamps of a file or directory at |url| with
  // |last_access_time| and |last_modified_time|. The function DOES NOT
  // create a file unlike 'touch' command on Linux.
  //
  // FileSystemOperationImpl::TouchFile calls this.
  // This is used only by Pepper/NaCl File API.
  //
  virtual void Touch(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback) = 0;

  // Truncates a file at |path| to |length|. If |length| is larger than
  // the original file size, the file will be extended, and the extended
  // part is filled with null bytes.
  //
  // FileSystemOperationImpl::Truncate calls this.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if the file doesn't exist.
  //
  virtual void Truncate(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      int64 length,
      const StatusCallback& callback) = 0;

  // Copies a file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  // |progress_callback| is a callback to report the progress update.
  // See file_system_operations.h for details. This should be called on the
  // same thread as where the method's called (IO thread). Calling this
  // is optional. It is recommended to use this callback for heavier operations
  // (such as file network downloading), so that, e.g., clients (UIs) can
  // update its state to show progress to users. This may be a null callback.
  //
  // FileSystemOperationImpl::Copy calls this for same-filesystem copy case.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  virtual void CopyFileLocal(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const CopyFileProgressCallback& progress_callback,
      const StatusCallback& callback) = 0;

  // Moves a local file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  //
  // FileSystemOperationImpl::Move calls this for same-filesystem move case.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  virtual void MoveFileLocal(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const StatusCallback& callback) = 0;

  // Copies in a single file from a different filesystem.
  //
  // FileSystemOperationImpl::Copy or Move calls this for cross-filesystem
  // cases.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |src_file_path|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  virtual void CopyInForeignFile(
        scoped_ptr<FileSystemOperationContext> context,
        const base::FilePath& src_file_path,
        const FileSystemURL& dest_url,
        const StatusCallback& callback) = 0;

  // Deletes a single file.
  //
  // FileSystemOperationImpl::RemoveFile calls this.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |url| is not a file.
  //
  virtual void DeleteFile(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const StatusCallback& callback) = 0;

  // Removes a single empty directory.
  //
  // FileSystemOperationImpl::RemoveDirectory calls this.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_DIRECTORY if |url| is not a directory.
  // - File::FILE_ERROR_NOT_EMPTY if |url| is not empty.
  //
  virtual void DeleteDirectory(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const StatusCallback& callback) = 0;

  // Removes a single file or a single directory with its contents
  // (i.e. files/subdirectories under the directory).
  //
  // FileSystemOperationImpl::Remove calls this.
  // On some platforms, such as Chrome OS Drive File System, recursive file
  // deletion can be implemented more efficiently than calling DeleteFile() and
  // DeleteDirectory() for each files/directories.
  // This method is optional, so if not supported,
  // File::FILE_ERROR_INVALID_OPERATION should be returned via |callback|.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_INVALID_OPERATION if this operation is not supported.
  virtual void DeleteRecursively(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const StatusCallback& callback) = 0;

  // Creates a local snapshot file for a given |url| and returns the
  // metadata and platform path of the snapshot file via |callback|.
  // In regular filesystem cases the implementation may simply return
  // the metadata of the file itself (as well as GetMetadata does),
  // while in non-regular filesystem case the backend may create a
  // temporary snapshot file which holds the file data and return
  // the metadata of the temporary file.
  //
  // In the callback, it returns:
  // |file_info| is the metadata of the snapshot file created.
  // |platform_path| is the full absolute platform path to the snapshot
  // file created.  If a file is not backed by a real local file in
  // the implementor's FileSystem, the implementor must create a
  // local snapshot file and return the path of the created file.
  //
  // If implementors creates a temporary file for snapshotting and wants
  // FileAPI backend to take care of the lifetime of the file (so that
  // it won't get deleted while JS layer has any references to the created
  // File/Blob object), it should return non-empty |file_ref|.
  // Via the |file_ref| implementors can schedule a file deletion
  // or arbitrary callbacks when the last reference of File/Blob is dropped.
  //
  // FileSystemOperationImpl::CreateSnapshotFile calls this.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |url| exists but is a directory.
  //
  // The field values of |file_info| are undefined (implementation
  // dependent) in error cases, and the caller should always
  // check the return code.
  virtual void CreateSnapshotFile(
      scoped_ptr<FileSystemOperationContext> context,
      const FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_ASYNC_FILE_UTIL_H_
