// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/CanvasRenderingContext2D.h"

#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameView.h"
#include "core/frame/ImageBitmap.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/ImageData.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/PtrUtil.h"
#include <memory>

using ::testing::Mock;

namespace blink {

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
    RefPtr<SkImage> image = fromSkSp(surface->makeImageSnapshot());
    m_image = StaticBitmapImage::create(image);
}

PassRefPtr<Image> FakeImageSource::getSourceImageForCanvas(SourceImageStatus* status, AccelerationHint, SnapshotReason, const FloatSize&) const
{
    if (status)
        *status = NormalSourceImageStatus;
    return m_image;
}

//============================================================================

class CanvasRenderingContext2DTest : public ::testing::Test {
protected:
    CanvasRenderingContext2DTest();
    void SetUp() override;

    DummyPageHolder& page() const { return *m_dummyPageHolder; }
    HTMLDocument& document() const { return *m_document; }
    HTMLCanvasElement& canvasElement() const { return *m_canvasElement; }
    CanvasRenderingContext2D* context2d() const { return static_cast<CanvasRenderingContext2D*>(canvasElement().renderingContext()); }
    intptr_t getGlobalGPUMemoryUsage() const { return ImageBuffer::getGlobalGPUMemoryUsage(); }
    unsigned getGlobalAcceleratedImageBufferCount() const { return ImageBuffer::getGlobalAcceleratedImageBufferCount(); }
    intptr_t getCurrentGPUMemoryUsage() const { return canvasElement().buffer()->getGPUMemoryUsage(); }

    void createContext(OpacityMode);
    void TearDown();
    void unrefCanvas();

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLDocument> m_document;
    Persistent<HTMLCanvasElement> m_canvasElement;
    Persistent<MemoryCache> m_globalMemoryCache;

    class WrapGradients final : public GarbageCollectedFinalized<WrapGradients> {
    public:
        static WrapGradients* create()
        {
            return new WrapGradients;
        }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(m_opaqueGradient);
            visitor->trace(m_alphaGradient);
        }

        StringOrCanvasGradientOrCanvasPattern m_opaqueGradient;
        StringOrCanvasGradientOrCanvasPattern m_alphaGradient;
    };

    // TODO(Oilpan): avoid tedious part-object wrapper by supporting on-heap ::testing::Tests.
    Persistent<WrapGradients> m_wrapGradients;

protected:
    // Pre-canned objects for testing
    Persistent<ImageData> m_fullImageData;
    Persistent<ImageData> m_partialImageData;
    FakeImageSource m_opaqueBitmap;
    FakeImageSource m_alphaBitmap;

    StringOrCanvasGradientOrCanvasPattern& opaqueGradient() { return m_wrapGradients->m_opaqueGradient; }
    StringOrCanvasGradientOrCanvasPattern& alphaGradient() { return m_wrapGradients->m_alphaGradient; }
};

CanvasRenderingContext2DTest::CanvasRenderingContext2DTest()
    : m_wrapGradients(WrapGradients::create())
    , m_opaqueBitmap(IntSize(10, 10), OpaqueBitmap)
    , m_alphaBitmap(IntSize(10, 10), TransparentBitmap)
{ }

void CanvasRenderingContext2DTest::createContext(OpacityMode opacityMode)
{
    String canvasType("2d");
    CanvasContextCreationAttributes attributes;
    attributes.setAlpha(opacityMode == NonOpaque);
    m_canvasElement->getCanvasRenderingContext(canvasType, attributes);
}

void CanvasRenderingContext2DTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    m_document->documentElement()->setInnerHTML("<body><canvas id='c'></canvas></body>", ASSERT_NO_EXCEPTION);
    m_document->view()->updateAllLifecyclePhases();
    m_canvasElement = toHTMLCanvasElement(m_document->getElementById("c"));

    m_fullImageData = ImageData::create(IntSize(10, 10));
    m_partialImageData = ImageData::create(IntSize(2, 2));

    NonThrowableExceptionState exceptionState;
    CanvasGradient* opaqueGradient = CanvasGradient::create(FloatPoint(0, 0), FloatPoint(10, 0));
    opaqueGradient->addColorStop(0, String("green"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    opaqueGradient->addColorStop(1, String("blue"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    this->opaqueGradient().setCanvasGradient(opaqueGradient);

    CanvasGradient* alphaGradient = CanvasGradient::create(FloatPoint(0, 0), FloatPoint(10, 0));
    alphaGradient->addColorStop(0, String("green"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    alphaGradient->addColorStop(1, String("rgba(0, 0, 255, 0.5)"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    StringOrCanvasGradientOrCanvasPattern wrappedAlphaGradient;
    this->alphaGradient().setCanvasGradient(alphaGradient);

    m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
}

void CanvasRenderingContext2DTest::TearDown()
{
    ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);
    replaceMemoryCacheForTesting(m_globalMemoryCache.release());
}

//============================================================================

class FakeAcceleratedImageBufferSurfaceForTesting : public UnacceleratedImageBufferSurface {
public:
    FakeAcceleratedImageBufferSurfaceForTesting(const IntSize& size, OpacityMode mode)
        : UnacceleratedImageBufferSurface(size, mode)
        , m_isAccelerated(true) { }
    ~FakeAcceleratedImageBufferSurfaceForTesting() override { }
    bool isAccelerated() const override { return m_isAccelerated; }
    void setIsAccelerated(bool isAccelerated)
    {
        if (isAccelerated != m_isAccelerated)
            m_isAccelerated = isAccelerated;
    }

private:
    bool m_isAccelerated;
};

//============================================================================

class MockImageBufferSurfaceForOverwriteTesting : public UnacceleratedImageBufferSurface {
public:
    MockImageBufferSurfaceForOverwriteTesting(const IntSize& size, OpacityMode mode) : UnacceleratedImageBufferSurface(size, mode) { }
    ~MockImageBufferSurfaceForOverwriteTesting() override { }
    bool isRecording() const override { return true; } // otherwise overwrites are not tracked

    MOCK_METHOD0(willOverwriteCanvas, void());
};

//============================================================================

#define TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        std::unique_ptr<MockImageBufferSurfaceForOverwriteTesting> mockSurface = wrapUnique(new MockImageBufferSurfaceForOverwriteTesting(IntSize(10, 10), NonOpaque)); \
        MockImageBufferSurfaceForOverwriteTesting* surfacePtr = mockSurface.get(); \
        canvasElement().createImageBufferUsingSurfaceForTesting(std::move(mockSurface)); \
        EXPECT_CALL(*surfacePtr, willOverwriteCanvas()).Times(EXPECTED_OVERDRAWS); \
        context2d()->save();

#define TEST_OVERDRAW_FINALIZE \
        context2d()->restore(); \
        Mock::VerifyAndClearExpectations(surfacePtr);

#define TEST_OVERDRAW_1(EXPECTED_OVERDRAWS, CALL1) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

#define TEST_OVERDRAW_2(EXPECTED_OVERDRAWS, CALL1, CALL2) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        context2d()->CALL2; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

#define TEST_OVERDRAW_3(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        context2d()->CALL2; \
        context2d()->CALL3; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

#define TEST_OVERDRAW_4(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3, CALL4) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        context2d()->CALL2; \
        context2d()->CALL3; \
        context2d()->CALL4; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

//============================================================================

class MockSurfaceFactory : public RecordingImageBufferFallbackSurfaceFactory {
public:
    enum FallbackExpectation {
        ExpectFallback,
        ExpectNoFallback
    };
    static std::unique_ptr<MockSurfaceFactory> create(FallbackExpectation expectation) { return wrapUnique(new MockSurfaceFactory(expectation)); }

    std::unique_ptr<ImageBufferSurface> createSurface(const IntSize& size, OpacityMode mode) override
    {
        EXPECT_EQ(ExpectFallback, m_expectation);
        m_didFallback = true;
        return wrapUnique(new UnacceleratedImageBufferSurface(size, mode));
    }

    ~MockSurfaceFactory() override
    {
        if (m_expectation == ExpectFallback) {
            EXPECT_TRUE(m_didFallback);
        }
    }

private:
    MockSurfaceFactory(FallbackExpectation expectation)
        : m_expectation(expectation)
        , m_didFallback(false) { }

    FallbackExpectation m_expectation;
    bool m_didFallback;
};

//============================================================================

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithFillRect)
{
    createContext(NonOpaque);

    TEST_OVERDRAW_1(1, fillRect(-1, -1, 12, 12));
    TEST_OVERDRAW_1(1, fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_1(0, strokeRect(0, 0, 10, 10)); // stroking instead of filling does not overwrite
    TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_1(0, fillRect(0, 0, 9, 9));
    TEST_OVERDRAW_2(0, translate(1, 1), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, translate(1, 1), fillRect(-1, -1, 10, 10));
    TEST_OVERDRAW_2(1, setFillStyle(opaqueGradient()), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(0, setFillStyle(alphaGradient()), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(0, setGlobalAlpha(0.5), setFillStyle(opaqueGradient()), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 9, 9));
    TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_4(0, rect(0, 0, 5, 5), clip(), setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithClearRect)
{
    createContext(NonOpaque);

    TEST_OVERDRAW_1(1, clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_1(0, clearRect(0, 0, 9, 9));
    TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f), clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setFillStyle(alphaGradient()), clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(0, translate(1, 1), clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, translate(1, 1), clearRect(-1, -1, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("destination-in")), clearRect(0, 0, 10, 10)); // composite op ignored
    TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), clearRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithDrawImage)
{
    createContext(NonOpaque);
    NonThrowableExceptionState exceptionState;

    TEST_OVERDRAW_1(1, drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(1, drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 1, 1, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, drawImage(canvasElement().getExecutionContext(), &m_alphaBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), drawImage(canvasElement().getExecutionContext(), &m_alphaBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 1, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 9, 9, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(1, drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 11, 11, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, translate(-1, 0), drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 1, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, translate(-1, 0), drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, setFillStyle(opaqueGradient()), drawImage(canvasElement().getExecutionContext(), &m_alphaBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState)); // fillStyle ignored by drawImage
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, setFillStyle(alphaGradient()), drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState)); // fillStyle ignored by drawImage
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 1, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), drawImage(canvasElement().getExecutionContext(), &m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithPutImageData)
{
    createContext(NonOpaque);
    NonThrowableExceptionState exceptionState;

    // Test putImageData
    TEST_OVERDRAW_1(1, putImageData(m_fullImageData.get(), 0, 0, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(1, putImageData(m_fullImageData.get(), 0, 0, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, putImageData(m_fullImageData.get(), 0, 0, 1, 1, 8, 8, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f), putImageData(m_fullImageData.get(), 0, 0, exceptionState)); // alpha has no effect
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, putImageData(m_partialImageData.get(), 0, 0, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, translate(1, 1), putImageData(m_fullImageData.get(), 0, 0, exceptionState)); // ignores tranforms
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, putImageData(m_fullImageData.get(), 1, 0, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_3(1, rect(0, 0, 5, 5), clip(), putImageData(m_fullImageData.get(), 0, 0, exceptionState)); // ignores clip
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithCompositeOperations)
{
    createContext(NonOpaque);

    // Test composite operators with an opaque rect that covers the entire canvas
    // Note: all the untested composite operations take the same code path as source-in,
    // which assumes that the destination may not be overwritten
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("clear")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("source-over")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")), fillRect(0, 0, 10, 10));
    // Test composite operators with a transparent rect that covers the entire canvas
    TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("clear")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("source-over")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("source-in")), fillRect(0, 0, 10, 10));
    // Test composite operators with an opaque rect that does not cover the entire canvas
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("clear")), fillRect(0, 0, 5, 5));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 5, 5));
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-over")), fillRect(0, 0, 5, 5));
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")), fillRect(0, 0, 5, 5));
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionByDefault)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionUnderOverdrawLimit)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->setGlobalAlpha(0.5f); // To prevent overdraw optimization
    for (int i = 0; i < ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold - 1; i++) {
        context2d()->fillRect(0, 0, 10, 10);
    }

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionOverOverdrawLimit)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->setGlobalAlpha(0.5f); // To prevent overdraw optimization
    for (int i = 0; i < ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold; i++) {
        context2d()->fillRect(0, 0, 10, 10);
    }

    EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionUnderImageSizeRatioLimit)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    NonThrowableExceptionState exceptionState;
    Element* sourceCanvasElement = document().createElement("canvas", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    HTMLCanvasElement* sourceCanvas = static_cast<HTMLCanvasElement*>(sourceCanvasElement);
    IntSize sourceSize(10, 10 * ExpensiveCanvasHeuristicParameters::ExpensiveImageSizeRatio);
    std::unique_ptr<UnacceleratedImageBufferSurface> sourceSurface = wrapUnique(new UnacceleratedImageBufferSurface(sourceSize, NonOpaque));
    sourceCanvas->createImageBufferUsingSurfaceForTesting(std::move(sourceSurface));

    const ImageBitmapOptions defaultOptions;
    // Go through an ImageBitmap to avoid triggering a display list fallback
    ImageBitmap* sourceImageBitmap = ImageBitmap::create(sourceCanvas, IntRect(IntPoint(0, 0), sourceSize), defaultOptions);

    context2d()->drawImage(canvasElement().getExecutionContext(), sourceImageBitmap, 0, 0, 1, 1, 0, 0, 1, 1, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionOverImageSizeRatioLimit)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    NonThrowableExceptionState exceptionState;
    Element* sourceCanvasElement = document().createElement("canvas", exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    HTMLCanvasElement* sourceCanvas = static_cast<HTMLCanvasElement*>(sourceCanvasElement);
    IntSize sourceSize(10, 10 * ExpensiveCanvasHeuristicParameters::ExpensiveImageSizeRatio + 1);
    std::unique_ptr<UnacceleratedImageBufferSurface> sourceSurface = wrapUnique(new UnacceleratedImageBufferSurface(sourceSize, NonOpaque));
    sourceCanvas->createImageBufferUsingSurfaceForTesting(std::move(sourceSurface));

    const ImageBitmapOptions defaultOptions;
    // Go through an ImageBitmap to avoid triggering a display list fallback
    ImageBitmap* sourceImageBitmap = ImageBitmap::create(sourceCanvas, IntRect(IntPoint(0, 0), sourceSize), defaultOptions);

    context2d()->drawImage(canvasElement().getExecutionContext(), sourceImageBitmap, 0, 0, 1, 1, 0, 0, 1, 1, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionUnderExpensivePathPointCount)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->beginPath();
    context2d()->moveTo(7, 5);
    for (int i = 1; i < ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount-1; i++) {
        float angleRad = twoPiFloat * i / (ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount - 1);
        context2d()->lineTo(5 + 2 * cos(angleRad), 5 + 2 * sin(angleRad));
    }
    context2d()->fill();

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionOverExpensivePathPointCount)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->beginPath();
    context2d()->moveTo(7, 5);
    for (int i = 1; i < ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount + 1; i++) {
        float angleRad = twoPiFloat * i / (ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount + 1);
        context2d()->lineTo(5 + 2 * cos(angleRad), 5 + 2 * sin(angleRad));
    }
    context2d()->fill();

    EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWhenPathIsConcave)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(5, 5);
    context2d()->lineTo(9, 1);
    context2d()->lineTo(5, 9);
    context2d()->fill();

    if (ExpensiveCanvasHeuristicParameters::ConcavePathsAreExpensive) {
        EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
    } else {
        EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
    }
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionWithRectangleClip)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->beginPath();
    context2d()->rect(1, 1, 2, 2);
    context2d()->clip();
    context2d()->fillRect(0, 0, 4, 4);

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWithComplexClip)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(5, 5);
    context2d()->lineTo(9, 1);
    context2d()->lineTo(5, 9);
    context2d()->clip();
    context2d()->fillRect(0, 0, 4, 4);

    if (ExpensiveCanvasHeuristicParameters::ComplexClipsAreExpensive) {
        EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
    } else {
        EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
    }
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWithBlurredShadow)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->setShadowColor(String("red"));
    context2d()->setShadowBlur(1.0f);
    context2d()->fillRect(1, 1, 1, 1);

    if (ExpensiveCanvasHeuristicParameters::BlurredShadowsAreExpensive) {
        EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
    } else {
        EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
    }
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionWithSharpShadow)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->setShadowColor(String("red"));
    context2d()->setShadowOffsetX(1.0f);
    context2d()->fillRect(1, 1, 1, 1);

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, NoFallbackWithSmallState)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->fillRect(0, 0, 1, 1); // To have a non-empty dirty rect
    for (int i = 0; i < ExpensiveCanvasHeuristicParameters::ExpensiveRecordingStackDepth - 1; ++i) {
        context2d()->save();
        context2d()->translate(1.0f, 0.0f);
    }
    canvasElement().doDeferredPaintInvalidation(); // To close the current frame
}

TEST_F(CanvasRenderingContext2DTest, FallbackWithLargeState)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->fillRect(0, 0, 1, 1); // To have a non-empty dirty rect
    for (int i = 0; i < ExpensiveCanvasHeuristicParameters::ExpensiveRecordingStackDepth; ++i) {
        context2d()->save();
        context2d()->translate(1.0f, 0.0f);
    }
    canvasElement().doDeferredPaintInvalidation(); // To close the current frame
}

TEST_F(CanvasRenderingContext2DTest, OpaqueDisplayListFallsBackForText)
{
    // Verify that drawing text to an opaque canvas, which is expected to
    // render with subpixel text anti-aliasing, results in falling out
    // of display list mode because the current diplay list implementation
    // does not support pixel geometry settings.
    // See: crbug.com/583809
    createContext(Opaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectFallback), Opaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->fillText("Text", 0, 5);
}

TEST_F(CanvasRenderingContext2DTest, NonOpaqueDisplayListDoesNotFallBackForText)
{
    createContext(NonOpaque);
    std::unique_ptr<RecordingImageBufferSurface> surface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), MockSurfaceFactory::create(MockSurfaceFactory::ExpectNoFallback), NonOpaque));
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(surface));

    context2d()->fillText("Text", 0, 5);
}

TEST_F(CanvasRenderingContext2DTest, ImageResourceLifetime)
{
    NonThrowableExceptionState nonThrowableExceptionState;
    Element* canvasElement = document().createElement("canvas", nonThrowableExceptionState);
    EXPECT_FALSE(nonThrowableExceptionState.hadException());
    HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(canvasElement);
    canvas->setHeight(40);
    canvas->setWidth(40);
    ImageBitmap* imageBitmapDerived = nullptr;
    {
        const ImageBitmapOptions defaultOptions;
        ImageBitmap* imageBitmapFromCanvas = ImageBitmap::create(canvas, IntRect(0, 0, canvas->width(), canvas->height()), defaultOptions);
        imageBitmapDerived = ImageBitmap::create(imageBitmapFromCanvas, IntRect(0, 0, 20, 20), defaultOptions);
    }
    CanvasContextCreationAttributes attributes;
    CanvasRenderingContext2D* context = static_cast<CanvasRenderingContext2D*>(canvas->getCanvasRenderingContext("2d", attributes));
    TrackExceptionState exceptionState;
    CanvasImageSourceUnion imageSource;
    imageSource.setImageBitmap(imageBitmapDerived);
    context->drawImage(canvas->getExecutionContext(), imageSource, 0, 0, exceptionState);
}

TEST_F(CanvasRenderingContext2DTest, GPUMemoryUpdateForAcceleratedCanvas)
{
    createContext(NonOpaque);

    std::unique_ptr<FakeAcceleratedImageBufferSurfaceForTesting> fakeAccelerateSurface = wrapUnique(new FakeAcceleratedImageBufferSurfaceForTesting(IntSize(10, 10), NonOpaque));
    FakeAcceleratedImageBufferSurfaceForTesting* fakeAccelerateSurfacePtr = fakeAccelerateSurface.get();
    canvasElement().createImageBufferUsingSurfaceForTesting(std::move(fakeAccelerateSurface));
    // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per pixel per buffer,
    // and 2 is an estimate of num of gpu buffers required
    EXPECT_EQ(800, getCurrentGPUMemoryUsage());
    EXPECT_EQ(800, getGlobalGPUMemoryUsage());
    EXPECT_EQ(1u, getGlobalAcceleratedImageBufferCount());

    // Switching accelerated mode to non-accelerated mode
    fakeAccelerateSurfacePtr->setIsAccelerated(false);
    canvasElement().buffer()->updateGPUMemoryUsage();
    EXPECT_EQ(0, getCurrentGPUMemoryUsage());
    EXPECT_EQ(0, getGlobalGPUMemoryUsage());
    EXPECT_EQ(0u, getGlobalAcceleratedImageBufferCount());

    // Switching non-accelerated mode to accelerated mode
    fakeAccelerateSurfacePtr->setIsAccelerated(true);
    canvasElement().buffer()->updateGPUMemoryUsage();
    EXPECT_EQ(800, getCurrentGPUMemoryUsage());
    EXPECT_EQ(800, getGlobalGPUMemoryUsage());
    EXPECT_EQ(1u, getGlobalAcceleratedImageBufferCount());

    // Creating a different accelerated image buffer
    std::unique_ptr<FakeAcceleratedImageBufferSurfaceForTesting> fakeAccelerateSurface2 = wrapUnique(new FakeAcceleratedImageBufferSurfaceForTesting(IntSize(10, 5), NonOpaque));
    std::unique_ptr<ImageBuffer> imageBuffer2 = ImageBuffer::create(std::move(fakeAccelerateSurface2));
    EXPECT_EQ(800, getCurrentGPUMemoryUsage());
    EXPECT_EQ(1200, getGlobalGPUMemoryUsage());
    EXPECT_EQ(2u, getGlobalAcceleratedImageBufferCount());

    // Tear down the first image buffer that resides in current canvas element
    canvasElement().setSize(IntSize(20, 20));
    Mock::VerifyAndClearExpectations(fakeAccelerateSurfacePtr);
    EXPECT_EQ(400, getGlobalGPUMemoryUsage());
    EXPECT_EQ(1u, getGlobalAcceleratedImageBufferCount());

    // Tear down the second image buffer
    imageBuffer2.reset();
    EXPECT_EQ(0, getGlobalGPUMemoryUsage());
    EXPECT_EQ(0u, getGlobalAcceleratedImageBufferCount());
}

TEST_F(CanvasRenderingContext2DTest, CanvasDisposedBeforeContext)
{
    createContext(NonOpaque);
    context2d()->fillRect(0, 0, 1, 1); // results in task observer registration

    context2d()->detachCanvas();

    // This is the only method that is callable after detachCanvas
    // Test passes by not crashing.
    context2d()->didProcessTask();

    // Test passes by not crashing during teardown
}

TEST_F(CanvasRenderingContext2DTest, ContextDisposedBeforeCanvas)
{
    createContext(NonOpaque);

    canvasElement().detachContext();
    // Passes by not crashing later during teardown
}

} // namespace blink
