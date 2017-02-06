/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/LinkHighlightImpl.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/input/EventHandler.h"
#include "core/page/Page.h"
#include "core/page/TouchDisambiguation.h"
#include "platform/geometry/IntRect.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebViewClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

GestureEventWithHitTestResults getTargetedEvent(WebViewImpl* webViewImpl, WebGestureEvent& touchEvent)
{
    PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
    return webViewImpl->page()->deprecatedLocalMainFrame()->eventHandler().targetGestureEvent(platformEvent, true);
}

TEST(LinkHighlightImplTest, verifyWebViewImplIntegration)
{
    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(baseURL + fileName, true);
    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->updateAllLifecyclePhases();

    WebGestureEvent touchEvent;
    touchEvent.type = WebInputEvent::GestureShowPress;
    touchEvent.sourceDevice = WebGestureDeviceTouchscreen;

    // The coordinates below are linked to absolute positions in the referenced .html file.
    touchEvent.x = 20;
    touchEvent.y = 20;

    ASSERT_TRUE(webViewImpl->bestTapNode(getTargetedEvent(webViewImpl, touchEvent)));

    touchEvent.y = 40;
    EXPECT_FALSE(webViewImpl->bestTapNode(getTargetedEvent(webViewImpl, touchEvent)));

    touchEvent.y = 20;
    // Shouldn't crash.
    webViewImpl->enableTapHighlightAtPoint(getTargetedEvent(webViewImpl, touchEvent));

    EXPECT_TRUE(webViewImpl->getLinkHighlight(0));
    EXPECT_TRUE(webViewImpl->getLinkHighlight(0)->contentLayer());
    EXPECT_TRUE(webViewImpl->getLinkHighlight(0)->clipLayer());

    // Find a target inside a scrollable div
    touchEvent.y = 100;
    webViewImpl->enableTapHighlightAtPoint(getTargetedEvent(webViewImpl, touchEvent));
    ASSERT_TRUE(webViewImpl->getLinkHighlight(0));

    // Don't highlight if no "hand cursor"
    touchEvent.y = 220; // An A-link with cross-hair cursor.
    webViewImpl->enableTapHighlightAtPoint(getTargetedEvent(webViewImpl, touchEvent));
    ASSERT_EQ(0U, webViewImpl->numLinkHighlights());

    touchEvent.y = 260; // A text input box.
    webViewImpl->enableTapHighlightAtPoint(getTargetedEvent(webViewImpl, touchEvent));
    ASSERT_EQ(0U, webViewImpl->numLinkHighlights());

    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
}

namespace {

class FakeCompositingWebViewClient : public FrameTestHelpers::TestWebViewClient {
public:
    FrameTestHelpers::TestWebFrameClient m_fakeWebFrameClient;
};

FakeCompositingWebViewClient* compositingWebViewClient()
{
    DEFINE_STATIC_LOCAL(FakeCompositingWebViewClient, client, ());
    return &client;
}

} // anonymous namespace

TEST(LinkHighlightImplTest, resetDuringNodeRemoval)
{
    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(baseURL + fileName, true, 0, compositingWebViewClient());

    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->updateAllLifecyclePhases();

    WebGestureEvent touchEvent;
    touchEvent.type = WebInputEvent::GestureShowPress;
    touchEvent.sourceDevice = WebGestureDeviceTouchscreen;
    touchEvent.x = 20;
    touchEvent.y = 20;

    GestureEventWithHitTestResults targetedEvent = getTargetedEvent(webViewImpl, touchEvent);
    Node* touchNode = webViewImpl->bestTapNode(targetedEvent);
    ASSERT_TRUE(touchNode);

    webViewImpl->enableTapHighlightAtPoint(targetedEvent);
    ASSERT_TRUE(webViewImpl->getLinkHighlight(0));

    GraphicsLayer* highlightLayer = webViewImpl->getLinkHighlight(0)->currentGraphicsLayerForTesting();
    ASSERT_TRUE(highlightLayer);
    EXPECT_TRUE(highlightLayer->getLinkHighlight(0));

    touchNode->remove(IGNORE_EXCEPTION);
    webViewImpl->updateAllLifecyclePhases();
    ASSERT_EQ(0U, highlightLayer->numLinkHighlights());

    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
}

// A lifetime test: delete LayerTreeView while running LinkHighlights.
TEST(LinkHighlightImplTest, resetLayerTreeView)
{
    std::unique_ptr<FakeCompositingWebViewClient> webViewClient = wrapUnique(new FakeCompositingWebViewClient());

    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(baseURL + fileName, true, 0, webViewClient.get());

    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->updateAllLifecyclePhases();

    WebGestureEvent touchEvent;
    touchEvent.type = WebInputEvent::GestureShowPress;
    touchEvent.sourceDevice = WebGestureDeviceTouchscreen;
    touchEvent.x = 20;
    touchEvent.y = 20;

    GestureEventWithHitTestResults targetedEvent = getTargetedEvent(webViewImpl, touchEvent);
    Node* touchNode = webViewImpl->bestTapNode(targetedEvent);
    ASSERT_TRUE(touchNode);

    webViewImpl->enableTapHighlightAtPoint(targetedEvent);
    ASSERT_TRUE(webViewImpl->getLinkHighlight(0));

    GraphicsLayer* highlightLayer = webViewImpl->getLinkHighlight(0)->currentGraphicsLayerForTesting();
    ASSERT_TRUE(highlightLayer);
    EXPECT_TRUE(highlightLayer->getLinkHighlight(0));

    // Mimic the logic from RenderWidget::Close:
    webViewImpl->willCloseLayerTreeView();
    webViewHelper.reset();

    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
}

TEST(LinkHighlightImplTest, multipleHighlights)
{
    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(baseURL + fileName, true, 0, compositingWebViewClient());

    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->updateAllLifecyclePhases();

    WebGestureEvent touchEvent;
    touchEvent.x = 50;
    touchEvent.y = 310;
    touchEvent.data.tap.width = 30;
    touchEvent.data.tap.height = 30;

    Vector<IntRect> goodTargets;
    HeapVector<Member<Node>> highlightNodes;
    IntRect boundingBox(touchEvent.x - touchEvent.data.tap.width / 2, touchEvent.y - touchEvent.data.tap.height / 2, touchEvent.data.tap.width, touchEvent.data.tap.height);
    findGoodTouchTargets(boundingBox, webViewImpl->mainFrameImpl()->frame(), goodTargets, highlightNodes);

    webViewImpl->enableTapHighlights(highlightNodes);
    EXPECT_EQ(2U, webViewImpl->numLinkHighlights());

    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
}

} // namespace blink
