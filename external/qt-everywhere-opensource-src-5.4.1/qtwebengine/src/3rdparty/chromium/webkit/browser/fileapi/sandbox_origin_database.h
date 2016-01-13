// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_ORIGIN_DATABASE_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_ORIGIN_DATABASE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "webkit/browser/fileapi/sandbox_origin_database_interface.h"

namespace leveldb {
class DB;
class Env;
class Status;
}

namespace tracked_objects {
class Location;
}

namespace fileapi {

// All methods of this class other than the constructor may be used only from
// the browser's FILE thread.  The constructor may be used on any thread.
class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE SandboxOriginDatabase
    : public SandboxOriginDatabaseInterface {
 public:
  // Only one instance of SandboxOriginDatabase should exist for a given path
  // at a given time.
  SandboxOriginDatabase(const base::FilePath& file_system_directory,
                        leveldb::Env* env_override);
  virtual ~SandboxOriginDatabase();

  // SandboxOriginDatabaseInterface overrides.
  virtual bool HasOriginPath(const std::string& origin) OVERRIDE;
  virtual bool GetPathForOrigin(const std::string& origin,
                                base::FilePath* directory) OVERRIDE;
  virtual bool RemovePathForOrigin(const std::string& origin) OVERRIDE;
  virtual bool ListAllOrigins(std::vector<OriginRecord>* origins) OVERRIDE;
  virtual void DropDatabase() OVERRIDE;

  base::FilePath GetDatabasePath() const;
  void RemoveDatabase();

 private:
  enum RecoveryOption {
    REPAIR_ON_CORRUPTION,
    DELETE_ON_CORRUPTION,
    FAIL_ON_CORRUPTION,
  };

  enum InitOption {
    CREATE_IF_NONEXISTENT,
    FAIL_IF_NONEXISTENT,
  };

  bool Init(InitOption init_option, RecoveryOption recovery_option);
  bool RepairDatabase(const std::string& db_path);
  void HandleError(const tracked_objects::Location& from_here,
                   const leveldb::Status& status);
  void ReportInitStatus(const leveldb::Status& status);
  bool GetLastPathNumber(int* number);

  base::FilePath file_system_directory_;
  leveldb::Env* env_override_;
  scoped_ptr<leveldb::DB> db_;
  base::Time last_reported_time_;
  DISALLOW_COPY_AND_ASSIGN(SandboxOriginDatabase);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_ORIGIN_DATABASE_H_
