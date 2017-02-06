// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_

#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/fileapi/file_system_quota_util.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/fileapi/file_system_types.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {

class FileSystemContext;

// An instance of this class is created per-profile.  This class
// is self-destructed and will delete itself when OnQuotaManagerDestroyed
// is called.
// All of the public methods of this class are called by the quota manager
// (except for the constructor/destructor).
class STORAGE_EXPORT FileSystemQuotaClient
    : public NON_EXPORTED_BASE(storage::QuotaClient) {
 public:
  FileSystemQuotaClient(
      FileSystemContext* file_system_context,
      bool is_incognito);
  ~FileSystemQuotaClient() override;

  // QuotaClient methods.
  storage::QuotaClient::ID id() const override;
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const GURL& origin_url,
                      storage::StorageType type,
                      const GetUsageCallback& callback) override;
  void GetOriginsForType(storage::StorageType type,
                         const GetOriginsCallback& callback) override;
  void GetOriginsForHost(storage::StorageType type,
                         const std::string& host,
                         const GetOriginsCallback& callback) override;
  void DeleteOriginData(const GURL& origin,
                        storage::StorageType type,
                        const DeletionCallback& callback) override;
  bool DoesSupport(storage::StorageType type) const override;

 private:
  base::SequencedTaskRunner* file_task_runner() const;

  scoped_refptr<FileSystemContext> file_system_context_;

  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaClient);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_
