// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/spdy_util.h"

#include <string>

#include "net/tools/dump_cache/url_to_filename_encoder.h"

namespace net {

bool g_need_to_encode_url = false;

// Encode the URL.
std::string EncodeURL(std::string uri, std::string host, std::string method) {
  if (!g_need_to_encode_url) {
    // TODO(mbelshe): if uri is fully qualified, need to strip protocol/host.
    return std::string(method + "_" + uri);
  }

  std::string filename;
  if (uri[0] == '/') {
    // uri is not fully qualified.
    filename = UrlToFilenameEncoder::Encode(
        "http://" + host + uri, method + "_/", false);
  } else {
    filename = UrlToFilenameEncoder::Encode(uri, method + "_/", false);
  }
  return filename;
}

}  // namespace net
