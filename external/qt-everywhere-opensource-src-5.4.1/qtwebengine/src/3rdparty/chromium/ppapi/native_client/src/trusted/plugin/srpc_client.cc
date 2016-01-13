/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/trusted/plugin/srpc_client.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/srpc_params.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

typedef bool (*RpcFunction)(void* obj, SrpcParams* params);

// MethodInfo records the method names and type signatures of an SRPC server.
class MethodInfo {
 public:
  // statically defined method - called through a pointer
  MethodInfo(const RpcFunction function_ptr,
             const char* name,
             const char* ins,
             const char* outs,
             // index is set to UINT_MAX for methods implemented by the plugin,
             // All methods implemented by nacl modules have indexes
             // that are lower than UINT_MAX.
             const uint32_t index = UINT_MAX) :
    function_ptr_(function_ptr),
    name_(STRDUP(name)),
    ins_(STRDUP(ins)),
    outs_(STRDUP(outs)),
    index_(index) { }

  ~MethodInfo() {
    free(reinterpret_cast<void*>(name_));
    free(reinterpret_cast<void*>(ins_));
    free(reinterpret_cast<void*>(outs_));
  }

  RpcFunction function_ptr() const { return function_ptr_; }
  char* name() const { return name_; }
  char* ins() const { return ins_; }
  char* outs() const { return outs_; }
  uint32_t index() const { return index_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MethodInfo);
  RpcFunction function_ptr_;
  char* name_;
  char* ins_;
  char* outs_;
  uint32_t index_;
};

SrpcClient::SrpcClient()
    : srpc_channel_initialised_(false) {
  PLUGIN_PRINTF(("SrpcClient::SrpcClient (this=%p)\n",
                 static_cast<void*>(this)));
  NaClSrpcChannelInitialize(&srpc_channel_);
}

SrpcClient* SrpcClient::New(nacl::DescWrapper* wrapper) {
  nacl::scoped_ptr<SrpcClient> srpc_client(new SrpcClient());
  if (!srpc_client->Init(wrapper)) {
    PLUGIN_PRINTF(("SrpcClient::New (SrpcClient::Init failed)\n"));
    return NULL;
  }
  return srpc_client.release();
}

bool SrpcClient::Init(nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("SrpcClient::Init (this=%p, wrapper=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(wrapper)));
  // Open the channel to pass RPC information back and forth
  if (!NaClSrpcClientCtor(&srpc_channel_, wrapper->desc())) {
    return false;
  }
  srpc_channel_initialised_ = true;
  PLUGIN_PRINTF(("SrpcClient::Init (Ctor worked)\n"));
  // Record the method names in a convenient way for later dispatches.
  GetMethods();
  PLUGIN_PRINTF(("SrpcClient::Init (GetMethods worked)\n"));
  return true;
}

SrpcClient::~SrpcClient() {
  PLUGIN_PRINTF(("SrpcClient::~SrpcClient (this=%p, has_srpc_channel=%d)\n",
                 static_cast<void*>(this), srpc_channel_initialised_));
  // And delete the connection.
  if (srpc_channel_initialised_) {
    PLUGIN_PRINTF(("SrpcClient::~SrpcClient (destroying srpc_channel)\n"));
    NaClSrpcDtor(&srpc_channel_);
  }
  for (Methods::iterator iter = methods_.begin();
       iter != methods_.end();
       ++iter) {
    delete iter->second;
  }
  PLUGIN_PRINTF(("SrpcClient::~SrpcClient (return)\n"));
}

void SrpcClient::GetMethods() {
  PLUGIN_PRINTF(("SrpcClient::GetMethods (this=%p)\n",
                 static_cast<void*>(this)));
  if (NULL == srpc_channel_.client) {
    return;
  }
  uint32_t method_count = NaClSrpcServiceMethodCount(srpc_channel_.client);
  // Intern the methods into a mapping from identifiers to MethodInfo.
  for (uint32_t i = 0; i < method_count; ++i) {
    int retval;
    const char* method_name;
    const char* input_types;
    const char* output_types;

    retval = NaClSrpcServiceMethodNameAndTypes(srpc_channel_.client,
                                               i,
                                               &method_name,
                                               &input_types,
                                               &output_types);
    if (!retval) {
      return;
    }
    if (!IsValidIdentifierString(method_name, NULL)) {
      // If name is not an ECMAScript identifier, do not enter it into the
      // methods_ table.
      continue;
    }
    MethodInfo* method_info =
        new MethodInfo(NULL, method_name, input_types, output_types, i);
    if (NULL == method_info) {
      return;
    }
    // Install in the map only if successfully read.
    methods_[method_name] = method_info;
  }
}

bool SrpcClient::HasMethod(const nacl::string& method_name) {
  bool has_method = (NULL != methods_[method_name]);
  PLUGIN_PRINTF((
      "SrpcClient::HasMethod (this=%p, method_name='%s', return %d)\n",
      static_cast<void*>(this), method_name.c_str(), has_method));
  return has_method;
}

bool SrpcClient::InitParams(const nacl::string& method_name,
                            SrpcParams* params) {
  MethodInfo* method_info = methods_[method_name];
  if (method_info) {
    return params->Init(method_info->ins(), method_info->outs());
  }
  return false;
}

bool SrpcClient::Invoke(const nacl::string& method_name, SrpcParams* params) {
  // It would be better if we could set the exception on each detailed failure
  // case.  However, there are calls to Invoke from within the plugin itself,
  // and these could leave residual exceptions pending.  This seems to be
  // happening specifically with hard_shutdowns.
  PLUGIN_PRINTF(("SrpcClient::Invoke (this=%p, method_name='%s', params=%p)\n",
                 static_cast<void*>(this),
                 method_name.c_str(),
                 static_cast<void*>(params)));

  // Ensure Invoke was called with a method name that has a binding.
  if (NULL == methods_[method_name]) {
    PLUGIN_PRINTF(("SrpcClient::Invoke (ident not in methods_)\n"));
    return false;
  }

  PLUGIN_PRINTF(("SrpcClient::Invoke (sending the rpc)\n"));
  // Call the method
  last_error_ = NaClSrpcInvokeV(&srpc_channel_,
                                methods_[method_name]->index(),
                                params->ins(),
                                params->outs());
  PLUGIN_PRINTF(("SrpcClient::Invoke (response=%d)\n", last_error_));
  if (NACL_SRPC_RESULT_OK != last_error_) {
    PLUGIN_PRINTF(("SrpcClient::Invoke (err='%s', return 0)\n",
                   NaClSrpcErrorString(last_error_)));
    return false;
  }

  PLUGIN_PRINTF(("SrpcClient::Invoke (return 1)\n"));
  return true;
}

void SrpcClient::AttachService(NaClSrpcService* service, void* instance_data) {
  srpc_channel_.server = service;
  srpc_channel_.server_instance_data = instance_data;
}

}  // namespace plugin
