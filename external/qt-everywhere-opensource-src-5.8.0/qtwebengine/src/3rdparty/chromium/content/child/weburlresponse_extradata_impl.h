// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBURLRESPONSE_EXTRADATA_IMPL_H_
#define CONTENT_CHILD_WEBURLRESPONSE_EXTRADATA_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "net/http/http_response_info.h"
#include "net/nqe/network_quality_estimator.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace content {

class CONTENT_EXPORT WebURLResponseExtraDataImpl :
    public NON_EXPORTED_BASE(blink::WebURLResponse::ExtraData) {
 public:
  explicit WebURLResponseExtraDataImpl(
      const std::string& npn_negotiated_protocol);
  ~WebURLResponseExtraDataImpl() override;

  const std::string& npn_negotiated_protocol() const {
    return npn_negotiated_protocol_;
  }

  // Flag whether this request was loaded via an explicit proxy
  // (HTTP, SOCKS, etc).
  bool was_fetched_via_proxy() const {
    return was_fetched_via_proxy_;
  }
  void set_was_fetched_via_proxy(bool was_fetched_via_proxy) {
    was_fetched_via_proxy_ = was_fetched_via_proxy;
  }

  // The proxy server used if this request was loaded via an explicit proxy
  // (HTTP, SOCKS, etc).
  net::HostPortPair proxy_server() const {
    return proxy_server_;
  }
  void set_proxy_server(net::HostPortPair proxy_server) {
    proxy_server_ = proxy_server;
  }

  /// Flag whether this request was loaded via the SPDY protocol or not.
  // SPDY is an experimental web protocol, see http://dev.chromium.org/spdy
  bool was_fetched_via_spdy() const {
    return was_fetched_via_spdy_;
  }
  void set_was_fetched_via_spdy(bool was_fetched_via_spdy) {
    was_fetched_via_spdy_ = was_fetched_via_spdy;
  }

  // Information about the type of connection used to fetch this response.
  net::HttpResponseInfo::ConnectionInfo connection_info() const {
    return connection_info_;
  }
  void set_connection_info(
      net::HttpResponseInfo::ConnectionInfo connection_info) {
    connection_info_ = connection_info;
  }

  // Flag whether this request was loaded after the
  // TLS/Next-Protocol-Negotiation was used.
  // This is related to SPDY.
  bool was_npn_negotiated() const {
    return was_npn_negotiated_;
  }
  void set_was_npn_negotiated(bool was_npn_negotiated) {
    was_npn_negotiated_ = was_npn_negotiated;
  }

  // Flag whether this request was made when "Alternate-Protocol: xxx"
  // is present in server's response.
  bool was_alternate_protocol_available() const {
    return was_alternate_protocol_available_;
  }
  void set_was_alternate_protocol_available(
      bool was_alternate_protocol_available) {
    was_alternate_protocol_available_ = was_alternate_protocol_available;
  }

  bool is_ftp_directory_listing() const { return is_ftp_directory_listing_; }
  void set_is_ftp_directory_listing(bool is_ftp_directory_listing) {
    is_ftp_directory_listing_ = is_ftp_directory_listing;
  }

  bool is_using_lofi() const { return is_using_lofi_; }
  void set_is_using_lofi(bool is_using_lofi) { is_using_lofi_ = is_using_lofi; }

  net::NetworkQualityEstimator::EffectiveConnectionType
  effective_connection_type() const {
    return effective_connection_type_;
  }
  void set_effective_connection_type(
      net::NetworkQualityEstimator::EffectiveConnectionType
          effective_connection_type) {
    effective_connection_type_ = effective_connection_type;
  }

 private:
  std::string npn_negotiated_protocol_;
  bool is_ftp_directory_listing_;
  bool was_fetched_via_proxy_;
  net::HostPortPair proxy_server_;
  bool was_fetched_via_spdy_;
  bool was_npn_negotiated_;
  net::HttpResponseInfo::ConnectionInfo connection_info_;
  bool was_alternate_protocol_available_;
  bool is_using_lofi_;
  net::NetworkQualityEstimator::EffectiveConnectionType
      effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(WebURLResponseExtraDataImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBURLRESPONSE_EXTRADATA_IMPL_H_
