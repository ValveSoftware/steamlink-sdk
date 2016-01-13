// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_

#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

namespace mojo {

class Application;

namespace internal {

class ServiceConnectorBase;

class ServiceRegistry : public ServiceProvider {
 public:
  ServiceRegistry(Application* application);
  ServiceRegistry(Application* application,
                  ScopedMessagePipeHandle service_provider_handle);
  virtual ~ServiceRegistry();

  void AddServiceConnector(ServiceConnectorBase* service_connector);
  void RemoveServiceConnector(ServiceConnectorBase* service_connector);

  ServiceProvider* remote_service_provider() {
    return remote_service_provider_.get();
  }

  void BindRemoteServiceProvider(
      ScopedMessagePipeHandle service_provider_handle);

  // ServiceProvider method.
  virtual void ConnectToService(const mojo::String& service_url,
                                const mojo::String& service_name,
                                ScopedMessagePipeHandle client_handle,
                                const mojo::String& requestor_url)
      MOJO_OVERRIDE;

 private:
  Application* application_;
  typedef std::map<std::string, ServiceConnectorBase*>
      NameToServiceConnectorMap;
  NameToServiceConnectorMap name_to_service_connector_;
  ServiceProviderPtr remote_service_provider_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceRegistry);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_
