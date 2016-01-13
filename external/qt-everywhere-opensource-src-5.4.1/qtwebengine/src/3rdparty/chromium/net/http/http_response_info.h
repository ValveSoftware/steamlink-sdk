// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_INFO_H_
#define NET_HTTP_HTTP_RESPONSE_INFO_H_

#include <string>

#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_vary_data.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_info.h"

class Pickle;

namespace net {

class AuthChallengeInfo;
class HttpResponseHeaders;
class IOBufferWithSize;
class SSLCertRequestInfo;

class NET_EXPORT HttpResponseInfo {
 public:
  // Describes the kind of connection used to fetch this response.
  //
  // NOTE: This is persisted to the cache, so make sure not to reorder
  // these values.
  //
  // TODO(akalin): Better yet, just use a string instead of an enum,
  // like |npn_negotiated_protocol|.
  enum ConnectionInfo {
    CONNECTION_INFO_UNKNOWN = 0,
    CONNECTION_INFO_HTTP1 = 1,
    CONNECTION_INFO_DEPRECATED_SPDY2 = 2,
    CONNECTION_INFO_SPDY3 = 3,
    CONNECTION_INFO_SPDY4 = 4,
    CONNECTION_INFO_QUIC1_SPDY3 = 5,
    NUM_OF_CONNECTION_INFOS,
  };

  HttpResponseInfo();
  HttpResponseInfo(const HttpResponseInfo& rhs);
  ~HttpResponseInfo();
  HttpResponseInfo& operator=(const HttpResponseInfo& rhs);
  // Even though we could get away with the copy ctor and default operator=,
  // that would prevent us from doing a bunch of forward declaration.

  // Initializes from the representation stored in the given pickle.
  bool InitFromPickle(const Pickle& pickle, bool* response_truncated);

  // Call this method to persist the response info.
  void Persist(Pickle* pickle,
               bool skip_transient_headers,
               bool response_truncated) const;

  // The following is only defined if the request_time member is set.
  // If this resource was found in the cache, then this bool is set, and
  // request_time may corresponds to a time "far" in the past.  Note that
  // stale content (perhaps un-cacheable) may be fetched from cache subject to
  // the load flags specified on the request info.  For example, this is done
  // when a user presses the back button to re-render pages, or at startup,
  // when reloading previously visited pages (without going over the network).
  // Note also that under normal circumstances, was_cached is set to the correct
  // value even if the request fails.
  bool was_cached;

  // True if the request was fetched from cache rather than the network
  // because of a LOAD_FROM_CACHE_IF_OFFLINE flag when the system
  // was unable to contact the server.
  bool server_data_unavailable;

  // True if the request accessed the network in the process of retrieving
  // data.
  bool network_accessed;

  // True if the request was fetched over a SPDY channel.
  bool was_fetched_via_spdy;

  // True if the npn was negotiated for this request.
  bool was_npn_negotiated;

  // True if the request was fetched via an explicit proxy.  The proxy could
  // be any type of proxy, HTTP or SOCKS.  Note, we do not know if a
  // transparent proxy may have been involved. If true, |proxy_server| contains
  // the name of the proxy server that was used.
  bool was_fetched_via_proxy;
  HostPortPair proxy_server;

  // Whether the request use http proxy or server authentication.
  bool did_use_http_auth;

  // Remote address of the socket which fetched this resource.
  //
  // NOTE: If the response was served from the cache (was_cached is true),
  // the socket address will be set to the address that the content came from
  // originally.  This is true even if the response was re-validated using a
  // different remote address, or if some of the content came from a byte-range
  // request to a different address.
  HostPortPair socket_address;

  // Protocol negotiated with the server.
  std::string npn_negotiated_protocol;

  // The type of connection used for this response.
  ConnectionInfo connection_info;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this is the last time the cache entry was validated.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // this is the last time the cache entry was validated.
  base::Time response_time;

  // If the response headers indicate a 401 or 407 failure, then this structure
  // will contain additional information about the authentication challenge.
  scoped_refptr<AuthChallengeInfo> auth_challenge;

  // The SSL client certificate request info.
  // TODO(wtc): does this really belong in HttpResponseInfo?  I put it here
  // because it is similar to |auth_challenge|, but unlike HTTP authentication
  // challenge, client certificate request is not part of an HTTP response.
  scoped_refptr<SSLCertRequestInfo> cert_request_info;

  // The SSL connection info (if HTTPS).
  SSLInfo ssl_info;

  // The parsed response headers and status line.
  scoped_refptr<HttpResponseHeaders> headers;

  // The "Vary" header data for this response.
  HttpVaryData vary_data;

  // Any metadata asociated with this resource's cached data.
  scoped_refptr<IOBufferWithSize> metadata;

  static ConnectionInfo ConnectionInfoFromNextProto(NextProto next_proto);

  static std::string ConnectionInfoToString(ConnectionInfo connection_info);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_INFO_H_
