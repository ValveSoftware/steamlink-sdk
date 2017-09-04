// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/file/file_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "components/filesystem/lock_table.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "services/file/file_system.h"
#include "services/file/user_id_map.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace file {

class FileService::FileSystemObjects
    : public base::SupportsWeakPtr<FileSystemObjects> {
 public:
  // Created on the main thread.
  FileSystemObjects(base::FilePath user_dir) : user_dir_(user_dir) {}

  // Destroyed on the |file_service_runner_|.
  ~FileSystemObjects() {}

  // Called on the |file_service_runner_|.
  void OnFileSystemRequest(const service_manager::Identity& remote_identity,
                           mojom::FileSystemRequest request) {
    if (!lock_table_)
      lock_table_ = new filesystem::LockTable;
    file_system_bindings_.AddBinding(new FileSystem(user_dir_, lock_table_),
                                     std::move(request));
  }

 private:
  mojo::BindingSet<mojom::FileSystem> file_system_bindings_;
  scoped_refptr<filesystem::LockTable> lock_table_;
  base::FilePath user_dir_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemObjects);
};

class FileService::LevelDBServiceObjects
    : public base::SupportsWeakPtr<LevelDBServiceObjects> {
 public:
  // Created on the main thread.
  LevelDBServiceObjects(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  // Destroyed on the |leveldb_service_runner_|.
  ~LevelDBServiceObjects() {}

  // Called on the |leveldb_service_runner_|.
  void OnLevelDBServiceRequest(const service_manager::Identity& remote_identity,
                               leveldb::mojom::LevelDBServiceRequest request) {
    if (!leveldb_service_)
      leveldb_service_.reset(new leveldb::LevelDBServiceImpl(task_runner_));
    leveldb_bindings_.AddBinding(leveldb_service_.get(), std::move(request));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Variables that are only accessible on the |leveldb_service_runner_| thread.
  std::unique_ptr<leveldb::mojom::LevelDBService> leveldb_service_;
  mojo::BindingSet<leveldb::mojom::LevelDBService> leveldb_bindings_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBServiceObjects);
};

std::unique_ptr<service_manager::Service> CreateFileService(
    scoped_refptr<base::SingleThreadTaskRunner> file_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner) {
  return base::MakeUnique<FileService>(std::move(file_service_runner),
                                       std::move(leveldb_service_runner));
}

FileService::FileService(
    scoped_refptr<base::SingleThreadTaskRunner> file_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner)
    : file_service_runner_(std::move(file_service_runner)),
      leveldb_service_runner_(std::move(leveldb_service_runner)) {}

FileService::~FileService() {
  file_service_runner_->DeleteSoon(FROM_HERE, file_system_objects_.release());
  leveldb_service_runner_->DeleteSoon(FROM_HERE, leveldb_objects_.release());
}

void FileService::OnStart() {
  file_system_objects_.reset(new FileService::FileSystemObjects(
      GetUserDirForUserId(context()->identity().user_id())));
  leveldb_objects_.reset(
      new FileService::LevelDBServiceObjects(leveldb_service_runner_));
}

bool FileService::OnConnect(const service_manager::ServiceInfo& remote_info,
                            service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<leveldb::mojom::LevelDBService>(this);
  registry->AddInterface<mojom::FileSystem>(this);
  return true;
}

void FileService::Create(const service_manager::Identity& remote_identity,
                         mojom::FileSystemRequest request) {
  file_service_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FileService::FileSystemObjects::OnFileSystemRequest,
                 file_system_objects_->AsWeakPtr(), remote_identity,
                 base::Passed(&request)));
}

void FileService::Create(const service_manager::Identity& remote_identity,
                         leveldb::mojom::LevelDBServiceRequest request) {
  leveldb_service_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &FileService::LevelDBServiceObjects::OnLevelDBServiceRequest,
          leveldb_objects_->AsWeakPtr(), remote_identity,
          base::Passed(&request)));
}

}  // namespace user_service
