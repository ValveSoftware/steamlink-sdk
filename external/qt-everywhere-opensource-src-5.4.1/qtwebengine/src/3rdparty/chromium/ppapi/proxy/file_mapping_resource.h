// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_MAPPING_RESOURCE_H_
#define PPAPI_PROXY_FILE_MAPPING_RESOURCE_H_

#include "ppapi/proxy/file_io_resource.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_file_mapping_api.h"

namespace ppapi {
namespace proxy {

// The proxy-side resource for PPB_FileMapping.
class PPAPI_PROXY_EXPORT FileMappingResource
      : public PluginResource,
        public thunk::PPB_FileMapping_API {
 public:
  FileMappingResource(Connection connection, PP_Instance instance);

 private:
  virtual ~FileMappingResource();

  // Resource implementation.
  virtual thunk::PPB_FileMapping_API* AsPPB_FileMapping_API() OVERRIDE;

  // PPB_FileMapping_API implementation.
  virtual int32_t Map(PP_Instance instance,
                      PP_Resource file_io,
                      int64_t length,
                      uint32_t protection,
                      uint32_t flags,
                      int64_t offset,
                      void** address,
                      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Unmap(PP_Instance instance,
                        const void* address,
                        int64_t length,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int64_t GetMapPageSize(PP_Instance instance) OVERRIDE;

  struct MapResult {
    MapResult() : result(PP_ERROR_FAILED), address(NULL) {
    }
    int32_t result;
    void* address;
  };
  void OnMapCompleted(void** mapped_address_out_param,
                      int64_t length,
                      scoped_refptr<TrackedCallback> callback,
                      const MapResult& map_result);

  // These functions perform potentially blocking operations so they should not
  // be called on the main thread or while we hold the proxy lock. Their
  // implementation is platform specific. See file_mapping_resource_posix.cc and
  // file_mapping_resource_win.cc.
  static MapResult DoMapBlocking(
      scoped_refptr<FileIOResource::FileHolder> file_holder,
      void* address_hint,
      int64_t length,
      uint32_t protection,
      uint32_t flags,
      int64_t offset);
  static int32_t DoUnmapBlocking(const void* address, int64_t length);
  // DoGetMapPageSize is platform-specific, but fast enough that we can call it
  // on the main thread with the lock.
  static int64_t DoGetMapPageSize();

  DISALLOW_COPY_AND_ASSIGN(FileMappingResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_MAPPING_RESOURCE_H_
