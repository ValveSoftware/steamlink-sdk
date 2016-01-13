// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_operation_runner.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "net/url_request/url_request_context.h"
#include "webkit/browser/blob/blob_url_request_job_factory.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_writer_delegate.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace fileapi {

typedef FileSystemOperationRunner::OperationID OperationID;

class FileSystemOperationRunner::BeginOperationScoper
    : public base::SupportsWeakPtr<
          FileSystemOperationRunner::BeginOperationScoper> {
 public:
  BeginOperationScoper() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(BeginOperationScoper);
};

FileSystemOperationRunner::OperationHandle::OperationHandle() {}
FileSystemOperationRunner::OperationHandle::~OperationHandle() {}

FileSystemOperationRunner::~FileSystemOperationRunner() {
}

void FileSystemOperationRunner::Shutdown() {
  operations_.Clear();
}

OperationID FileSystemOperationRunner::CreateFile(
    const FileSystemURL& url,
    bool exclusive,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);

  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForWrite(handle.id, url);
  operation->CreateFile(
      url, exclusive,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::CreateDirectory(
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForWrite(handle.id, url);
  operation->CreateDirectory(
      url, exclusive, recursive,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::Copy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyProgressCallback& progress_callback,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(dest_url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForWrite(handle.id, dest_url);
  PrepareForRead(handle.id, src_url);
  operation->Copy(
      src_url, dest_url, option,
      progress_callback.is_null() ?
          CopyProgressCallback() :
          base::Bind(&FileSystemOperationRunner::OnCopyProgress, AsWeakPtr(),
                     handle, progress_callback),
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::Move(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(dest_url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForWrite(handle.id, dest_url);
  PrepareForWrite(handle.id, src_url);
  operation->Move(
      src_url, dest_url, option,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::DirectoryExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForRead(handle.id, url);
  operation->DirectoryExists(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::FileExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForRead(handle.id, url);
  operation->FileExists(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::GetMetadata(
    const FileSystemURL& url,
    const GetMetadataCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidGetMetadata(handle, callback, error, base::File::Info());
    return handle.id;
  }
  PrepareForRead(handle.id, url);
  operation->GetMetadata(
      url,
      base::Bind(&FileSystemOperationRunner::DidGetMetadata, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::ReadDirectory(
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidReadDirectory(handle, callback, error, std::vector<DirectoryEntry>(),
                     false);
    return handle.id;
  }
  PrepareForRead(handle.id, url);
  operation->ReadDirectory(
      url,
      base::Bind(&FileSystemOperationRunner::DidReadDirectory, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::Remove(
    const FileSystemURL& url, bool recursive,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForWrite(handle.id, url);
  operation->Remove(
      url, recursive,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::Write(
    const net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    scoped_ptr<webkit_blob::BlobDataHandle> blob,
    int64 offset,
    const WriteCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);

  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidWrite(handle, callback, error, 0, true);
    return handle.id;
  }

  scoped_ptr<FileStreamWriter> writer(
      file_system_context_->CreateFileStreamWriter(url, offset));
  if (!writer) {
    // Write is not supported.
    DidWrite(handle, callback, base::File::FILE_ERROR_SECURITY, 0, true);
    return handle.id;
  }

  FileWriterDelegate::FlushPolicy flush_policy =
      file_system_context_->ShouldFlushOnWriteCompletion(url.type())
          ? FileWriterDelegate::FLUSH_ON_COMPLETION
          : FileWriterDelegate::NO_FLUSH_ON_COMPLETION;
  scoped_ptr<FileWriterDelegate> writer_delegate(
      new FileWriterDelegate(writer.Pass(), flush_policy));

  scoped_ptr<net::URLRequest> blob_request(
      webkit_blob::BlobProtocolHandler::CreateBlobRequest(
          blob.Pass(),
          url_request_context,
          writer_delegate.get()));

  PrepareForWrite(handle.id, url);
  operation->Write(
      url, writer_delegate.Pass(), blob_request.Pass(),
      base::Bind(&FileSystemOperationRunner::DidWrite, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::Truncate(
    const FileSystemURL& url, int64 length,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForWrite(handle.id, url);
  operation->Truncate(
      url, length,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

void FileSystemOperationRunner::Cancel(
    OperationID id,
    const StatusCallback& callback) {
  if (ContainsKey(finished_operations_, id)) {
    DCHECK(!ContainsKey(stray_cancel_callbacks_, id));
    stray_cancel_callbacks_[id] = callback;
    return;
  }
  FileSystemOperation* operation = operations_.Lookup(id);
  if (!operation) {
    // There is no operation with |id|.
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  operation->Cancel(callback);
}

OperationID FileSystemOperationRunner::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  PrepareForWrite(handle.id, url);
  operation->TouchFile(
      url, last_access_time, last_modified_time,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::OpenFile(
    const FileSystemURL& url,
    int file_flags,
    const OpenFileCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidOpenFile(handle, callback, base::File(error), base::Closure());
    return handle.id;
  }
  if (file_flags &
      (base::File::FLAG_CREATE | base::File::FLAG_OPEN_ALWAYS |
       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_OPEN_TRUNCATED |
       base::File::FLAG_WRITE | base::File::FLAG_EXCLUSIVE_WRITE |
       base::File::FLAG_DELETE_ON_CLOSE |
       base::File::FLAG_WRITE_ATTRIBUTES)) {
    PrepareForWrite(handle.id, url);
  } else {
    PrepareForRead(handle.id, url);
  }
  operation->OpenFile(
      url, file_flags,
      base::Bind(&FileSystemOperationRunner::DidOpenFile, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::CreateSnapshotFile(
    const FileSystemURL& url,
    const SnapshotFileCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidCreateSnapshot(handle, callback, error, base::File::Info(),
                      base::FilePath(), NULL);
    return handle.id;
  }
  PrepareForRead(handle.id, url);
  operation->CreateSnapshotFile(
      url,
      base::Bind(&FileSystemOperationRunner::DidCreateSnapshot, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::CopyInForeignFile(
    const base::FilePath& src_local_disk_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(dest_url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  operation->CopyInForeignFile(
      src_local_disk_path, dest_url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::RemoveFile(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  operation->RemoveFile(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::RemoveDirectory(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  operation->RemoveDirectory(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::CopyFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(src_url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  operation->CopyFileLocal(
      src_url, dest_url, option, progress_callback,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

OperationID FileSystemOperationRunner::MoveFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(src_url, &error);
  BeginOperationScoper scope;
  OperationHandle handle = BeginOperation(operation, scope.AsWeakPtr());
  if (!operation) {
    DidFinish(handle, callback, error);
    return handle.id;
  }
  operation->MoveFileLocal(
      src_url, dest_url, option,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 handle, callback));
  return handle.id;
}

base::File::Error FileSystemOperationRunner::SyncGetPlatformPath(
    const FileSystemURL& url,
    base::FilePath* platform_path) {
  base::File::Error error = base::File::FILE_OK;
  scoped_ptr<FileSystemOperation> operation(
      file_system_context_->CreateFileSystemOperation(url, &error));
  if (!operation.get())
    return error;
  return operation->SyncGetPlatformPath(url, platform_path);
}

FileSystemOperationRunner::FileSystemOperationRunner(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context) {}

void FileSystemOperationRunner::DidFinish(
    const OperationHandle& handle,
    const StatusCallback& callback,
    base::File::Error rv) {
  if (handle.scope) {
    finished_operations_.insert(handle.id);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&FileSystemOperationRunner::DidFinish,
                              AsWeakPtr(), handle, callback, rv));
    return;
  }
  callback.Run(rv);
  FinishOperation(handle.id);
}

void FileSystemOperationRunner::DidGetMetadata(
    const OperationHandle& handle,
    const GetMetadataCallback& callback,
    base::File::Error rv,
    const base::File::Info& file_info) {
  if (handle.scope) {
    finished_operations_.insert(handle.id);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&FileSystemOperationRunner::DidGetMetadata,
                              AsWeakPtr(), handle, callback, rv, file_info));
    return;
  }
  callback.Run(rv, file_info);
  FinishOperation(handle.id);
}

void FileSystemOperationRunner::DidReadDirectory(
    const OperationHandle& handle,
    const ReadDirectoryCallback& callback,
    base::File::Error rv,
    const std::vector<DirectoryEntry>& entries,
    bool has_more) {
  if (handle.scope) {
    finished_operations_.insert(handle.id);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&FileSystemOperationRunner::DidReadDirectory,
                              AsWeakPtr(), handle, callback, rv,
                              entries, has_more));
    return;
  }
  callback.Run(rv, entries, has_more);
  if (rv != base::File::FILE_OK || !has_more)
    FinishOperation(handle.id);
}

void FileSystemOperationRunner::DidWrite(
    const OperationHandle& handle,
    const WriteCallback& callback,
    base::File::Error rv,
    int64 bytes,
    bool complete) {
  if (handle.scope) {
    finished_operations_.insert(handle.id);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&FileSystemOperationRunner::DidWrite, AsWeakPtr(),
                              handle, callback, rv, bytes, complete));
    return;
  }
  callback.Run(rv, bytes, complete);
  if (rv != base::File::FILE_OK || complete)
    FinishOperation(handle.id);
}

void FileSystemOperationRunner::DidOpenFile(
    const OperationHandle& handle,
    const OpenFileCallback& callback,
    base::File file,
    const base::Closure& on_close_callback) {
  if (handle.scope) {
    finished_operations_.insert(handle.id);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&FileSystemOperationRunner::DidOpenFile,
                              AsWeakPtr(), handle, callback, Passed(&file),
                              on_close_callback));
    return;
  }
  callback.Run(file.Pass(), on_close_callback);
  FinishOperation(handle.id);
}

void FileSystemOperationRunner::DidCreateSnapshot(
    const OperationHandle& handle,
    const SnapshotFileCallback& callback,
    base::File::Error rv,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  if (handle.scope) {
    finished_operations_.insert(handle.id);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&FileSystemOperationRunner::DidCreateSnapshot,
                              AsWeakPtr(), handle, callback, rv, file_info,
                              platform_path, file_ref));
    return;
  }
  callback.Run(rv, file_info, platform_path, file_ref);
  FinishOperation(handle.id);
}

void FileSystemOperationRunner::OnCopyProgress(
    const OperationHandle& handle,
    const CopyProgressCallback& callback,
    FileSystemOperation::CopyProgressType type,
    const FileSystemURL& source_url,
    const FileSystemURL& dest_url,
    int64 size) {
  if (handle.scope) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(
            &FileSystemOperationRunner::OnCopyProgress,
            AsWeakPtr(), handle, callback, type, source_url, dest_url, size));
    return;
  }
  callback.Run(type, source_url, dest_url, size);
}

void FileSystemOperationRunner::PrepareForWrite(OperationID id,
                                                const FileSystemURL& url) {
  if (file_system_context_->GetUpdateObservers(url.type())) {
    file_system_context_->GetUpdateObservers(url.type())->Notify(
        &FileUpdateObserver::OnStartUpdate, MakeTuple(url));
  }
  write_target_urls_[id].insert(url);
}

void FileSystemOperationRunner::PrepareForRead(OperationID id,
                                               const FileSystemURL& url) {
  if (file_system_context_->GetAccessObservers(url.type())) {
    file_system_context_->GetAccessObservers(url.type())->Notify(
        &FileAccessObserver::OnAccess, MakeTuple(url));
  }
}

FileSystemOperationRunner::OperationHandle
FileSystemOperationRunner::BeginOperation(
    FileSystemOperation* operation,
    base::WeakPtr<BeginOperationScoper> scope) {
  OperationHandle handle;
  handle.id = operations_.Add(operation);
  handle.scope = scope;
  return handle;
}

void FileSystemOperationRunner::FinishOperation(OperationID id) {
  OperationToURLSet::iterator found = write_target_urls_.find(id);
  if (found != write_target_urls_.end()) {
    const FileSystemURLSet& urls = found->second;
    for (FileSystemURLSet::const_iterator iter = urls.begin();
        iter != urls.end(); ++iter) {
      if (file_system_context_->GetUpdateObservers(iter->type())) {
        file_system_context_->GetUpdateObservers(iter->type())->Notify(
            &FileUpdateObserver::OnEndUpdate, MakeTuple(*iter));
      }
    }
    write_target_urls_.erase(found);
  }

  // IDMap::Lookup fails if the operation is NULL, so we don't check
  // operations_.Lookup(id) here.

  operations_.Remove(id);
  finished_operations_.erase(id);

  // Dispatch stray cancel callback if exists.
  std::map<OperationID, StatusCallback>::iterator found_cancel =
      stray_cancel_callbacks_.find(id);
  if (found_cancel != stray_cancel_callbacks_.end()) {
    // This cancel has been requested after the operation has finished,
    // so report that we failed to stop it.
    found_cancel->second.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    stray_cancel_callbacks_.erase(found_cancel);
  }
}

}  // namespace fileapi
