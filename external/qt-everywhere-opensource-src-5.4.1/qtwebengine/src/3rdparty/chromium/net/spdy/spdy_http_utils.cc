// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"

namespace net {

bool SpdyHeadersToHttpResponse(const SpdyHeaderBlock& headers,
                               SpdyMajorVersion protocol_version,
                               HttpResponseInfo* response) {
  std::string status_key = (protocol_version >= SPDY3) ? ":status" : "status";
  std::string version_key =
      (protocol_version >= SPDY3) ? ":version" : "version";
  std::string version;
  std::string status;

  // The "status" header is required. "version" is required below SPDY4.
  SpdyHeaderBlock::const_iterator it;
  it = headers.find(status_key);
  if (it == headers.end())
    return false;
  status = it->second;

  if (protocol_version >= SPDY4) {
    version = "HTTP/1.1";
  } else {
    it = headers.find(version_key);
    if (it == headers.end())
      return false;
    version = it->second;
  }
  std::string raw_headers(version);
  raw_headers.push_back(' ');
  raw_headers.append(status);
  raw_headers.push_back('\0');
  for (it = headers.begin(); it != headers.end(); ++it) {
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    std::string value = it->second;
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      std::string tval;
      if (end != value.npos)
        tval = value.substr(start, (end - start));
      else
        tval = value.substr(start);
      if (protocol_version >= 3 && it->first[0] == ':')
        raw_headers.append(it->first.substr(1));
      else
        raw_headers.append(it->first);
      raw_headers.push_back(':');
      raw_headers.append(tval);
      raw_headers.push_back('\0');
      start = end + 1;
    } while (end != value.npos);
  }

  response->headers = new HttpResponseHeaders(raw_headers);
  response->was_fetched_via_spdy = true;
  return true;
}

void CreateSpdyHeadersFromHttpRequest(const HttpRequestInfo& info,
                                      const HttpRequestHeaders& request_headers,
                                      SpdyHeaderBlock* headers,
                                      SpdyMajorVersion protocol_version,
                                      bool direct) {

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = StringToLowerASCII(it.name());
    if (name == "connection" || name == "proxy-connection" ||
        name == "transfer-encoding" || name == "host") {
      continue;
    }
    if (headers->find(name) == headers->end()) {
      (*headers)[name] = it.value();
    } else {
      std::string new_value = (*headers)[name];
      new_value.append(1, '\0');  // +=() doesn't append 0's
      new_value += it.value();
      (*headers)[name] = new_value;
    }
  }
  static const char kHttpProtocolVersion[] = "HTTP/1.1";

  if (protocol_version < SPDY3) {
    (*headers)["version"] = kHttpProtocolVersion;
    (*headers)["method"] = info.method;
    (*headers)["host"] = GetHostAndOptionalPort(info.url);
    (*headers)["scheme"] = info.url.scheme();
    if (direct)
      (*headers)["url"] = HttpUtil::PathForRequest(info.url);
    else
      (*headers)["url"] = HttpUtil::SpecForRequest(info.url);
  } else {
    if (protocol_version < SPDY4) {
      (*headers)[":version"] = kHttpProtocolVersion;
      (*headers)[":host"] = GetHostAndOptionalPort(info.url);
    } else {
      (*headers)[":authority"] = GetHostAndOptionalPort(info.url);
    }
    (*headers)[":method"] = info.method;
    (*headers)[":scheme"] = info.url.scheme();
    (*headers)[":path"] = HttpUtil::PathForRequest(info.url);
  }
}

COMPILE_ASSERT(HIGHEST - LOWEST < 4 &&
               HIGHEST - MINIMUM_PRIORITY < 5,
               request_priority_incompatible_with_spdy);

SpdyPriority ConvertRequestPriorityToSpdyPriority(
    const RequestPriority priority,
    SpdyMajorVersion protocol_version) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);
  if (protocol_version == SPDY2) {
    // SPDY 2 only has 2 bits of priority, but we have 5 RequestPriorities.
    // Map IDLE => 3, LOWEST => 2, LOW => 2, MEDIUM => 1, HIGHEST => 0.
    if (priority > LOWEST) {
      return static_cast<SpdyPriority>(HIGHEST - priority);
    } else {
      return static_cast<SpdyPriority>(HIGHEST - priority - 1);
    }
  } else {
    return static_cast<SpdyPriority>(HIGHEST - priority);
  }
}

NET_EXPORT_PRIVATE RequestPriority ConvertSpdyPriorityToRequestPriority(
    SpdyPriority priority,
    SpdyMajorVersion protocol_version) {
  // Handle invalid values gracefully, and pick LOW to map 2 back
  // to for SPDY/2.
  SpdyPriority idle_cutoff = (protocol_version == SPDY2) ? 3 : 5;
  return (priority >= idle_cutoff) ?
      IDLE : static_cast<RequestPriority>(HIGHEST - priority);
}

GURL GetUrlFromHeaderBlock(const SpdyHeaderBlock& headers,
                           SpdyMajorVersion protocol_version,
                           bool pushed) {
  // SPDY 2 server push urls are specified in a single "url" header.
  if (pushed && protocol_version == SPDY2) {
      std::string url;
      SpdyHeaderBlock::const_iterator it;
      it = headers.find("url");
      if (it != headers.end())
        url = it->second;
      return GURL(url);
  }

  const char* scheme_header = protocol_version >= SPDY3 ? ":scheme" : "scheme";
  const char* host_header = protocol_version >= SPDY4 ? ":authority" :
      (protocol_version >= SPDY3 ? ":host" : "host");
  const char* path_header = protocol_version >= SPDY3 ? ":path" : "url";

  std::string scheme;
  std::string host_port;
  std::string path;
  SpdyHeaderBlock::const_iterator it;
  it = headers.find(scheme_header);
  if (it != headers.end())
    scheme = it->second;
  it = headers.find(host_header);
  if (it != headers.end())
    host_port = it->second;
  it = headers.find(path_header);
  if (it != headers.end())
    path = it->second;

  std::string url = (scheme.empty() || host_port.empty() || path.empty())
                        ? std::string()
                        : scheme + "://" + host_port + path;
  return GURL(url);
}

}  // namespace net
