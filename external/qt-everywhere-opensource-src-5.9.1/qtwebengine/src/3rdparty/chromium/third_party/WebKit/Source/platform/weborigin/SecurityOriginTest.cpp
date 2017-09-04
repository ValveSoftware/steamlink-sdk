/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/weborigin/SecurityOrigin.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/blob/BlobURL.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/weborigin/Suborigin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

const int MaxAllowedPort = 65535;

class SecurityOriginTest : public ::testing::Test {
 public:
  void SetUp() override {
    url::AddStandardScheme("http-so", url::SCHEME_WITH_PORT);
    url::AddStandardScheme("https-so", url::SCHEME_WITH_PORT);
  }
};

TEST_F(SecurityOriginTest, InvalidPortsCreateUniqueOrigins) {
  int ports[] = {-100, -1, MaxAllowedPort + 1, 1000000};

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(ports); ++i) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::create("http", "example.com", ports[i]);
    EXPECT_TRUE(origin->isUnique())
        << "Port " << ports[i] << " should have generated a unique origin.";
  }
}

TEST_F(SecurityOriginTest, ValidPortsCreateNonUniqueOrigins) {
  int ports[] = {0, 80, 443, 5000, MaxAllowedPort};

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(ports); ++i) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::create("http", "example.com", ports[i]);
    EXPECT_FALSE(origin->isUnique())
        << "Port " << ports[i] << " should not have generated a unique origin.";
  }
}

TEST_F(SecurityOriginTest, LocalAccess) {
  RefPtr<SecurityOrigin> file1 =
      SecurityOrigin::createFromString("file:///etc/passwd");
  RefPtr<SecurityOrigin> file2 =
      SecurityOrigin::createFromString("file:///etc/shadow");

  EXPECT_TRUE(file1->isSameSchemeHostPort(file1.get()));
  EXPECT_TRUE(file1->isSameSchemeHostPort(file2.get()));
  EXPECT_TRUE(file2->isSameSchemeHostPort(file1.get()));

  EXPECT_TRUE(file1->canAccess(file1.get()));
  EXPECT_TRUE(file1->canAccess(file2.get()));
  EXPECT_TRUE(file2->canAccess(file1.get()));

  // Block |file1|'s access to local origins. It should now be same-origin
  // with itself, but shouldn't have access to |file2|.
  file1->blockLocalAccessFromLocalOrigin();
  EXPECT_TRUE(file1->isSameSchemeHostPort(file1.get()));
  EXPECT_FALSE(file1->isSameSchemeHostPort(file2.get()));
  EXPECT_FALSE(file2->isSameSchemeHostPort(file1.get()));

  EXPECT_TRUE(file1->canAccess(file1.get()));
  EXPECT_FALSE(file1->canAccess(file2.get()));
  EXPECT_FALSE(file2->canAccess(file1.get()));
}

TEST_F(SecurityOriginTest, IsPotentiallyTrustworthy) {
  struct TestCase {
    bool accessGranted;
    const char* url;
  };

  TestCase inputs[] = {
      // Access is granted to webservers running on localhost.
      {true, "http://localhost"},
      {true, "http://LOCALHOST"},
      {true, "http://localhost:100"},
      {true, "http://127.0.0.1"},
      {true, "http://127.0.0.2"},
      {true, "http://127.1.0.2"},
      {true, "http://0177.00.00.01"},
      {true, "http://[::1]"},
      {true, "http://[0:0::1]"},
      {true, "http://[0:0:0:0:0:0:0:1]"},
      {true, "http://[::1]:21"},
      {true, "http://127.0.0.1:8080"},
      {true, "ftp://127.0.0.1"},
      {true, "ftp://127.0.0.1:443"},
      {true, "ws://127.0.0.1"},

      // Access is denied to non-localhost over HTTP
      {false, "http://[1::]"},
      {false, "http://[::2]"},
      {false, "http://[1::1]"},
      {false, "http://[1:2::3]"},
      {false, "http://[::127.0.0.1]"},
      {false, "http://a.127.0.0.1"},
      {false, "http://127.0.0.1.b"},
      {false, "http://localhost.a"},
      {false, "http://a.localhost"},

      // Access is granted to all secure transports.
      {true, "https://foobar.com"},
      {true, "wss://foobar.com"},

      // Access is denied to insecure transports.
      {false, "ftp://foobar.com"},
      {false, "http://foobar.com"},
      {false, "http://foobar.com:443"},
      {false, "ws://foobar.com"},

      // Access is granted to local files
      {true, "file:///home/foobar/index.html"},

      // blob: URLs must look to the inner URL's origin, and apply the same
      // rules as above. Spot check some of them
      {true, "blob:http://localhost:1000/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {true, "blob:https://foopy:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:http://baz:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a"},

      // filesystem: URLs work the same as blob: URLs, and look to the inner
      // URL for security origin.
      {true, "filesystem:http://localhost:1000/foo"},
      {true, "filesystem:https://foopy:99/foo"},
      {false, "filesystem:http://baz:99/foo"},
      {false, "filesystem:ftp://evil:99/foo"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(inputs); ++i) {
    SCOPED_TRACE(i);
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::createFromString(inputs[i].url);
    String errorMessage;
    EXPECT_EQ(inputs[i].accessGranted, origin->isPotentiallyTrustworthy());
  }

  // Unique origins are not considered secure.
  RefPtr<SecurityOrigin> uniqueOrigin = SecurityOrigin::createUnique();
  EXPECT_FALSE(uniqueOrigin->isPotentiallyTrustworthy());
  // ... unless they are specially marked as such.
  uniqueOrigin->setUniqueOriginIsPotentiallyTrustworthy(true);
  EXPECT_TRUE(uniqueOrigin->isPotentiallyTrustworthy());
  uniqueOrigin->setUniqueOriginIsPotentiallyTrustworthy(false);
  EXPECT_FALSE(uniqueOrigin->isPotentiallyTrustworthy());
}

TEST_F(SecurityOriginTest, IsSecure) {
  struct TestCase {
    bool isSecure;
    const char* url;
  } inputs[] = {
      {false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:http://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "file:///etc/passwd"},
      {false, "ftp://example.com/"},
      {false, "http://example.com/"},
      {false, "ws://example.com/"},
      {true, "blob:https://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {true, "https://example.com/"},
      {true, "wss://example.com/"},

      {true, "about:blank"},
      {false, ""},
      {false, "\0"},
  };

  for (auto test : inputs)
    EXPECT_EQ(test.isSecure,
              SecurityOrigin::isSecure(KURL(ParsedURLString, test.url)))
        << "URL: '" << test.url << "'";

  EXPECT_FALSE(SecurityOrigin::isSecure(KURL()));
}

TEST_F(SecurityOriginTest, IsSecureViaTrustworthy) {
  const char* urls[] = {"http://localhost/", "http://localhost:8080/",
                        "http://127.0.0.1/", "http://127.0.0.1:8080/",
                        "http://[::1]/"};

  for (const char* test : urls) {
    KURL url(ParsedURLString, test);
    EXPECT_FALSE(SecurityOrigin::isSecure(url));
    SecurityPolicy::addOriginTrustworthyWhiteList(SecurityOrigin::create(url));
    EXPECT_TRUE(SecurityOrigin::isSecure(url));
  }
}

TEST_F(SecurityOriginTest, Suborigins) {
  RuntimeEnabledFeatures::setSuboriginsEnabled(true);

  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::createFromString("https://test.com");
  Suborigin suborigin;
  suborigin.setName("foobar");
  EXPECT_FALSE(origin->hasSuborigin());
  origin->addSuborigin(suborigin);
  EXPECT_TRUE(origin->hasSuborigin());
  EXPECT_EQ("foobar", origin->suborigin()->name());

  origin = SecurityOrigin::createFromString("https-so://foobar.test.com");
  EXPECT_EQ("https", origin->protocol());
  EXPECT_EQ("test.com", origin->host());
  EXPECT_EQ("foobar", origin->suborigin()->name());

  origin = SecurityOrigin::createFromString("https-so://foobar.test.com");
  EXPECT_TRUE(origin->hasSuborigin());
  EXPECT_EQ("foobar", origin->suborigin()->name());

  origin = SecurityOrigin::createFromString("https://foobar+test.com");
  EXPECT_FALSE(origin->hasSuborigin());

  origin = SecurityOrigin::createFromString("https.so://foobar+test.com");
  EXPECT_FALSE(origin->hasSuborigin());

  origin = SecurityOrigin::createFromString("https://_test.com");
  EXPECT_FALSE(origin->hasSuborigin());

  origin = SecurityOrigin::createFromString("https-so://_test.com");
  EXPECT_TRUE(origin->hasSuborigin());
  EXPECT_EQ("_test", origin->suborigin()->name());

  origin = SecurityOrigin::createFromString("https-so-so://foobar.test.com");
  EXPECT_FALSE(origin->hasSuborigin());

  origin = adoptRef<SecurityOrigin>(new SecurityOrigin);
  EXPECT_FALSE(origin->hasSuborigin());

  origin = SecurityOrigin::createFromString("https-so://foobar.test.com");
  Suborigin emptySuborigin;
  EXPECT_DEATH(origin->addSuborigin(emptySuborigin), "");
}

TEST_F(SecurityOriginTest, SuboriginsParsing) {
  RuntimeEnabledFeatures::setSuboriginsEnabled(true);
  String protocol, realProtocol, host, realHost, suborigin;
  protocol = "https";
  host = "test.com";
  EXPECT_FALSE(SecurityOrigin::deserializeSuboriginAndProtocolAndHost(
      protocol, host, suborigin, realProtocol, realHost));

  protocol = "https-so";
  host = "foobar.test.com";
  EXPECT_TRUE(SecurityOrigin::deserializeSuboriginAndProtocolAndHost(
      protocol, host, suborigin, realProtocol, realHost));
  EXPECT_EQ("https", realProtocol);
  EXPECT_EQ("test.com", realHost);
  EXPECT_EQ("foobar", suborigin);

  RefPtr<SecurityOrigin> origin;
  StringBuilder builder;

  origin = SecurityOrigin::createFromString("https-so://foobar.test.com");
  origin->buildRawString(builder, true);
  EXPECT_EQ("https-so://foobar.test.com", builder.toString());
  EXPECT_EQ("https-so://foobar.test.com", origin->toString());
  builder.clear();
  origin->buildRawString(builder, false);
  EXPECT_EQ("https://test.com", builder.toString());
  EXPECT_EQ("https://test.com", origin->toPhysicalOriginString());

  Suborigin suboriginObj;
  suboriginObj.setName("foobar");
  builder.clear();
  origin = SecurityOrigin::createFromString("https://test.com");
  origin->addSuborigin(suboriginObj);
  origin->buildRawString(builder, true);
  EXPECT_EQ("https-so://foobar.test.com", builder.toString());
  EXPECT_EQ("https-so://foobar.test.com", origin->toString());
  builder.clear();
  origin->buildRawString(builder, false);
  EXPECT_EQ("https://test.com", builder.toString());
  EXPECT_EQ("https://test.com", origin->toPhysicalOriginString());
}

TEST_F(SecurityOriginTest, SuboriginsIsSameSchemeHostPortAndSuborigin) {
  blink::RuntimeEnabledFeatures::setSuboriginsEnabled(true);
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::createFromString("https-so://foobar.test.com");
  RefPtr<SecurityOrigin> other1 =
      SecurityOrigin::createFromString("https-so://bazbar.test.com");
  RefPtr<SecurityOrigin> other2 =
      SecurityOrigin::createFromString("http-so://foobar.test.com");
  RefPtr<SecurityOrigin> other3 =
      SecurityOrigin::createFromString("https-so://foobar.test.com:1234");
  RefPtr<SecurityOrigin> other4 =
      SecurityOrigin::createFromString("https://test.com");

  EXPECT_TRUE(origin->isSameSchemeHostPortAndSuborigin(origin.get()));
  EXPECT_FALSE(origin->isSameSchemeHostPortAndSuborigin(other1.get()));
  EXPECT_FALSE(origin->isSameSchemeHostPortAndSuborigin(other2.get()));
  EXPECT_FALSE(origin->isSameSchemeHostPortAndSuborigin(other3.get()));
  EXPECT_FALSE(origin->isSameSchemeHostPortAndSuborigin(other4.get()));
}

TEST_F(SecurityOriginTest, CanAccess) {
  RuntimeEnabledFeatures::setSuboriginsEnabled(true);

  struct TestCase {
    bool canAccess;
    bool canAccessCheckSuborigins;
    const char* origin1;
    const char* origin2;
  };

  TestCase tests[] = {
      {true, true, "https://foobar.com", "https://foobar.com"},
      {false, false, "https://foobar.com", "https://bazbar.com"},
      {true, false, "https://foobar.com", "https-so://name.foobar.com"},
      {true, false, "https-so://name.foobar.com", "https://foobar.com"},
      {true, true, "https-so://name.foobar.com", "https-so://name.foobar.com"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(tests); ++i) {
    RefPtr<SecurityOrigin> origin1 =
        SecurityOrigin::createFromString(tests[i].origin1);
    RefPtr<SecurityOrigin> origin2 =
        SecurityOrigin::createFromString(tests[i].origin2);
    EXPECT_EQ(tests[i].canAccess, origin1->canAccess(origin2.get()));
    EXPECT_EQ(tests[i].canAccessCheckSuborigins,
              origin1->canAccessCheckSuborigins(origin2.get()));
  }
}

TEST_F(SecurityOriginTest, CanRequest) {
  RuntimeEnabledFeatures::setSuboriginsEnabled(true);

  struct TestCase {
    bool canRequest;
    bool canRequestNoSuborigin;
    const char* origin;
    const char* url;
  };

  TestCase tests[] = {
      {true, true, "https://foobar.com", "https://foobar.com"},
      {false, false, "https://foobar.com", "https://bazbar.com"},
      {true, false, "https-so://name.foobar.com", "https://foobar.com"},
      {false, false, "https-so://name.foobar.com", "https://bazbar.com"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(tests); ++i) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::createFromString(tests[i].origin);
    blink::KURL url(blink::ParsedURLString, tests[i].url);
    EXPECT_EQ(tests[i].canRequest, origin->canRequest(url));
    EXPECT_EQ(tests[i].canRequestNoSuborigin,
              origin->canRequestNoSuborigin(url));
  }
}

TEST_F(SecurityOriginTest, EffectivePort) {
  struct TestCase {
    unsigned short port;
    unsigned short effectivePort;
    const char* origin;
  } cases[] = {
      {0, 80, "http://example.com"},
      {0, 80, "http://example.com:80"},
      {81, 81, "http://example.com:81"},
      {0, 443, "https://example.com"},
      {0, 443, "https://example.com:443"},
      {444, 444, "https://example.com:444"},
  };

  for (const auto& test : cases) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::createFromString(test.origin);
    EXPECT_EQ(test.port, origin->port());
    EXPECT_EQ(test.effectivePort, origin->effectivePort());
  }
}

TEST_F(SecurityOriginTest, CreateFromTuple) {
  struct TestCase {
    const char* scheme;
    const char* host;
    unsigned short port;
    const char* origin;
  } cases[] = {
      {"http", "example.com", 80, "http://example.com"},
      {"http", "example.com", 81, "http://example.com:81"},
      {"https", "example.com", 443, "https://example.com"},
      {"https", "example.com", 444, "https://example.com:444"},
      {"file", "", 0, "file://"},
      {"file", "example.com", 0, "file://"},
  };

  for (const auto& test : cases) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::create(test.scheme, test.host, test.port);
    EXPECT_EQ(test.origin, origin->toString()) << test.origin;
  }
}

TEST_F(SecurityOriginTest, CreateFromTupleWithSuborigin) {
  struct TestCase {
    const char* scheme;
    const char* host;
    unsigned short port;
    const char* suborigin;
    const char* origin;
  } cases[] = {
      {"http", "example.com", 80, "", "http://example.com"},
      {"http", "example.com", 81, "", "http://example.com:81"},
      {"https", "example.com", 443, "", "https://example.com"},
      {"https", "example.com", 444, "", "https://example.com:444"},
      {"file", "", 0, "", "file://"},
      {"file", "example.com", 0, "", "file://"},
      {"http", "example.com", 80, "foobar", "http-so://foobar.example.com"},
      {"http", "example.com", 81, "foobar", "http-so://foobar.example.com:81"},
      {"https", "example.com", 443, "foobar", "https-so://foobar.example.com"},
      {"https", "example.com", 444, "foobar",
       "https-so://foobar.example.com:444"},
      {"file", "", 0, "foobar", "file://"},
      {"file", "example.com", 0, "foobar", "file://"},
  };

  for (const auto& test : cases) {
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(
        test.scheme, test.host, test.port, test.suborigin);
    EXPECT_EQ(test.origin, origin->toString()) << test.origin;
  }
}

TEST_F(SecurityOriginTest, UniquenessPropagatesToBlobUrls) {
  struct TestCase {
    const char* url;
    bool expectedUniqueness;
    const char* expectedOriginString;
  } cases[]{
      {"", true, "null"},
      {"null", true, "null"},
      {"data:text/plain,hello_world", true, "null"},
      {"file:///path", false, "file://"},
      {"filesystem:http://host/filesystem-path", false, "http://host"},
      {"filesystem:file:///filesystem-path", false, "file://"},
      {"filesystem:null/filesystem-path", true, "null"},
      {"blob:http://host/blob-id", false, "http://host"},
      {"blob:file:///blob-id", false, "file://"},
      {"blob:null/blob-id", true, "null"},
  };

  for (const TestCase& test : cases) {
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString(test.url);
    EXPECT_EQ(test.expectedUniqueness, origin->isUnique());
    EXPECT_EQ(test.expectedOriginString, origin->toString());

    KURL blobUrl = BlobURL::createPublicURL(origin.get());
    RefPtr<SecurityOrigin> blobUrlOrigin = SecurityOrigin::create(blobUrl);
    EXPECT_EQ(blobUrlOrigin->isUnique(), origin->isUnique());
    EXPECT_EQ(blobUrlOrigin->toString(), origin->toString());
    EXPECT_EQ(blobUrlOrigin->toRawString(), origin->toRawString());
  }
}

TEST_F(SecurityOriginTest, UniqueOriginIsSameSchemeHostPort) {
  RefPtr<SecurityOrigin> uniqueOrigin = SecurityOrigin::createUnique();
  RefPtr<SecurityOrigin> tupleOrigin =
      SecurityOrigin::createFromString("http://example.com");

  EXPECT_TRUE(uniqueOrigin->isSameSchemeHostPort(uniqueOrigin.get()));
  EXPECT_FALSE(
      SecurityOrigin::createUnique()->isSameSchemeHostPort(uniqueOrigin.get()));
  EXPECT_FALSE(tupleOrigin->isSameSchemeHostPort(uniqueOrigin.get()));
  EXPECT_FALSE(uniqueOrigin->isSameSchemeHostPort(tupleOrigin.get()));
}

TEST_F(SecurityOriginTest, CanonicalizeHost) {
  struct TestCase {
    const char* host;
    const char* canonicalOutput;
    bool expectedSuccess;
  } cases[] = {
      {"", "", true},
      {"example.test", "example.test", true},
      {"EXAMPLE.TEST", "example.test", true},
      {"eXaMpLe.TeSt/path", "example.test%2Fpath", false},
      {",", "%2C", true},
      {"💩", "xn--ls8h", true},
      {"[]", "[]", false},
      {"%yo", "%25yo", false},
  };

  for (const TestCase& test : cases) {
    SCOPED_TRACE(testing::Message() << "raw host: '" << test.host << "'");
    String host = String::fromUTF8(test.host);
    bool success = false;
    String canonicalHost = SecurityOrigin::canonicalizeHost(host, &success);
    EXPECT_EQ(test.canonicalOutput, canonicalHost);
    EXPECT_EQ(test.expectedSuccess, success);
  }
}

}  // namespace blink
