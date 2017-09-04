// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_info.h"

namespace service_manager {

ServiceInfo::ServiceInfo() = default;
ServiceInfo::ServiceInfo(const Identity& identity,
                         const InterfaceProviderSpecMap& specs)
    : identity(identity),
      interface_provider_specs(specs) {}
ServiceInfo::ServiceInfo(const ServiceInfo& other) = default;
ServiceInfo::~ServiceInfo() {}

}  // namespace service_manager
