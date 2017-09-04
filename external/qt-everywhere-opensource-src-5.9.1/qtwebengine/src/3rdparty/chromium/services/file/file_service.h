// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILE_FILE_SERVICE_H_
#define SERVICES_FILE_FILE_SERVICE_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/filesystem/lock_table.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/file/public/interfaces/file_system.mojom.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

namespace file {

std::unique_ptr<service_manager::Service> CreateFileService(
    scoped_refptr<base::SingleThreadTaskRunner> file_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner);

class FileService
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::FileSystem>,
      public service_manager::InterfaceFactory<leveldb::mojom::LevelDBService> {
 public:
  FileService(
      scoped_refptr<base::SingleThreadTaskRunner> file_service_runner,
      scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner);
  ~FileService() override;

 private:
  // |Service| override:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // |InterfaceFactory<mojom::FileSystem>| implementation:
  void Create(const service_manager::Identity& remote_identity,
              mojom::FileSystemRequest request) override;

  // |InterfaceFactory<LevelDBService>| implementation:
  void Create(const service_manager::Identity& remote_identity,
              leveldb::mojom::LevelDBServiceRequest request) override;

  void OnLevelDBServiceError();

  scoped_refptr<base::SingleThreadTaskRunner> file_service_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner_;

  // We create these two objects so we can delete them on the correct task
  // runners.
  class FileSystemObjects;
  std::unique_ptr<FileSystemObjects> file_system_objects_;

  class LevelDBServiceObjects;
  std::unique_ptr<LevelDBServiceObjects> leveldb_objects_;

  DISALLOW_COPY_AND_ASSIGN(FileService);
};

}  // namespace file

#endif  // SERVICES_FILE_FILE_SERVICE_H_
