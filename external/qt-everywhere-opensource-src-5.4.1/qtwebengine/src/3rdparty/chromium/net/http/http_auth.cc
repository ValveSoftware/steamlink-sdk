// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

HttpAuth::Identity::Identity() : source(IDENT_SRC_NONE), invalid(true) {}

// static
void HttpAuth::ChooseBestChallenge(
    HttpAuthHandlerFactory* http_auth_handler_factory,
    const HttpResponseHeaders* headers,
    Target target,
    const GURL& origin,
    const std::set<Scheme>& disabled_schemes,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  DCHECK(http_auth_handler_factory);
  DCHECK(handler->get() == NULL);

  // Choose the challenge whose authentication handler gives the maximum score.
  scoped_ptr<HttpAuthHandler> best;
  const std::string header_name = GetChallengeHeaderName(target);
  std::string cur_challenge;
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, header_name, &cur_challenge)) {
    scoped_ptr<HttpAuthHandler> cur;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        cur_challenge, target, origin, net_log, &cur);
    if (rv != OK) {
      VLOG(1) << "Unable to create AuthHandler. Status: "
              << ErrorToString(rv) << " Challenge: " << cur_challenge;
      continue;
    }
    if (cur.get() && (!best.get() || best->score() < cur->score()) &&
        (disabled_schemes.find(cur->auth_scheme()) == disabled_schemes.end()))
      best.swap(cur);
  }
  handler->swap(best);
}

// static
HttpAuth::AuthorizationResult HttpAuth::HandleChallengeResponse(
    HttpAuthHandler* handler,
    const HttpResponseHeaders* headers,
    Target target,
    const std::set<Scheme>& disabled_schemes,
    std::string* challenge_used) {
  DCHECK(handler);
  DCHECK(headers);
  DCHECK(challenge_used);
  challenge_used->clear();
  HttpAuth::Scheme current_scheme = handler->auth_scheme();
  if (disabled_schemes.find(current_scheme) != disabled_schemes.end())
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  std::string current_scheme_name = SchemeToString(current_scheme);
  const std::string header_name = GetChallengeHeaderName(target);
  void* iter = NULL;
  std::string challenge;
  HttpAuth::AuthorizationResult authorization_result =
      HttpAuth::AUTHORIZATION_RESULT_INVALID;
  while (headers->EnumerateHeader(&iter, header_name, &challenge)) {
    HttpAuthChallengeTokenizer props(challenge.begin(), challenge.end());
    if (!LowerCaseEqualsASCII(props.scheme(), current_scheme_name.c_str()))
      continue;
    authorization_result = handler->HandleAnotherChallenge(&props);
    if (authorization_result != HttpAuth::AUTHORIZATION_RESULT_INVALID) {
      *challenge_used = challenge;
      return authorization_result;
    }
  }
  // Finding no matches is equivalent to rejection.
  return HttpAuth::AUTHORIZATION_RESULT_REJECT;
}

// static
std::string HttpAuth::GetChallengeHeaderName(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return "Proxy-Authenticate";
    case AUTH_SERVER:
      return "WWW-Authenticate";
    default:
      NOTREACHED();
      return std::string();
  }
}

// static
std::string HttpAuth::GetAuthorizationHeaderName(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return HttpRequestHeaders::kProxyAuthorization;
    case AUTH_SERVER:
      return HttpRequestHeaders::kAuthorization;
    default:
      NOTREACHED();
      return std::string();
  }
}

// static
std::string HttpAuth::GetAuthTargetString(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return "proxy";
    case AUTH_SERVER:
      return "server";
    default:
      NOTREACHED();
      return std::string();
  }
}

// static
const char* HttpAuth::SchemeToString(Scheme scheme) {
  static const char* const kSchemeNames[] = {
    "basic",
    "digest",
    "ntlm",
    "negotiate",
    "spdyproxy",
    "mock",
  };
  COMPILE_ASSERT(arraysize(kSchemeNames) == AUTH_SCHEME_MAX,
                 http_auth_scheme_names_incorrect_size);
  if (scheme < AUTH_SCHEME_BASIC || scheme >= AUTH_SCHEME_MAX) {
    NOTREACHED();
    return "invalid_scheme";
  }
  return kSchemeNames[scheme];
}

}  // namespace net
