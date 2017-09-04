// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_INIT_INIT_H_
#define MASH_INIT_INIT_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mash/init/public/interfaces/init.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {
class Connection;
}

namespace mash {
namespace init {

class Init : public service_manager::Service,
             public service_manager::InterfaceFactory<mojom::Init>,
             public mojom::Init {
 public:
  Init();
  ~Init() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // service_manager::InterfaceFactory<mojom::Login>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::InitRequest request) override;

  // mojom::Init:
  void StartService(const std::string& name,
                    const std::string& user_id) override;
  void StopServicesForUser(const std::string& user_id) override;

  void UserServiceQuit(const std::string& user_id);

  void StartTracing();
  void StartLogin();

  std::unique_ptr<service_manager::Connection> login_connection_;
  mojo::BindingSet<mojom::Init> init_bindings_;
  std::map<std::string, std::unique_ptr<service_manager::Connection>>
      user_services_;

  DISALLOW_COPY_AND_ASSIGN(Init);
};

}  // namespace init
}  // namespace mash

#endif  // MASH_INIT_INIT_H_
