// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebViewImpl.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/compositing/PaintArtifactCompositor.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/web/WebViewClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/FrameTestHelpers.h"

// Tests that WebViewImpl attaches the layer owned by PaintArtifactCompositor to
// the WebLayerTreeView when requested.

using testing::Property;

namespace blink {
namespace {

class MockWebLayerTreeView : public WebLayerTreeView {
public:
    // WebLayerTreeView
    MOCK_METHOD1(setRootLayer, void(const WebLayer&));
    MOCK_METHOD0(clearRootLayer, void());
};

class TestWebViewClientWithLayerTreeView : public FrameTestHelpers::TestWebViewClient {
public:
    TestWebViewClientWithLayerTreeView(WebLayerTreeView* layerTreeView)
        : m_layerTreeView(layerTreeView) { }

    // WebViewClient
    WebLayerTreeView* layerTreeView() override { return m_layerTreeView; }

private:
    WebLayerTreeView* m_layerTreeView;
};

class WebViewImplPaintArtifactCompositorTest : public testing::Test {
protected:
    WebViewImplPaintArtifactCompositorTest()
        : m_webViewClient(&m_webLayerTreeView) { }

    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
        m_helper.initialize(false, nullptr, &m_webViewClient);
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

    MockWebLayerTreeView& webLayerTreeView() { return m_webLayerTreeView; }
    WebViewImpl& webViewImpl() { return *m_helper.webViewImpl(); }
    PaintArtifactCompositor& getPaintArtifactCompositor() { return webViewImpl().getPaintArtifactCompositor(); }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
    MockWebLayerTreeView m_webLayerTreeView;
    TestWebViewClientWithLayerTreeView m_webViewClient;
    FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(WebViewImplPaintArtifactCompositorTest, AttachAndDetach)
{
    cc::Layer* rootLayer = getPaintArtifactCompositor().rootLayer();
    ASSERT_TRUE(rootLayer);

    EXPECT_CALL(webLayerTreeView(), setRootLayer(Property(&WebLayer::ccLayer, rootLayer)));
    webViewImpl().attachPaintArtifactCompositor();
    testing::Mock::VerifyAndClearExpectations(&webLayerTreeView());

    EXPECT_CALL(webLayerTreeView(), clearRootLayer());
    webViewImpl().detachPaintArtifactCompositor();
    testing::Mock::VerifyAndClearExpectations(&webLayerTreeView());
}

} // namespace
} // namespace blink
