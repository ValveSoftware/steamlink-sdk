// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpAuthHandlerBasicTest, GenerateAuthToken) {
  static const struct {
    const char* username;
    const char* password;
    const char* expected_credentials;
  } tests[] = {
    { "foo", "bar", "Basic Zm9vOmJhcg==" },
    // Empty username
    { "", "foobar", "Basic OmZvb2Jhcg==" },
    // Empty password
    { "anon", "", "Basic YW5vbjo=" },
    // Empty username and empty password.
    { "", "", "Basic Og==" },
  };
  GURL origin("http://www.example.com");
  HttpAuthHandlerBasic::Factory factory;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge = "Basic realm=\"Atlantis\"";
    scoped_ptr<HttpAuthHandler> basic;
    EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
        challenge, HttpAuth::AUTH_SERVER, origin, BoundNetLog(), &basic));
    AuthCredentials credentials(base::ASCIIToUTF16(tests[i].username),
                                base::ASCIIToUTF16(tests[i].password));
    HttpRequestInfo request_info;
    std::string auth_token;
    int rv = basic->GenerateAuthToken(&credentials, &request_info,
                                      CompletionCallback(), &auth_token);
    EXPECT_EQ(OK, rv);
    EXPECT_STREQ(tests[i].expected_credentials, auth_token.c_str());
  }
}

TEST(HttpAuthHandlerBasicTest, HandleAnotherChallenge) {
  static const struct {
    const char* challenge;
    HttpAuth::AuthorizationResult expected_rv;
  } tests[] = {
    // The handler is initialized using this challenge.  The first
    // time HandleAnotherChallenge is called with it should cause it
    // to treat the second challenge as a rejection since it is for
    // the same realm.
    {
      "Basic realm=\"First\"",
      HttpAuth::AUTHORIZATION_RESULT_REJECT
    },

    // A challenge for a different realm.
    {
      "Basic realm=\"Second\"",
      HttpAuth::AUTHORIZATION_RESULT_DIFFERENT_REALM
    },

    // Although RFC 2617 isn't explicit about this case, if there is
    // more than one realm directive, we pick the last one.  So this
    // challenge should be treated as being for "First" realm.
    {
      "Basic realm=\"Second\",realm=\"First\"",
      HttpAuth::AUTHORIZATION_RESULT_REJECT
    },

    // And this one should be treated as if it was for "Second."
    {
      "basic realm=\"First\",realm=\"Second\"",
      HttpAuth::AUTHORIZATION_RESULT_DIFFERENT_REALM
    }
  };

  GURL origin("http://www.example.com");
  HttpAuthHandlerBasic::Factory factory;
  scoped_ptr<HttpAuthHandler> basic;
  EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
      tests[0].challenge, HttpAuth::AUTH_SERVER, origin,
      BoundNetLog(), &basic));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge(tests[i].challenge);
    HttpAuthChallengeTokenizer tok(challenge.begin(),
                                   challenge.end());
    EXPECT_EQ(tests[i].expected_rv, basic->HandleAnotherChallenge(&tok));
  }
}

TEST(HttpAuthHandlerBasicTest, InitFromChallenge) {
  static const struct {
    const char* challenge;
    int expected_rv;
    const char* expected_realm;
  } tests[] = {
    // No realm (we allow this even though realm is supposed to be required
    // according to RFC 2617.)
    {
      "Basic",
      OK,
      "",
    },

    // Realm is empty string.
    {
      "Basic realm=\"\"",
      OK,
      "",
    },

    // Realm is valid.
    {
      "Basic realm=\"test_realm\"",
      OK,
      "test_realm",
    },

    // The parser ignores tokens which aren't known.
    {
      "Basic realm=\"test_realm\",unknown_token=foobar",
      OK,
      "test_realm",
    },

    // The parser skips over tokens which aren't known.
    {
      "Basic unknown_token=foobar,realm=\"test_realm\"",
      OK,
      "test_realm",
    },

#if 0
    // TODO(cbentzel): It's unclear what the parser should do in these cases.
    //                 It seems like this should either be treated as invalid,
    //                 or the spaces should be used as a separator.
    {
      "Basic realm=\"test_realm\" unknown_token=foobar",
      OK,
      "test_realm",
    },

    // The parser skips over tokens which aren't known.
    {
      "Basic unknown_token=foobar realm=\"test_realm\"",
      OK,
      "test_realm",
    },
#endif

    // The parser fails when the first token is not "Basic".
    {
      "Negotiate",
      ERR_INVALID_RESPONSE,
      ""
    },

    // Although RFC 2617 isn't explicit about this case, if there is
    // more than one realm directive, we pick the last one.
    {
      "Basic realm=\"foo\",realm=\"bar\"",
      OK,
      "bar",
    },

    // Handle ISO-8859-1 character as part of the realm. The realm is converted
    // to UTF-8.
    {
      "Basic realm=\"foo-\xE5\"",
      OK,
      "foo-\xC3\xA5",
    },
  };
  HttpAuthHandlerBasic::Factory factory;
  GURL origin("http://www.example.com");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge = tests[i].challenge;
    scoped_ptr<HttpAuthHandler> basic;
    int rv = factory.CreateAuthHandlerFromString(
        challenge, HttpAuth::AUTH_SERVER, origin, BoundNetLog(), &basic);
    EXPECT_EQ(tests[i].expected_rv, rv);
    if (rv == OK)
      EXPECT_EQ(tests[i].expected_realm, basic->realm());
  }
}

}  // namespace net
