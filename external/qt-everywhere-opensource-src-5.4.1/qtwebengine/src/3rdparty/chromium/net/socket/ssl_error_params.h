// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_ERROR_PARAMS_H_
#define NET_SOCKET_SSL_ERROR_PARAMS_H_

#include "net/base/net_log.h"

namespace net {

// Creates NetLog callback for when we receive an SSL error.
NetLog::ParametersCallback CreateNetLogSSLErrorCallback(int net_error,
                                                        int ssl_lib_error);

}  // namespace net

#endif  // NET_SOCKET_SSL_ERROR_PARAMS_H_
