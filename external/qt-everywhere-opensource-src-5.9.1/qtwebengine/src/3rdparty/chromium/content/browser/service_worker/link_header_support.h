// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_LINK_HEADER_SUPPORT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_LINK_HEADER_SUPPORT_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace net {
class URLRequest;
}

namespace content {
class ServiceWorkerContextWrapper;

void ProcessRequestForLinkHeaders(const net::URLRequest* request);

CONTENT_EXPORT void ProcessLinkHeaderForRequest(
    const net::URLRequest* request,
    const std::string& link_header,
    ServiceWorkerContextWrapper* service_worker_context_for_testing = nullptr);

CONTENT_EXPORT void SplitLinkHeaderForTesting(const std::string& header,
                                              std::vector<std::string>* values);
CONTENT_EXPORT bool ParseLinkHeaderValueForTesting(
    const std::string& link,
    std::string* url,
    std::unordered_map<std::string, std::string>* params);

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_LINK_HEADER_SUPPORT_H_
