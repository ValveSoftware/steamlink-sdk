// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is useful for building a simple URLRequestContext. Most creators
// of new URLRequestContexts should use this helper class to construct it. Call
// any configuration params, and when done, invoke Build() to construct the
// URLRequestContext. This URLRequestContext will own all its own storage.
//
// URLRequestContextBuilder and its associated params classes are initially
// populated with "sane" default values. Read through the comments to figure out
// what these are.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/base/network_delegate.h"
#include "net/dns/host_resolver.h"
#include "net/socket/next_proto.h"

namespace net {

class FtpTransactionFactory;
class HostMappingRules;
class HttpAuthHandlerFactory;
class ProxyConfigService;
class URLRequestContext;

class NET_EXPORT URLRequestContextBuilder {
 public:
  struct NET_EXPORT HttpCacheParams {
    enum Type {
      IN_MEMORY,
      DISK,
    };

    HttpCacheParams();
    ~HttpCacheParams();

    // The type of HTTP cache. Default is IN_MEMORY.
    Type type;

    // The max size of the cache in bytes. Default is algorithmically determined
    // based off available disk space.
    int max_size;

    // The cache path (when type is DISK).
    base::FilePath path;
  };

  struct NET_EXPORT HttpNetworkSessionParams {
    HttpNetworkSessionParams();
    ~HttpNetworkSessionParams();

    // These fields mirror those in net::HttpNetworkSession::Params;
    bool ignore_certificate_errors;
    HostMappingRules* host_mapping_rules;
    uint16 testing_fixed_http_port;
    uint16 testing_fixed_https_port;
    NextProtoVector next_protos;
    std::string trusted_spdy_proxy;
    bool use_alternate_protocols;
  };

  URLRequestContextBuilder();
  ~URLRequestContextBuilder();

  void set_proxy_config_service(ProxyConfigService* proxy_config_service);

  // Call these functions to specify hard-coded Accept-Language
  // or User-Agent header values for all requests that don't
  // have the headers already set.
  void set_accept_language(const std::string& accept_language) {
    accept_language_ = accept_language;
  }
  void set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
  }

  // Control support for data:// requests. By default it's disabled.
  void set_data_enabled(bool enable) {
    data_enabled_ = enable;
  }

#if !defined(DISABLE_FILE_SUPPORT)
  // Control support for file:// requests. By default it's disabled.
  void set_file_enabled(bool enable) {
    file_enabled_ = enable;
  }
#endif

#if !defined(DISABLE_FTP_SUPPORT)
  // Control support for ftp:// requests. By default it's disabled.
  void set_ftp_enabled(bool enable) {
    ftp_enabled_ = enable;
  }
#endif

  // By default host_resolver is constructed with CreateDefaultResolver.
  void set_host_resolver(HostResolver* host_resolver) {
    host_resolver_.reset(host_resolver);
  }

  // Uses BasicNetworkDelegate by default. Note that calling Build will unset
  // any custom delegate in builder, so this must be called each time before
  // Build is called.
  void set_network_delegate(NetworkDelegate* delegate) {
    network_delegate_.reset(delegate);
  }


  // Adds additional auth handler factories to be used in addition to what is
  // provided in the default |HttpAuthHandlerRegistryFactory|. The auth |scheme|
  // and |factory| are provided. The builder takes ownership of the factory and
  // Build() must be called after this method.
  void add_http_auth_handler_factory(const std::string& scheme,
                                     net::HttpAuthHandlerFactory* factory) {
    extra_http_auth_handlers_.push_back(SchemeFactory(scheme, factory));
  }

  // By default HttpCache is enabled with a default constructed HttpCacheParams.
  void EnableHttpCache(const HttpCacheParams& params) {
    http_cache_enabled_ = true;
    http_cache_params_ = params;
  }

  void DisableHttpCache() {
    http_cache_enabled_ = false;
    http_cache_params_ = HttpCacheParams();
  }

  // Override default net::HttpNetworkSession::Params settings.
  void set_http_network_session_params(
      const HttpNetworkSessionParams& http_network_session_params) {
    http_network_session_params_ = http_network_session_params;
  }

  // Adjust |http_network_session_params_.next_protos| to enable SPDY and QUIC.
  void SetSpdyAndQuicEnabled(bool spdy_enabled,
                             bool quic_enabled);

  URLRequestContext* Build();

 private:
  struct SchemeFactory {
    SchemeFactory(const std::string& scheme,
                  net::HttpAuthHandlerFactory* factory);
    ~SchemeFactory();

    std::string scheme;
    net::HttpAuthHandlerFactory* factory;
  };

  std::string accept_language_;
  std::string user_agent_;
  // Include support for data:// requests.
  bool data_enabled_;
#if !defined(DISABLE_FILE_SUPPORT)
  // Include support for file:// requests.
  bool file_enabled_;
#endif
#if !defined(DISABLE_FTP_SUPPORT)
  // Include support for ftp:// requests.
  bool ftp_enabled_;
#endif
  bool http_cache_enabled_;
  HttpCacheParams http_cache_params_;
  HttpNetworkSessionParams http_network_session_params_;
  scoped_ptr<HostResolver> host_resolver_;
  scoped_ptr<ProxyConfigService> proxy_config_service_;
  scoped_ptr<NetworkDelegate> network_delegate_;
  scoped_ptr<FtpTransactionFactory> ftp_transaction_factory_;
  std::vector<SchemeFactory> extra_http_auth_handlers_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextBuilder);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
