// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FLASH_DRM_RESOURCE_H_
#define PPAPI_PROXY_FLASH_DRM_RESOURCE_H_

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_flash_drm_api.h"

namespace ppapi {
struct FileRefCreateInfo;
}

namespace ppapi {
namespace proxy {

class FlashDRMResource
    : public PluginResource,
      public thunk::PPB_Flash_DRM_API {
 public:
  FlashDRMResource(Connection connection,
                   PP_Instance instance);
  virtual ~FlashDRMResource();

  // Resource override.
  virtual thunk::PPB_Flash_DRM_API* AsPPB_Flash_DRM_API() OVERRIDE;

  // PPB_Flash_DRM_API implementation.
  virtual int32_t GetDeviceID(PP_Var* id,
                              scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool GetHmonitor(int64_t* hmonitor) OVERRIDE;
  virtual int32_t GetVoucherFile(
      PP_Resource* file_ref,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t MonitorIsExternal(
      PP_Bool* is_external,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  void OnPluginMsgGetDeviceIDReply(PP_Var* dest,
                                   scoped_refptr<TrackedCallback> callback,
                                   const ResourceMessageReplyParams& params,
                                   const std::string& id);
  void OnPluginMsgGetVoucherFileReply(PP_Resource* dest,
                                      scoped_refptr<TrackedCallback> callback,
                                      const ResourceMessageReplyParams& params,
                                      const FileRefCreateInfo& file_info);
  void OnPluginMsgMonitorIsExternalReply(
      PP_Bool* dest,
      scoped_refptr<TrackedCallback> callback,
      const ResourceMessageReplyParams& params,
      PP_Bool is_external);

  DISALLOW_COPY_AND_ASSIGN(FlashDRMResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FLASH_DRM_RESOURCE_H_
