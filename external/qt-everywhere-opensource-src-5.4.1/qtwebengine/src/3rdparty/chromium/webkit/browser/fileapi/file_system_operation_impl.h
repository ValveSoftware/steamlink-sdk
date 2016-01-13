// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPERATION_IMPL_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPERATION_IMPL_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/file_writer_delegate.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/blob/scoped_file.h"
#include "webkit/common/quota/quota_types.h"

namespace fileapi {

class AsyncFileUtil;
class FileSystemContext;
class RecursiveOperationDelegate;

// The default implementation of FileSystemOperation for file systems.
class WEBKIT_STORAGE_BROWSER_EXPORT FileSystemOperationImpl
    : public NON_EXPORTED_BASE(FileSystemOperation) {
 public:
  virtual ~FileSystemOperationImpl();

  // FileSystemOperation overrides.
  virtual void CreateFile(const FileSystemURL& url,
                          bool exclusive,
                          const StatusCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const FileSystemURL& url,
                               bool exclusive,
                               bool recursive,
                               const StatusCallback& callback) OVERRIDE;
  virtual void Copy(const FileSystemURL& src_url,
                    const FileSystemURL& dest_url,
                    CopyOrMoveOption option,
                    const CopyProgressCallback& progress_callback,
                    const StatusCallback& callback) OVERRIDE;
  virtual void Move(const FileSystemURL& src_url,
                    const FileSystemURL& dest_url,
                    CopyOrMoveOption option,
                    const StatusCallback& callback) OVERRIDE;
  virtual void DirectoryExists(const FileSystemURL& url,
                               const StatusCallback& callback) OVERRIDE;
  virtual void FileExists(const FileSystemURL& url,
                          const StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(const FileSystemURL& url,
                           const GetMetadataCallback& callback) OVERRIDE;
  virtual void ReadDirectory(const FileSystemURL& url,
                             const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Remove(const FileSystemURL& url, bool recursive,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Write(const FileSystemURL& url,
                     scoped_ptr<FileWriterDelegate> writer_delegate,
                     scoped_ptr<net::URLRequest> blob_request,
                     const WriteCallback& callback) OVERRIDE;
  virtual void Truncate(const FileSystemURL& url, int64 length,
                        const StatusCallback& callback) OVERRIDE;
  virtual void TouchFile(const FileSystemURL& url,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const StatusCallback& callback) OVERRIDE;
  virtual void OpenFile(const FileSystemURL& url,
                        int file_flags,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void Cancel(const StatusCallback& cancel_callback) OVERRIDE;
  virtual void CreateSnapshotFile(
      const FileSystemURL& path,
      const SnapshotFileCallback& callback) OVERRIDE;
  virtual void CopyInForeignFile(const base::FilePath& src_local_disk_path,
                                 const FileSystemURL& dest_url,
                                 const StatusCallback& callback) OVERRIDE;
  virtual void RemoveFile(const FileSystemURL& url,
                          const StatusCallback& callback) OVERRIDE;
  virtual void RemoveDirectory(const FileSystemURL& url,
                               const StatusCallback& callback) OVERRIDE;
  virtual void CopyFileLocal(const FileSystemURL& src_url,
                             const FileSystemURL& dest_url,
                             CopyOrMoveOption option,
                             const CopyFileProgressCallback& progress_callback,
                             const StatusCallback& callback) OVERRIDE;
  virtual void MoveFileLocal(const FileSystemURL& src_url,
                             const FileSystemURL& dest_url,
                             CopyOrMoveOption option,
                             const StatusCallback& callback) OVERRIDE;
  virtual base::File::Error SyncGetPlatformPath(
      const FileSystemURL& url,
      base::FilePath* platform_path) OVERRIDE;

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

 private:
  friend class FileSystemOperation;

  FileSystemOperationImpl(
      const FileSystemURL& url,
      FileSystemContext* file_system_context,
      scoped_ptr<FileSystemOperationContext> operation_context);

  // Queries the quota and usage and then runs the given |task|.
  // If an error occurs during the quota query it runs |error_callback| instead.
  void GetUsageAndQuotaThenRunTask(
      const FileSystemURL& url,
      const base::Closure& task,
      const base::Closure& error_callback);

  // Called after the quota info is obtained from the quota manager
  // (which is triggered by GetUsageAndQuotaThenRunTask).
  // Sets the quota info in the operation_context_ and then runs the given
  // |task| if the returned quota status is successful, otherwise runs
  // |error_callback|.
  void DidGetUsageAndQuotaAndRunTask(
      const base::Closure& task,
      const base::Closure& error_callback,
      quota::QuotaStatusCode status,
      int64 usage, int64 quota);

  // The 'body' methods that perform the actual work (i.e. posting the
  // file task on proxy_) after the quota check.
  void DoCreateFile(const FileSystemURL& url,
                    const StatusCallback& callback, bool exclusive);
  void DoCreateDirectory(const FileSystemURL& url,
                         const StatusCallback& callback,
                         bool exclusive,
                         bool recursive);
  void DoCopyFileLocal(const FileSystemURL& src,
                       const FileSystemURL& dest,
                       CopyOrMoveOption option,
                       const CopyFileProgressCallback& progress_callback,
                       const StatusCallback& callback);
  void DoMoveFileLocal(const FileSystemURL& src,
                       const FileSystemURL& dest,
                       CopyOrMoveOption option,
                       const StatusCallback& callback);
  void DoCopyInForeignFile(const base::FilePath& src_local_disk_file_path,
                           const FileSystemURL& dest,
                           const StatusCallback& callback);
  void DoTruncate(const FileSystemURL& url,
                  const StatusCallback& callback, int64 length);
  void DoOpenFile(const FileSystemURL& url,
                  const OpenFileCallback& callback, int file_flags);

  // Callback for CreateFile for |exclusive|=true cases.
  void DidEnsureFileExistsExclusive(const StatusCallback& callback,
                                    base::File::Error rv,
                                    bool created);

  // Callback for CreateFile for |exclusive|=false cases.
  void DidEnsureFileExistsNonExclusive(const StatusCallback& callback,
                                       base::File::Error rv,
                                       bool created);

  void DidFinishOperation(const StatusCallback& callback,
                          base::File::Error rv);
  void DidDirectoryExists(const StatusCallback& callback,
                          base::File::Error rv,
                          const base::File::Info& file_info);
  void DidFileExists(const StatusCallback& callback,
                     base::File::Error rv,
                     const base::File::Info& file_info);
  void DidDeleteRecursively(const FileSystemURL& url,
                            const StatusCallback& callback,
                            base::File::Error rv);
  void DidWrite(const FileSystemURL& url,
                const WriteCallback& callback,
                base::File::Error rv,
                int64 bytes,
                FileWriterDelegate::WriteProgressStatus write_status);
  void DidOpenFile(const OpenFileCallback& callback,
                   base::File file,
                   const base::Closure& on_close_callback);

  // Used only for internal assertions.
  // Returns false if there's another inflight pending operation.
  bool SetPendingOperationType(OperationType type);

  scoped_refptr<FileSystemContext> file_system_context_;

  scoped_ptr<FileSystemOperationContext> operation_context_;
  AsyncFileUtil* async_file_util_;  // Not owned.

  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<RecursiveOperationDelegate> recursive_operation_delegate_;

  StatusCallback cancel_callback_;

  // A flag to make sure we call operation only once per instance.
  OperationType pending_operation_;

  base::WeakPtrFactory<FileSystemOperationImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationImpl);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_OPERATION_IMPL_H_
