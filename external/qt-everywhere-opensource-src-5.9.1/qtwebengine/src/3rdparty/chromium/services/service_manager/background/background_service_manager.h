// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_BACKGROUND_BACKGROUND_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_BACKGROUND_BACKGROUND_SERVICE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/catalog/store.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace catalog {
class Store;
}

namespace service_manager {

class NativeRunnerDelegate;
class ServiceManager;

// BackgroundServiceManager starts up a Service Manager on a background thread,
// and
// destroys the thread in the destructor. Once created use CreateApplication()
// to obtain an InterfaceRequest for the Application. The InterfaceRequest can
// then be bound to an ApplicationImpl.
class BackgroundServiceManager {
 public:
  struct InitParams {
    InitParams();
    ~InitParams();

    NativeRunnerDelegate* native_runner_delegate = nullptr;
    std::unique_ptr<catalog::Store> catalog_store;
    // If true the edk is initialized.
    bool init_edk = true;
  };

  BackgroundServiceManager();
  ~BackgroundServiceManager();

  // Starts the background service manager. |command_line_switches| are
  // additional
  // switches applied to any processes spawned by this call.
  void Init(std::unique_ptr<InitParams> init_params);

  // Obtains an InterfaceRequest for the specified name.
  mojom::ServiceRequest CreateServiceRequest(const std::string& name);

  // Use to do processing on the thread running the Service Manager. The
  // callback is supplied a pointer to the Service Manager. The callback does
  // *not* own the Service Manager.
  using ServiceManagerThreadCallback = base::Callback<void(ServiceManager*)>;
  void ExecuteOnServiceManagerThread(
      const ServiceManagerThreadCallback& callback);

 private:
  class MojoThread;

  std::unique_ptr<MojoThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundServiceManager);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_BACKGROUND_BACKGROUND_SERVICE_MANAGER_H_
