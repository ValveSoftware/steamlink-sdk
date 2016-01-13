// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents contextual information (cookies, cache, etc.)
// that's useful when processing resource requests.
// The class is reference-counted so that it can be cleaned up after any
// requests that are using it have been completed.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request.h"

namespace net {
class CertVerifier;
class CookieStore;
class CTVerifier;
class FraudulentCertificateReporter;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class NetworkDelegate;
class SdchManager;
class ServerBoundCertService;
class ProxyService;
class URLRequest;
class URLRequestJobFactory;
class URLRequestThrottlerManager;

// Subclass to provide application-specific context for URLRequest
// instances. Note that URLRequestContext typically does not provide storage for
// these member variables, since they may be shared. For the ones that aren't
// shared, URLRequestContextStorage can be helpful in defining their storage.
class NET_EXPORT URLRequestContext
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  URLRequestContext();
  virtual ~URLRequestContext();

  // Copies the state from |other| into this context.
  void CopyFrom(const URLRequestContext* other);

  // May return NULL if this context doesn't have an associated network session.
  const HttpNetworkSession::Params* GetNetworkSessionParams() const;

  // Creates a URLRequest. |cookie_store| optionally specifies a cookie store
  // to be used rather than the one represented by the context, or NULL
  // otherwise.
  scoped_ptr<URLRequest> CreateRequest(const GURL& url,
                                       RequestPriority priority,
                                       URLRequest::Delegate* delegate,
                                       CookieStore* cookie_store) const;

  NetLog* net_log() const {
    return net_log_;
  }

  void set_net_log(NetLog* net_log) {
    net_log_ = net_log;
  }

  HostResolver* host_resolver() const {
    return host_resolver_;
  }

  void set_host_resolver(HostResolver* host_resolver) {
    host_resolver_ = host_resolver;
  }

  CertVerifier* cert_verifier() const {
    return cert_verifier_;
  }

  void set_cert_verifier(CertVerifier* cert_verifier) {
    cert_verifier_ = cert_verifier;
  }

  ServerBoundCertService* server_bound_cert_service() const {
    return server_bound_cert_service_;
  }

  void set_server_bound_cert_service(
      ServerBoundCertService* server_bound_cert_service) {
    server_bound_cert_service_ = server_bound_cert_service;
  }

  FraudulentCertificateReporter* fraudulent_certificate_reporter() const {
    return fraudulent_certificate_reporter_;
  }
  void set_fraudulent_certificate_reporter(
      FraudulentCertificateReporter* fraudulent_certificate_reporter) {
    fraudulent_certificate_reporter_ = fraudulent_certificate_reporter;
  }

  // Get the proxy service for this context.
  ProxyService* proxy_service() const { return proxy_service_; }
  void set_proxy_service(ProxyService* proxy_service) {
    proxy_service_ = proxy_service;
  }

  // Get the ssl config service for this context.
  SSLConfigService* ssl_config_service() const {
    return ssl_config_service_.get();
  }
  void set_ssl_config_service(SSLConfigService* service) {
    ssl_config_service_ = service;
  }

  // Gets the HTTP Authentication Handler Factory for this context.
  // The factory is only valid for the lifetime of this URLRequestContext
  HttpAuthHandlerFactory* http_auth_handler_factory() const {
    return http_auth_handler_factory_;
  }
  void set_http_auth_handler_factory(HttpAuthHandlerFactory* factory) {
    http_auth_handler_factory_ = factory;
  }

  // Gets the http transaction factory for this context.
  HttpTransactionFactory* http_transaction_factory() const {
    return http_transaction_factory_;
  }
  void set_http_transaction_factory(HttpTransactionFactory* factory) {
    http_transaction_factory_ = factory;
  }

  void set_network_delegate(NetworkDelegate* network_delegate) {
    network_delegate_ = network_delegate;
  }
  NetworkDelegate* network_delegate() const { return network_delegate_; }

  void set_http_server_properties(
      const base::WeakPtr<HttpServerProperties>& http_server_properties) {
    http_server_properties_ = http_server_properties;
  }
  base::WeakPtr<HttpServerProperties> http_server_properties() const {
    return http_server_properties_;
  }

  // Gets the cookie store for this context (may be null, in which case
  // cookies are not stored).
  CookieStore* cookie_store() const { return cookie_store_.get(); }
  void set_cookie_store(CookieStore* cookie_store);

  TransportSecurityState* transport_security_state() const {
    return transport_security_state_;
  }
  void set_transport_security_state(
      TransportSecurityState* state) {
    transport_security_state_ = state;
  }

  CTVerifier* cert_transparency_verifier() const {
    return cert_transparency_verifier_;
  }
  void set_cert_transparency_verifier(CTVerifier* verifier) {
    cert_transparency_verifier_ = verifier;
  }

  const URLRequestJobFactory* job_factory() const { return job_factory_; }
  void set_job_factory(const URLRequestJobFactory* job_factory) {
    job_factory_ = job_factory;
  }

  // May be NULL.
  URLRequestThrottlerManager* throttler_manager() const {
    return throttler_manager_;
  }
  void set_throttler_manager(URLRequestThrottlerManager* throttler_manager) {
    throttler_manager_ = throttler_manager;
  }

  // May be NULL.
  SdchManager* sdch_manager() const {
    return sdch_manager_;
  }
  void set_sdch_manager(SdchManager* sdch_manager) {
    sdch_manager_ = sdch_manager;
  }

  // Gets the URLRequest objects that hold a reference to this
  // URLRequestContext.
  std::set<const URLRequest*>* url_requests() const {
    return url_requests_.get();
  }

  void AssertNoURLRequests() const;

  // Get the underlying |HttpUserAgentSettings| implementation that provides
  // the HTTP Accept-Language and User-Agent header values.
  const HttpUserAgentSettings* http_user_agent_settings() const {
    return http_user_agent_settings_;
  }
  void set_http_user_agent_settings(
      HttpUserAgentSettings* http_user_agent_settings) {
    http_user_agent_settings_ = http_user_agent_settings;
  }

 private:
  // ---------------------------------------------------------------------------
  // Important: When adding any new members below, consider whether they need to
  // be added to CopyFrom.
  // ---------------------------------------------------------------------------

  // Ownership for these members are not defined here. Clients should either
  // provide storage elsewhere or have a subclass take ownership.
  NetLog* net_log_;
  HostResolver* host_resolver_;
  CertVerifier* cert_verifier_;
  ServerBoundCertService* server_bound_cert_service_;
  FraudulentCertificateReporter* fraudulent_certificate_reporter_;
  HttpAuthHandlerFactory* http_auth_handler_factory_;
  ProxyService* proxy_service_;
  scoped_refptr<SSLConfigService> ssl_config_service_;
  NetworkDelegate* network_delegate_;
  base::WeakPtr<HttpServerProperties> http_server_properties_;
  HttpUserAgentSettings* http_user_agent_settings_;
  scoped_refptr<CookieStore> cookie_store_;
  TransportSecurityState* transport_security_state_;
  CTVerifier* cert_transparency_verifier_;
  HttpTransactionFactory* http_transaction_factory_;
  const URLRequestJobFactory* job_factory_;
  URLRequestThrottlerManager* throttler_manager_;
  SdchManager* sdch_manager_;

  // ---------------------------------------------------------------------------
  // Important: When adding any new members below, consider whether they need to
  // be added to CopyFrom.
  // ---------------------------------------------------------------------------

  scoped_ptr<std::set<const URLRequest*> > url_requests_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContext);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
