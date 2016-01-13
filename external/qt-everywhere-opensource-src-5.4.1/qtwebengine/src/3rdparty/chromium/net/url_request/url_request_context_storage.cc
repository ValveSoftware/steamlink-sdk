// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_storage.h"

#include "base/logging.h"
#include "net/base/net_log.h"
#include "net/base/network_delegate.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/url_request/fraudulent_certificate_reporter.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_throttler_manager.h"

namespace net {

URLRequestContextStorage::URLRequestContextStorage(URLRequestContext* context)
    : context_(context) {
  DCHECK(context);
}

URLRequestContextStorage::~URLRequestContextStorage() {}

void URLRequestContextStorage::set_net_log(NetLog* net_log) {
  context_->set_net_log(net_log);
  net_log_.reset(net_log);
}

void URLRequestContextStorage::set_host_resolver(
    scoped_ptr<HostResolver> host_resolver) {
  context_->set_host_resolver(host_resolver.get());
  host_resolver_ = host_resolver.Pass();
}

void URLRequestContextStorage::set_cert_verifier(CertVerifier* cert_verifier) {
  context_->set_cert_verifier(cert_verifier);
  cert_verifier_.reset(cert_verifier);
}

void URLRequestContextStorage::set_server_bound_cert_service(
    ServerBoundCertService* server_bound_cert_service) {
  context_->set_server_bound_cert_service(server_bound_cert_service);
  server_bound_cert_service_.reset(server_bound_cert_service);
}

void URLRequestContextStorage::set_fraudulent_certificate_reporter(
    FraudulentCertificateReporter* fraudulent_certificate_reporter) {
  context_->set_fraudulent_certificate_reporter(
      fraudulent_certificate_reporter);
  fraudulent_certificate_reporter_.reset(fraudulent_certificate_reporter);
}

void URLRequestContextStorage::set_http_auth_handler_factory(
    HttpAuthHandlerFactory* http_auth_handler_factory) {
  context_->set_http_auth_handler_factory(http_auth_handler_factory);
  http_auth_handler_factory_.reset(http_auth_handler_factory);
}

void URLRequestContextStorage::set_proxy_service(ProxyService* proxy_service) {
  context_->set_proxy_service(proxy_service);
  proxy_service_.reset(proxy_service);
}

void URLRequestContextStorage::set_ssl_config_service(
    SSLConfigService* ssl_config_service) {
  context_->set_ssl_config_service(ssl_config_service);
  ssl_config_service_ = ssl_config_service;
}

void URLRequestContextStorage::set_network_delegate(
    NetworkDelegate* network_delegate) {
  context_->set_network_delegate(network_delegate);
  network_delegate_.reset(network_delegate);
}

void URLRequestContextStorage::set_http_server_properties(
    scoped_ptr<HttpServerProperties> http_server_properties) {
  http_server_properties_ = http_server_properties.Pass();
  context_->set_http_server_properties(http_server_properties_->GetWeakPtr());
}

void URLRequestContextStorage::set_cookie_store(CookieStore* cookie_store) {
  context_->set_cookie_store(cookie_store);
  cookie_store_ = cookie_store;
}

void URLRequestContextStorage::set_transport_security_state(
    TransportSecurityState* transport_security_state) {
  context_->set_transport_security_state(transport_security_state);
  transport_security_state_.reset(transport_security_state);
}

void URLRequestContextStorage::set_http_transaction_factory(
    HttpTransactionFactory* http_transaction_factory) {
  context_->set_http_transaction_factory(http_transaction_factory);
  http_transaction_factory_.reset(http_transaction_factory);
}

void URLRequestContextStorage::set_job_factory(
    URLRequestJobFactory* job_factory) {
  context_->set_job_factory(job_factory);
  job_factory_.reset(job_factory);
}

void URLRequestContextStorage::set_throttler_manager(
    URLRequestThrottlerManager* throttler_manager) {
  context_->set_throttler_manager(throttler_manager);
  throttler_manager_.reset(throttler_manager);
}

void URLRequestContextStorage::set_http_user_agent_settings(
    HttpUserAgentSettings* http_user_agent_settings) {
  context_->set_http_user_agent_settings(http_user_agent_settings);
  http_user_agent_settings_.reset(http_user_agent_settings);
}

}  // namespace net
