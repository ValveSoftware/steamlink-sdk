/*
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/module.h"

namespace plugin {

class ModulePpapi : public pp::Module {
 public:
  ModulePpapi();

  virtual ~ModulePpapi();

  virtual bool Init();

  virtual pp::Instance* CreateInstance(PP_Instance pp_instance);

 private:
  bool init_was_successful_;
  const PPB_NaCl_Private* private_interface_;
};

}  // namespace plugin


namespace pp {

Module* CreateModule();

}  // namespace pp
