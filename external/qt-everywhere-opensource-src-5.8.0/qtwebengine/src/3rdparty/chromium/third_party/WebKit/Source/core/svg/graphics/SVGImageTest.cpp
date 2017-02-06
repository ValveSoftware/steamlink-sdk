// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/graphics/SVGImage.h"

#include "core/svg/graphics/SVGImageChromeClient.h"
#include "platform/SharedBuffer.h"
#include "platform/Timer.h"
#include "platform/geometry/FloatRect.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"

namespace blink {
namespace {

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

class MockTimer : public Timer<SVGImageChromeClient> {
    typedef void (SVGImageChromeClient::*TimerFiredFunction)(Timer*);
public:
    MockTimer(SVGImageChromeClient* o, TimerFiredFunction f)
    : Timer<SVGImageChromeClient>(o, f, &m_taskRunner)
    {
    }

    void fire()
    {
        this->Timer<SVGImageChromeClient>::fired();
        stop();
    }

    void setTime(double newTime)
    {
        m_taskRunner.setTime(newTime);
    }

private:
    MockTaskRunner m_taskRunner;
};

} // namespace

class SVGImageTest : public testing::Test {
public:
    SVGImage& image() { return *m_image; }

    void load(const char* data, bool shouldPause)
    {
        m_observer = new PauseControlImageObserver(shouldPause);
        m_image = SVGImage::create(m_observer);
        m_image->setData(SharedBuffer::create(data, strlen(data)), true);
    }

    void pumpFrame()
    {
        Image* image = m_image.get();
        RefPtr<SkCanvas> nullCanvas = adoptRef(SkCreateNullCanvas());
        SkPaint paint;
        FloatRect dummyRect(0, 0, 100, 100);
        image->draw(
            nullCanvas.get(), paint,
            dummyRect, dummyRect,
            DoNotRespectImageOrientation, Image::DoNotClampImageToSourceRect);
    }

private:
    class PauseControlImageObserver
        : public GarbageCollectedFinalized<PauseControlImageObserver>, public ImageObserver {
        USING_GARBAGE_COLLECTED_MIXIN(PauseControlImageObserver);
    public:
        PauseControlImageObserver(bool shouldPause) : m_shouldPause(shouldPause) { }

        void decodedSizeChangedTo(const Image*, size_t newSize) override { }
        void didDraw(const Image*) override { }

        bool shouldPauseAnimation(const Image*) override { return m_shouldPause; }
        void animationAdvanced(const Image*) override { }

        void changedInRect(const Image*, const IntRect&) override { }

        DEFINE_INLINE_VIRTUAL_TRACE() { ImageObserver::trace(visitor); }
    private:
        bool m_shouldPause;
    };
    Persistent<PauseControlImageObserver> m_observer;
    RefPtr<SVGImage> m_image;
};

const char kAnimatedDocument[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<style>"
    "@keyframes rot {"
    " from { transform: rotate(0deg); } to { transform: rotate(-360deg); }"
    "}"
    ".spinner {"
    " transform-origin: 50%% 50%%;"
    " animation-name: rot;"
    " animation-duration: 4s;"
    " animation-iteration-count: infinite;"
    " animation-timing-function: linear;"
    "}"
    "</style>"
    "<path class='spinner' fill='none' d='M 8,1.125 A 6.875,6.875 0 1 1 1.125,8' stroke-width='2' stroke='blue'/>"
    "</svg>";

TEST_F(SVGImageTest, TimelineSuspendAndResume)
{
    const bool shouldPause = true;
    load(kAnimatedDocument, shouldPause);
    SVGImageChromeClient& chromeClient = image().chromeClientForTesting();
    MockTimer* timer = new MockTimer(&chromeClient, &SVGImageChromeClient::animationTimerFired);
    chromeClient.setTimer(timer);

    // Simulate a draw. Cause a frame (timer) to be scheduled.
    pumpFrame();
    EXPECT_TRUE(image().hasAnimations());
    EXPECT_TRUE(timer->isActive());

    // Fire the timer/trigger a frame update. Since the observer always returns
    // true for shouldPauseAnimation, this will result in the timeline being
    // suspended.
    timer->fire();
    EXPECT_TRUE(chromeClient.isSuspended());
    EXPECT_FALSE(timer->isActive());

    // Simulate a draw. This should resume the animation again.
    pumpFrame();
    EXPECT_TRUE(timer->isActive());
    EXPECT_FALSE(chromeClient.isSuspended());
}

TEST_F(SVGImageTest, ResetAnimation)
{
    const bool shouldPause = false;
    load(kAnimatedDocument, shouldPause);
    SVGImageChromeClient& chromeClient = image().chromeClientForTesting();
    MockTimer* timer = new MockTimer(&chromeClient, &SVGImageChromeClient::animationTimerFired);
    chromeClient.setTimer(timer);

    // Simulate a draw. Cause a frame (timer) to be scheduled.
    pumpFrame();
    EXPECT_TRUE(image().hasAnimations());
    EXPECT_TRUE(timer->isActive());

    // Reset the animation. This will suspend the timeline but not cancel the
    // timer.
    image().resetAnimation();
    EXPECT_TRUE(chromeClient.isSuspended());
    EXPECT_TRUE(timer->isActive());

    // Fire the timer/trigger a frame update. The timeline will remain
    // suspended and no frame will be scheduled.
    timer->fire();
    EXPECT_TRUE(chromeClient.isSuspended());
    EXPECT_FALSE(timer->isActive());

    // Simulate a draw. This should resume the animation again.
    pumpFrame();
    EXPECT_FALSE(chromeClient.isSuspended());
    EXPECT_TRUE(timer->isActive());
}

} // namespace blink
