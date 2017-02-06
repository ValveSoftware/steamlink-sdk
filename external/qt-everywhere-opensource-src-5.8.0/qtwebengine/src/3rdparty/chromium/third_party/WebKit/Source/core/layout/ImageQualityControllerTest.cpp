// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ImageQualityController.h"

#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class ImageQualityControllerTest : public RenderingTest {
protected:
    ImageQualityController* controller() { return m_controller; }

private:
    void SetUp() override
    {
        m_controller = ImageQualityController::imageQualityController();
        RenderingTest::SetUp();
    }
    void TearDown() override
    {
    }

    ImageQualityController* m_controller;
};

TEST_F(ImageQualityControllerTest, RegularImage)
{
    setBodyInnerHTML("<img src='myimage'></img>");
    LayoutObject* obj = document().body()->firstChild()->layoutObject();

    EXPECT_EQ(InterpolationDefault, controller()->chooseInterpolationQuality(*obj, nullptr, nullptr, LayoutSize()));
}

TEST_F(ImageQualityControllerTest, ImageRenderingPixelated)
{
    setBodyInnerHTML("<img src='myimage' style='image-rendering: pixelated'></img>");
    LayoutObject* obj = document().body()->firstChild()->layoutObject();

    EXPECT_EQ(InterpolationNone, controller()->chooseInterpolationQuality(*obj, nullptr, nullptr, LayoutSize()));
}

#if !USE(LOW_QUALITY_IMAGE_INTERPOLATION)

class TestImageAnimated : public Image {
public:
    bool maybeAnimated() override { return true; }
    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override { return false; }
    IntSize size() const override { return IntSize(); }
    void destroyDecodedData() override { }
    void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) override { }
    PassRefPtr<SkImage> imageForCurrentFrame() override { return nullptr; }
};

TEST_F(ImageQualityControllerTest, ImageMaybeAnimated)
{
    setBodyInnerHTML("<img src='myimage'></img>");
    LayoutImage* img = toLayoutImage(document().body()->firstChild()->layoutObject());

    RefPtr<TestImageAnimated> testImage = adoptRef(new TestImageAnimated);
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*img, testImage.get(), nullptr, LayoutSize()));
}

class TestImageWithContrast : public Image {
public:
    bool maybeAnimated() override { return true; }
    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override { return false; }
    IntSize size() const override { return IntSize(); }
    void destroyDecodedData() override { }
    void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) override { }

    bool isBitmapImage() const override { return true; }
    PassRefPtr<SkImage> imageForCurrentFrame() override { return nullptr; }
};

TEST_F(ImageQualityControllerTest, LowQualityFilterForContrast)
{
    setBodyInnerHTML("<img src='myimage' style='image-rendering: -webkit-optimize-contrast'></img>");
    LayoutImage* img = toLayoutImage(document().body()->firstChild()->layoutObject());

    RefPtr<TestImageWithContrast> testImage = adoptRef(new TestImageWithContrast);
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize()));
}

class TestImageLowQuality : public Image {
public:
    bool maybeAnimated() override { return true; }
    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override { return false; }
    IntSize size() const override { return IntSize(1, 1); }
    void destroyDecodedData() override { }
    void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) override { }

    bool isBitmapImage() const override { return true; }
    PassRefPtr<SkImage> imageForCurrentFrame() override { return nullptr; }
};

TEST_F(ImageQualityControllerTest, MediumQualityFilterForUnscaledImage)
{
    setBodyInnerHTML("<img src='myimage'></img>");
    LayoutImage* img = toLayoutImage(document().body()->firstChild()->layoutObject());

    RefPtr<TestImageLowQuality> testImage = adoptRef(new TestImageLowQuality);
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(1, 1)));
}

class MockTaskRunner : public WebTaskRunner {
public:
    void setTime(double newTime) { m_time = newTime; }

    MockTaskRunner()
    : WebTaskRunner(), m_time(0.0), m_currentTask(nullptr)
    { }

    virtual ~MockTaskRunner()
    {
        if (m_currentTask)
            delete m_currentTask;
    }

private:
    void postTask(const WebTraceLocation&, Task*) override { }
    void postDelayedTask(const WebTraceLocation&, Task* task, double) override
    {
        if (m_currentTask)
            delete m_currentTask;
        m_currentTask = task;

    }
    WebTaskRunner* clone() override { return nullptr; }
    double virtualTimeSeconds() const override { return 0.0; }
    double monotonicallyIncreasingVirtualTimeSeconds() const override { return m_time; }

    double m_time;
    Task* m_currentTask;
};

class MockTimer : public Timer<ImageQualityController> {
    typedef void (ImageQualityController::*TimerFiredFunction)(Timer*);
public:
    MockTimer(ImageQualityController* o, TimerFiredFunction f)
    : Timer<ImageQualityController>(o, f, &m_taskRunner)
    {
    }

    void fire()
    {
        this->Timer<ImageQualityController>::fired();
        stop();
    }

    void setTime(double newTime)
    {
        m_taskRunner.setTime(newTime);
    }

private:
    MockTaskRunner m_taskRunner;
};

TEST_F(ImageQualityControllerTest, LowQualityFilterForResizingImage)
{
    MockTimer* mockTimer = new MockTimer(controller(), &ImageQualityController::highQualityRepaintTimerFired);
    controller()->setTimer(mockTimer);
    setBodyInnerHTML("<img src='myimage'></img>");
    LayoutImage* img = toLayoutImage(document().body()->firstChild()->layoutObject());

    RefPtr<TestImageLowQuality> testImage = adoptRef(new TestImageLowQuality);
    std::unique_ptr<PaintController> paintController = PaintController::create();
    GraphicsContext context(*paintController);

    // Paint once. This will kick off a timer to see if we resize it during that timer's execution.
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(2, 2)));

    // Go into low-quality mode now that the size changed.
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(3, 3)));

    // Stay in low-quality mode since the size changed again.
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(4, 4)));

    mockTimer->fire();
    // The timer fired before painting at another size, so this doesn't count as animation. Therefore not painting at low quality.
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(4, 4)));
}

TEST_F(ImageQualityControllerTest, MediumQualityFilterForNotAnimatedWhileAnotherAnimates)
{
    MockTimer* mockTimer = new MockTimer(controller(), &ImageQualityController::highQualityRepaintTimerFired);
    controller()->setTimer(mockTimer);
    setBodyInnerHTML("<img id='myAnimatingImage' src='myimage'></img> <img id='myNonAnimatingImage' src='myimage2'></img>");
    LayoutImage* animatingImage = toLayoutImage(document().getElementById("myAnimatingImage")->layoutObject());
    LayoutImage* nonAnimatingImage = toLayoutImage(document().getElementById("myNonAnimatingImage")->layoutObject());

    RefPtr<TestImageLowQuality> testImage = adoptRef(new TestImageLowQuality);
    std::unique_ptr<PaintController> paintController = PaintController::create();
    GraphicsContext context(*paintController);

    // Paint once. This will kick off a timer to see if we resize it during that timer's execution.
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*animatingImage, testImage.get(), testImage.get(), LayoutSize(2, 2)));

    // Go into low-quality mode now that the size changed.
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*animatingImage, testImage.get(), testImage.get(), LayoutSize(3, 3)));

    // The non-animating image receives a medium-quality filter, even though the other one is animating.
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*nonAnimatingImage, testImage.get(), testImage.get(), LayoutSize(4, 4)));

    // Now the second image has animated, so it also gets painted with a low-quality filter.
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*nonAnimatingImage, testImage.get(), testImage.get(), LayoutSize(3, 3)));

    mockTimer->fire();
    // The timer fired before painting at another size, so this doesn't count as animation. Therefore not painting at low quality for any image.
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*animatingImage, testImage.get(), testImage.get(), LayoutSize(4, 4)));
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*nonAnimatingImage, testImage.get(), testImage.get(), LayoutSize(4, 4)));
}

TEST_F(ImageQualityControllerTest, DontKickTheAnimationTimerWhenPaintingAtTheSameSize)
{
    MockTimer* mockTimer = new MockTimer(controller(), &ImageQualityController::highQualityRepaintTimerFired);
    controller()->setTimer(mockTimer);
    setBodyInnerHTML("<img src='myimage'></img>");
    LayoutImage* img = toLayoutImage(document().body()->firstChild()->layoutObject());

    RefPtr<TestImageLowQuality> testImage = adoptRef(new TestImageLowQuality);

    // Paint once. This will kick off a timer to see if we resize it during that timer's execution.
    EXPECT_EQ(InterpolationMedium, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(2, 2)));

    // Go into low-quality mode now that the size changed.
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(3, 3)));

    // Stay in low-quality mode since the size changed again.
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(4, 4)));

    mockTimer->stop();
    EXPECT_FALSE(mockTimer->isActive());
    // Painted at the same size, so even though timer is still executing, don't go to low quality.
    EXPECT_EQ(InterpolationLow, controller()->chooseInterpolationQuality(*img, testImage.get(), testImage.get(), LayoutSize(4, 4)));
    // Check that the timer was not kicked. It should not have been, since the image was painted at the same size as last time.
    EXPECT_FALSE(mockTimer->isActive());
}

TEST_F(ImageQualityControllerTest, DontRestartTimerUnlessAdvanced)
{
    MockTimer* mockTimer = new MockTimer(controller(), &ImageQualityController::highQualityRepaintTimerFired);
    controller()->setTimer(mockTimer);
    setBodyInnerHTML("<img src='myimage'></img>");
    LayoutImage* img = toLayoutImage(document().body()->firstChild()->layoutObject());

    RefPtr<TestImageLowQuality> testImage = adoptRef(new TestImageLowQuality);

    // Paint once. This will kick off a timer to see if we resize it during that timer's execution.
    mockTimer->setTime(0.1);
    EXPECT_FALSE(controller()->shouldPaintAtLowQuality(*img, testImage.get(), testImage.get(), LayoutSize(2, 2), 0.1));
    EXPECT_EQ(ImageQualityController::cLowQualityTimeThreshold, mockTimer->nextFireInterval());

    // Go into low-quality mode now that the size changed.
    double nextTime = 0.1 + ImageQualityController::cTimerRestartThreshold / 2.0;
    mockTimer->setTime(nextTime);
    EXPECT_EQ(true, controller()->shouldPaintAtLowQuality(*img, testImage.get(), testImage.get(), LayoutSize(3, 3), nextTime));
    // The fire interval has decreased, because we have not restarted the timer.
    EXPECT_EQ(ImageQualityController::cLowQualityTimeThreshold - ImageQualityController::cTimerRestartThreshold / 2.0, mockTimer->nextFireInterval());

    // This animation is far enough in the future to make the timer restart, since it is half over.
    nextTime = 0.1 + ImageQualityController::cTimerRestartThreshold + 0.01;
    EXPECT_EQ(true, controller()->shouldPaintAtLowQuality(*img, testImage.get(), testImage.get(), LayoutSize(4, 4), nextTime));
    // Now the timer has restarted, leading to a larger fire interval.
    EXPECT_EQ(ImageQualityController::cLowQualityTimeThreshold, mockTimer->nextFireInterval());
}

#endif

} // namespace blink
