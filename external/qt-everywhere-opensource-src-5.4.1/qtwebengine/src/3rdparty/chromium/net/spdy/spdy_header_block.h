// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HEADER_BLOCK_H_
#define NET_SPDY_SPDY_HEADER_BLOCK_H_

#include <map>
#include <string>

#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace net {

// A data structure for holding a set of headers from either a
// SYN_STREAM or SYN_REPLY frame.
typedef std::map<std::string, std::string> SpdyHeaderBlock;

// Converts a SpdyHeaderBlock into NetLog event parameters.  Caller takes
// ownership of returned value.
NET_EXPORT base::Value* SpdyHeaderBlockNetLogCallback(
    const SpdyHeaderBlock* headers,
    NetLog::LogLevel log_level);

// Converts NetLog event parameters into a SPDY header block and writes them
// to |headers|.  |event_param| must have been created by
// SpdyHeaderBlockNetLogCallback.  On failure, returns false and clears
// |headers|.
NET_EXPORT bool SpdyHeaderBlockFromNetLogParam(
    const base::Value* event_param,
    SpdyHeaderBlock* headers);

}  // namespace net

#endif  // NET_SPDY_SPDY_HEADER_BLOCK_H_
