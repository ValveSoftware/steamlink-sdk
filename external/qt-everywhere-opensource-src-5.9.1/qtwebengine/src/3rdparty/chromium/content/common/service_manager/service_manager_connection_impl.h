// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_
#define CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace service_manager {
class Connector;
}

namespace content {

class EmbeddedServiceRunner;

class ServiceManagerConnectionImpl : public ServiceManagerConnection {
 public:
  explicit ServiceManagerConnectionImpl(
      service_manager::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~ServiceManagerConnectionImpl() override;

 private:
  class IOThreadContext;

  // ServiceManagerConnection:
  void Start() override;
  void SetInitializeHandler(const base::Closure& handler) override;
  service_manager::Connector* GetConnector() override;
  const service_manager::Identity& GetIdentity() const override;
  void SetConnectionLostClosure(const base::Closure& closure) override;
  void SetupInterfaceRequestProxies(
      service_manager::InterfaceRegistry* registry,
      service_manager::InterfaceProvider* provider) override;
  int AddConnectionFilter(std::unique_ptr<ConnectionFilter> filter) override;
  void RemoveConnectionFilter(int filter_id) override;
  void AddEmbeddedService(const std::string& name,
                          const ServiceInfo& info) override;
  void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) override;
  int AddOnConnectHandler(const OnConnectHandler& handler) override;
  void RemoveOnConnectHandler(int id) override;

  void OnContextInitialized(const service_manager::Identity& identity);
  void OnConnectionLost();
  void OnConnect(const service_manager::ServiceInfo& local_info,
                 const service_manager::ServiceInfo& remote_info);
  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name);
  void GetInterface(service_manager::mojom::InterfaceProvider* provider,
                    const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle request_handle);

  service_manager::Identity identity_;
  service_manager::ServiceInfo local_info_;
  service_manager::ServiceInfo last_remote_info_;

  std::unique_ptr<service_manager::Connector> connector_;
  scoped_refptr<IOThreadContext> context_;

  base::Closure initialize_handler_;
  base::Closure connection_lost_handler_;

  std::unordered_map<std::string, std::unique_ptr<EmbeddedServiceRunner>>
      embedded_services_;
  std::unordered_map<std::string, ServiceRequestHandler> request_handlers_;
  int next_on_connect_handler_id_ = 0;
  std::map<int, OnConnectHandler> on_connect_handlers_;

  base::WeakPtrFactory<ServiceManagerConnectionImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_MANAGER_SERVICE_MANAGER_CONNECTION_IMPL_H_
