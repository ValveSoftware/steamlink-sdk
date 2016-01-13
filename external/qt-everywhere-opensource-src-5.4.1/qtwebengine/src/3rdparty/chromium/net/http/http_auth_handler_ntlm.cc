// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_ntlm.h"

#if !defined(NTLM_SSPI)
#include "base/base64.h"
#endif
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_challenge_tokenizer.h"

namespace net {

HttpAuth::AuthorizationResult HttpAuthHandlerNTLM::HandleAnotherChallenge(
    HttpAuthChallengeTokenizer* challenge) {
  return ParseChallenge(challenge, false);
}

bool HttpAuthHandlerNTLM::Init(HttpAuthChallengeTokenizer* tok) {
  auth_scheme_ = HttpAuth::AUTH_SCHEME_NTLM;
  score_ = 3;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;

  return ParseChallenge(tok, true) == HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
}

int HttpAuthHandlerNTLM::GenerateAuthTokenImpl(
    const AuthCredentials* credentials, const HttpRequestInfo* request,
    const CompletionCallback& callback, std::string* auth_token) {
#if defined(NTLM_SSPI)
  return auth_sspi_.GenerateAuthToken(
      credentials,
      CreateSPN(origin_),
      auth_token);
#else  // !defined(NTLM_SSPI)
  // TODO(cbentzel): Shouldn't be hitting this case.
  if (!credentials) {
    LOG(ERROR) << "Username and password are expected to be non-NULL.";
    return ERR_MISSING_AUTH_CREDENTIALS;
  }
  // TODO(wtc): See if we can use char* instead of void* for in_buf and
  // out_buf.  This change will need to propagate to GetNextToken,
  // GenerateType1Msg, and GenerateType3Msg, and perhaps further.
  const void* in_buf;
  void* out_buf;
  uint32 in_buf_len, out_buf_len;
  std::string decoded_auth_data;

  // The username may be in the form "DOMAIN\user".  Parse it into the two
  // components.
  base::string16 domain;
  base::string16 user;
  const base::string16& username = credentials->username();
  const base::char16 backslash_character = '\\';
  size_t backslash_idx = username.find(backslash_character);
  if (backslash_idx == base::string16::npos) {
    user = username;
  } else {
    domain = username.substr(0, backslash_idx);
    user = username.substr(backslash_idx + 1);
  }
  domain_ = domain;
  credentials_.Set(user, credentials->password());

  // Initial challenge.
  if (auth_data_.empty()) {
    in_buf_len = 0;
    in_buf = NULL;
    int rv = InitializeBeforeFirstChallenge();
    if (rv != OK)
      return rv;
  } else {
    if (!base::Base64Decode(auth_data_, &decoded_auth_data)) {
      LOG(ERROR) << "Unexpected problem Base64 decoding.";
      return ERR_UNEXPECTED;
    }
    in_buf_len = decoded_auth_data.length();
    in_buf = decoded_auth_data.data();
  }

  int rv = GetNextToken(in_buf, in_buf_len, &out_buf, &out_buf_len);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend "NTLM ".
  std::string encode_input(static_cast<char*>(out_buf), out_buf_len);
  std::string encode_output;
  base::Base64Encode(encode_input, &encode_output);
  // OK, we are done with |out_buf|
  free(out_buf);
  *auth_token = std::string("NTLM ") + encode_output;
  return OK;
#endif
}

// The NTLM challenge header looks like:
//   WWW-Authenticate: NTLM auth-data
HttpAuth::AuthorizationResult HttpAuthHandlerNTLM::ParseChallenge(
    HttpAuthChallengeTokenizer* tok, bool initial_challenge) {
#if defined(NTLM_SSPI)
  // auth_sspi_ contains state for whether or not this is the initial challenge.
  return auth_sspi_.ParseChallenge(tok);
#else
  // TODO(cbentzel): Most of the logic between SSPI, GSSAPI, and portable NTLM
  // authentication parsing could probably be shared - just need to know if
  // there was previously a challenge round.
  // TODO(cbentzel): Write a test case to validate that auth_data_ is left empty
  // in all failure conditions.
  auth_data_.clear();

  // Verify the challenge's auth-scheme.
  if (!LowerCaseEqualsASCII(tok->scheme(), "ntlm"))
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;

  std::string base64_param = tok->base64_param();
  if (base64_param.empty()) {
    if (!initial_challenge)
      return HttpAuth::AUTHORIZATION_RESULT_REJECT;
    return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
  } else {
    if (initial_challenge)
      return HttpAuth::AUTHORIZATION_RESULT_INVALID;
  }

  auth_data_ = base64_param;
  return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
#endif  // defined(NTLM_SSPI)
}

// static
std::string HttpAuthHandlerNTLM::CreateSPN(const GURL& origin) {
  // The service principal name of the destination server.  See
  // http://msdn.microsoft.com/en-us/library/ms677949%28VS.85%29.aspx
  std::string target("HTTP/");
  target.append(GetHostAndPort(origin));
  return target;
}

}  // namespace net
