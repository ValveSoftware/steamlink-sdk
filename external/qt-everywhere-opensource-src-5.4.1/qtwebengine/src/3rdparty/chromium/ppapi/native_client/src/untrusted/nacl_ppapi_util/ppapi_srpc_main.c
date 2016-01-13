/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/native_client/src/untrusted/nacl_ppapi_util/ppapi_srpc_main.h"

/*
 * Here be dragons.  Beware.
 *
 * We need to provide a main so that when this code is compiled with
 * irt=1, we can invoke NaClSrpcModuleInit.  This is not needed when
 * we compile with irt=0 because the PpapiPluginMain uses SRPC and
 * will invoke NaClSrpcModuleInit.  However, with irt=1, there are two
 * copies of the SRPC (and platform, and ...) libraries: the
 * PpapiPluginMain code uses a copy of SRPC in the IRT, and the user
 * code -- that's us -- has another copy.  The two copies have
 * separate allocators, globals, etc because of the IRT separation, so
 * that the NaClSrpcModuleInit that is invoked in the IRT will not
 * initialize the data structures used by the copy in the user code.
 *
 * The weak reference to the function __nacl_register_thread_creator
 * is an IRT implementation-dependent hack.  Here's how it works.  In
 * irt_stub, there is a definition for __nacl_register_thread_creator,
 * so if this code is compiled and linked with irt=1, the weak symbol
 * will be overridden with a real function, and the tests inside of
 * main will succeed.  If this code is compiled and linked with irt=0,
 * however, we will not link against irt_stub (-lppapi which is a
 * linker script which causes libppapi_stub.a to get included, which
 * includes the code from the src/untrusted/irt_stub directory), and
 * so the __nacl_register_thread_creator weak symbol will have the
 * value zero.  Voila, we call NaClSrpcModuleInit in the main function
 * below if the scons command invocation had irt=1 set, and we will
 * leave it to PpapiPluginMain to invoke NaClSrpcModuleInit when the
 * scons invocation had irt=0 set.
 *
 * Yes, this could be conditionally linked in via scons magic by
 * testing the irt bit, avoiding the weak attribute magic.
 */

struct nacl_irt_ppapihook;

void __nacl_register_thread_creator(const struct nacl_irt_ppapihook *)
    __attribute__((weak));

int main(void) {
  int rv;

  if (__nacl_register_thread_creator) {
    if (!NaClSrpcModuleInit()) {
      return 1;
    }
  }

  rv = PpapiPluginMain();

  if (__nacl_register_thread_creator) {
    NaClSrpcModuleFini();
  }

  return rv;
}
