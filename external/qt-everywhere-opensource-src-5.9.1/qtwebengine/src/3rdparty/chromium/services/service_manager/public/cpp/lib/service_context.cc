// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_context.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/lib/connector_impl.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, public:

ServiceContext::ServiceContext(
    std::unique_ptr<service_manager::Service> service,
    mojom::ServiceRequest request,
    std::unique_ptr<Connector> connector,
    mojom::ConnectorRequest connector_request)
    : pending_connector_request_(std::move(connector_request)),
      service_(std::move(service)),
      binding_(this, std::move(request)),
      connector_(std::move(connector)),
      weak_factory_(this) {
  DCHECK(binding_.is_bound());
  binding_.set_connection_error_handler(
      base::Bind(&ServiceContext::OnConnectionError, base::Unretained(this)));
  if (!connector_) {
    connector_ = Connector::Create(&pending_connector_request_);
  } else {
    DCHECK(pending_connector_request_.is_pending());
  }
}

ServiceContext::~ServiceContext() {}

void ServiceContext::SetConnectionLostClosure(const base::Closure& closure) {
  connection_lost_closure_ = closure;
  if (service_quit_)
    QuitNow();
}

void ServiceContext::RequestQuit() {
  DCHECK(service_control_.is_bound());
  service_control_->RequestQuit();
}

void ServiceContext::DisconnectFromServiceManager() {
  if (binding_.is_bound())
    binding_.Close();
  connector_.reset();
}

void ServiceContext::QuitNow() {
  if (binding_.is_bound())
    binding_.Close();
  if (!connection_lost_closure_.is_null())
    base::ResetAndReturn(&connection_lost_closure_).Run();
}

void ServiceContext::DestroyService() {
  QuitNow();
  service_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, mojom::Service implementation:

void ServiceContext::OnStart(const ServiceInfo& info,
                             const OnStartCallback& callback) {
  local_info_ = info;
  callback.Run(std::move(pending_connector_request_),
               mojo::GetProxy(&service_control_, binding_.associated_group()));

  service_->set_context(this);
  service_->OnStart();
}

void ServiceContext::OnConnect(
    const ServiceInfo& source_info,
    mojom::InterfaceProviderRequest interfaces,
    const OnConnectCallback& callback) {
  InterfaceProviderSpec source_spec, target_spec;
  GetInterfaceProviderSpec(mojom::kServiceManager_ConnectorSpec,
                           local_info_.interface_provider_specs, &target_spec);
  GetInterfaceProviderSpec(mojom::kServiceManager_ConnectorSpec,
                           source_info.interface_provider_specs, &source_spec);
  auto registry =
      base::MakeUnique<InterfaceRegistry>(mojom::kServiceManager_ConnectorSpec);
  registry->Bind(std::move(interfaces), local_info_.identity, target_spec,
                 source_info.identity, source_spec);

  // Acknowledge the request regardless of whether it's accepted.
  callback.Run();

  if (!service_->OnConnect(source_info, registry.get()))
    return;

  InterfaceRegistry* raw_registry = registry.get();
  registry->AddConnectionLostClosure(base::Bind(
      &ServiceContext::OnRegistryConnectionError, base::Unretained(this),
      raw_registry));
  connection_interface_registries_.insert(
      std::make_pair(raw_registry, std::move(registry)));
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, private:

void ServiceContext::OnConnectionError() {
  // Note that the Service doesn't technically have to quit now, it may live
  // on to service existing connections. All existing Connectors however are
  // invalid.
  service_quit_ = service_->OnStop();
  if (service_quit_) {
    QuitNow();
    // NOTE: This call may delete |this|, so don't access any ServiceContext
    // state beyond this point.
    return;
  }

  // We don't reset the connector as clients may have taken a raw pointer to it.
  // Connect() will return nullptr if they try to connect to anything.
}

void ServiceContext::OnRegistryConnectionError(InterfaceRegistry* registry) {
  // NOTE: We destroy the InterfaceRegistry asynchronously since it's calling
  // into us from its own connection error handler which may continue to access
  // the InterfaceRegistry's own state after we return.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceContext::DestroyConnectionInterfaceRegistry,
                 weak_factory_.GetWeakPtr(), registry));
}

void ServiceContext::DestroyConnectionInterfaceRegistry(
    InterfaceRegistry* registry) {
  auto it = connection_interface_registries_.find(registry);
  CHECK(it != connection_interface_registries_.end());
  connection_interface_registries_.erase(it);
}

}  // namespace service_manager
