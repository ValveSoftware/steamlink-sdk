// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
#define NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"

class GURL;

namespace base {
class Value;
}

namespace net {

// Returns a Value containing NetLog parameters for starting a URLRequest.
NET_EXPORT base::Value* NetLogURLRequestStartCallback(
    const GURL* url,
    const std::string* method,
    int load_flags,
    RequestPriority priority,
    int64 upload_id,
    NetLog::LogLevel /* log_level */);

// Attempts to extract the load flags from a Value created by the above
// function.  On success, sets |load_flags| accordingly and returns true.
// On failure, sets |load_flags| to 0.
NET_EXPORT bool StartEventLoadFlagsFromEventParams(
    const base::Value* event_params,
    int* load_flags);

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
