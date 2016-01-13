// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/load_from_memory_cache_details.h"

namespace content {

LoadFromMemoryCacheDetails::LoadFromMemoryCacheDetails(
    const GURL& url,
    int pid,
    int cert_id,
    net::CertStatus cert_status,
    const std::string& http_method,
    const std::string& mime_type,
    ResourceType::Type resource_type)
    : url(url),
      pid(pid),
      cert_id(cert_id),
      cert_status(cert_status),
      http_method(http_method),
      mime_type(mime_type),
      resource_type(resource_type) {
}

LoadFromMemoryCacheDetails::~LoadFromMemoryCacheDetails() {
}

}  // namespace content
