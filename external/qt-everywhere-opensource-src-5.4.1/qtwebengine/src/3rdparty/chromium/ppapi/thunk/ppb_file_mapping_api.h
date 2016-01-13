// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_MAPPING_API_H_
#define PPAPI_THUNK_PPB_FILE_MAPPING_API_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_file_mapping.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_FileMapping_API {
 public:
  virtual ~PPB_FileMapping_API() {}

  virtual int32_t Map(PP_Instance instance,
                      PP_Resource file_io,
                      int64_t length,
                      uint32_t map_protection,
                      uint32_t map_flags,
                      int64_t offset,
                      void** address,
                      scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Unmap(PP_Instance instance,
                        const void* address,
                        int64_t length,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int64_t GetMapPageSize(PP_Instance instance) = 0;

  static const SingletonResourceID kSingletonResourceID =
      FILE_MAPPING_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_MAPPING_API_H_
