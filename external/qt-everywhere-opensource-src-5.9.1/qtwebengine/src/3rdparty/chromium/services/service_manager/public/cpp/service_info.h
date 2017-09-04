// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_INFO_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_INFO_H_

#include <map>
#include <string>

#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"

namespace service_manager {

class Identity;

struct ServiceInfo {
  ServiceInfo();
  ServiceInfo(const Identity& identity, const InterfaceProviderSpecMap& specs);
  ServiceInfo(const ServiceInfo& other);
  ~ServiceInfo();

  Identity identity;
  InterfaceProviderSpecMap interface_provider_specs;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_INFO_H_
