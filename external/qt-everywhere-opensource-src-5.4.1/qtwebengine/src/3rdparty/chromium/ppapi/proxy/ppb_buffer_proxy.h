// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_BUFFER_PROXY_H_
#define PPAPI_PROXY_PPB_BUFFER_PROXY_H_

#include "base/memory/shared_memory.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_buffer_api.h"

namespace ppapi {

class HostResource;

namespace proxy {

class SerializedHandle;

class Buffer : public thunk::PPB_Buffer_API, public Resource {
 public:
  Buffer(const HostResource& resource,
         const base::SharedMemoryHandle& shm_handle,
         uint32_t size);
  virtual ~Buffer();

  // Resource overrides.
  virtual thunk::PPB_Buffer_API* AsPPB_Buffer_API() OVERRIDE;

  // PPB_Buffer_API implementation.
  virtual PP_Bool Describe(uint32_t* size_in_bytes) OVERRIDE;
  virtual PP_Bool IsMapped() OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;

  // Trusted
  virtual int32_t GetSharedMemory(int* handle) OVERRIDE;

 private:
  base::SharedMemory shm_;
  uint32_t size_;
  int map_count_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

class PPB_Buffer_Proxy : public InterfaceProxy {
 public:
  explicit PPB_Buffer_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Buffer_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         uint32_t size);
  static PP_Resource AddProxyResource(const HostResource& resource,
                                      base::SharedMemoryHandle shm_handle,
                                      uint32_t size);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_BUFFER;

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance,
                   uint32_t size,
                   HostResource* result_resource,
                   ppapi::proxy::SerializedHandle* result_shm_handle);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_BUFFER_PROXY_H_
