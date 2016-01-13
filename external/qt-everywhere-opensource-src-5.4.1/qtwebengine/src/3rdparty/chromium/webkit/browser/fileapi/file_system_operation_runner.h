// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPERATION_RUNNER_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPERATION_RUNNER_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace net {
class URLRequestContext;
}

namespace fileapi {

class FileSystemURL;
class FileSystemContext;

// A central interface for running FileSystem API operations.
// All operation methods take callback and returns OperationID, which is
// an integer value which can be used for cancelling an operation.
// All operation methods return kErrorOperationID if running (posting) an
// operation fails, in addition to dispatching the callback with an error
// code (therefore in most cases the caller does not need to check the
// returned operation ID).
class WEBKIT_STORAGE_BROWSER_EXPORT FileSystemOperationRunner
    : public base::SupportsWeakPtr<FileSystemOperationRunner> {
 public:
  typedef FileSystemOperation::GetMetadataCallback GetMetadataCallback;
  typedef FileSystemOperation::ReadDirectoryCallback ReadDirectoryCallback;
  typedef FileSystemOperation::SnapshotFileCallback SnapshotFileCallback;
  typedef FileSystemOperation::StatusCallback StatusCallback;
  typedef FileSystemOperation::WriteCallback WriteCallback;
  typedef FileSystemOperation::OpenFileCallback OpenFileCallback;
  typedef FileSystemOperation::CopyProgressCallback CopyProgressCallback;
  typedef FileSystemOperation::CopyFileProgressCallback
      CopyFileProgressCallback;
  typedef FileSystemOperation::CopyOrMoveOption CopyOrMoveOption;

  typedef int OperationID;

  virtual ~FileSystemOperationRunner();

  // Cancels all inflight operations.
  void Shutdown();

  // Creates a file at |url|. If |exclusive| is true, an error is raised
  // in case a file is already present at the URL.
  OperationID CreateFile(const FileSystemURL& url,
                         bool exclusive,
                         const StatusCallback& callback);

  OperationID CreateDirectory(const FileSystemURL& url,
                              bool exclusive,
                              bool recursive,
                              const StatusCallback& callback);

  // Copies a file or directory from |src_url| to |dest_url|. If
  // |src_url| is a directory, the contents of |src_url| are copied to
  // |dest_url| recursively. A new file or directory is created at
  // |dest_url| as needed.
  // For |option| and |progress_callback|, see file_system_operation.h for
  // details.
  OperationID Copy(const FileSystemURL& src_url,
                   const FileSystemURL& dest_url,
                   CopyOrMoveOption option,
                   const CopyProgressCallback& progress_callback,
                   const StatusCallback& callback);

  // Moves a file or directory from |src_url| to |dest_url|. A new file
  // or directory is created at |dest_url| as needed.
  // For |option|, see file_system_operation.h for details.
  OperationID Move(const FileSystemURL& src_url,
                   const FileSystemURL& dest_url,
                   CopyOrMoveOption option,
                   const StatusCallback& callback);

  // Checks if a directory is present at |url|.
  OperationID DirectoryExists(const FileSystemURL& url,
                              const StatusCallback& callback);

  // Checks if a file is present at |url|.
  OperationID FileExists(const FileSystemURL& url,
                         const StatusCallback& callback);

  // Gets the metadata of a file or directory at |url|.
  OperationID GetMetadata(const FileSystemURL& url,
                          const GetMetadataCallback& callback);

  // Reads contents of a directory at |url|.
  OperationID ReadDirectory(const FileSystemURL& url,
                            const ReadDirectoryCallback& callback);

  // Removes a file or directory at |url|. If |recursive| is true, remove
  // all files and directories under the directory at |url| recursively.
  OperationID Remove(const FileSystemURL& url, bool recursive,
                     const StatusCallback& callback);

  // Writes contents of |blob_url| to |url| at |offset|.
  // |url_request_context| is used to read contents in |blob|.
  OperationID Write(const net::URLRequestContext* url_request_context,
                    const FileSystemURL& url,
                    scoped_ptr<webkit_blob::BlobDataHandle> blob,
                    int64 offset,
                    const WriteCallback& callback);

  // Truncates a file at |url| to |length|. If |length| is larger than
  // the original file size, the file will be extended, and the extended
  // part is filled with null bytes.
  OperationID Truncate(const FileSystemURL& url, int64 length,
                       const StatusCallback& callback);

  // Tries to cancel the operation |id| [we support cancelling write or
  // truncate only]. Reports failure for the current operation, then reports
  // success for the cancel operation itself via the |callback|.
  void Cancel(OperationID id, const StatusCallback& callback);

  // Modifies timestamps of a file or directory at |url| with
  // |last_access_time| and |last_modified_time|. The function DOES NOT
  // create a file unlike 'touch' command on Linux.
  //
  // This function is used only by Pepper as of writing.
  OperationID TouchFile(const FileSystemURL& url,
                        const base::Time& last_access_time,
                        const base::Time& last_modified_time,
                        const StatusCallback& callback);

  // Opens a file at |url| with |file_flags|, where flags are OR'ed
  // values of base::PlatformFileFlags.
  //
  // |peer_handle| is the process handle of a pepper plugin process, which
  // is necessary for underlying IPC calls with Pepper plugins.
  //
  // This function is used only by Pepper as of writing.
  OperationID OpenFile(const FileSystemURL& url,
                       int file_flags,
                       const OpenFileCallback& callback);

  // Creates a local snapshot file for a given |url| and returns the
  // metadata and platform url of the snapshot file via |callback|.
  // In local filesystem cases the implementation may simply return
  // the metadata of the file itself (as well as GetMetadata does),
  // while in remote filesystem case the backend may want to download the file
  // into a temporary snapshot file and return the metadata of the
  // temporary file.  Or if the implementaiton already has the local cache
  // data for |url| it can simply return the url to the cache.
  OperationID CreateSnapshotFile(const FileSystemURL& url,
                                 const SnapshotFileCallback& callback);

  // Copies in a single file from a different filesystem.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |src_file_path|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  OperationID CopyInForeignFile(const base::FilePath& src_local_disk_path,
                                const FileSystemURL& dest_url,
                                const StatusCallback& callback);

  // Removes a single file.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |url| is not a file.
  //
  OperationID RemoveFile(const FileSystemURL& url,
                         const StatusCallback& callback);

  // Removes a single empty directory.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_DIRECTORY if |url| is not a directory.
  // - File::FILE_ERROR_NOT_EMPTY if |url| is not empty.
  //
  OperationID RemoveDirectory(const FileSystemURL& url,
                              const StatusCallback& callback);

  // Copies a file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  // For |option| and |progress_callback|, see file_system_operation.h for
  // details.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  OperationID CopyFileLocal(const FileSystemURL& src_url,
                            const FileSystemURL& dest_url,
                            CopyOrMoveOption option,
                            const CopyFileProgressCallback& progress_callback,
                            const StatusCallback& callback);

  // Moves a local file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  // For |option|, see file_system_operation.h for details.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  OperationID MoveFileLocal(const FileSystemURL& src_url,
                            const FileSystemURL& dest_url,
                            CopyOrMoveOption option,
                            const StatusCallback& callback);

  // This is called only by pepper plugin as of writing to synchronously get
  // the underlying platform path to upload a file in the sandboxed filesystem
  // (e.g. TEMPORARY or PERSISTENT).
  base::File::Error SyncGetPlatformPath(const FileSystemURL& url,
                                        base::FilePath* platform_path);

 private:
  class BeginOperationScoper;

  struct OperationHandle {
    OperationID id;
    base::WeakPtr<BeginOperationScoper> scope;

    OperationHandle();
    ~OperationHandle();
  };

  friend class FileSystemContext;
  explicit FileSystemOperationRunner(FileSystemContext* file_system_context);

  void DidFinish(const OperationHandle& handle,
                 const StatusCallback& callback,
                 base::File::Error rv);
  void DidGetMetadata(const OperationHandle& handle,
                      const GetMetadataCallback& callback,
                      base::File::Error rv,
                      const base::File::Info& file_info);
  void DidReadDirectory(const OperationHandle& handle,
                        const ReadDirectoryCallback& callback,
                        base::File::Error rv,
                        const std::vector<DirectoryEntry>& entries,
                        bool has_more);
  void DidWrite(const OperationHandle& handle,
                const WriteCallback& callback,
                base::File::Error rv,
                int64 bytes,
                bool complete);
  void DidOpenFile(
      const OperationHandle& handle,
      const OpenFileCallback& callback,
      base::File file,
      const base::Closure& on_close_callback);
  void DidCreateSnapshot(
      const OperationHandle& handle,
      const SnapshotFileCallback& callback,
      base::File::Error rv,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  void OnCopyProgress(
      const OperationHandle& handle,
      const CopyProgressCallback& callback,
      FileSystemOperation::CopyProgressType type,
      const FileSystemURL& source_url,
      const FileSystemURL& dest_url,
      int64 size);

  void PrepareForWrite(OperationID id, const FileSystemURL& url);
  void PrepareForRead(OperationID id, const FileSystemURL& url);

  // These must be called at the beginning and end of any async operations.
  OperationHandle BeginOperation(FileSystemOperation* operation,
                                 base::WeakPtr<BeginOperationScoper> scope);
  void FinishOperation(OperationID id);

  // Not owned; file_system_context owns this.
  FileSystemContext* file_system_context_;

  // IDMap<FileSystemOperation, IDMapOwnPointer> operations_;
  IDMap<FileSystemOperation, IDMapOwnPointer> operations_;

  // We keep track of the file to be modified by each operation so that
  // we can notify observers when we're done.
  typedef std::map<OperationID, FileSystemURLSet> OperationToURLSet;
  OperationToURLSet write_target_urls_;

  // Operations that are finished but not yet fire their callbacks.
  std::set<OperationID> finished_operations_;

  // Callbacks for stray cancels whose target operation is already finished.
  std::map<OperationID, StatusCallback> stray_cancel_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationRunner);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPERATION_RUNNER_H_
