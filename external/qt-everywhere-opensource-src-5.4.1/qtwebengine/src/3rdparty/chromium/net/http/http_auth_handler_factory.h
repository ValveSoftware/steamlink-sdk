// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/http/url_security_manager.h"

class GURL;

namespace net {

class BoundNetLog;
class HostResolver;
class HttpAuthChallengeTokenizer;
class HttpAuthHandler;
class HttpAuthHandlerRegistryFactory;

// An HttpAuthHandlerFactory is used to create HttpAuthHandler objects.
// The HttpAuthHandlerFactory object _must_ outlive any of the HttpAuthHandler
// objects that it creates.
class NET_EXPORT HttpAuthHandlerFactory {
 public:
  enum CreateReason {
    CREATE_CHALLENGE,     // Create a handler in response to a challenge.
    CREATE_PREEMPTIVE,    // Create a handler preemptively.
  };

  HttpAuthHandlerFactory() : url_security_manager_(NULL) {}
  virtual ~HttpAuthHandlerFactory() {}

  // Sets an URL security manager.  HttpAuthHandlerFactory doesn't own the URL
  // security manager, and the URL security manager should outlive this object.
  void set_url_security_manager(URLSecurityManager* url_security_manager) {
    url_security_manager_ = url_security_manager;
  }

  // Retrieves the associated URL security manager.
  URLSecurityManager* url_security_manager() {
    return url_security_manager_;
  }

  // Creates an HttpAuthHandler object based on the authentication
  // challenge specified by |*challenge|. |challenge| must point to a valid
  // non-NULL tokenizer.
  //
  // If an HttpAuthHandler object is successfully created it is passed back to
  // the caller through |*handler| and OK is returned.
  //
  // If |*challenge| specifies an unsupported authentication scheme, |*handler|
  // is set to NULL and ERR_UNSUPPORTED_AUTH_SCHEME is returned.
  //
  // If |*challenge| is improperly formed, |*handler| is set to NULL and
  // ERR_INVALID_RESPONSE is returned.
  //
  // |create_reason| indicates why the handler is being created. This is used
  // since NTLM and Negotiate schemes do not support preemptive creation.
  //
  // |digest_nonce_count| is specifically intended for the Digest authentication
  // scheme, and indicates the number of handlers generated for a particular
  // server nonce challenge.
  //
  // For the NTLM and Negotiate handlers:
  // If |origin| does not match the authentication method's filters for
  // the specified |target|, ERR_INVALID_AUTH_CREDENTIALS is returned.
  // NOTE: This will apply to ALL |origin| values if the filters are empty.
  //
  // |*challenge| should not be reused after a call to |CreateAuthHandler()|,
  virtual int CreateAuthHandler(HttpAuthChallengeTokenizer* challenge,
                                HttpAuth::Target target,
                                const GURL& origin,
                                CreateReason create_reason,
                                int digest_nonce_count,
                                const BoundNetLog& net_log,
                                scoped_ptr<HttpAuthHandler>* handler) = 0;

  // Creates an HTTP authentication handler based on the authentication
  // challenge string |challenge|.
  // This is a convenience function which creates a ChallengeTokenizer for
  // |challenge| and calls |CreateAuthHandler|. See |CreateAuthHandler| for
  // more details on return values.
  int CreateAuthHandlerFromString(const std::string& challenge,
                                  HttpAuth::Target target,
                                  const GURL& origin,
                                  const BoundNetLog& net_log,
                                  scoped_ptr<HttpAuthHandler>* handler);

  // Creates an HTTP authentication handler based on the authentication
  // challenge string |challenge|.
  // This is a convenience function which creates a ChallengeTokenizer for
  // |challenge| and calls |CreateAuthHandler|. See |CreateAuthHandler| for
  // more details on return values.
  int CreatePreemptiveAuthHandlerFromString(
      const std::string& challenge,
      HttpAuth::Target target,
      const GURL& origin,
      int digest_nonce_count,
      const BoundNetLog& net_log,
      scoped_ptr<HttpAuthHandler>* handler);

  // Creates a standard HttpAuthHandlerRegistryFactory. The caller is
  // responsible for deleting the factory.
  // The default factory supports Basic, Digest, NTLM, and Negotiate schemes.
  //
  // |resolver| is used by the Negotiate authentication handler to perform
  // CNAME lookups to generate a Kerberos SPN for the server. It must be
  // non-NULL.  |resolver| must remain valid for the lifetime of the
  // HttpAuthHandlerRegistryFactory and any HttpAuthHandlers created by said
  // factory.
  static HttpAuthHandlerRegistryFactory* CreateDefault(HostResolver* resolver);

 private:
  // The URL security manager
  URLSecurityManager* url_security_manager_;

  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandlerFactory);
};

// The HttpAuthHandlerRegistryFactory dispatches create requests out
// to other factories based on the auth scheme.
class NET_EXPORT HttpAuthHandlerRegistryFactory
    : public HttpAuthHandlerFactory {
 public:
  HttpAuthHandlerRegistryFactory();
  virtual ~HttpAuthHandlerRegistryFactory();

  // Sets an URL security manager into the factory associated with |scheme|.
  void SetURLSecurityManager(const std::string& scheme,
                             URLSecurityManager* url_security_manager);

  // Registers a |factory| that will be used for a particular HTTP
  // authentication scheme such as Basic, Digest, or Negotiate.
  // The |*factory| object is assumed to be new-allocated, and its lifetime
  // will be managed by this HttpAuthHandlerRegistryFactory object (including
  // deleting it when it is no longer used.
  // A NULL |factory| value means that HttpAuthHandlers's will not be created
  // for |scheme|. If a factory object used to exist for |scheme|, it will be
  // deleted.
  void RegisterSchemeFactory(const std::string& scheme,
                             HttpAuthHandlerFactory* factory);

  // Retrieve the factory for the specified |scheme|. If no factory exists
  // for the |scheme|, NULL is returned. The returned factory must not be
  // deleted by the caller, and it is guaranteed to be valid until either
  // a new factory is registered for the same scheme, or until this
  // registry factory is destroyed.
  HttpAuthHandlerFactory* GetSchemeFactory(const std::string& scheme) const;

  // Creates an HttpAuthHandlerRegistryFactory.
  //
  // |supported_schemes| is a list of authentication schemes. Valid values
  // include "basic", "digest", "ntlm", and "negotiate", where case matters.
  //
  // |security_manager| is used by the NTLM and Negotiate authenticators
  // to determine which servers Integrated Authentication can be used with. If
  // NULL, Integrated Authentication will not be used with any server.
  //
  // |host_resolver| is used by the Negotiate authentication handler to perform
  // CNAME lookups to generate a Kerberos SPN for the server. If the "negotiate"
  // scheme is used and |negotiate_disable_cname_lookup| is false,
  // |host_resolver| must not be NULL.
  //
  // |gssapi_library_name| specifies the name of the GSSAPI library that will
  // be loaded on all platforms except Windows.
  //
  // |negotiate_disable_cname_lookup| and |negotiate_enable_port| both control
  // how Negotiate does SPN generation, by default these should be false.
  static HttpAuthHandlerRegistryFactory* Create(
      const std::vector<std::string>& supported_schemes,
      URLSecurityManager* security_manager,
      HostResolver* host_resolver,
      const std::string& gssapi_library_name,
      bool negotiate_disable_cname_lookup,
      bool negotiate_enable_port);

  // Creates an auth handler by dispatching out to the registered factories
  // based on the first token in |challenge|.
  virtual int CreateAuthHandler(HttpAuthChallengeTokenizer* challenge,
                                HttpAuth::Target target,
                                const GURL& origin,
                                CreateReason reason,
                                int digest_nonce_count,
                                const BoundNetLog& net_log,
                                scoped_ptr<HttpAuthHandler>* handler) OVERRIDE;

 private:
  typedef std::map<std::string, HttpAuthHandlerFactory*> FactoryMap;

  FactoryMap factory_map_;
  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandlerRegistryFactory);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H_
