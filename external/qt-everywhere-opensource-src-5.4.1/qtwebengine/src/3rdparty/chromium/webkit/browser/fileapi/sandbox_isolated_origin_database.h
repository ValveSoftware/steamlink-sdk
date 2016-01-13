// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_

#include <string>
#include <vector>

#include "webkit/browser/fileapi/sandbox_origin_database_interface.h"

namespace fileapi {

class SandboxOriginDatabase;

// This origin database implementation supports only one origin
// (therefore is expected to run very fast).
class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE SandboxIsolatedOriginDatabase
    : public SandboxOriginDatabaseInterface {
 public:
  static const base::FilePath::CharType kObsoleteOriginDirectory[];

  // Initialize this database for |origin| which makes GetPathForOrigin return
  // |origin_directory| (in |file_system_directory|).
  SandboxIsolatedOriginDatabase(
      const std::string& origin,
      const base::FilePath& file_system_directory,
      const base::FilePath& origin_directory);
  virtual ~SandboxIsolatedOriginDatabase();

  // SandboxOriginDatabaseInterface overrides.
  virtual bool HasOriginPath(const std::string& origin) OVERRIDE;
  virtual bool GetPathForOrigin(const std::string& origin,
                                base::FilePath* directory) OVERRIDE;
  virtual bool RemovePathForOrigin(const std::string& origin) OVERRIDE;
  virtual bool ListAllOrigins(std::vector<OriginRecord>* origins) OVERRIDE;
  virtual void DropDatabase() OVERRIDE;

  // TODO(kinuko): Deprecate this after a few release cycles, e.g. around M33.
  static void MigrateBackFromObsoleteOriginDatabase(
      const std::string& origin,
      const base::FilePath& file_system_directory,
      SandboxOriginDatabase* origin_database);

  const std::string& origin() const { return origin_; }

 private:
  void MigrateDatabaseIfNeeded();

  bool migration_checked_;
  const std::string origin_;
  const base::FilePath file_system_directory_;
  const base::FilePath origin_directory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxIsolatedOriginDatabase);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_
