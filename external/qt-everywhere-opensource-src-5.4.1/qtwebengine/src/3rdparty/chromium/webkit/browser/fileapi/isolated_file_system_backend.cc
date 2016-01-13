// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/isolated_file_system_backend.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util_proxy.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/dragged_file_util.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/browser/fileapi/transient_file_util.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

IsolatedFileSystemBackend::IsolatedFileSystemBackend()
    : isolated_file_util_(new AsyncFileUtilAdapter(new LocalFileUtil())),
      dragged_file_util_(new AsyncFileUtilAdapter(new DraggedFileUtil())),
      transient_file_util_(new AsyncFileUtilAdapter(new TransientFileUtil())) {
}

IsolatedFileSystemBackend::~IsolatedFileSystemBackend() {
}

bool IsolatedFileSystemBackend::CanHandleType(FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeIsolated:
    case kFileSystemTypeDragged:
    case kFileSystemTypeForTransientFile:
      return true;
#if !defined(OS_CHROMEOS)
    case kFileSystemTypeNativeLocal:
    case kFileSystemTypeNativeForPlatformApp:
      return true;
#endif
    default:
      return false;
  }
}

void IsolatedFileSystemBackend::Initialize(FileSystemContext* context) {
}

void IsolatedFileSystemBackend::ResolveURL(
    const FileSystemURL& url,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  // We never allow opening a new isolated FileSystem via usual ResolveURL.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 GURL(),
                 std::string(),
                 base::File::FILE_ERROR_SECURITY));
}

AsyncFileUtil* IsolatedFileSystemBackend::GetAsyncFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeNativeLocal:
      return isolated_file_util_.get();
    case kFileSystemTypeDragged:
      return dragged_file_util_.get();
    case kFileSystemTypeForTransientFile:
      return transient_file_util_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

CopyOrMoveFileValidatorFactory*
IsolatedFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type, base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return NULL;
}

FileSystemOperation* IsolatedFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  return FileSystemOperation::Create(
      url, context, make_scoped_ptr(new FileSystemOperationContext(context)));
}

bool IsolatedFileSystemBackend::SupportsStreaming(
    const fileapi::FileSystemURL& url) const {
  return false;
}

scoped_ptr<webkit_blob::FileStreamReader>
IsolatedFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      webkit_blob::FileStreamReader::CreateForLocalFile(
          context->default_file_task_runner(),
          url.path(), offset, expected_modification_time));
}

scoped_ptr<FileStreamWriter> IsolatedFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<FileStreamWriter>(
      FileStreamWriter::CreateForLocalFile(
          context->default_file_task_runner(),
          url.path(),
          offset,
          FileStreamWriter::OPEN_EXISTING_FILE));
}

FileSystemQuotaUtil* IsolatedFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

}  // namespace fileapi
