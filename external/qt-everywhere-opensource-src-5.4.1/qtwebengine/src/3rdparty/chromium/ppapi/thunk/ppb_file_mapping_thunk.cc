// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_file_mapping.idl modified Mon Jan 27 11:00:43 2014.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_mapping.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_file_mapping_api.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t Map(PP_Instance instance,
            PP_Resource file_io,
            int64_t length,
            uint32_t map_protection,
            uint32_t map_flags,
            int64_t offset,
            void** address,
            struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileMapping::Map()";
  EnterInstanceAPI<PPB_FileMapping_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.functions()->Map(instance,
                                                file_io,
                                                length,
                                                map_protection,
                                                map_flags,
                                                offset,
                                                address,
                                                enter.callback()));
}

int32_t Unmap(PP_Instance instance,
              const void* address,
              int64_t length,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileMapping::Unmap()";
  EnterInstanceAPI<PPB_FileMapping_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.functions()->Unmap(instance,
                                                  address,
                                                  length,
                                                  enter.callback()));
}

int64_t GetMapPageSize(PP_Instance instance) {
  VLOG(4) << "PPB_FileMapping::GetMapPageSize()";
  EnterInstanceAPI<PPB_FileMapping_API> enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->GetMapPageSize(instance);
}

const PPB_FileMapping_0_1 g_ppb_filemapping_thunk_0_1 = {
  &Map,
  &Unmap,
  &GetMapPageSize
};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_FileMapping_0_1* GetPPB_FileMapping_0_1_Thunk() {
  return &g_ppb_filemapping_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
