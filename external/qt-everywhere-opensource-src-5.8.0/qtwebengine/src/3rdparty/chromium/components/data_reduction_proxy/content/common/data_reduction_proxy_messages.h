// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/host_port_pair.h"

#define IPC_MESSAGE_START DataReductionProxyStart

IPC_SYNC_MESSAGE_CONTROL1_1(
    DataReductionProxyViewHostMsg_IsDataReductionProxy,
    net::HostPortPair /* proxy server */,
    bool /* true iff the proxy server is a Data Reduction Proxy */)
