/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
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

#include "core/dom/Document.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class DocumentTest : public ::testing::Test {
protected:
    void SetUp() override;

    void TearDown() override
    {
        ThreadHeap::collectAllGarbage();
    }

    Document& document() const { return m_dummyPageHolder->document(); }
    Page& page() const { return m_dummyPageHolder->page(); }

    void setHtmlInnerHTML(const char*);

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void DocumentTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

void DocumentTest::setHtmlInnerHTML(const char* htmlContent)
{
    document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent), ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
}

// This tests that we properly resize and re-layout pages for printing in the presence of
// media queries effecting elements in a subtree layout boundary
TEST_F(DocumentTest, PrintRelayout)
{
    setHtmlInnerHTML(
        "<style>"
        "    div {"
        "        width: 100px;"
        "        height: 100px;"
        "        overflow: hidden;"
        "    }"
        "    span {"
        "        width: 50px;"
        "        height: 50px;"
        "    }"
        "    @media screen {"
        "        span {"
        "            width: 20px;"
        "        }"
        "    }"
        "</style>"
        "<p><div><span></span></div></p>");
    FloatSize pageSize(400, 400);
    float maximumShrinkRatio = 1.6;

    document().frame()->setPrinting(true, pageSize, pageSize, maximumShrinkRatio);
    EXPECT_EQ(document().documentElement()->offsetWidth(), 400);
    document().frame()->setPrinting(false, FloatSize(), FloatSize(), 0);
    EXPECT_EQ(document().documentElement()->offsetWidth(), 800);

}

// This test checks that Documunt::linkManifest() returns a value conform to the specification.
TEST_F(DocumentTest, LinkManifest)
{
    // Test the default result.
    EXPECT_EQ(0, document().linkManifest());

    // Check that we use the first manifest with <link rel=manifest>
    HTMLLinkElement* link = HTMLLinkElement::create(document(), false);
    link->setAttribute(blink::HTMLNames::relAttr, "manifest");
    link->setAttribute(blink::HTMLNames::hrefAttr, "foo.json");
    document().head()->appendChild(link);
    EXPECT_EQ(link, document().linkManifest());

    HTMLLinkElement* link2 = HTMLLinkElement::create(document(), false);
    link2->setAttribute(blink::HTMLNames::relAttr, "manifest");
    link2->setAttribute(blink::HTMLNames::hrefAttr, "bar.json");
    document().head()->insertBefore(link2, link);
    EXPECT_EQ(link2, document().linkManifest());
    document().head()->appendChild(link2);
    EXPECT_EQ(link, document().linkManifest());

    // Check that crazy URLs are accepted.
    link->setAttribute(blink::HTMLNames::hrefAttr, "http:foo.json");
    EXPECT_EQ(link, document().linkManifest());

    // Check that empty URLs are accepted.
    link->setAttribute(blink::HTMLNames::hrefAttr, "");
    EXPECT_EQ(link, document().linkManifest());

    // Check that URLs from different origins are accepted.
    link->setAttribute(blink::HTMLNames::hrefAttr, "http://example.org/manifest.json");
    EXPECT_EQ(link, document().linkManifest());
    link->setAttribute(blink::HTMLNames::hrefAttr, "http://foo.example.org/manifest.json");
    EXPECT_EQ(link, document().linkManifest());
    link->setAttribute(blink::HTMLNames::hrefAttr, "http://foo.bar/manifest.json");
    EXPECT_EQ(link, document().linkManifest());

    // More than one token in @rel is accepted.
    link->setAttribute(blink::HTMLNames::relAttr, "foo bar manifest");
    EXPECT_EQ(link, document().linkManifest());

    // Such as spaces around the token.
    link->setAttribute(blink::HTMLNames::relAttr, " manifest ");
    EXPECT_EQ(link, document().linkManifest());

    // Check that rel=manifest actually matters.
    link->setAttribute(blink::HTMLNames::relAttr, "");
    EXPECT_EQ(link2, document().linkManifest());
    link->setAttribute(blink::HTMLNames::relAttr, "manifest");

    // Check that link outside of the <head> are ignored.
    document().head()->removeChild(link, ASSERT_NO_EXCEPTION);
    document().head()->removeChild(link2, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(0, document().linkManifest());
    document().body()->appendChild(link);
    EXPECT_EQ(0, document().linkManifest());
    document().head()->appendChild(link);
    document().head()->appendChild(link2);

    // Check that some attribute values do not have an effect.
    link->setAttribute(blink::HTMLNames::crossoriginAttr, "use-credentials");
    EXPECT_EQ(link, document().linkManifest());
    link->setAttribute(blink::HTMLNames::hreflangAttr, "klingon");
    EXPECT_EQ(link, document().linkManifest());
    link->setAttribute(blink::HTMLNames::typeAttr, "image/gif");
    EXPECT_EQ(link, document().linkManifest());
    link->setAttribute(blink::HTMLNames::sizesAttr, "16x16");
    EXPECT_EQ(link, document().linkManifest());
    link->setAttribute(blink::HTMLNames::mediaAttr, "print");
    EXPECT_EQ(link, document().linkManifest());
}

TEST_F(DocumentTest, referrerPolicyParsing)
{
    EXPECT_EQ(ReferrerPolicyDefault, document().getReferrerPolicy());

    struct TestCase {
        const char* policy;
        ReferrerPolicy expected;
    } tests[] = {
        { "", ReferrerPolicyDefault },
        // Test that invalid policy values are ignored.
        { "not-a-real-policy", ReferrerPolicyDefault },
        { "not-a-real-policy,also-not-a-real-policy", ReferrerPolicyDefault },
        { "not-a-real-policy,unsafe-url", ReferrerPolicyAlways },
        { "unsafe-url,not-a-real-policy", ReferrerPolicyAlways },
        // Test parsing each of the policy values.
        { "always", ReferrerPolicyAlways },
        { "default", ReferrerPolicyNoReferrerWhenDowngrade },
        { "never", ReferrerPolicyNever },
        { "no-referrer", ReferrerPolicyNever },
        { "no-referrer-when-downgrade", ReferrerPolicyNoReferrerWhenDowngrade },
        { "origin", ReferrerPolicyOrigin },
        { "origin-when-crossorigin", ReferrerPolicyOriginWhenCrossOrigin },
        { "origin-when-cross-origin", ReferrerPolicyOriginWhenCrossOrigin },
        { "unsafe-url", ReferrerPolicyAlways },
    };

    for (auto test : tests) {
        document().setReferrerPolicy(ReferrerPolicyDefault);
        document().parseAndSetReferrerPolicy(test.policy);
        EXPECT_EQ(test.expected, document().getReferrerPolicy()) << test.policy;
    }
}

TEST_F(DocumentTest, OutgoingReferrer)
{
    document().setURL(KURL(KURL(), "https://www.example.com/hoge#fuga?piyo"));
    document().setSecurityOrigin(SecurityOrigin::create(KURL(KURL(), "https://www.example.com/")));
    EXPECT_EQ("https://www.example.com/hoge", document().outgoingReferrer());
}

TEST_F(DocumentTest, OutgoingReferrerWithUniqueOrigin)
{
    document().setURL(KURL(KURL(), "https://www.example.com/hoge#fuga?piyo"));
    document().setSecurityOrigin(SecurityOrigin::createUnique());
    EXPECT_EQ(String(), document().outgoingReferrer());
}

TEST_F(DocumentTest, StyleVersion)
{
    setHtmlInnerHTML(
        "<style>"
        "    .a * { color: green }"
        "    .b .c { color: green }"
        "</style>"
        "<div id='x'><span class='c'></span></div>");

    Element* element = document().getElementById("x");
    EXPECT_TRUE(element);

    uint64_t previousStyleVersion = document().styleVersion();
    element->setAttribute(blink::HTMLNames::classAttr, "notfound");
    EXPECT_EQ(previousStyleVersion, document().styleVersion());

    document().view()->updateAllLifecyclePhases();

    previousStyleVersion = document().styleVersion();
    element->setAttribute(blink::HTMLNames::classAttr, "a");
    EXPECT_NE(previousStyleVersion, document().styleVersion());

    document().view()->updateAllLifecyclePhases();

    previousStyleVersion = document().styleVersion();
    element->setAttribute(blink::HTMLNames::classAttr, "a b");
    EXPECT_NE(previousStyleVersion, document().styleVersion());
}

TEST_F(DocumentTest, EnforceSandboxFlags)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString("http://example.test");
    document().setSecurityOrigin(origin);
    SandboxFlags mask = SandboxNavigation;
    document().enforceSandboxFlags(mask);
    EXPECT_EQ(origin, document().getSecurityOrigin());
    EXPECT_FALSE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

    mask |= SandboxOrigin;
    document().enforceSandboxFlags(mask);
    EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
    EXPECT_FALSE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

    // A unique origin does not bypass secure context checks unless it
    // is also potentially trustworthy.
    SchemeRegistry::registerURLSchemeBypassingSecureContextCheck("very-special-scheme");
    origin = SecurityOrigin::createFromString("very-special-scheme://example.test");
    document().setSecurityOrigin(origin);
    document().enforceSandboxFlags(mask);
    EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
    EXPECT_FALSE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

    SchemeRegistry::registerURLSchemeAsSecure("very-special-scheme");
    document().setSecurityOrigin(origin);
    document().enforceSandboxFlags(mask);
    EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
    EXPECT_TRUE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

    origin = SecurityOrigin::createFromString("https://example.test");
    document().setSecurityOrigin(origin);
    document().enforceSandboxFlags(mask);
    EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
    EXPECT_TRUE(document().getSecurityOrigin()->isPotentiallyTrustworthy());
}

} // namespace blink
