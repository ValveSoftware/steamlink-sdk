// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_error_params.h"

#include "base/bind.h"
#include "base/values.h"

namespace net {

namespace {

base::Value* NetLogSSLErrorCallback(int net_error,
                                    int ssl_lib_error,
                                    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("net_error", net_error);
  if (ssl_lib_error)
    dict->SetInteger("ssl_lib_error", ssl_lib_error);
  return dict;
}

}  // namespace

NetLog::ParametersCallback CreateNetLogSSLErrorCallback(int net_error,
                                                        int ssl_lib_error) {
  return base::Bind(&NetLogSSLErrorCallback, net_error, ssl_lib_error);
}

}  // namespace net
