// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Parameter types for SRPC Invocation.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PARAMS_H
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PARAMS_H

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace plugin {

// A utility class that builds and deletes parameter vectors used in rpcs.
class SrpcParams {
 public:
  SrpcParams() {
    memset(ins_, 0, sizeof(ins_));
    memset(outs_, 0, sizeof(outs_));
  }

  SrpcParams(const char* in_types, const char* out_types) {
    if (!Init(in_types, out_types)) {
      FreeAll();
    }
  }

  ~SrpcParams() {
    FreeAll();
  }

  bool Init(const char* in_types, const char* out_types);

  NaClSrpcArg** ins() const { return const_cast<NaClSrpcArg**>(ins_); }
  NaClSrpcArg** outs() const { return const_cast<NaClSrpcArg**>(outs_); }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SrpcParams);
  void FreeAll();
  // The ins_ and outs_ arrays contain one more element, to hold a NULL pointer
  // to indicate the end of the list.
  NaClSrpcArg* ins_[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg* outs_[NACL_SRPC_MAX_ARGS + 1];
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PARAMS_H
