// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_RETURNED_RESOURCE_STRUCT_TRAITS_H_
#define CC_IPC_RETURNED_RESOURCE_STRUCT_TRAITS_H_

#include "cc/ipc/returned_resource.mojom.h"
#include "cc/resources/returned_resource.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::ReturnedResource, cc::ReturnedResource> {
  static uint32_t id(const cc::ReturnedResource& resource) {
    return resource.id;
  }

  static const gpu::SyncToken& sync_token(
      const cc::ReturnedResource& resource) {
    return resource.sync_token;
  }

  static int32_t count(const cc::ReturnedResource& resource) {
    return resource.count;
  }

  static bool lost(const cc::ReturnedResource& resource) {
    return resource.lost;
  }

  static bool Read(cc::mojom::ReturnedResourceDataView data,
                   cc::ReturnedResource* out) {
    if (!data.ReadSyncToken(&out->sync_token))
      return false;
    out->id = data.id();
    out->count = data.count();
    out->lost = data.lost();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_RETURNED_RESOURCE_STRUCT_TRAITS_H_
