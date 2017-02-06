// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/user/user_shell_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "components/filesystem/lock_table.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "services/shell/public/cpp/connection.h"
#include "services/user/user_id_map.h"
#include "services/user/user_service.h"

namespace user_service {

class UserShellClient::UserServiceObjects
    : public base::SupportsWeakPtr<UserServiceObjects> {
 public:
  // Created on the main thread.
  UserServiceObjects(base::FilePath user_dir) : user_dir_(user_dir) {}

  // Destroyed on the |user_service_runner_|.
  ~UserServiceObjects() {}

  // Called on the |user_service_runner_|.
  void OnUserServiceRequest(shell::Connection* connection,
                            mojom::UserServiceRequest request) {
    if (!lock_table_)
      lock_table_ = new filesystem::LockTable;
    user_service_bindings_.AddBinding(new UserService(user_dir_, lock_table_),
                                      std::move(request));
  }

 private:
  mojo::BindingSet<mojom::UserService> user_service_bindings_;
  scoped_refptr<filesystem::LockTable> lock_table_;
  base::FilePath user_dir_;

  DISALLOW_COPY_AND_ASSIGN(UserServiceObjects);
};

class UserShellClient::LevelDBServiceObjects
    : public base::SupportsWeakPtr<LevelDBServiceObjects> {
 public:
  // Created on the main thread.
  LevelDBServiceObjects(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  // Destroyed on the |leveldb_service_runner_|.
  ~LevelDBServiceObjects() {}

  // Called on the |leveldb_service_runner_|.
  void OnLevelDBServiceRequest(shell::Connection* connection,
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

std::unique_ptr<shell::ShellClient> CreateUserShellClient(
    scoped_refptr<base::SingleThreadTaskRunner> user_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner,
    const base::Closure& quit_closure) {
  return base::WrapUnique(new UserShellClient(
      std::move(user_service_runner), std::move(leveldb_service_runner)));
}

UserShellClient::UserShellClient(
    scoped_refptr<base::SingleThreadTaskRunner> user_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner)
    : user_service_runner_(std::move(user_service_runner)),
      leveldb_service_runner_(std::move(leveldb_service_runner)) {}

UserShellClient::~UserShellClient() {
  user_service_runner_->DeleteSoon(FROM_HERE, user_objects_.release());
  leveldb_service_runner_->DeleteSoon(FROM_HERE, leveldb_objects_.release());
}

void UserShellClient::Initialize(shell::Connector* connector,
                                 const shell::Identity& identity,
                                 uint32_t id) {
  user_objects_.reset(new UserShellClient::UserServiceObjects(
      GetUserDirForUserId(identity.user_id())));
  leveldb_objects_.reset(
      new UserShellClient::LevelDBServiceObjects(leveldb_service_runner_));
}

bool UserShellClient::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<leveldb::mojom::LevelDBService>(this);
  connection->AddInterface<mojom::UserService>(this);
  return true;
}

void UserShellClient::Create(shell::Connection* connection,
                             mojom::UserServiceRequest request) {
  user_service_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserShellClient::UserServiceObjects::OnUserServiceRequest,
                 user_objects_->AsWeakPtr(), connection,
                 base::Passed(&request)));
}

void UserShellClient::Create(shell::Connection* connection,
                             leveldb::mojom::LevelDBServiceRequest request) {
  leveldb_service_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &UserShellClient::LevelDBServiceObjects::OnLevelDBServiceRequest,
          leveldb_objects_->AsWeakPtr(), connection,
          base::Passed(&request)));
}

}  // namespace user_service
