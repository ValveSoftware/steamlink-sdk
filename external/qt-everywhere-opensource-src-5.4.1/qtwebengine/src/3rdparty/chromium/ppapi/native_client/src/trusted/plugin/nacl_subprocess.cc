// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/nacl_subprocess.h"

#include <stdarg.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/srpc_params.h"

namespace plugin {

nacl::string NaClSubprocess::detailed_description() const {
  nacl::stringstream ss;
  ss << description()
     << "={ this=" << static_cast<const void*>(this)
     << ", srpc_client=" << static_cast<void*>(srpc_client_.get())
     << ", service_runtime=" << static_cast<void*>(service_runtime_.get())
     << " }";
  return ss.str();
}

// Shutdown the socket connection and service runtime, in that order.
void NaClSubprocess::Shutdown() {
  srpc_client_.reset(NULL);
  if (service_runtime_.get() != NULL) {
    service_runtime_->Shutdown();
    service_runtime_.reset(NULL);
  }
}

NaClSubprocess::~NaClSubprocess() {
  Shutdown();
}

bool NaClSubprocess::StartSrpcServices() {
  srpc_client_.reset(service_runtime_->SetupAppChannel());
  return NULL != srpc_client_.get();
}

bool NaClSubprocess::InvokeSrpcMethod(const nacl::string& method_name,
                                      const nacl::string& input_signature,
                                      SrpcParams* params,
                                      ...) {
  va_list vl;
  va_start(vl, params);
  bool result = VInvokeSrpcMethod(method_name, input_signature, params, vl);
  va_end(vl);
  return result;
}

bool NaClSubprocess::VInvokeSrpcMethod(const nacl::string& method_name,
                                       const nacl::string& input_signature,
                                       SrpcParams* params,
                                       va_list vl) {
  if (NULL == srpc_client_.get()) {
    PLUGIN_PRINTF(("VInvokeSrpcMethod (no srpc_client_)\n"));
    return false;
  }
  if (!srpc_client_->HasMethod(method_name)) {
    PLUGIN_PRINTF(("VInvokeSrpcMethod (no %s method found)\n",
                   method_name.c_str()));
    return false;
  }
  if (!srpc_client_->InitParams(method_name, params)) {
    PLUGIN_PRINTF(("VInvokeSrpcMethod (InitParams failed)\n"));
    return false;
  }
  // Marshall inputs.
  for (size_t i = 0; i < input_signature.length(); ++i) {
    char c = input_signature[i];
    // Only handle the limited number of SRPC types used for PNaCl.
    // Add more as needed.
    switch (c) {
      default:
        PLUGIN_PRINTF(("PnaclSrpcLib::InvokeSrpcMethod unhandled type: %c\n",
                       c));
        return false;
      case NACL_SRPC_ARG_TYPE_BOOL: {
        int input = va_arg(vl, int);
        params->ins()[i]->u.bval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_DOUBLE: {
        double input = va_arg(vl, double);
        params->ins()[i]->u.dval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY: {
        // SrpcParam's destructor *should* free the allocated array
        const char* orig_arr = va_arg(vl, const char*);
        size_t len = va_arg(vl, size_t);
        char* input = (char *)malloc(len);
        if (!input) {
          PLUGIN_PRINTF(("VInvokeSrpcMethod (allocation failure)\n"));
          return false;
        }
        memcpy(input, orig_arr, len);
        params->ins()[i]->arrays.carr = input;
        params->ins()[i]->u.count = static_cast<nacl_abi_size_t>(len);
        break;
      }
      case NACL_SRPC_ARG_TYPE_HANDLE: {
        NaClSrpcImcDescType input = va_arg(vl, NaClSrpcImcDescType);
        params->ins()[i]->u.hval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_INT: {
        int32_t input = va_arg(vl, int32_t);
        params->ins()[i]->u.ival = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_LONG: {
        int64_t input = va_arg(vl, int64_t);
        params->ins()[i]->u.lval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_STRING: {
        // SrpcParam's destructor *should* free the dup'ed string.
        const char* orig_str = va_arg(vl, const char*);
        char* input = strdup(orig_str);
        if (!input) {
          PLUGIN_PRINTF(("VInvokeSrpcMethod (allocation failure)\n"));
          return false;
        }
        params->ins()[i]->arrays.str = input;
        break;
      }
    }
  }
  return srpc_client_->Invoke(method_name, params);
}

}  // namespace plugin
