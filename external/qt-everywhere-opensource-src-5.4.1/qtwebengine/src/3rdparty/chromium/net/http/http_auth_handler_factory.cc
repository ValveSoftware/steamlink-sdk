// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_factory.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_auth_handler_digest.h"
#include "net/http/http_auth_handler_ntlm.h"

#if defined(USE_KERBEROS)
#include "net/http/http_auth_handler_negotiate.h"
#endif

namespace net {

int HttpAuthHandlerFactory::CreateAuthHandlerFromString(
    const std::string& challenge,
    HttpAuth::Target target,
    const GURL& origin,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  HttpAuthChallengeTokenizer props(challenge.begin(), challenge.end());
  return CreateAuthHandler(&props, target, origin, CREATE_CHALLENGE, 1,
                           net_log, handler);
}

int HttpAuthHandlerFactory::CreatePreemptiveAuthHandlerFromString(
    const std::string& challenge,
    HttpAuth::Target target,
    const GURL& origin,
    int digest_nonce_count,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  HttpAuthChallengeTokenizer props(challenge.begin(), challenge.end());
  return CreateAuthHandler(&props, target, origin, CREATE_PREEMPTIVE,
                           digest_nonce_count, net_log, handler);
}

// static
HttpAuthHandlerRegistryFactory* HttpAuthHandlerFactory::CreateDefault(
    HostResolver* host_resolver) {
  DCHECK(host_resolver);
  HttpAuthHandlerRegistryFactory* registry_factory =
      new HttpAuthHandlerRegistryFactory();
  registry_factory->RegisterSchemeFactory(
      "basic", new HttpAuthHandlerBasic::Factory());
  registry_factory->RegisterSchemeFactory(
      "digest", new HttpAuthHandlerDigest::Factory());

#if defined(USE_KERBEROS)
  HttpAuthHandlerNegotiate::Factory* negotiate_factory =
      new HttpAuthHandlerNegotiate::Factory();
#if defined(OS_POSIX)
  negotiate_factory->set_library(new GSSAPISharedLibrary(std::string()));
#elif defined(OS_WIN)
  negotiate_factory->set_library(new SSPILibraryDefault());
#endif
  negotiate_factory->set_host_resolver(host_resolver);
  registry_factory->RegisterSchemeFactory("negotiate", negotiate_factory);
#endif  // defined(USE_KERBEROS)

  HttpAuthHandlerNTLM::Factory* ntlm_factory =
      new HttpAuthHandlerNTLM::Factory();
#if defined(OS_WIN)
  ntlm_factory->set_sspi_library(new SSPILibraryDefault());
#endif
  registry_factory->RegisterSchemeFactory("ntlm", ntlm_factory);
  return registry_factory;
}

namespace {

bool IsSupportedScheme(const std::vector<std::string>& supported_schemes,
                       const std::string& scheme) {
  std::vector<std::string>::const_iterator it = std::find(
      supported_schemes.begin(), supported_schemes.end(), scheme);
  return it != supported_schemes.end();
}

}  // namespace

HttpAuthHandlerRegistryFactory::HttpAuthHandlerRegistryFactory() {
}

HttpAuthHandlerRegistryFactory::~HttpAuthHandlerRegistryFactory() {
  STLDeleteContainerPairSecondPointers(factory_map_.begin(),
                                       factory_map_.end());
}

void HttpAuthHandlerRegistryFactory::SetURLSecurityManager(
    const std::string& scheme,
    URLSecurityManager* security_manager) {
  HttpAuthHandlerFactory* factory = GetSchemeFactory(scheme);
  if (factory)
    factory->set_url_security_manager(security_manager);
}

void HttpAuthHandlerRegistryFactory::RegisterSchemeFactory(
    const std::string& scheme,
    HttpAuthHandlerFactory* factory) {
  std::string lower_scheme = StringToLowerASCII(scheme);
  FactoryMap::iterator it = factory_map_.find(lower_scheme);
  if (it != factory_map_.end()) {
    delete it->second;
  }
  if (factory)
    factory_map_[lower_scheme] = factory;
  else
    factory_map_.erase(it);
}

HttpAuthHandlerFactory* HttpAuthHandlerRegistryFactory::GetSchemeFactory(
    const std::string& scheme) const {
  std::string lower_scheme = StringToLowerASCII(scheme);
  FactoryMap::const_iterator it = factory_map_.find(lower_scheme);
  if (it == factory_map_.end()) {
    return NULL;                  // |scheme| is not registered.
  }
  return it->second;
}

// static
HttpAuthHandlerRegistryFactory* HttpAuthHandlerRegistryFactory::Create(
    const std::vector<std::string>& supported_schemes,
    URLSecurityManager* security_manager,
    HostResolver* host_resolver,
    const std::string& gssapi_library_name,
    bool negotiate_disable_cname_lookup,
    bool negotiate_enable_port) {
  HttpAuthHandlerRegistryFactory* registry_factory =
      new HttpAuthHandlerRegistryFactory();
  if (IsSupportedScheme(supported_schemes, "basic"))
    registry_factory->RegisterSchemeFactory(
        "basic", new HttpAuthHandlerBasic::Factory());
  if (IsSupportedScheme(supported_schemes, "digest"))
    registry_factory->RegisterSchemeFactory(
        "digest", new HttpAuthHandlerDigest::Factory());
  if (IsSupportedScheme(supported_schemes, "ntlm")) {
    HttpAuthHandlerNTLM::Factory* ntlm_factory =
        new HttpAuthHandlerNTLM::Factory();
    ntlm_factory->set_url_security_manager(security_manager);
#if defined(OS_WIN)
    ntlm_factory->set_sspi_library(new SSPILibraryDefault());
#endif
    registry_factory->RegisterSchemeFactory("ntlm", ntlm_factory);
  }
#if defined(USE_KERBEROS)
  if (IsSupportedScheme(supported_schemes, "negotiate")) {
    HttpAuthHandlerNegotiate::Factory* negotiate_factory =
        new HttpAuthHandlerNegotiate::Factory();
#if defined(OS_POSIX)
    negotiate_factory->set_library(
        new GSSAPISharedLibrary(gssapi_library_name));
#elif defined(OS_WIN)
    negotiate_factory->set_library(new SSPILibraryDefault());
#endif
    negotiate_factory->set_url_security_manager(security_manager);
    DCHECK(host_resolver || negotiate_disable_cname_lookup);
    negotiate_factory->set_host_resolver(host_resolver);
    negotiate_factory->set_disable_cname_lookup(negotiate_disable_cname_lookup);
    negotiate_factory->set_use_port(negotiate_enable_port);
    registry_factory->RegisterSchemeFactory("negotiate", negotiate_factory);
  }
#endif  // defined(USE_KERBEROS)

  return registry_factory;
}

int HttpAuthHandlerRegistryFactory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    CreateReason reason,
    int digest_nonce_count,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  std::string scheme = challenge->scheme();
  if (scheme.empty()) {
    handler->reset();
    return ERR_INVALID_RESPONSE;
  }
  std::string lower_scheme = StringToLowerASCII(scheme);
  FactoryMap::iterator it = factory_map_.find(lower_scheme);
  if (it == factory_map_.end()) {
    handler->reset();
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  }
  DCHECK(it->second);
  return it->second->CreateAuthHandler(challenge, target, origin, reason,
                                       digest_nonce_count, net_log, handler);
}

}  // namespace net
