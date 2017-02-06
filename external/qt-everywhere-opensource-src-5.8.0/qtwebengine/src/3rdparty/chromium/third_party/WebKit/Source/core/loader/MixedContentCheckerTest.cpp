// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/MixedContentChecker.h"

#include "core/frame/Settings.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gmock/include/gmock/gmock-generated-function-mockers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"
#include <base/macros.h>
#include <memory>

namespace blink {

TEST(MixedContentCheckerTest, IsMixedContent)
{
    struct TestCase {
        const char* origin;
        const char* target;
        bool expectation;
    } cases[] = {
        {"http://example.com/foo", "http://example.com/foo", false},
        {"http://example.com/foo", "https://example.com/foo", false},
        {"http://example.com/foo", "data:text/html,<p>Hi!</p>", false},
        {"http://example.com/foo", "about:blank", false},
        {"https://example.com/foo", "https://example.com/foo", false},
        {"https://example.com/foo", "wss://example.com/foo", false},
        {"https://example.com/foo", "data:text/html,<p>Hi!</p>", false},
        {"https://example.com/foo", "http://127.0.0.1/", false},
        {"https://example.com/foo", "http://[::1]/", false},

        {"https://example.com/foo", "http://example.com/foo", true},
        {"https://example.com/foo", "http://google.com/foo", true},
        {"https://example.com/foo", "ws://example.com/foo", true},
        {"https://example.com/foo", "ws://google.com/foo", true},
        {"https://example.com/foo", "http://192.168.1.1/", true},
        {"https://example.com/foo", "http://localhost/", true},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(::testing::Message() << "Origin: " << test.origin << ", Target: " << test.target << ", Expectation: " << test.expectation);
        KURL originUrl(KURL(), test.origin);
        RefPtr<SecurityOrigin> securityOrigin(SecurityOrigin::create(originUrl));
        KURL targetUrl(KURL(), test.target);
        EXPECT_EQ(test.expectation, MixedContentChecker::isMixedContent(securityOrigin.get(), targetUrl));
    }
}

TEST(MixedContentCheckerTest, ContextTypeForInspector)
{
    std::unique_ptr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(1, 1));
    dummyPageHolder->frame().document()->setSecurityOrigin(SecurityOrigin::createFromString("http://example.test"));

    ResourceRequest notMixedContent("https://example.test/foo.jpg");
    notMixedContent.setFrameType(WebURLRequest::FrameTypeAuxiliary);
    notMixedContent.setRequestContext(WebURLRequest::RequestContextScript);
    EXPECT_EQ(WebMixedContent::ContextType::NotMixedContent, MixedContentChecker::contextTypeForInspector(&dummyPageHolder->frame(), notMixedContent));

    dummyPageHolder->frame().document()->setSecurityOrigin(SecurityOrigin::createFromString("https://example.test"));
    EXPECT_EQ(WebMixedContent::ContextType::NotMixedContent, MixedContentChecker::contextTypeForInspector(&dummyPageHolder->frame(), notMixedContent));

    ResourceRequest blockableMixedContent("http://example.test/foo.jpg");
    blockableMixedContent.setFrameType(WebURLRequest::FrameTypeAuxiliary);
    blockableMixedContent.setRequestContext(WebURLRequest::RequestContextScript);
    EXPECT_EQ(WebMixedContent::ContextType::Blockable, MixedContentChecker::contextTypeForInspector(&dummyPageHolder->frame(), blockableMixedContent));

    ResourceRequest optionallyBlockableMixedContent("http://example.test/foo.jpg");
    blockableMixedContent.setFrameType(WebURLRequest::FrameTypeAuxiliary);
    blockableMixedContent.setRequestContext(WebURLRequest::RequestContextImage);
    EXPECT_EQ(WebMixedContent::ContextType::OptionallyBlockable, MixedContentChecker::contextTypeForInspector(&dummyPageHolder->frame(), blockableMixedContent));
}

namespace {

    class MockFrameLoaderClient : public EmptyFrameLoaderClient {
    public:
        MockFrameLoaderClient()
            : EmptyFrameLoaderClient()
        {
        }
        MOCK_METHOD2(didDisplayContentWithCertificateErrors, void(const KURL&, const CString&));
        MOCK_METHOD2(didRunContentWithCertificateErrors, void(const KURL&, const CString&));
    };

} // namespace

TEST(MixedContentCheckerTest, HandleCertificateError)
{
    MockFrameLoaderClient* client = new MockFrameLoaderClient;
    std::unique_ptr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(1, 1), nullptr, client);

    KURL mainResourceUrl(KURL(), "https://example.test");
    KURL displayedUrl(KURL(), "https://example-displayed.test");
    KURL ranUrl(KURL(), "https://example-ran.test");

    dummyPageHolder->frame().document()->setURL(mainResourceUrl);
    ResourceResponse response1;
    response1.setURL(ranUrl);
    response1.setSecurityInfo("security info1");
    EXPECT_CALL(*client, didRunContentWithCertificateErrors(ranUrl, response1.getSecurityInfo()));
    MixedContentChecker::handleCertificateError(&dummyPageHolder->frame(), response1, WebURLRequest::FrameTypeNone, WebURLRequest::RequestContextScript);

    ResourceResponse response2;
    WebURLRequest::RequestContext requestContext = WebURLRequest::RequestContextImage;
    ASSERT_EQ(WebMixedContent::ContextType::OptionallyBlockable, WebMixedContent::contextTypeFromRequestContext(requestContext, dummyPageHolder->frame().settings()->strictMixedContentCheckingForPlugin()));
    response2.setURL(displayedUrl);
    response2.setSecurityInfo("security info2");
    EXPECT_CALL(*client, didDisplayContentWithCertificateErrors(displayedUrl, response2.getSecurityInfo()));
    MixedContentChecker::handleCertificateError(&dummyPageHolder->frame(), response2, WebURLRequest::FrameTypeNone, requestContext);
}

} // namespace blink
