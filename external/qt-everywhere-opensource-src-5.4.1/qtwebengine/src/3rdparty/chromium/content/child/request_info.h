// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_REQUEST_INFO_H_
#define CONTENT_CHILD_REQUEST_INFO_H_

#include <stdint.h>

#include <string>

#include "content/common/content_export.h"
#include "net/base/request_priority.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"
#include "webkit/common/resource_type.h"

namespace content {

// Structure used when calling BlinkPlatformImpl::CreateResourceLoader().
struct CONTENT_EXPORT RequestInfo {
  RequestInfo();
  ~RequestInfo();

  // HTTP-style method name (e.g., "GET" or "POST").
  std::string method;

  // Absolute URL encoded in ASCII per the rules of RFC-2396.
  GURL url;

  // URL of the document in the top-level window, which may be checked by the
  // third-party cookie blocking policy.
  GURL first_party_for_cookies;

  // Optional parameter, a URL with similar constraints in how it must be
  // encoded as the url member.
  GURL referrer;

  // The referrer policy that applies to the referrer.
  blink::WebReferrerPolicy referrer_policy;

  // For HTTP(S) requests, the headers parameter can be a \r\n-delimited and
  // \r\n-terminated list of MIME headers.  They should be ASCII-encoded using
  // the standard MIME header encoding rules.  The headers parameter can also
  // be null if no extra request headers need to be set.
  std::string headers;

  // Composed of the values defined in url_request_load_flags.h.
  int load_flags;

  // Process id of the process making the request.
  int requestor_pid;

  // Indicates if the current request is the main frame load, a sub-frame
  // load, or a sub objects load.
  ResourceType::Type request_type;

  // Indicates the priority of this request, as determined by WebKit.
  net::RequestPriority priority;

  // Used for plugin to browser requests.
  uint32_t request_context;

  // Identifies what appcache host this request is associated with.
  int appcache_host_id;

  // Used to associated the bridge with a frame's network context.
  int routing_id;

  // If true, then the response body will be downloaded to a file and the
  // path to that file will be provided in ResponseInfo::download_file_path.
  bool download_to_file;

  // True if the request was user initiated.
  bool has_user_gesture;

  // Extra data associated with this request.  We do not own this pointer.
  blink::WebURLRequest::ExtraData* extra_data;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestInfo);
};

}  // namespace content

#endif  // CONTENT_CHILD_REQUEST_INFO_H_
