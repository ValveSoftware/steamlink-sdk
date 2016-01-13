// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_file_system_backend.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task_runner_util.h"
#include "url/gurl.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend_delegate.h"
#include "webkit/browser/fileapi/sandbox_quota_observer.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using quota::QuotaManagerProxy;
using quota::SpecialStoragePolicy;

namespace fileapi {

SandboxFileSystemBackend::SandboxFileSystemBackend(
    SandboxFileSystemBackendDelegate* delegate)
    : delegate_(delegate),
      enable_temporary_file_system_in_incognito_(false) {
}

SandboxFileSystemBackend::~SandboxFileSystemBackend() {
}

bool SandboxFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent;
}

void SandboxFileSystemBackend::Initialize(FileSystemContext* context) {
  DCHECK(delegate_);

  // Set quota observers.
  delegate_->RegisterQuotaUpdateObserver(fileapi::kFileSystemTypeTemporary);
  delegate_->AddFileAccessObserver(
      fileapi::kFileSystemTypeTemporary,
      delegate_->quota_observer(), NULL);

  delegate_->RegisterQuotaUpdateObserver(fileapi::kFileSystemTypePersistent);
  delegate_->AddFileAccessObserver(
      fileapi::kFileSystemTypePersistent,
      delegate_->quota_observer(), NULL);
}

void SandboxFileSystemBackend::ResolveURL(
    const FileSystemURL& url,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  if (delegate_->file_system_options().is_incognito() &&
      !(url.type() == kFileSystemTypeTemporary &&
        enable_temporary_file_system_in_incognito_)) {
    // TODO(kinuko): return an isolated temporary directory.
    callback.Run(GURL(), std::string(), base::File::FILE_ERROR_SECURITY);
    return;
  }

  delegate_->OpenFileSystem(url.origin(),
                            url.type(),
                            mode,
                            callback,
                            GetFileSystemRootURI(url.origin(), url.type()));
}

AsyncFileUtil* SandboxFileSystemBackend::GetAsyncFileUtil(
    FileSystemType type) {
  DCHECK(delegate_);
  return delegate_->file_util();
}

CopyOrMoveFileValidatorFactory*
SandboxFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return NULL;
}

FileSystemOperation* SandboxFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(error_code);

  DCHECK(delegate_);
  scoped_ptr<FileSystemOperationContext> operation_context =
      delegate_->CreateFileSystemOperationContext(url, context, error_code);
  if (!operation_context)
    return NULL;

  SpecialStoragePolicy* policy = delegate_->special_storage_policy();
  if (policy && policy->IsStorageUnlimited(url.origin()))
    operation_context->set_quota_limit_type(quota::kQuotaLimitTypeUnlimited);
  else
    operation_context->set_quota_limit_type(quota::kQuotaLimitTypeLimited);

  return FileSystemOperation::Create(url, context, operation_context.Pass());
}

bool SandboxFileSystemBackend::SupportsStreaming(
    const fileapi::FileSystemURL& url) const {
  return false;
}

scoped_ptr<webkit_blob::FileStreamReader>
SandboxFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  return delegate_->CreateFileStreamReader(
      url, offset, expected_modification_time, context);
}

scoped_ptr<fileapi::FileStreamWriter>
SandboxFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  return delegate_->CreateFileStreamWriter(url, offset, context, url.type());
}

FileSystemQuotaUtil* SandboxFileSystemBackend::GetQuotaUtil() {
  return delegate_;
}

SandboxFileSystemBackendDelegate::OriginEnumerator*
SandboxFileSystemBackend::CreateOriginEnumerator() {
  DCHECK(delegate_);
  return delegate_->CreateOriginEnumerator();
}

}  // namespace fileapi
