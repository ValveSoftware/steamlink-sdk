// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/PageOverlay.h"

#include "core/frame/FrameView.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebThread.h"
#include "public/web/WebSettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include <memory>

using testing::_;
using testing::AtLeast;
using testing::Property;

namespace blink {
namespace {

static const int viewportWidth = 800;
static const int viewportHeight = 600;

// These unit tests cover both PageOverlay and PageOverlayList.

void enableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(true);
}

void disableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(false);
}

// PageOverlay that paints a solid color.
class SolidColorOverlay : public PageOverlay::Delegate {
public:
    SolidColorOverlay(Color color) : m_color(color) { }

    void paintPageOverlay(const PageOverlay& pageOverlay, GraphicsContext& graphicsContext, const WebSize& size) const override
    {
        if (DrawingRecorder::useCachedDrawingIfPossible(graphicsContext, pageOverlay, DisplayItem::PageOverlay))
            return;
        FloatRect rect(0, 0, size.width, size.height);
        DrawingRecorder drawingRecorder(graphicsContext, pageOverlay, DisplayItem::PageOverlay, rect);
        graphicsContext.fillRect(rect, m_color);
    }

private:
    Color m_color;
};

class PageOverlayTest : public ::testing::Test {
protected:
    enum CompositingMode { AcceleratedCompositing, UnacceleratedCompositing };

    void initialize(CompositingMode compositingMode)
    {
        m_helper.initialize(
            false /* enableJavascript */, nullptr /* webFrameClient */, nullptr /* webViewClient */, nullptr /* webWidgetClient */,
            compositingMode == AcceleratedCompositing ? enableAcceleratedCompositing : disableAcceleratedCompositing);
        webViewImpl()->resize(WebSize(viewportWidth, viewportHeight));
        webViewImpl()->updateAllLifecyclePhases();
        ASSERT_EQ(compositingMode == AcceleratedCompositing, webViewImpl()->isAcceleratedCompositingActive());
    }

    WebViewImpl* webViewImpl() const { return m_helper.webViewImpl(); }

    std::unique_ptr<PageOverlay> createSolidYellowOverlay()
    {
        return PageOverlay::create(webViewImpl(), new SolidColorOverlay(SK_ColorYELLOW));
    }

    template <typename OverlayType>
    void runPageOverlayTestWithAcceleratedCompositing();

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

template <bool(*getter)(), void(*setter)(bool)>
class RuntimeFeatureChange {
public:
    RuntimeFeatureChange(bool newValue) : m_oldValue(getter()) { setter(newValue); }
    ~RuntimeFeatureChange() { setter(m_oldValue); }
private:
    bool m_oldValue;
};

class MockCanvas : public SkCanvas {
public:
    MockCanvas(int width, int height) : SkCanvas(width, height) { }
    MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

TEST_F(PageOverlayTest, PageOverlay_AcceleratedCompositing)
{
    initialize(AcceleratedCompositing);
    webViewImpl()->layerTreeView()->setViewportSize(WebSize(viewportWidth, viewportHeight));

    std::unique_ptr<PageOverlay> pageOverlay = createSolidYellowOverlay();
    pageOverlay->update();
    webViewImpl()->updateAllLifecyclePhases();

    // Ideally, we would get results from the compositor that showed that this
    // page overlay actually winds up getting drawn on top of the rest.
    // For now, we just check that the GraphicsLayer will draw the right thing.

    MockCanvas canvas(viewportWidth, viewportHeight);
    EXPECT_CALL(canvas, onDrawRect(_, _)).Times(AtLeast(0));
    EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(viewportWidth, viewportHeight), Property(&SkPaint::getColor, SK_ColorYELLOW)));

    GraphicsLayer* graphicsLayer = pageOverlay->graphicsLayer();
    WebRect rect(0, 0, viewportWidth, viewportHeight);

    // Paint the layer with a null canvas to get a display list, and then
    // replay that onto the mock canvas for examination.
    IntRect intRect = rect;
    graphicsLayer->paint(&intRect);

    PaintController& paintController = graphicsLayer->getPaintController();
    GraphicsContext graphicsContext(paintController);
    graphicsContext.beginRecording(intRect);
    paintController.paintArtifact().replay(graphicsContext);
    graphicsContext.endRecording()->playback(&canvas);
}

TEST_F(PageOverlayTest, PageOverlay_VisualRect)
{
    initialize(AcceleratedCompositing);
    std::unique_ptr<PageOverlay> pageOverlay = createSolidYellowOverlay();
    pageOverlay->update();
    webViewImpl()->updateAllLifecyclePhases();
    EXPECT_EQ(LayoutRect(0, 0, viewportWidth, viewportHeight), pageOverlay->visualRect());
}

} // namespace
} // namespace blink
