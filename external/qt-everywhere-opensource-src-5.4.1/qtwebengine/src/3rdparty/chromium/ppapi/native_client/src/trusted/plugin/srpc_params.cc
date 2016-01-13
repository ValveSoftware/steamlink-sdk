/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "ppapi/native_client/src/trusted/plugin/srpc_params.h"

#include <stdlib.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"


namespace plugin {

namespace {

bool FillVec(NaClSrpcArg* vec[], const char* types) {
  const size_t kLength = strlen(types);
  if (kLength > NACL_SRPC_MAX_ARGS) {
    return false;
  }
  // We use malloc/new here rather than new/delete, because the SRPC layer
  // is written in C and hence will use malloc/free.
  // This array will get deallocated by FreeArguments().
  if (kLength > 0) {
    NaClSrpcArg* args =
      reinterpret_cast<NaClSrpcArg*>(malloc(kLength * sizeof(*args)));
    if (NULL == args) {
      return false;
    }

    memset(static_cast<void*>(args), 0, kLength * sizeof(*args));
    for (size_t i = 0; i < kLength; ++i) {
      vec[i] = &args[i];
      args[i].tag = static_cast<NaClSrpcArgType>(types[i]);
    }
  }
  vec[kLength] = NULL;
  return true;
}

void FreeSrpcArg(NaClSrpcArg* arg) {
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      free(arg->arrays.carr);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      free(arg->arrays.darr);
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      free(arg->arrays.iarr);
      break;
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      free(arg->arrays.larr);
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      // All strings that are passed in SrpcArg must be allocated using
      // malloc! We cannot use browser's allocation API
      // since some of SRPC arguments is handled outside of the plugin code.
      free(arg->arrays.str);
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      if (arg->arrays.varr) {
        for (uint32_t i = 0; i < arg->u.count; i++) {
          FreeSrpcArg(&arg->arrays.varr[i]);
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_OBJECT:
      // This is a pointer to a scriptable object and should be released
      // by the browser
      break;
    case NACL_SRPC_ARG_TYPE_BOOL:
    case NACL_SRPC_ARG_TYPE_DOUBLE:
    case NACL_SRPC_ARG_TYPE_INT:
    case NACL_SRPC_ARG_TYPE_LONG:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      break;
  }
}

void FreeArguments(NaClSrpcArg* vec[]) {
  if (NULL == vec[0]) {
    return;
  }
  for (NaClSrpcArg** argp = vec; *argp; ++argp) {
    FreeSrpcArg(*argp);
  }
  // Free the vector containing the arguments themselves that was
  // allocated with FillVec().
  free(vec[0]);
}

}  // namespace

bool SrpcParams::Init(const char* in_types, const char* out_types) {
  if (!FillVec(ins_, in_types)) {
    return false;
  }
  if (!FillVec(outs_, out_types)) {
    FreeArguments(ins_);
    return false;
  }
  return true;
}

void SrpcParams::FreeAll() {
  FreeArguments(ins_);
  FreeArguments(outs_);
  memset(ins_, 0, sizeof(ins_));
  memset(outs_, 0, sizeof(outs_));
}

}  // namespace plugin
