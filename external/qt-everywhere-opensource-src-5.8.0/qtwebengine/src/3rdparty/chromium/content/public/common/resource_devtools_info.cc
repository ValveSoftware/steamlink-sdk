// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_devtools_info.h"

namespace content {

ResourceDevToolsInfo::ResourceDevToolsInfo()
    : http_status_code(0) {
}

ResourceDevToolsInfo::~ResourceDevToolsInfo() {
}

scoped_refptr<ResourceDevToolsInfo> ResourceDevToolsInfo::DeepCopy() const {
  scoped_refptr<ResourceDevToolsInfo> new_info = new ResourceDevToolsInfo;
  new_info->http_status_code = http_status_code;
  new_info->http_status_text = http_status_text;
  new_info->request_headers = request_headers;
  new_info->response_headers = response_headers;
  new_info->request_headers_text = request_headers_text;
  new_info->response_headers_text = response_headers_text;
  return new_info;
}

}  // namespace content
