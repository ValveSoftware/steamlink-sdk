// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_
#define WEBKIT_BROWSER_FILEAPI_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class PluginPrivateFileSystemBackendTest;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace fileapi {

class ObfuscatedFileUtil;

class WEBKIT_STORAGE_BROWSER_EXPORT PluginPrivateFileSystemBackend
    : public FileSystemBackend,
      public FileSystemQuotaUtil {
 public:
  class FileSystemIDToPluginMap;
  typedef base::Callback<void(base::File::Error result)> StatusCallback;

  PluginPrivateFileSystemBackend(
      base::SequencedTaskRunner* file_task_runner,
      const base::FilePath& profile_path,
      quota::SpecialStoragePolicy* special_storage_policy,
      const FileSystemOptions& file_system_options);
  virtual ~PluginPrivateFileSystemBackend();

  // This must be used to open 'private' filesystem instead of regular
  // OpenFileSystem.
  // |plugin_id| must be an identifier string for per-plugin
  // isolation, e.g. name, MIME type etc.
  // NOTE: |plugin_id| must be sanitized ASCII string that doesn't
  // include *any* dangerous character like '/'.
  void OpenPrivateFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      const std::string& filesystem_id,
      const std::string& plugin_id,
      OpenFileSystemMode mode,
      const StatusCallback& callback);

  // FileSystemBackend overrides.
  virtual bool CanHandleType(FileSystemType type) const OVERRIDE;
  virtual void Initialize(FileSystemContext* context) OVERRIDE;
  virtual void ResolveURL(const FileSystemURL& url,
                          OpenFileSystemMode mode,
                          const OpenFileSystemCallback& callback) OVERRIDE;
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) OVERRIDE;
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) OVERRIDE;
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const OVERRIDE;
  virtual bool SupportsStreaming(const FileSystemURL& url) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const OVERRIDE;
  virtual FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

  // FileSystemQuotaUtil overrides.
  virtual base::File::Error DeleteOriginDataOnFileTaskRunner(
      FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void GetOriginsForTypeOnFileTaskRunner(
      FileSystemType type,
      std::set<GURL>* origins) OVERRIDE;
  virtual void GetOriginsForHostOnFileTaskRunner(
      FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins) OVERRIDE;
  virtual int64 GetOriginUsageOnFileTaskRunner(
      FileSystemContext* context,
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual scoped_refptr<QuotaReservation>
      CreateQuotaReservationOnFileTaskRunner(
          const GURL& origin_url,
          FileSystemType type) OVERRIDE;
  virtual void AddFileUpdateObserver(
      FileSystemType type,
      FileUpdateObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual void AddFileChangeObserver(
      FileSystemType type,
      FileChangeObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual void AddFileAccessObserver(
      FileSystemType type,
      FileAccessObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const OVERRIDE;
  virtual const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const OVERRIDE;
  virtual const AccessObserverList* GetAccessObservers(
      FileSystemType type) const OVERRIDE;

 private:
  friend class content::PluginPrivateFileSystemBackendTest;

  ObfuscatedFileUtil* obfuscated_file_util();
  const base::FilePath& base_path() const { return base_path_; }

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  const FileSystemOptions file_system_options_;
  const base::FilePath base_path_;
  scoped_ptr<AsyncFileUtil> file_util_;
  FileSystemIDToPluginMap* plugin_map_;  // Owned by file_util_.
  base::WeakPtrFactory<PluginPrivateFileSystemBackend> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginPrivateFileSystemBackend);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_
