// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_server_request_info.h"

#include "base/strings/string_util.h"

namespace net {

HttpServerRequestInfo::HttpServerRequestInfo() {}

HttpServerRequestInfo::~HttpServerRequestInfo() {}

std::string HttpServerRequestInfo::GetHeaderValue(
    const std::string& header_name) const {
  DCHECK_EQ(StringToLowerASCII(header_name), header_name);
  HttpServerRequestInfo::HeadersMap::const_iterator it =
      headers.find(header_name);
  if (it != headers.end())
    return it->second;
  return std::string();
}

bool HttpServerRequestInfo::HasHeaderValue(
    const std::string& header_name,
    const std::string& header_value) const {
  DCHECK_EQ(StringToLowerASCII(header_value), header_value);
  std::string complete_value = GetHeaderValue(header_name);
  StringToLowerASCII(&complete_value);
  std::vector<std::string> value_items;
  Tokenize(complete_value, ",", &value_items);
  for (std::vector<std::string>::iterator it = value_items.begin();
      it != value_items.end(); ++it) {
    base::TrimString(*it, " \t", &*it);
    if (*it == header_value)
      return true;
  }
  return false;
}

}  // namespace net
