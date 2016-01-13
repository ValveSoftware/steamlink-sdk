// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/plugin_private_file_system_backend.h"

#include <map>

#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "net/base/net_util.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

class PluginPrivateFileSystemBackend::FileSystemIDToPluginMap {
 public:
  explicit FileSystemIDToPluginMap(base::SequencedTaskRunner* task_runner)
      : task_runner_(task_runner) {}
  ~FileSystemIDToPluginMap() {}

  std::string GetPluginIDForURL(const FileSystemURL& url) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    Map::iterator found = map_.find(url.filesystem_id());
    if (url.type() != kFileSystemTypePluginPrivate || found == map_.end()) {
      NOTREACHED() << "Unsupported url is given: " << url.DebugString();
      return std::string();
    }
    return found->second;
  }

  void RegisterFileSystem(const std::string& filesystem_id,
                          const std::string& plugin_id) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    DCHECK(!filesystem_id.empty());
    DCHECK(!ContainsKey(map_, filesystem_id)) << filesystem_id;
    map_[filesystem_id] = plugin_id;
  }

  void RemoveFileSystem(const std::string& filesystem_id) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    map_.erase(filesystem_id);
  }

 private:
  typedef std::map<std::string, std::string> Map;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Map map_;
};

namespace {

const base::FilePath::CharType* kFileSystemDirectory =
    SandboxFileSystemBackendDelegate::kFileSystemDirectory;
const base::FilePath::CharType* kPluginPrivateDirectory =
    FILE_PATH_LITERAL("Plugins");

base::File::Error OpenFileSystemOnFileTaskRunner(
    ObfuscatedFileUtil* file_util,
    PluginPrivateFileSystemBackend::FileSystemIDToPluginMap* plugin_map,
    const GURL& origin_url,
    const std::string& filesystem_id,
    const std::string& plugin_id,
    OpenFileSystemMode mode) {
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  const bool create = (mode == OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
  file_util->GetDirectoryForOriginAndType(
      origin_url, plugin_id, create, &error);
  if (error == base::File::FILE_OK)
    plugin_map->RegisterFileSystem(filesystem_id, plugin_id);
  return error;
}

}  // namespace

PluginPrivateFileSystemBackend::PluginPrivateFileSystemBackend(
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& profile_path,
    quota::SpecialStoragePolicy* special_storage_policy,
    const FileSystemOptions& file_system_options)
    : file_task_runner_(file_task_runner),
      file_system_options_(file_system_options),
      base_path_(profile_path.Append(
          kFileSystemDirectory).Append(kPluginPrivateDirectory)),
      plugin_map_(new FileSystemIDToPluginMap(file_task_runner)),
      weak_factory_(this) {
  file_util_.reset(
      new AsyncFileUtilAdapter(new ObfuscatedFileUtil(
          special_storage_policy,
          base_path_, file_system_options.env_override(),
          file_task_runner,
          base::Bind(&FileSystemIDToPluginMap::GetPluginIDForURL,
                     base::Owned(plugin_map_)),
          std::set<std::string>(),
          NULL)));
}

PluginPrivateFileSystemBackend::~PluginPrivateFileSystemBackend() {
  if (!file_task_runner_->RunsTasksOnCurrentThread()) {
    AsyncFileUtil* file_util = file_util_.release();
    if (!file_task_runner_->DeleteSoon(FROM_HERE, file_util))
      delete file_util;
  }
}

void PluginPrivateFileSystemBackend::OpenPrivateFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    const std::string& filesystem_id,
    const std::string& plugin_id,
    OpenFileSystemMode mode,
    const StatusCallback& callback) {
  if (!CanHandleType(type) || file_system_options_.is_incognito()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_SECURITY));
    return;
  }

  PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&OpenFileSystemOnFileTaskRunner,
                 obfuscated_file_util(), plugin_map_,
                 origin_url, filesystem_id, plugin_id, mode),
      callback);
}

bool PluginPrivateFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypePluginPrivate;
}

void PluginPrivateFileSystemBackend::Initialize(FileSystemContext* context) {
}

void PluginPrivateFileSystemBackend::ResolveURL(
    const FileSystemURL& url,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  // We never allow opening a new plugin-private filesystem via usual
  // ResolveURL.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, GURL(), std::string(),
                 base::File::FILE_ERROR_SECURITY));
}

AsyncFileUtil*
PluginPrivateFileSystemBackend::GetAsyncFileUtil(FileSystemType type) {
  return file_util_.get();
}

CopyOrMoveFileValidatorFactory*
PluginPrivateFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return NULL;
}

FileSystemOperation* PluginPrivateFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  return FileSystemOperation::Create(url, context, operation_context.Pass());
}

bool PluginPrivateFileSystemBackend::SupportsStreaming(
    const fileapi::FileSystemURL& url) const {
  return false;
}

scoped_ptr<webkit_blob::FileStreamReader>
PluginPrivateFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>();
}

scoped_ptr<FileStreamWriter>
PluginPrivateFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<FileStreamWriter>();
}

FileSystemQuotaUtil* PluginPrivateFileSystemBackend::GetQuotaUtil() {
  return this;
}

base::File::Error
PluginPrivateFileSystemBackend::DeleteOriginDataOnFileTaskRunner(
    FileSystemContext* context,
    quota::QuotaManagerProxy* proxy,
    const GURL& origin_url,
    FileSystemType type) {
  if (!CanHandleType(type))
    return base::File::FILE_ERROR_SECURITY;
  bool result = obfuscated_file_util()->DeleteDirectoryForOriginAndType(
      origin_url, std::string());
  if (result)
    return base::File::FILE_OK;
  return base::File::FILE_ERROR_FAILED;
}

void PluginPrivateFileSystemBackend::GetOriginsForTypeOnFileTaskRunner(
    FileSystemType type,
    std::set<GURL>* origins) {
  if (!CanHandleType(type))
    return;
  scoped_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator> enumerator(
      obfuscated_file_util()->CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty())
    origins->insert(origin);
}

void PluginPrivateFileSystemBackend::GetOriginsForHostOnFileTaskRunner(
    FileSystemType type,
    const std::string& host,
    std::set<GURL>* origins) {
  if (!CanHandleType(type))
    return;
  scoped_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator> enumerator(
      obfuscated_file_util()->CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (host == net::GetHostOrSpecFromURL(origin))
      origins->insert(origin);
  }
}

int64 PluginPrivateFileSystemBackend::GetOriginUsageOnFileTaskRunner(
    FileSystemContext* context,
    const GURL& origin_url,
    FileSystemType type) {
  // We don't track usage on this filesystem.
  return 0;
}

scoped_refptr<QuotaReservation>
PluginPrivateFileSystemBackend::CreateQuotaReservationOnFileTaskRunner(
    const GURL& origin_url,
    FileSystemType type) {
  // We don't track usage on this filesystem.
  NOTREACHED();
  return scoped_refptr<QuotaReservation>();
}

void PluginPrivateFileSystemBackend::AddFileUpdateObserver(
    FileSystemType type,
    FileUpdateObserver* observer,
    base::SequencedTaskRunner* task_runner) {}

void PluginPrivateFileSystemBackend::AddFileChangeObserver(
    FileSystemType type,
    FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {}

void PluginPrivateFileSystemBackend::AddFileAccessObserver(
    FileSystemType type,
    FileAccessObserver* observer,
    base::SequencedTaskRunner* task_runner) {}

const UpdateObserverList* PluginPrivateFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  return NULL;
}

const ChangeObserverList* PluginPrivateFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  return NULL;
}

const AccessObserverList* PluginPrivateFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  return NULL;
}

ObfuscatedFileUtil* PluginPrivateFileSystemBackend::obfuscated_file_util() {
  return static_cast<ObfuscatedFileUtil*>(
      static_cast<AsyncFileUtilAdapter*>(file_util_.get())->sync_file_util());
}

}  // namespace fileapi
