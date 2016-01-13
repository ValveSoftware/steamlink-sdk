// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/lib/service_registry.h"

#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/application/lib/service_connector.h"

namespace mojo {
namespace internal {

ServiceRegistry::ServiceRegistry(Application* application)
    : application_(application) {
}

ServiceRegistry::ServiceRegistry(
    Application* application,
    ScopedMessagePipeHandle service_provider_handle)
        : application_(application) {
  remote_service_provider_.Bind(service_provider_handle.Pass());
  remote_service_provider_.set_client(this);
}

ServiceRegistry::~ServiceRegistry() {
  for (NameToServiceConnectorMap::iterator i =
           name_to_service_connector_.begin();
       i != name_to_service_connector_.end(); ++i) {
    delete i->second;
  }
  name_to_service_connector_.clear();
}

void ServiceRegistry::AddServiceConnector(
    ServiceConnectorBase* service_connector) {
  name_to_service_connector_[service_connector->name()] = service_connector;
  service_connector->set_registry(this);
}

void ServiceRegistry::RemoveServiceConnector(
    ServiceConnectorBase* service_connector) {
  NameToServiceConnectorMap::iterator it =
      name_to_service_connector_.find(service_connector->name());
  assert(it != name_to_service_connector_.end());
  delete it->second;
  name_to_service_connector_.erase(it);
  if (name_to_service_connector_.empty())
    remote_service_provider_.reset();
}

void ServiceRegistry::BindRemoteServiceProvider(
    ScopedMessagePipeHandle service_provider_handle) {
  remote_service_provider_.Bind(service_provider_handle.Pass());
  remote_service_provider_.set_client(this);
}

void ServiceRegistry::ConnectToService(const mojo::String& service_url,
                                       const mojo::String& service_name,
                                       ScopedMessagePipeHandle client_handle,
                                       const mojo::String& requestor_url) {
  if (name_to_service_connector_.find(service_name) ==
          name_to_service_connector_.end() ||
      !application_->AllowIncomingConnection(service_name, requestor_url)) {
    client_handle.reset();
    return;
  }

  internal::ServiceConnectorBase* service_connector =
      name_to_service_connector_[service_name];
  assert(service_connector);
  // requestor_url is ignored because the service_connector stores the url
  // of the requestor safely.
  return service_connector->ConnectToService(
      service_url, service_name, client_handle.Pass());
}

}  // namespace internal
}  // namespace mojo
