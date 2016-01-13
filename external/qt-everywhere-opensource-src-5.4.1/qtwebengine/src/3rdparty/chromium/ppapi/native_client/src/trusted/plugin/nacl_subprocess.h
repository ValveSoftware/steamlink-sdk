// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Instances of NaCl modules spun up within the plugin as a subprocess.
// This may represent the "main" nacl module, or it may represent helpers
// that perform various tasks within the plugin, for example,
// a NaCl module for a compiler could be loaded to translate LLVM bitcode
// into native code.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_SUBPROCESS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_SUBPROCESS_H_

#include <stdarg.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"

#include "ppapi/native_client/src/trusted/plugin/service_runtime.h"
#include "ppapi/native_client/src/trusted/plugin/srpc_client.h"

namespace plugin {

class Plugin;
class ServiceRuntime;
class SrpcParams;


// A class representing an instance of a NaCl module, loaded by the plugin.
class NaClSubprocess {
 public:
  NaClSubprocess(const nacl::string& description,
                 ServiceRuntime* service_runtime,
                 SrpcClient* srpc_client)
    : description_(description),
      service_runtime_(service_runtime),
      srpc_client_(srpc_client) {
  }
  virtual ~NaClSubprocess();

  ServiceRuntime* service_runtime() const { return service_runtime_.get(); }
  void set_service_runtime(ServiceRuntime* service_runtime) {
    service_runtime_.reset(service_runtime);
  }

  // The socket used for communicating w/ the NaCl module.
  SrpcClient* srpc_client() const { return srpc_client_.get(); }

  // A basic description of the subprocess.
  nacl::string description() const { return description_; }

  // A detailed description of the subprocess that may contain addresses.
  // Only use for debugging, but do not expose this to untrusted webapps.
  nacl::string detailed_description() const;

  // Start up interfaces.
  bool StartSrpcServices();

  // Invoke an Srpc Method.  |out_params| must be allocated and cleaned up
  // outside of this function, but it will be initialized by this function, and
  // on success any out-params (if any) will be placed in |out_params|.
  // Input types must be listed in |input_signature|, with the actual
  // arguments passed in as var-args.  Returns |true| on success.
  bool InvokeSrpcMethod(const nacl::string& method_name,
                        const nacl::string& input_signature,
                        SrpcParams* out_params,
                        ...);

  // Fully shut down the subprocess.
  void Shutdown();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NaClSubprocess);

  bool VInvokeSrpcMethod(const nacl::string& method_name,
                         const nacl::string& signature,
                         SrpcParams* params,
                         va_list vl);

  nacl::string description_;

  // The service runtime representing the NaCl module instance.
  nacl::scoped_ptr<ServiceRuntime> service_runtime_;
  // Ownership of srpc_client taken from the service runtime.
  nacl::scoped_ptr<SrpcClient> srpc_client_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_SUBPROCESS_H_
