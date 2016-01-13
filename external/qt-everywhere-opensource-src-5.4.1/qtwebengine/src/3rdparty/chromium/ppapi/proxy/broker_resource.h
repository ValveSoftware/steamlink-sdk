// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_BROKER_RESOURCE_H_
#define PPAPI_PROXY_BROKER_RESOURCE_H_

#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/thunk/ppb_broker_api.h"

namespace ppapi {
namespace proxy {

class BrokerResource
    : public PluginResource,
      public thunk::PPB_Broker_Instance_API {
 public:
  BrokerResource(Connection connection, PP_Instance instance);
  virtual ~BrokerResource();

  // Resource override.
  virtual thunk::PPB_Broker_Instance_API* AsPPB_Broker_Instance_API() OVERRIDE;

  // thunk::PPB_Broker_Instance_API implementation.
  virtual PP_Bool IsAllowed() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrokerResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_BROKER_RESOURCE_H_
