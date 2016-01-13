// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_FLASH_MESSAGE_LOOP_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_FLASH_MESSAGE_LOOP_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_flash_message_loop_api.h"

namespace content {

class PPB_Flash_MessageLoop_Impl
    : public ppapi::Resource,
      public ppapi::thunk::PPB_Flash_MessageLoop_API {
 public:
  static PP_Resource Create(PP_Instance instance);

  // Resource.
  virtual ppapi::thunk::PPB_Flash_MessageLoop_API* AsPPB_Flash_MessageLoop_API()
      OVERRIDE;

  // PPB_Flash_MessageLoop_API implementation.
  virtual int32_t Run() OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void RunFromHostProxy(const RunFromHostProxyCallback& callback)
      OVERRIDE;

 private:
  class State;

  explicit PPB_Flash_MessageLoop_Impl(PP_Instance instance);
  virtual ~PPB_Flash_MessageLoop_Impl();

  // If |callback| is valid, it will be called when the message loop is signaled
  // to quit, and the result passed into it will be the same value as what this
  // method returns.
  // Please note that |callback| happens before this method returns.
  int32_t InternalRun(const RunFromHostProxyCallback& callback);
  void InternalQuit(int32_t result);

  scoped_refptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_MessageLoop_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_FLASH_MESSAGE_LOOP_IMPL_H_
