// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_mapping_resource.h"

#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace proxy {

// static
FileMappingResource::MapResult FileMappingResource::DoMapBlocking(
    scoped_refptr<FileIOResource::FileHolder> file_holder,
    void* address_hint,
    int64_t length,
    uint32_t map_protection,
    uint32_t map_flags,
    int64_t offset) {
  // TODO(dmichael): Implement for Windows (crbug.com/83774).
  MapResult map_result;
  map_result.result = PP_ERROR_NOTSUPPORTED;
  return map_result;
}

// static
int32_t FileMappingResource::DoUnmapBlocking(const void* address,
                                             int64_t length) {
  // TODO(dmichael): Implement for Windows (crbug.com/83774).
  return PP_ERROR_NOTSUPPORTED;
}

// static
int64_t FileMappingResource::DoGetMapPageSize() {
  // TODO(dmichael): Implement for Windows (crbug.com/83774).
  return 0;
}

}  // namespace proxy
}  // namespace ppapi
