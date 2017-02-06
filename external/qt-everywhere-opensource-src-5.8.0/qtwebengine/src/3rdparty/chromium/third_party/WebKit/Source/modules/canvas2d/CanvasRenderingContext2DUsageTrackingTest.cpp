// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/CanvasRenderingContext2D.h"

#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/ImageData.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "modules/canvas2d/HitRegionOptions.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace blink {
namespace UsageTrackingTests {

enum BitmapOpacity {
    OpaqueBitmap,
    TransparentBitmap
};

class FakeImageSource : public CanvasImageSource {
public:
    FakeImageSource(IntSize, BitmapOpacity);

    PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*, AccelerationHint, SnapshotReason, const FloatSize&) const override;

    bool wouldTaintOrigin(SecurityOrigin* destinationSecurityOrigin) const override { return false; }
    FloatSize elementSize(const FloatSize&) const override { return FloatSize(m_size); }
    bool isOpaque() const override { return m_isOpaque; }
    int sourceWidth() override { return m_size.width(); }
    int sourceHeight() override { return m_size.height(); }

    ~FakeImageSource() override { }

private:
    IntSize m_size;
    RefPtr<Image> m_image;
    bool m_isOpaque;
};

FakeImageSource::FakeImageSource(IntSize size, BitmapOpacity opacity)
    : m_size(size)
    , m_isOpaque(opacity == OpaqueBitmap)
{
    sk_sp<SkSurface> surface(SkSurface::MakeRasterN32Premul(m_size.width(), m_size.height()));
    surface->getCanvas()->clear(opacity == OpaqueBitmap ? SK_ColorWHITE : SK_ColorTRANSPARENT);
    RefPtr<SkImage> image = adoptRef(surface->makeImageSnapshot().release());
    m_image = StaticBitmapImage::create(image);
}

PassRefPtr<Image> FakeImageSource::getSourceImageForCanvas(SourceImageStatus* status, AccelerationHint, SnapshotReason, const FloatSize&) const
{
    if (status)
        *status = NormalSourceImageStatus;
    return m_image;
}

class CanvasRenderingContextUsageTrackingTest : public ::testing::Test {
protected:
    CanvasRenderingContextUsageTrackingTest();
    void SetUp() override;

    DummyPageHolder& page() const { return *m_dummyPageHolder; }
    HTMLDocument& document() const { return *m_document; }
    HTMLCanvasElement& canvasElement() const { return *m_canvasElement; }
    CanvasRenderingContext2D* context2d() const;

    void createContext(OpacityMode);

    FakeImageSource m_opaqueBitmap;
    FakeImageSource m_alphaBitmap;
    Persistent<ImageData> m_fullImageData;
    void TearDown();
private:
    std::shared_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLDocument> m_document;
    Persistent<HTMLCanvasElement> m_canvasElement;
    Persistent<MemoryCache> m_globalMemoryCache;
};

CanvasRenderingContextUsageTrackingTest::CanvasRenderingContextUsageTrackingTest()
    : m_opaqueBitmap(IntSize(10, 10), OpaqueBitmap)
    , m_alphaBitmap(IntSize(10, 10), TransparentBitmap)
{ }

void CanvasRenderingContextUsageTrackingTest::TearDown()
{
    ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);
    replaceMemoryCacheForTesting(m_globalMemoryCache.release());
}

CanvasRenderingContext2D* CanvasRenderingContextUsageTrackingTest::context2d() const
{
    // If the following check fails, perhaps you forgot to call createContext
    // in your test?
    EXPECT_NE(nullptr, canvasElement().renderingContext());
    EXPECT_TRUE(canvasElement().renderingContext()->is2d());
    return static_cast<CanvasRenderingContext2D*>(canvasElement().renderingContext());
}

void CanvasRenderingContextUsageTrackingTest::createContext(OpacityMode opacityMode)
{
    String canvasType("2d");
    CanvasContextCreationAttributes attributes;
    attributes.setAlpha(opacityMode == NonOpaque);
    m_canvasElement->getCanvasRenderingContext(canvasType, attributes);
    context2d(); // Calling this for the checks
}

void CanvasRenderingContextUsageTrackingTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    m_document->documentElement()->setInnerHTML("<body><canvas id='c'></canvas></body>", ASSERT_NO_EXCEPTION);
    m_document->view()->updateAllLifecyclePhases();
    m_canvasElement = toHTMLCanvasElement(m_document->getElementById("c"));
    m_fullImageData = ImageData::create(IntSize(10, 10));

    m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
}

TEST_F(CanvasRenderingContextUsageTrackingTest, FillTracking)
{
    createContext(NonOpaque);

    int numReps = 100;
    for (int i = 0; i < numReps; i++) {
        context2d()->beginPath();
        context2d()->fillRect(10, 10, 100, 100);

        context2d()->moveTo(1, 1);
        context2d()->lineTo(4, 1);
        context2d()->lineTo(4, 4);
        context2d()->lineTo(2, 2);
        context2d()->lineTo(1, 4);

        context2d()->fillText("Hello World!!!", 10, 10, 1);
        context2d()->fill(); // path is not convex

        context2d()->fillRect(10, 10, 100, 100);
        context2d()->fillText("Hello World!!!", 10, 10);

        context2d()->beginPath();
        context2d()->moveTo(1, 1);
        context2d()->lineTo(4, 1);
        context2d()->lineTo(4, 4);
        context2d()->lineTo(1, 4);
        context2d()->fill(); // path is convex
    }

    EXPECT_EQ(numReps * 2, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
    EXPECT_EQ(numReps * 2, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillRect]);
    EXPECT_EQ(numReps, context2d()->getUsage().numNonConvexFillPathCalls);
    EXPECT_EQ(numReps * 2, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);

    // create gradient
    CanvasGradient* gradient;
    gradient = context2d()->createLinearGradient(0, 0, 100, 100);
    context2d()->setFillStyle(StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
    context2d()->fillRect(10, 10, 100, 100);
    EXPECT_EQ(1, context2d()->getUsage().numGradients);

    // create pattern
    NonThrowableExceptionState exceptionState;
    ExecutionContext* execCtx = canvasElement().getExecutionContext();
    CanvasPattern* canvasPattern = context2d()->createPattern(execCtx, &m_opaqueBitmap, "repeat-x", exceptionState);
    StringOrCanvasGradientOrCanvasPattern pattern = StringOrCanvasGradientOrCanvasPattern::fromCanvasPattern(canvasPattern);
    context2d()->setFillStyle(pattern);
    context2d()->fillRect(10, 10, 100, 100);
    EXPECT_EQ(1, context2d()->getUsage().numPatterns);

    // Other fill method
    context2d()->fill(Path2D::create("Hello World!!!"));
    EXPECT_EQ(numReps * 2 + 1, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);

    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);
    EXPECT_EQ(0, context2d()->getUsage().numPutImageDataCalls);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::DrawImage]);
    EXPECT_EQ(0, context2d()->getUsage().numGetImageDataCalls);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, StrokeTracking)
{
    createContext(NonOpaque);

    int numReps = 100;
    for (int i = 0; i < numReps; i++) {
        context2d()->beginPath();
        context2d()->strokeRect(10, 10, 100, 100);

        context2d()->moveTo(1, 1);
        context2d()->lineTo(4, 1);
        context2d()->lineTo(4, 4);
        context2d()->lineTo(2, 2);
        context2d()->lineTo(1, 4);

        context2d()->strokeText("Hello World!!!", 10, 10, 1);
        context2d()->stroke();

        context2d()->strokeRect(10, 10, 100, 100);
        context2d()->strokeText("Hello World!!!", 10, 10);
    }

    EXPECT_EQ(numReps, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
    EXPECT_EQ(numReps * 2, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeRect]);
    EXPECT_EQ(numReps * 2, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);

    // create gradient
    CanvasGradient* gradient;
    gradient = context2d()->createLinearGradient(0, 0, 100, 100);
    context2d()->setStrokeStyle(StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
    context2d()->strokeRect(10, 10, 100, 100);
    EXPECT_EQ(1, context2d()->getUsage().numGradients);

    // create pattern
    NonThrowableExceptionState exceptionState;
    ExecutionContext* execCtx = canvasElement().getExecutionContext();
    CanvasPattern* canvasPattern = context2d()->createPattern(execCtx, &m_opaqueBitmap, "repeat-x", exceptionState);
    StringOrCanvasGradientOrCanvasPattern pattern = StringOrCanvasGradientOrCanvasPattern::fromCanvasPattern(canvasPattern);
    context2d()->setStrokeStyle(pattern);
    context2d()->strokeRect(10, 10, 100, 100);
    EXPECT_EQ(1, context2d()->getUsage().numPatterns);

    // Other stroke method
    context2d()->stroke(Path2D::create("Hello World!!!"));
    EXPECT_EQ(numReps + 1, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
    EXPECT_EQ(0, context2d()->getUsage().numNonConvexFillPathCalls);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);
    EXPECT_EQ(0, context2d()->getUsage().numPutImageDataCalls);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::DrawImage]);
    EXPECT_EQ(0, context2d()->getUsage().numGetImageDataCalls);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ImageTracking)
{
    // Testing draw, put, get and filters
    createContext(NonOpaque);
    NonThrowableExceptionState exceptionState;

    int numReps = 5;
    for (int i = 0; i < numReps; i++) {
        context2d()->putImageData(m_fullImageData.get(), 0, 0, exceptionState);
        context2d()->drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 1, 1, 0, 0, 10, 10, exceptionState);
        context2d()->getImageData(0, 0, 1, 1, exceptionState);
    }

    context2d()->setFilter("blur(5px)");
    context2d()->drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 1, 1, 0, 0, 10, 10, exceptionState);

    EXPECT_EQ(numReps, context2d()->getUsage().numPutImageDataCalls);
    EXPECT_EQ(numReps + 1, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::DrawImage]);
    EXPECT_EQ(numReps, context2d()->getUsage().numGetImageDataCalls);
    EXPECT_EQ(1, context2d()->getUsage().numFilters);

    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
    EXPECT_EQ(0, context2d()->getUsage().numNonConvexFillPathCalls);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ClearRectTracking)
{
    createContext(NonOpaque);
    context2d()->clearRect(0, 0, 1, 1);
    context2d()->clearRect(0, 0, 2, 2);
    EXPECT_EQ(2, context2d()->getUsage().numClearRectCalls);

    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
    EXPECT_EQ(0, context2d()->getUsage().numNonConvexFillPathCalls);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
    EXPECT_EQ(0, context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ComplexClipsTracking)
{
    createContext(NonOpaque);
    context2d()->save();

    // simple clip
    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(4, 1);
    context2d()->lineTo(4, 4);
    context2d()->lineTo(1, 4);
    context2d()->clip("nonzero");
    context2d()->fillRect(0, 0, 1, 1);
    EXPECT_EQ(0, context2d()->getUsage().numDrawWithComplexClips);

    // complex clips
    int numReps = 5;
    for (int i = 0; i < numReps; i++) {
        context2d()->beginPath();
        context2d()->moveTo(1, 1);
        context2d()->lineTo(4, 1);
        context2d()->lineTo(4, 4);
        context2d()->lineTo(2, 2);
        context2d()->lineTo(1, 4);
        context2d()->clip("nonzero");
        context2d()->fillRect(0, 0, 1, 1);
    }

    // remove all previous clips
    context2d()->restore();

    // simple clip again
    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(4, 1);
    context2d()->lineTo(4, 4);
    context2d()->lineTo(1, 4);
    context2d()->clip("nonzero");
    context2d()->fillRect(0, 0, 1, 1);

    EXPECT_EQ(numReps, context2d()->getUsage().numDrawWithComplexClips);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ShadowsTracking)
{
    createContext(NonOpaque);

    context2d()->beginPath();
    context2d()->fillRect(0, 0, 1, 1);

    // shadow with blur = 0
    context2d()->beginPath();
    context2d()->setShadowBlur(0.0);
    context2d()->setShadowColor("rgba(255, 0, 0, 0.5)");
    context2d()->setShadowOffsetX(1.0);
    context2d()->setShadowOffsetY(1.0);
    context2d()->fillRect(0, 0, 1, 1);

    // shadow with alpha = 0
    context2d()->beginPath();
    context2d()->setShadowBlur(0.5);
    context2d()->setShadowColor("rgba(255, 0, 0, 0)");
    context2d()->setShadowOffsetX(1.0);
    context2d()->setShadowOffsetY(1.0);
    context2d()->fillRect(0, 0, 1, 1);

    EXPECT_EQ(0, context2d()->getUsage().numBlurredShadows);

    int numReps = 5;
    for (int i = 0; i < numReps; i++) {
        context2d()->beginPath();
        context2d()->setShadowBlur(0.5);
        context2d()->setShadowColor("rgba(255, 0, 0, 0.5)");
        context2d()->setShadowOffsetX(1.0);
        context2d()->setShadowOffsetY(1.0);
        context2d()->fillRect(0, 0, 1, 1);
    }
    EXPECT_EQ(numReps, context2d()->getUsage().numBlurredShadows);
}

} // namespace UsageTrackingTests
} // namespace blink
