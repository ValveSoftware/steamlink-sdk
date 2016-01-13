// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_INFO_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_devtools_info.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_response_info.h"
#include "url/gurl.h"

namespace content {

struct ResourceResponseInfo {
  CONTENT_EXPORT ResourceResponseInfo();
  CONTENT_EXPORT ~ResourceResponseInfo();

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time;

  // The response headers or NULL if the URL type does not support headers.
  scoped_refptr<net::HttpResponseHeaders> headers;

  // The mime type of the response.  This may be a derived value.
  std::string mime_type;

  // The character encoding of the response or none if not applicable to the
  // response's mime type.  This may be a derived value.
  std::string charset;

  // An opaque string carrying security information pertaining to this
  // response.  This may include information about the SSL connection used.
  std::string security_info;

  // Content length if available. -1 if not available
  int64 content_length;

  // Length of the encoded data transferred over the network. In case there is
  // no data, contains -1.
  int64 encoded_data_length;

  // The appcache this response was loaded from, or kAppCacheNoCacheId.
  int64 appcache_id;

  // The manifest url of the appcache this response was loaded from.
  // Note: this value is only populated for main resource requests.
  GURL appcache_manifest_url;

  // Detailed timing information used by the WebTiming, HAR and Developer
  // Tools.  Includes socket ID and socket reuse information.
  net::LoadTimingInfo load_timing;

  // Actual request and response headers, as obtained from the network stack.
  // Only present if request had LOAD_REPORT_RAW_HEADERS in load_flags, and
  // requesting renderer had CanReadRowCookies permission.
  scoped_refptr<ResourceDevToolsInfo> devtools_info;

  // The path to a file that will contain the response body.  It may only
  // contain a portion of the response body at the time that the ResponseInfo
  // becomes available.
  base::FilePath download_file_path;

  // True if the response was delivered using SPDY.
  bool was_fetched_via_spdy;

  // True if the response was delivered after NPN is negotiated.
  bool was_npn_negotiated;

  // True if response could use alternate protocol. However, browser will
  // ignore the alternate protocol when spdy is not enabled on browser side.
  bool was_alternate_protocol_available;

  // Information about the type of connection used to fetch this response.
  net::HttpResponseInfo::ConnectionInfo connection_info;

  // True if the response was fetched via an explicit proxy (as opposed to a
  // transparent proxy). The proxy could be any type of proxy, HTTP or SOCKS.
  // Note: we cannot tell if a transparent proxy may have been involved.
  bool was_fetched_via_proxy;

  // NPN protocol negotiated with the server.
  std::string npn_negotiated_protocol;

  // Remote address of the socket which fetched this resource.
  net::HostPortPair socket_address;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_RESPONSE_INFO_H_
