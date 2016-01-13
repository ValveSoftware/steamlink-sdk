/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"

#include "ppapi/native_client/src/trusted/plugin/module_ppapi.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

ModulePpapi::ModulePpapi() : pp::Module(),
                             init_was_successful_(false),
                             private_interface_(NULL) {
  MODULE_PRINTF(("ModulePpapi::ModulePpapi (this=%p)\n",
                 static_cast<void*>(this)));
}

ModulePpapi::~ModulePpapi() {
  if (init_was_successful_) {
    NaClSrpcModuleFini();
    NaClNrdAllModulesFini();
  }
  MODULE_PRINTF(("ModulePpapi::~ModulePpapi (this=%p)\n",
                 static_cast<void*>(this)));
}

bool ModulePpapi::Init() {
  // Ask the browser for an interface which provides missing functions
  private_interface_ = reinterpret_cast<const PPB_NaCl_Private*>(
      GetBrowserInterface(PPB_NACL_PRIVATE_INTERFACE));

  if (NULL == private_interface_) {
    MODULE_PRINTF(("ModulePpapi::Init failed: "
                   "GetBrowserInterface returned NULL\n"));
    return false;
  }
  SetNaClInterface(private_interface_);

#if NACL_LINUX || NACL_OSX
  // Note that currently we do not need random numbers inside the
  // NaCl trusted plugin on Unix, but NaClSecureRngModuleInit() is
  // strict and will raise a fatal error unless we provide it with a
  // /dev/urandom FD beforehand.
  NaClSecureRngModuleSetUrandomFd(dup(private_interface_->UrandomFD()));
#endif

  // In the plugin, we don't need high resolution time of day.
  NaClAllowLowResolutionTimeOfDay();
  NaClNrdAllModulesInit();
  NaClSrpcModuleInit();

#if NACL_WINDOWS
  NaClSetBrokerDuplicateHandleFunc(private_interface_->BrokerDuplicateHandle);
#endif

  init_was_successful_ = true;
  return true;
}

pp::Instance* ModulePpapi::CreateInstance(PP_Instance pp_instance) {
  MODULE_PRINTF(("ModulePpapi::CreateInstance (pp_instance=%" NACL_PRId32
                 ")\n",
                 pp_instance));
  Plugin* plugin = new Plugin(pp_instance);
  MODULE_PRINTF(("ModulePpapi::CreateInstance (return %p)\n",
                 static_cast<void* >(plugin)));
  return plugin;
}

}  // namespace plugin


namespace pp {

Module* CreateModule() {
  MODULE_PRINTF(("CreateModule ()\n"));
  return new plugin::ModulePpapi();
}

}  // namespace pp
