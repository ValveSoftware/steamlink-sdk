// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FLASH_MENU_RESOURCE_H_
#define PPAPI_PROXY_FLASH_MENU_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_flash_menu_api.h"

struct PP_Flash_Menu;

namespace ppapi {
namespace proxy {

class FlashMenuResource
    : public PluginResource,
      public thunk::PPB_Flash_Menu_API {
 public:
  // You must call Initialize after construction.
  FlashMenuResource(Connection connection, PP_Instance instance);
  virtual ~FlashMenuResource();

  // Returns true on success. False means that this object can not be used.
  // This has to be separate from the constructor because the menu data could
  // be invalid.
  bool Initialize(const PP_Flash_Menu* menu_data);

  // Resource overrides.
  virtual thunk::PPB_Flash_Menu_API* AsPPB_Flash_Menu_API() OVERRIDE;

  // PPB_Flash_Menu_API.
  virtual int32_t Show(const PP_Point* location,
                       int32_t* selected_id,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  virtual void OnReplyReceived(const proxy::ResourceMessageReplyParams& params,
                               const IPC::Message& msg) OVERRIDE;

  void OnShowReply(
      const proxy::ResourceMessageReplyParams& params,
      int32_t selected_id);

  int* selected_id_dest_;
  scoped_refptr<TrackedCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(FlashMenuResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FLASH_MENU_RESOURCE_H_
