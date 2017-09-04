// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/SourceListDirective.h"

#include "core/dom/Document.h"
#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SourceListDirectiveTest : public ::testing::Test {
 public:
  SourceListDirectiveTest() : csp(ContentSecurityPolicy::create()) {}

 protected:
  struct Source {
    String scheme;
    String host;
    const int port;
    String path;
    CSPSource::WildcardDisposition hostWildcard;
    CSPSource::WildcardDisposition portWildcard;
  };

  virtual void SetUp() {
    KURL secureURL(ParsedURLString, "https://example.test/image.png");
    RefPtr<SecurityOrigin> secureOrigin(SecurityOrigin::create(secureURL));
    document = Document::create();
    document->setSecurityOrigin(secureOrigin);
    csp->bindToExecutionContext(document.get());
  }

  bool equalSources(const Source& a, const Source& b) {
    return a.scheme == b.scheme && a.host == b.host && a.port == b.port &&
           a.path == b.path && a.hostWildcard == b.hostWildcard &&
           a.portWildcard == b.portWildcard;
  }

  Persistent<ContentSecurityPolicy> csp;
  Persistent<Document> document;
};

TEST_F(SourceListDirectiveTest, BasicMatchingNone) {
  KURL base;
  String sources = "'none'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingStrictDynamic) {
  String sources = "'strict-dynamic'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allowDynamic());
}

TEST_F(SourceListDirectiveTest, BasicMatchingUnsafeHashedAttributes) {
  String sources = "'unsafe-hashed-attributes'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allowHashedAttributes());
}

TEST_F(SourceListDirectiveTest, BasicMatchingStar) {
  KURL base;
  String sources = "*";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "ftp://example.com/")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "data:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "blob:https://example.test/")));
  EXPECT_FALSE(
      sourceList.allows(KURL(base, "filesystem:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "file:///etc/hosts")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "applewebdata://example.test/")));
}

TEST_F(SourceListDirectiveTest, StarallowsSelf) {
  KURL base;
  String sources = "*";
  SourceListDirective sourceList("script-src", sources, csp.get());

  // With a protocol of 'file', '*' allows 'file:':
  RefPtr<SecurityOrigin> origin = SecurityOrigin::create("file", "", 0);
  csp->setupSelf(*origin);
  EXPECT_TRUE(sourceList.allows(KURL(base, "file:///etc/hosts")));

  // The other results are the same as above:
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/bar")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "data:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "blob:https://example.test/")));
  EXPECT_FALSE(
      sourceList.allows(KURL(base, "filesystem:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "applewebdata://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingSelf) {
  KURL base;
  String sources = "'self'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://not-example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BlobMatchingSelf) {
  KURL base;
  String sources = "'self'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "blob:https://example.test/")));

  // Register "https" as bypassing CSP, which should trigger the innerURL
  // behavior.
  SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "blob:https://example.test/")));

  // Unregister the scheme to clean up after ourselves.
  SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(SourceListDirectiveTest, BlobMatchingBlob) {
  KURL base;
  String sources = "blob:";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "blob:https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatching) {
  KURL base;
  String sources = "http://example1.com:8000/foo/ https://example2.com/";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example1.com:8000/foo/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example2.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example2.com/foo/")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "https://not-example.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example1.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example1.com/foo")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example1.com:8000/FOO/")));
}

TEST_F(SourceListDirectiveTest, WildcardMatching) {
  KURL base;
  String sources =
      "http://example1.com:*/foo/ https://*.example2.com/bar/ http://*.test/";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://foo.example2.com/bar/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.bar.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example1.com/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example1.com:8000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example1.com:9000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://foo.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://foo.bar.test/")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example1.com:8000/foo")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example2.com:8000/bar")));
  EXPECT_FALSE(
      sourceList.allows(KURL(base, "https://foo.example2.com:8000/bar")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example2.foo.com/bar")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://foo.test.bar/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example2.com/bar/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://test/")));
}

TEST_F(SourceListDirectiveTest, RedirectMatching) {
  KURL base;
  String sources = "http://example1.com/foo/ http://example2.com/bar/";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example1.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example1.com/bar/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example2.com/bar/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example2.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "https://example1.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "https://example1.com/bar/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));

  EXPECT_FALSE(
      sourceList.allows(KURL(base, "http://example3.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
}

TEST_F(SourceListDirectiveTest, GetIntersectCSPSources) {
  KURL base;
  String sources =
      "http://example1.com/foo/ http://*.example2.com/bar/ "
      "http://*.example3.com:*/bar/";
  SourceListDirective sourceList("script-src", sources, csp.get());
  struct TestCase {
    String sources;
    String expected;
  } cases[] = {
      {"http://example1.com/foo/ http://example2.com/bar/",
       "http://example1.com/foo/ http://example2.com/bar/"},
      // Normalizing schemes.
      {"https://example1.com/foo/ http://example2.com/bar/",
       "https://example1.com/foo/ http://example2.com/bar/"},
      {"https://example1.com/foo/ https://example2.com/bar/",
       "https://example1.com/foo/ https://example2.com/bar/"},
      {"https://example1.com/foo/ wss://example2.com/bar/",
       "https://example1.com/foo/"},
      // Normalizing hosts.
      {"http://*.example1.com/foo/ http://*.example2.com/bar/",
       "http://example1.com/foo/ http://*.example2.com/bar/"},
      {"http://*.example1.com/foo/ http://foo.example2.com/bar/",
       "http://example1.com/foo/ http://foo.example2.com/bar/"},
      // Normalizing ports.
      {"http://example1.com:80/foo/ http://example2.com/bar/",
       "http://example1.com:80/foo/ http://example2.com/bar/"},
      {"http://example1.com/foo/ http://example2.com:90/bar/",
       "http://example1.com/foo/"},
      {"http://example1.com:*/foo/ http://example2.com/bar/",
       "http://example1.com/foo/ http://example2.com/bar/"},
      {"http://*.example3.com:100/bar/ http://example1.com/foo/",
       "http://example1.com/foo/ http://*.example3.com:100/bar/"},
      // Normalizing paths.
      {"http://example1.com/ http://example2.com/",
       "http://example1.com/foo/ http://example2.com/bar/"},
      {"http://example1.com/foo/index.html http://example2.com/bar/",
       "http://example1.com/foo/index.html http://example2.com/bar/"},
      {"http://example1.com/bar http://example2.com/bar/",
       "http://example2.com/bar/"},
      // Not similar to be normalized
      {"http://non-example1.com/foo/ http://non-example2.com/bar/", ""},
      {"https://non-example1.com/foo/ wss://non-example2.com/bar/", ""},
  };

  for (const auto& test : cases) {
    SourceListDirective secondList("script-src", test.sources, csp.get());
    HeapVector<Member<CSPSource>> normalized =
        sourceList.getIntersectCSPSources(secondList.m_list);
    SourceListDirective helperSourceList("script-src", test.expected,
                                         csp.get());
    HeapVector<Member<CSPSource>> expected = helperSourceList.m_list;
    EXPECT_EQ(normalized.size(), expected.size());
    for (size_t i = 0; i < normalized.size(); i++) {
      Source a = {normalized[i]->m_scheme,       normalized[i]->m_host,
                  normalized[i]->m_port,         normalized[i]->m_path,
                  normalized[i]->m_hostWildcard, normalized[i]->m_portWildcard};
      Source b = {expected[i]->m_scheme,       expected[i]->m_host,
                  expected[i]->m_port,         expected[i]->m_path,
                  expected[i]->m_hostWildcard, expected[i]->m_portWildcard};
      EXPECT_TRUE(equalSources(a, b));
    }
  }
}

TEST_F(SourceListDirectiveTest, Subsumes) {
  KURL base;
  String requiredSources =
      "http://example1.com/foo/ http://*.example2.com/bar/ "
      "http://*.example3.com:*/bar/";
  SourceListDirective required("script-src", requiredSources, csp.get());

  struct TestCase {
    std::vector<String> sourcesVector;
    bool expected;
  } cases[] = {
      // Non-intersecting source lists give an effective policy of 'none', which
      // is always subsumed.
      {{"http://example1.com/bar/", "http://*.example3.com:*/bar/"}, true},
      {{"http://example1.com/bar/",
        "http://*.example3.com:*/bar/ http://*.example2.com/bar/"},
       true},
      // Lists that intersect into one of the required sources are subsumed.
      {{"http://example1.com/foo/"}, true},
      {{"http://*.example2.com/bar/"}, true},
      {{"http://*.example3.com:*/bar/"}, true},
      {{"https://example1.com/foo/",
        "http://*.example1.com/foo/ http://*.example2.com/bar/"},
       true},
      {{"http://example2.com/bar/",
        "http://*.example3.com:*/bar/ http://*.example2.com/bar/"},
       true},
      {{"http://example3.com:100/bar/",
        "http://*.example3.com:*/bar/ http://*.example2.com/bar/"},
       true},
      // Lists that intersect into two of the required sources are subsumed.
      {{"http://example1.com/foo/ http://*.example2.com/bar/"}, true},
      {{"http://example1.com/foo/ http://example2.com/bar/",
        "http://example2.com/bar/ http://example1.com/foo/"},
       true},
      // Ordering should not matter.
      {{"https://example1.com/foo/ https://example2.com/bar/",
        "http://example2.com/bar/ http://example1.com/foo/"},
       true},
      // Lists that intersect into a policy identical to the required list are
      // subsumed.
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example1.com/foo/"},
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/"},
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/",
        "http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example4.com/foo/"},
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/",
        "http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example1.com/foo/"},
       true},
      // Lists that include sources that aren't subsumed by the required list
      // are not subsumed.
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://*.example4.com:*/bar/"},
       false},
      {{"http://example1.com/foo/ http://example2.com/foo/"}, false},
      {{"http://*.example1.com/bar/", "http://example1.com/bar/"}, false},
      {{"http://*.example1.com/foo/"}, false},
      {{"wss://example2.com/bar/"}, false},
      {{"http://*.non-example3.com:*/bar/"}, false},
      {{"http://example3.com/foo/"}, false},
      {{"http://not-example1.com", "http://not-example1.com"}, false},
  };

  for (const auto& test : cases) {
    HeapVector<Member<SourceListDirective>> returned;

    for (const auto& sources : test.sourcesVector) {
      SourceListDirective* member =
          new SourceListDirective("script-src", sources, csp.get());
      returned.append(member);
    }

    EXPECT_EQ(required.subsumes(returned), test.expected);

    // If required is empty, any returned should be subsumed by it.
    SourceListDirective requiredIsEmpty("script-src", "", csp.get());
    EXPECT_TRUE(
        requiredIsEmpty.subsumes(HeapVector<Member<SourceListDirective>>()));
    EXPECT_TRUE(requiredIsEmpty.subsumes(returned));
  }
}

}  // namespace blink
