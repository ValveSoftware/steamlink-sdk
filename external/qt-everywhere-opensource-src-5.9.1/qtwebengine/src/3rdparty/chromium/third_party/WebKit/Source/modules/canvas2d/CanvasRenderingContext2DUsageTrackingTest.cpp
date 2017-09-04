// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/CanvasRenderingContext2D.h"

#include "core/dom/Document.h"
#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/ImageData.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "modules/canvas2d/HitRegionOptions.h"
#include "modules/canvas2d/Path2D.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace blink {
namespace UsageTrackingTests {

enum BitmapOpacity { OpaqueBitmap, TransparentBitmap };

class FakeImageSource : public CanvasImageSource {
 public:
  FakeImageSource(IntSize, BitmapOpacity);

  PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const override;

  bool wouldTaintOrigin(
      SecurityOrigin* destinationSecurityOrigin) const override {
    return false;
  }
  FloatSize elementSize(const FloatSize&) const override {
    return FloatSize(m_size);
  }
  bool isOpaque() const override { return m_isOpaque; }
  bool isAccelerated() const override { return false; }
  int sourceWidth() override { return m_size.width(); }
  int sourceHeight() override { return m_size.height(); }

  ~FakeImageSource() override {}

 private:
  IntSize m_size;
  RefPtr<Image> m_image;
  bool m_isOpaque;
};

FakeImageSource::FakeImageSource(IntSize size, BitmapOpacity opacity)
    : m_size(size), m_isOpaque(opacity == OpaqueBitmap) {
  sk_sp<SkSurface> surface(
      SkSurface::MakeRasterN32Premul(m_size.width(), m_size.height()));
  surface->getCanvas()->clear(opacity == OpaqueBitmap ? SK_ColorWHITE
                                                      : SK_ColorTRANSPARENT);
  m_image = StaticBitmapImage::create(surface->makeImageSnapshot());
}

PassRefPtr<Image> FakeImageSource::getSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize&) const {
  if (status)
    *status = NormalSourceImageStatus;
  return m_image;
}

class CanvasRenderingContextUsageTrackingTest : public ::testing::Test {
 protected:
  CanvasRenderingContextUsageTrackingTest();
  void SetUp() override;

  DummyPageHolder& page() const { return *m_dummyPageHolder; }
  Document& document() const { return *m_document; }
  HTMLCanvasElement& canvasElement() const { return *m_canvasElement; }
  CanvasRenderingContext2D* context2d() const;

  void createContext(OpacityMode);

  FakeImageSource m_opaqueBitmap;
  FakeImageSource m_alphaBitmap;
  Persistent<ImageData> m_fullImageData;
  void TearDown();

 private:
  std::shared_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Document> m_document;
  Persistent<HTMLCanvasElement> m_canvasElement;
  Persistent<MemoryCache> m_globalMemoryCache;
};

CanvasRenderingContextUsageTrackingTest::
    CanvasRenderingContextUsageTrackingTest()
    : m_opaqueBitmap(IntSize(10, 10), OpaqueBitmap),
      m_alphaBitmap(IntSize(10, 10), TransparentBitmap) {}

void CanvasRenderingContextUsageTrackingTest::TearDown() {
  ThreadState::current()->collectGarbage(
      BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);
  replaceMemoryCacheForTesting(m_globalMemoryCache.release());
}

CanvasRenderingContext2D* CanvasRenderingContextUsageTrackingTest::context2d()
    const {
  // If the following check fails, perhaps you forgot to call createContext
  // in your test?
  EXPECT_NE(nullptr, canvasElement().renderingContext());
  EXPECT_TRUE(canvasElement().renderingContext()->is2d());
  return static_cast<CanvasRenderingContext2D*>(
      canvasElement().renderingContext());
}

void CanvasRenderingContextUsageTrackingTest::createContext(
    OpacityMode opacityMode) {
  String canvasType("2d");
  CanvasContextCreationAttributes attributes;
  attributes.setAlpha(opacityMode == NonOpaque);
  m_canvasElement->getCanvasRenderingContext(canvasType, attributes);
  context2d();  // Calling this for the checks
}

void CanvasRenderingContextUsageTrackingTest::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);
  m_document = &m_dummyPageHolder->document();
  m_document->documentElement()->setInnerHTML(
      "<body><canvas id='c'></canvas></body>");
  m_document->view()->updateAllLifecyclePhases();
  m_canvasElement = toHTMLCanvasElement(m_document->getElementById("c"));
  m_fullImageData = ImageData::create(IntSize(10, 10));

  m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());

  RuntimeEnabledFeatures::setEnableCanvas2dDynamicRenderingModeSwitchingEnabled(
      true);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, FillTracking) {
  createContext(NonOpaque);

  int numReps = 100;
  for (int i = 0; i < numReps; i++) {
    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(4, 1);
    context2d()->lineTo(4, 4);
    context2d()->lineTo(2, 2);
    context2d()->lineTo(1, 4);
    context2d()->fill();  // path is not convex

    context2d()->fillText("Hello World!!!", 10, 10, 1);
    context2d()->fillRect(10, 10, 100, 100);
    context2d()->fillText("Hello World!!!", 10, 10);

    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(4, 1);
    context2d()->lineTo(4, 4);
    context2d()->lineTo(1, 4);
    context2d()->fill();  // path is convex

    context2d()->fillRect(10, 10, 100, 100);
  }

  EXPECT_EQ(
      numReps * 2,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
  EXPECT_EQ(
      numReps * 2,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillRect]);
  EXPECT_EQ(numReps, context2d()->getUsage().numNonConvexFillPathCalls);
  EXPECT_EQ(
      numReps * 2,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);

  // areas
  EXPECT_NEAR(static_cast<double>(numReps * (100 * 100) * 2),
              context2d()
                  ->getUsage()
                  .boundingBoxAreaDrawCalls[BaseRenderingContext2D::FillRect],
              1.0);
  EXPECT_NEAR(
      static_cast<double>(numReps * (2 * 100 + 2 * 100) * 2),
      context2d()
          ->getUsage()
          .boundingBoxPerimeterDrawCalls[BaseRenderingContext2D::FillRect],
      1.0);

  EXPECT_TRUE(
      static_cast<double>(numReps * (100 * 100) * 2) <
      context2d()
          ->getUsage()
          .boundingBoxAreaFillType[BaseRenderingContext2D::ColorFillType]);
  EXPECT_TRUE(
      static_cast<double>(numReps * (100 * 100) * 2) * 1.1 >
      context2d()
          ->getUsage()
          .boundingBoxAreaFillType[BaseRenderingContext2D::ColorFillType]);

  EXPECT_NEAR(static_cast<double>(numReps * (3 * 3) * 2),
              context2d()
                  ->getUsage()
                  .boundingBoxAreaDrawCalls[BaseRenderingContext2D::FillPath],
              1.0);
  EXPECT_NEAR(
      static_cast<double>(numReps * (2 * 3 + 2 * 3) * 2),
      context2d()
          ->getUsage()
          .boundingBoxPerimeterDrawCalls[BaseRenderingContext2D::FillPath],
      1.0);

  EXPECT_TRUE(
      context2d()
          ->getUsage()
          .boundingBoxPerimeterDrawCalls[BaseRenderingContext2D::FillText] >
      0.0);
  EXPECT_TRUE(
      context2d()
          ->getUsage()
          .boundingBoxPerimeterDrawCalls[BaseRenderingContext2D::StrokeText] ==
      0.0);

  // create gradient
  CanvasGradient* gradient;
  gradient = context2d()->createLinearGradient(0, 0, 100, 100);
  context2d()->setFillStyle(
      StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
  context2d()->fillRect(10, 10, 100, 20);
  EXPECT_EQ(1, context2d()->getUsage().numLinearGradients);
  EXPECT_NEAR(100 * 20, context2d()->getUsage().boundingBoxAreaFillType
                            [BaseRenderingContext2D::LinearGradientFillType],
              1.0);

  NonThrowableExceptionState exceptionState;
  gradient = context2d()->createRadialGradient(0, 0, 100, 100, 200, 200,
                                               exceptionState);
  context2d()->setFillStyle(
      StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
  context2d()->fillRect(10, 10, 100, 20);
  EXPECT_EQ(1, context2d()->getUsage().numLinearGradients);
  EXPECT_EQ(1, context2d()->getUsage().numRadialGradients);
  EXPECT_NEAR(100 * 20, context2d()->getUsage().boundingBoxAreaFillType
                            [BaseRenderingContext2D::RadialGradientFillType],
              1.0);

  // create pattern
  NonThrowableExceptionState exceptionState2;
  ExecutionContext* execCtx = canvasElement().getExecutionContext();
  CanvasPattern* canvasPattern = context2d()->createPattern(
      execCtx, &m_opaqueBitmap, "repeat-x", exceptionState2);
  StringOrCanvasGradientOrCanvasPattern pattern =
      StringOrCanvasGradientOrCanvasPattern::fromCanvasPattern(canvasPattern);
  context2d()->setFillStyle(pattern);
  context2d()->fillRect(10, 10, 200, 100);
  EXPECT_EQ(1, context2d()->getUsage().numPatterns);
  EXPECT_NEAR(
      200 * 100,
      context2d()
          ->getUsage()
          .boundingBoxAreaFillType[BaseRenderingContext2D::PatternFillType],
      1.0);

  // Other fill method
  context2d()->fill(Path2D::create("Hello World!!!"));
  EXPECT_EQ(
      numReps * 2 + 1,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);

  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);
  EXPECT_EQ(0, context2d()->getUsage().numPutImageDataCalls);
  EXPECT_EQ(0, context2d()
                   ->getUsage()
                   .numDrawCalls[BaseRenderingContext2D::DrawVectorImage]);
  EXPECT_EQ(0, context2d()
                   ->getUsage()
                   .numDrawCalls[BaseRenderingContext2D::DrawBitmapImage]);
  EXPECT_EQ(0, context2d()->getUsage().numGetImageDataCalls);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, StrokeTracking) {
  createContext(NonOpaque);

  int numReps = 100;
  for (int i = 0; i < numReps; i++) {
    context2d()->strokeRect(10, 10, 100, 100);

    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(4, 1);
    context2d()->lineTo(4, 4);
    context2d()->lineTo(2, 2);
    context2d()->lineTo(1, 4);
    context2d()->stroke();

    context2d()->strokeText("Hello World!!!", 10, 10, 1);

    context2d()->strokeRect(10, 10, 100, 100);
    context2d()->strokeText("Hello World!!!", 10, 10);
  }

  EXPECT_EQ(
      numReps,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
  EXPECT_EQ(
      numReps * 2,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeRect]);
  EXPECT_EQ(
      numReps * 2,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);

  EXPECT_NEAR(static_cast<double>(numReps * (100 * 100) * 2),
              context2d()
                  ->getUsage()
                  .boundingBoxAreaDrawCalls[BaseRenderingContext2D::StrokeRect],
              1.0);
  EXPECT_NEAR(
      static_cast<double>(numReps * (2 * 100 + 2 * 100) * 2),
      context2d()
          ->getUsage()
          .boundingBoxPerimeterDrawCalls[BaseRenderingContext2D::StrokeRect],
      1.0);
  EXPECT_NEAR(static_cast<double>(numReps * (3 * 3)),
              context2d()
                  ->getUsage()
                  .boundingBoxAreaDrawCalls[BaseRenderingContext2D::StrokePath],
              1.0);
  EXPECT_NEAR(
      static_cast<double>(numReps * (2 * 3 + 2 * 3)),
      context2d()
          ->getUsage()
          .boundingBoxPerimeterDrawCalls[BaseRenderingContext2D::StrokePath],
      1.0);

  // create gradient
  CanvasGradient* gradient;
  gradient = context2d()->createLinearGradient(0, 0, 100, 100);
  context2d()->setStrokeStyle(
      StringOrCanvasGradientOrCanvasPattern::fromCanvasGradient(gradient));
  context2d()->strokeRect(10, 10, 100, 100);
  EXPECT_EQ(1, context2d()->getUsage().numLinearGradients);
  EXPECT_NEAR(100 * 100, context2d()->getUsage().boundingBoxAreaFillType
                             [BaseRenderingContext2D::LinearGradientFillType],
              1.0);

  // create pattern
  NonThrowableExceptionState exceptionState;
  ExecutionContext* execCtx = canvasElement().getExecutionContext();
  CanvasPattern* canvasPattern = context2d()->createPattern(
      execCtx, &m_opaqueBitmap, "repeat-x", exceptionState);
  StringOrCanvasGradientOrCanvasPattern pattern =
      StringOrCanvasGradientOrCanvasPattern::fromCanvasPattern(canvasPattern);
  context2d()->setStrokeStyle(pattern);
  context2d()->strokeRect(10, 10, 100, 100);
  EXPECT_EQ(1, context2d()->getUsage().numPatterns);
  EXPECT_NEAR(
      100 * 100,
      context2d()
          ->getUsage()
          .boundingBoxAreaFillType[BaseRenderingContext2D::PatternFillType],
      1.0);

  // Other stroke method
  context2d()->stroke(Path2D::create("Hello World!!!"));
  EXPECT_EQ(
      numReps + 1,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
  EXPECT_EQ(0, context2d()->getUsage().numNonConvexFillPathCalls);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);
  EXPECT_EQ(0, context2d()->getUsage().numPutImageDataCalls);
  EXPECT_EQ(0, context2d()
                   ->getUsage()
                   .numDrawCalls[BaseRenderingContext2D::DrawVectorImage]);
  EXPECT_EQ(0, context2d()
                   ->getUsage()
                   .numDrawCalls[BaseRenderingContext2D::DrawBitmapImage]);
  EXPECT_EQ(0, context2d()->getUsage().numGetImageDataCalls);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ImageTracking) {
  // Testing draw, put, get and filters
  createContext(NonOpaque);
  NonThrowableExceptionState exceptionState;

  int numReps = 5;
  int imgWidth = 100;
  int imgHeight = 200;
  for (int i = 0; i < numReps; i++) {
    context2d()->putImageData(m_fullImageData.get(), 0, 0, exceptionState);
    context2d()->drawImage(canvasElement().getExecutionContext(),
                           &m_opaqueBitmap, 0, 0, 1, 1, 0, 0, imgWidth,
                           imgHeight, exceptionState);
    context2d()->getImageData(0, 0, 10, 100, exceptionState);
  }

  EXPECT_NEAR(
      numReps * imgWidth * imgHeight,
      context2d()
          ->getUsage()
          .boundingBoxAreaDrawCalls[BaseRenderingContext2D::DrawBitmapImage],
      0.1);
  EXPECT_NEAR(numReps * (2 * imgWidth + 2 * imgHeight),
              context2d()->getUsage().boundingBoxPerimeterDrawCalls
                  [BaseRenderingContext2D::DrawBitmapImage],
              0.1);

  EXPECT_NEAR(
      0.0,
      context2d()
          ->getUsage()
          .boundingBoxAreaDrawCalls[BaseRenderingContext2D::DrawVectorImage],
      0.1);
  EXPECT_NEAR(0.0, context2d()->getUsage().boundingBoxPerimeterDrawCalls
                       [BaseRenderingContext2D::DrawVectorImage],
              0.1);

  context2d()->setFilter("blur(5px)");
  context2d()->drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap,
                         0, 0, 1, 1, 0, 0, 10, 10, exceptionState);

  EXPECT_EQ(numReps, context2d()->getUsage().numPutImageDataCalls);
  EXPECT_NE(0, context2d()->getUsage().areaPutImageDataCalls);
  EXPECT_NEAR(numReps * m_fullImageData.get()->width() *
                  m_fullImageData.get()->height(),
              context2d()->getUsage().areaPutImageDataCalls, 0.1);

  EXPECT_EQ(numReps + 1,
            context2d()
                ->getUsage()
                .numDrawCalls[BaseRenderingContext2D::DrawBitmapImage]);
  EXPECT_EQ(numReps, context2d()->getUsage().numGetImageDataCalls);

  EXPECT_EQ(1, context2d()->getUsage().numFilters);
  EXPECT_NEAR(numReps * 10 * 100, context2d()->getUsage().areaGetImageDataCalls,
              0.1);

  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
  EXPECT_EQ(0, context2d()->getUsage().numNonConvexFillPathCalls);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ClearRectTracking) {
  createContext(NonOpaque);
  context2d()->clearRect(0, 0, 1, 1);
  context2d()->clearRect(0, 0, 2, 2);
  EXPECT_EQ(2, context2d()->getUsage().numClearRectCalls);

  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillPath]);
  EXPECT_EQ(0, context2d()->getUsage().numNonConvexFillPathCalls);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillText]);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokePath]);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::StrokeText]);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, ComplexClipsTracking) {
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

TEST_F(CanvasRenderingContextUsageTrackingTest, ShadowsTracking) {
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
  EXPECT_NEAR(
      0.0, context2d()->getUsage().boundingBoxAreaTimesShadowBlurSquared, 0.1);
  EXPECT_NEAR(
      0.0, context2d()->getUsage().boundingBoxPerimeterTimesShadowBlurSquared,
      0.1);

  int numReps = 5;
  double shadowBlur = 0.5;
  for (int i = 0; i < numReps; i++) {
    context2d()->beginPath();
    context2d()->setShadowBlur(shadowBlur);
    context2d()->setShadowColor("rgba(255, 0, 0, 0.5)");
    context2d()->setShadowOffsetX(1.0);
    context2d()->setShadowOffsetY(1.0);
    context2d()->fillRect(0, 0, 3, 10);
  }
  EXPECT_EQ(numReps, context2d()->getUsage().numBlurredShadows);

  double areaTimesShadowBlurSquared =
      shadowBlur * shadowBlur * numReps * 3 * 10;
  double perimeterTimesShadowBlurSquared =
      shadowBlur * shadowBlur * numReps * (2 * 3 + 2 * 10);
  EXPECT_NEAR(areaTimesShadowBlurSquared,
              context2d()->getUsage().boundingBoxAreaTimesShadowBlurSquared,
              0.1);
  EXPECT_NEAR(
      perimeterTimesShadowBlurSquared,
      context2d()->getUsage().boundingBoxPerimeterTimesShadowBlurSquared, 0.1);

  double shadowBlur2 = 4.5;
  context2d()->beginPath();
  context2d()->setShadowBlur(shadowBlur2);
  context2d()->moveTo(1, 1);
  context2d()->lineTo(4, 1);
  context2d()->lineTo(4, 4);
  context2d()->lineTo(2, 2);
  context2d()->lineTo(1, 4);
  context2d()->fill();

  areaTimesShadowBlurSquared += 3.0 * 3.0 * shadowBlur2 * shadowBlur2;
  perimeterTimesShadowBlurSquared +=
      (2 * 3 + 2 * 3) * shadowBlur2 * shadowBlur2;
  EXPECT_NEAR(areaTimesShadowBlurSquared,
              context2d()->getUsage().boundingBoxAreaTimesShadowBlurSquared,
              0.1);
  EXPECT_NEAR(
      perimeterTimesShadowBlurSquared,
      context2d()->getUsage().boundingBoxPerimeterTimesShadowBlurSquared, 0.1);

  NonThrowableExceptionState exceptionState;
  int imgWidth = 100;
  int imgHeight = 200;
  for (int i = 0; i < numReps; i++) {
    context2d()->putImageData(m_fullImageData.get(), 0, 0, exceptionState);
    context2d()->setShadowBlur(shadowBlur2);
    context2d()->drawImage(canvasElement().getExecutionContext(),
                           &m_opaqueBitmap, 0, 0, 1, 1, 0, 0, imgWidth,
                           imgHeight, exceptionState);
    context2d()->getImageData(0, 0, 1, 1, exceptionState);
  }

  areaTimesShadowBlurSquared +=
      imgWidth * imgHeight * shadowBlur2 * shadowBlur2 * numReps;
  perimeterTimesShadowBlurSquared +=
      (2 * imgWidth + 2 * imgHeight) * shadowBlur2 * shadowBlur2 * numReps;
  EXPECT_NEAR(areaTimesShadowBlurSquared,
              context2d()->getUsage().boundingBoxAreaTimesShadowBlurSquared,
              0.1);
  EXPECT_NEAR(
      perimeterTimesShadowBlurSquared,
      context2d()->getUsage().boundingBoxPerimeterTimesShadowBlurSquared, 0.1);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, incrementFrameCount) {
  createContext(NonOpaque);
  EXPECT_EQ(0, context2d()->getUsage().numFramesSinceReset);

  context2d()->incrementFrameCount();
  EXPECT_EQ(1, context2d()->getUsage().numFramesSinceReset);

  context2d()->incrementFrameCount();
  context2d()->incrementFrameCount();
  EXPECT_EQ(3, context2d()->getUsage().numFramesSinceReset);
}

TEST_F(CanvasRenderingContextUsageTrackingTest, resetUsageTracking) {
  createContext(NonOpaque);
  EXPECT_EQ(0, context2d()->getUsage().numFramesSinceReset);

  context2d()->incrementFrameCount();
  context2d()->incrementFrameCount();
  EXPECT_EQ(2, context2d()->getUsage().numFramesSinceReset);

  const int numReps = 100;
  for (int i = 0; i < numReps; i++) {
    context2d()->fillRect(10, 10, 100, 100);
  }
  EXPECT_EQ(
      numReps,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillRect]);

  context2d()->resetUsageTracking();
  EXPECT_EQ(0, context2d()->getUsage().numFramesSinceReset);
  EXPECT_EQ(
      0,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillRect]);

  for (int i = 0; i < numReps; i++) {
    context2d()->fillRect(10, 10, 100, 100);
  }
  EXPECT_EQ(
      numReps,
      context2d()->getUsage().numDrawCalls[BaseRenderingContext2D::FillRect]);
}

}  // namespace UsageTrackingTests
}  // namespace blink
