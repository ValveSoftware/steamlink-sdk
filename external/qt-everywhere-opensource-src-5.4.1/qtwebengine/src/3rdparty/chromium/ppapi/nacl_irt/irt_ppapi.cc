// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/public/irt_core.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_private.h"
#include "ppapi/nacl_irt/irt_ppapi.h"
#include "ppapi/nacl_irt/plugin_main.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/irt_shim_ppapi.h"

static struct PP_StartFunctions g_pp_functions;

int irt_ppapi_start(const struct PP_StartFunctions* funcs) {
  // Disable NaCl's open_resource() interface on this thread.
  g_is_main_thread = 1;

  g_pp_functions = *funcs;
  return PpapiPluginMain();
}

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  return g_pp_functions.PPP_InitializeModule(module_id, get_browser_interface);
}

void PPP_ShutdownModule(void) {
  g_pp_functions.PPP_ShutdownModule();
}

const void* PPP_GetInterface(const char* interface_name) {
  return g_pp_functions.PPP_GetInterface(interface_name);
}

static const struct nacl_irt_ppapihook nacl_irt_ppapihook = {
  irt_ppapi_start,
  PpapiPluginRegisterThreadCreator,
};

static int ppapihook_pnacl_private_filter(void) {
  int pnacl_mode = sysconf(NACL_ABI__SC_NACL_PNACL_MODE);
  if (pnacl_mode == -1)
    return 0;
  return pnacl_mode;
}

static const struct nacl_irt_interface irt_interfaces[] = {
  { NACL_IRT_PPAPIHOOK_v0_1, &nacl_irt_ppapihook, sizeof(nacl_irt_ppapihook),
    NULL },
  { NACL_IRT_PPAPIHOOK_PNACL_PRIVATE_v0_1,
    &nacl_irt_ppapihook_pnacl_private, sizeof(nacl_irt_ppapihook_pnacl_private),
    ppapihook_pnacl_private_filter },
};

size_t chrome_irt_query(const char* interface_ident,
                        void* table, size_t tablesize) {
  size_t result = nacl_irt_query_core(interface_ident, table, tablesize);
  if (result != 0)
    return result;
  return nacl_irt_query_list(interface_ident, table, tablesize,
                             irt_interfaces, sizeof(irt_interfaces));
}
