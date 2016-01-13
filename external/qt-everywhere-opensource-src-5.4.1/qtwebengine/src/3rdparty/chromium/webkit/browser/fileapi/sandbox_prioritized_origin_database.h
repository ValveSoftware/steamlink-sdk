// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_PRIORITIZED_ORIGIN_DATABASE_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_PRIORITIZED_ORIGIN_DATABASE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/sandbox_origin_database_interface.h"

namespace leveldb {
class Env;
}

namespace fileapi {

class ObfuscatedFileUtil;
class SandboxIsolatedOriginDatabase;
class SandboxOriginDatabase;

class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE SandboxPrioritizedOriginDatabase
    : public SandboxOriginDatabaseInterface {
 public:
  SandboxPrioritizedOriginDatabase(const base::FilePath& file_system_directory,
                                   leveldb::Env* env_override);
  virtual ~SandboxPrioritizedOriginDatabase();

  // Sets |origin| as primary origin in this database (e.g. may
  // allow faster access).
  // Returns false if this database already has a primary origin
  // which is different from |origin|.
  bool InitializePrimaryOrigin(const std::string& origin);
  std::string GetPrimaryOrigin();

  // SandboxOriginDatabaseInterface overrides.
  virtual bool HasOriginPath(const std::string& origin) OVERRIDE;
  virtual bool GetPathForOrigin(const std::string& origin,
                                base::FilePath* directory) OVERRIDE;
  virtual bool RemovePathForOrigin(const std::string& origin) OVERRIDE;
  virtual bool ListAllOrigins(std::vector<OriginRecord>* origins) OVERRIDE;
  virtual void DropDatabase() OVERRIDE;

  const base::FilePath& primary_origin_file() const {
    return primary_origin_file_;
  }

 private:
  bool MaybeLoadPrimaryOrigin();
  bool ResetPrimaryOrigin(const std::string& origin);
  void MaybeMigrateDatabase(const std::string& origin);
  void MaybeInitializeDatabases(bool create);
  void MaybeInitializeNonPrimaryDatabase(bool create);

  // For migration.
  friend class ObfuscatedFileUtil;
  SandboxOriginDatabase* GetSandboxOriginDatabase();

  const base::FilePath file_system_directory_;
  leveldb::Env* env_override_;
  const base::FilePath primary_origin_file_;
  scoped_ptr<SandboxOriginDatabase> origin_database_;
  scoped_ptr<SandboxIsolatedOriginDatabase> primary_origin_database_;

  DISALLOW_COPY_AND_ASSIGN(SandboxPrioritizedOriginDatabase);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_PRIORITIZED_ORIGIN_DATABASE_H_
