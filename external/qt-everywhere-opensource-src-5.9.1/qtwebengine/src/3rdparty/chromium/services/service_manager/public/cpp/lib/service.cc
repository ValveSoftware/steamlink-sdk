// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service.h"

#include "base/logging.h"

namespace service_manager {

Service::Service() = default;

Service::~Service() = default;

void Service::OnStart() {}

bool Service::OnConnect(const ServiceInfo& remote_info,
                        InterfaceRegistry* registry) {
  return false;
}

bool Service::OnStop() { return true; }

ServiceContext* Service::context() const {
  DCHECK(service_context_)
      << "Service::context() may only be called during or after OnStart().";
  return service_context_;
}

ForwardingService::ForwardingService(Service* target) : target_(target) {}

ForwardingService::~ForwardingService() {}

void ForwardingService::OnStart() {
  target_->set_context(context());
  target_->OnStart();
}

bool ForwardingService::OnConnect(const ServiceInfo& remote_info,
                                  InterfaceRegistry* registry) {
  return target_->OnConnect(remote_info, registry);
}

bool ForwardingService::OnStop() { return target_->OnStop(); }

}  // namespace service_manager
