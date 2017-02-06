// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_system_quota_client.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_usage_cache.h"
#include "storage/browser/fileapi/sandbox_file_system_backend.h"
#include "storage/common/fileapi/file_system_util.h"
#include "url/gurl.h"

using storage::StorageType;

namespace storage {

namespace {

void GetOriginsForTypeOnFileTaskRunner(
    FileSystemContext* context,
    StorageType storage_type,
    std::set<GURL>* origins_ptr) {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return;
  quota_util->GetOriginsForTypeOnFileTaskRunner(type, origins_ptr);
}

void GetOriginsForHostOnFileTaskRunner(
    FileSystemContext* context,
    StorageType storage_type,
    const std::string& host,
    std::set<GURL>* origins_ptr) {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return;
  quota_util->GetOriginsForHostOnFileTaskRunner(type, host, origins_ptr);
}

void DidGetOrigins(const storage::QuotaClient::GetOriginsCallback& callback,
                   std::set<GURL>* origins_ptr) {
  callback.Run(*origins_ptr);
}

storage::QuotaStatusCode DeleteOriginOnFileTaskRunner(
    FileSystemContext* context,
    const GURL& origin,
    FileSystemType type) {
  FileSystemBackend* provider = context->GetFileSystemBackend(type);
  if (!provider || !provider->GetQuotaUtil())
    return storage::kQuotaErrorNotSupported;
  base::File::Error result =
      provider->GetQuotaUtil()->DeleteOriginDataOnFileTaskRunner(
          context, context->quota_manager_proxy(), origin, type);
  if (result == base::File::FILE_OK)
    return storage::kQuotaStatusOk;
  return storage::kQuotaErrorInvalidModification;
}

}  // namespace

FileSystemQuotaClient::FileSystemQuotaClient(
    FileSystemContext* file_system_context,
    bool is_incognito)
    : file_system_context_(file_system_context),
      is_incognito_(is_incognito) {
}

FileSystemQuotaClient::~FileSystemQuotaClient() {}

storage::QuotaClient::ID FileSystemQuotaClient::id() const {
  return storage::QuotaClient::kFileSystem;
}

void FileSystemQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void FileSystemQuotaClient::GetOriginUsage(
    const GURL& origin_url,
    StorageType storage_type,
    const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    callback.Run(0);
    return;
  }

  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = file_system_context_->GetQuotaUtil(type);
  if (!quota_util) {
    callback.Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      file_task_runner(), FROM_HERE,
      // It is safe to pass Unretained(quota_util) since context owns it.
      base::Bind(&FileSystemQuotaUtil::GetOriginUsageOnFileTaskRunner,
                 base::Unretained(quota_util),
                 base::RetainedRef(file_system_context_), origin_url, type),
      callback);
}

void FileSystemQuotaClient::GetOriginsForType(
    StorageType storage_type,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    std::set<GURL> origins;
    callback.Run(origins);
    return;
  }

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  file_task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&GetOriginsForTypeOnFileTaskRunner,
                            base::RetainedRef(file_system_context_),
                            storage_type, base::Unretained(origins_ptr)),
      base::Bind(&DidGetOrigins, callback, base::Owned(origins_ptr)));
}

void FileSystemQuotaClient::GetOriginsForHost(
    StorageType storage_type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    std::set<GURL> origins;
    callback.Run(origins);
    return;
  }

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  file_task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&GetOriginsForHostOnFileTaskRunner,
                            base::RetainedRef(file_system_context_),
                            storage_type, host, base::Unretained(origins_ptr)),
      base::Bind(&DidGetOrigins, callback, base::Owned(origins_ptr)));
}

void FileSystemQuotaClient::DeleteOriginData(
    const GURL& origin,
    StorageType type,
    const DeletionCallback& callback) {
  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);

  base::PostTaskAndReplyWithResult(
      file_task_runner(), FROM_HERE,
      base::Bind(&DeleteOriginOnFileTaskRunner,
                 base::RetainedRef(file_system_context_), origin, fs_type),
      callback);
}

bool FileSystemQuotaClient::DoesSupport(
    storage::StorageType storage_type) const {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);
  return file_system_context_->IsSandboxFileSystem(type);
}

base::SequencedTaskRunner* FileSystemQuotaClient::file_task_runner() const {
  return file_system_context_->default_file_task_runner();
}

}  // namespace storage
