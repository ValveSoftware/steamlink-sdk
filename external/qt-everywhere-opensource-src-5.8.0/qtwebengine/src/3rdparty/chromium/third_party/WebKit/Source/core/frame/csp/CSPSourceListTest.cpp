// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPSourceList.h"

#include "core/dom/Document.h"
#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CSPSourceListTest : public ::testing::Test {
public:
    CSPSourceListTest()
        : csp(ContentSecurityPolicy::create())
    {
    }

protected:
    virtual void SetUp()
    {
        KURL secureURL(ParsedURLString, "https://example.test/image.png");
        RefPtr<SecurityOrigin> secureOrigin(SecurityOrigin::create(secureURL));
        document = Document::create();
        document->setSecurityOrigin(secureOrigin);
        csp->bindToExecutionContext(document.get());
    }

    Persistent<ContentSecurityPolicy> csp;
    Persistent<Document> document;
};

static void parseSourceList(CSPSourceList& sourceList, String& sources)
{
    Vector<UChar> characters;
    sources.appendTo(characters);
    sourceList.parse(characters.data(), characters.data() + characters.size());
}

TEST_F(CSPSourceListTest, BasicMatchingNone)
{
    KURL base;
    String sources = "'none'";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_FALSE(sourceList.matches(KURL(base, "http://example.com/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "https://example.test/")));
}

TEST_F(CSPSourceListTest, BasicMatchingStrictDynamic)
{
    String sources = "'strict-dynamic'";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_TRUE(sourceList.allowDynamic());
}

TEST_F(CSPSourceListTest, BasicMatchingUnsafeHashedAttributes)
{
    String sources = "'unsafe-hashed-attributes'";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_TRUE(sourceList.allowHashedAttributes());
}


TEST_F(CSPSourceListTest, BasicMatchingStar)
{
    KURL base;
    String sources = "*";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example.com/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example.com/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example.com/bar")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://foo.example.com/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://foo.example.com/bar")));

    EXPECT_FALSE(sourceList.matches(KURL(base, "data:https://example.test/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "blob:https://example.test/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "filesystem:https://example.test/")));
}

TEST_F(CSPSourceListTest, BasicMatchingSelf)
{
    KURL base;
    String sources = "'self'";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_FALSE(sourceList.matches(KURL(base, "http://example.com/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "https://not-example.com/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example.test/")));
}

TEST_F(CSPSourceListTest, BlobMatchingSelf)
{
    KURL base;
    String sources = "'self'";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example.test/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "blob:https://example.test/")));

    // Register "https" as bypassing CSP, which should trigger the innerURL behavior.
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy("https");

    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example.test/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "blob:https://example.test/")));

    // Unregister the scheme to clean up after ourselves.
    SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy("https");
}

TEST_F(CSPSourceListTest, BlobMatchingBlob)
{
    KURL base;
    String sources = "blob:";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_FALSE(sourceList.matches(KURL(base, "https://example.test/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "blob:https://example.test/")));
}

TEST_F(CSPSourceListTest, BasicMatching)
{
    KURL base;
    String sources = "http://example1.com:8000/foo/ https://example2.com/";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example1.com:8000/foo/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example1.com:8000/foo/bar")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example2.com/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example2.com/foo/")));

    EXPECT_FALSE(sourceList.matches(KURL(base, "https://not-example.com/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "http://example1.com/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "https://example1.com/foo")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "http://example1.com:9000/foo/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "http://example1.com:8000/FOO/")));
}

TEST_F(CSPSourceListTest, WildcardMatching)
{
    KURL base;
    String sources = "http://example1.com:*/foo/ https://*.example2.com/bar/ http://*.test/";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example1.com/foo/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example1.com:8000/foo/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example1.com:9000/foo/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://foo.example2.com/bar/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://foo.test/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://foo.bar.test/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example1.com/foo/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example1.com:8000/foo/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example1.com:9000/foo/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://foo.test/")));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://foo.bar.test/")));

    EXPECT_FALSE(sourceList.matches(KURL(base, "https://example1.com:8000/foo")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "https://example2.com:8000/bar")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "https://foo.example2.com:8000/bar")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "https://example2.foo.com/bar")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "http://foo.test.bar/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "https://example2.com/bar/")));
    EXPECT_FALSE(sourceList.matches(KURL(base, "http://test/")));
}

TEST_F(CSPSourceListTest, RedirectMatching)
{
    KURL base;
    String sources = "http://example1.com/foo/ http://example2.com/bar/";
    CSPSourceList sourceList(csp.get(), "script-src");
    parseSourceList(sourceList, sources);

    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example1.com/foo/"), ResourceRequest::RedirectStatus::FollowedRedirect));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example1.com/bar/"), ResourceRequest::RedirectStatus::FollowedRedirect));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example2.com/bar/"), ResourceRequest::RedirectStatus::FollowedRedirect));
    EXPECT_TRUE(sourceList.matches(KURL(base, "http://example2.com/foo/"), ResourceRequest::RedirectStatus::FollowedRedirect));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example1.com/foo/"), ResourceRequest::RedirectStatus::FollowedRedirect));
    EXPECT_TRUE(sourceList.matches(KURL(base, "https://example1.com/bar/"), ResourceRequest::RedirectStatus::FollowedRedirect));

    EXPECT_FALSE(sourceList.matches(KURL(base, "http://example3.com/foo/"), ResourceRequest::RedirectStatus::FollowedRedirect));
}

} // namespace blink
