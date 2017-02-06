// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SPDY_UTIL_H_
#define NET_TOOLS_FLIP_SERVER_SPDY_UTIL_H_

#include <string>

namespace net {

// Flag indicating if we need to encode urls into filenames (legacy).
extern bool g_need_to_encode_url;

// Encode the URL.
std::string EncodeURL(const std::string& uri,
                      const std::string& host,
                      const std::string& method);

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SPDY_UTIL_H_
