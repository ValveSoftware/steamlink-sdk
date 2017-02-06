// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H_

#include <string>
#include "content/public/common/resource_type.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"

namespace content {

struct LoadFromMemoryCacheDetails {
  LoadFromMemoryCacheDetails(const GURL& url,
                             int cert_id,
                             net::CertStatus cert_status,
                             const std::string& http_method,
                             const std::string& mime_type,
                             ResourceType resource_type);
  ~LoadFromMemoryCacheDetails();

  GURL url;
  int cert_id;
  net::CertStatus cert_status;
  std::string http_method;
  std::string mime_type;
  ResourceType resource_type;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H_
