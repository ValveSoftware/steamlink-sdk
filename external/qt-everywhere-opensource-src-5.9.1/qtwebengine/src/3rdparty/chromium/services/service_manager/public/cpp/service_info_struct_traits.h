// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_INFO_STRUCT_TRAITS_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_INFO_STRUCT_TRAITS_H_

#include "services/service_manager/public/cpp/service_info.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace mojo {

template <>
struct StructTraits<service_manager::mojom::ServiceInfo::DataView,
                    service_manager::ServiceInfo> {
  static const service_manager::InterfaceProviderSpecMap&
      interface_provider_specs(const service_manager::ServiceInfo& info) {
    return info.interface_provider_specs;
  }
  static const service_manager::Identity& identity(
      const service_manager::ServiceInfo& info) {
    return info.identity;
  }
  static bool Read(service_manager::mojom::ServiceInfoDataView data,
                   service_manager::ServiceInfo* out) {
    return data.ReadIdentity(&out->identity) &&
           data.ReadInterfaceProviderSpecs(&out->interface_provider_specs);
  }
};

}  // namespace mojo

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_INFO_STRUCT_TRAITS_H_
