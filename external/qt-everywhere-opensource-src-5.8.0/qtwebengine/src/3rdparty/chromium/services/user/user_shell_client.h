// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_USER_USER_SHELL_CLIENT_H_
#define SERVICES_USER_USER_SHELL_CLIENT_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/filesystem/lock_table.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/user/public/interfaces/user_service.mojom.h"

namespace user_service {

std::unique_ptr<shell::ShellClient> CreateUserShellClient(
    scoped_refptr<base::SingleThreadTaskRunner> user_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner,
    const base::Closure& quit_closure);

class UserShellClient
    : public shell::ShellClient,
      public shell::InterfaceFactory<mojom::UserService>,
      public shell::InterfaceFactory<leveldb::mojom::LevelDBService> {
 public:
  UserShellClient(
      scoped_refptr<base::SingleThreadTaskRunner> user_service_runner,
      scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner);
  ~UserShellClient() override;

 private:
  // |ShellClient| override:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // |InterfaceFactory<mojom::UserService>| implementation:
  void Create(shell::Connection* connection,
              mojom::UserServiceRequest request) override;

  // |InterfaceFactory<LevelDBService>| implementation:
  void Create(shell::Connection* connection,
              leveldb::mojom::LevelDBServiceRequest request) override;

  void OnLevelDBServiceRequest(shell::Connection* connection,
                               leveldb::mojom::LevelDBServiceRequest request);
  void OnLevelDBServiceError();

  scoped_refptr<base::SingleThreadTaskRunner> user_service_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner_;

  // We create these two objects so we can delete them on the correct task
  // runners.
  class UserServiceObjects;
  std::unique_ptr<UserServiceObjects> user_objects_;

  class LevelDBServiceObjects;
  std::unique_ptr<LevelDBServiceObjects> leveldb_objects_;

  DISALLOW_COPY_AND_ASSIGN(UserShellClient);
};

}  // namespace user_service

#endif  // SERVICES_USER_USER_SHELL_CLIENT_H_
