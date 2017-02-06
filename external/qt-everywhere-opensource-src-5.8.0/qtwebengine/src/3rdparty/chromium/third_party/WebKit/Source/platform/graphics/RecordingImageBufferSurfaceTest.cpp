// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

using testing::Test;

namespace blink {

class FakeImageBufferClient : public ImageBufferClient, public WebThread::TaskObserver {
public:
    FakeImageBufferClient(ImageBuffer* imageBuffer)
        : m_isDirty(false)
        , m_imageBuffer(imageBuffer)
        , m_frameCount(0)
    { }

    ~FakeImageBufferClient() override { }

    // ImageBufferClient implementation
    void notifySurfaceInvalid() override { }
    bool isDirty() override { return m_isDirty; }
    void didFinalizeFrame() override
    {
        if (m_isDirty) {
            Platform::current()->currentThread()->removeTaskObserver(this);
            m_isDirty = false;
        }
        ++m_frameCount;
    }

    // TaskObserver implementation
    void willProcessTask() override { ASSERT_NOT_REACHED(); }
    void didProcessTask() override
    {
        ASSERT_TRUE(m_isDirty);
        FloatRect dirtyRect(0, 0, 1, 1);
        m_imageBuffer->finalizeFrame(dirtyRect);
        ASSERT_FALSE(m_isDirty);
    }
    void restoreCanvasMatrixClipStack(SkCanvas*) const override { }

    void fakeDraw()
    {
        if (m_isDirty)
            return;
        m_isDirty = true;
        Platform::current()->currentThread()->addTaskObserver(this);
    }

    int frameCount() { return m_frameCount; }

private:
    bool m_isDirty;
    ImageBuffer* m_imageBuffer;
    int m_frameCount;
};

class MockSurfaceFactory : public RecordingImageBufferFallbackSurfaceFactory {
public:
    MockSurfaceFactory() : m_createSurfaceCount(0) { }

    virtual std::unique_ptr<ImageBufferSurface> createSurface(const IntSize& size, OpacityMode opacityMode)
    {
        m_createSurfaceCount++;
        return wrapUnique(new UnacceleratedImageBufferSurface(size, opacityMode));
    }

    virtual ~MockSurfaceFactory() { }

    int createSurfaceCount() { return m_createSurfaceCount; }

private:
    int m_createSurfaceCount;
};

class RecordingImageBufferSurfaceTest : public Test {
protected:
    RecordingImageBufferSurfaceTest()
    {
        std::unique_ptr<MockSurfaceFactory> surfaceFactory = wrapUnique(new MockSurfaceFactory());
        m_surfaceFactory = surfaceFactory.get();
        std::unique_ptr<RecordingImageBufferSurface> testSurface = wrapUnique(new RecordingImageBufferSurface(IntSize(10, 10), std::move(surfaceFactory)));
        m_testSurface = testSurface.get();
        // We create an ImageBuffer in order for the testSurface to be
        // properly initialized with a GraphicsContext
        m_imageBuffer = ImageBuffer::create(std::move(testSurface));
        EXPECT_FALSE(!m_imageBuffer);
        m_fakeImageBufferClient = wrapUnique(new FakeImageBufferClient(m_imageBuffer.get()));
        m_imageBuffer->setClient(m_fakeImageBufferClient.get());
    }

public:
    void testEmptyPicture()
    {
        m_testSurface->initializeCurrentFrame();
        RefPtr<SkPicture> picture = m_testSurface->getPicture();
        EXPECT_TRUE((bool)picture.get());
        EXPECT_EQ(1, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
    }

    void testNoFallbackWithClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->willOverwriteCanvas();
        m_testSurface->getPicture();
        EXPECT_EQ(1, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
    }

    void testNonAnimatedCanvasUpdate()
    {
        m_testSurface->initializeCurrentFrame();
        // acquire picture twice to simulate a static canvas: nothing drawn between updates
        m_fakeImageBufferClient->fakeDraw();
        m_testSurface->getPicture();
        m_testSurface->getPicture();
        EXPECT_EQ(2, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
    }

    void testAnimatedWithoutClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_fakeImageBufferClient->fakeDraw();
        m_testSurface->getPicture();
        EXPECT_EQ(1, m_fakeImageBufferClient->frameCount());
        EXPECT_EQ(0, m_surfaceFactory->createSurfaceCount());
        expectDisplayListEnabled(true); // first frame has an implicit clear
        m_fakeImageBufferClient->fakeDraw();
        m_testSurface->getPicture();
        EXPECT_EQ(2, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(false);
    }

    void testFrameFinalizedByTaskObserver1()
    {
        m_testSurface->initializeCurrentFrame();
        expectDisplayListEnabled(true);
        m_testSurface->getPicture();
        EXPECT_EQ(1, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
        m_fakeImageBufferClient->fakeDraw();
        EXPECT_EQ(1, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
        m_testSurface->getPicture();
        EXPECT_EQ(2, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
        m_fakeImageBufferClient->fakeDraw();
        EXPECT_EQ(2, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
        // Display list will be disabled only after exiting the runLoop
    }
    void testFrameFinalizedByTaskObserver2()
    {
        EXPECT_EQ(3, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(false);
        m_testSurface->getPicture();
        EXPECT_EQ(3, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(false);
        m_fakeImageBufferClient->fakeDraw();
        EXPECT_EQ(3, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(false);
    }

    void testAnimatedWithClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->getPicture();
        m_testSurface->willOverwriteCanvas();
        m_fakeImageBufferClient->fakeDraw();
        EXPECT_EQ(1, m_fakeImageBufferClient->frameCount());
        m_testSurface->getPicture();
        EXPECT_EQ(2, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
        // clear after use
        m_fakeImageBufferClient->fakeDraw();
        m_testSurface->willOverwriteCanvas();
        EXPECT_EQ(2, m_fakeImageBufferClient->frameCount());
        m_testSurface->getPicture();
        EXPECT_EQ(3, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
    }

    void testClearRect()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->getPicture();
        SkPaint clearPaint;
        clearPaint.setXfermodeMode(SkXfermode::kClear_Mode);
        m_imageBuffer->canvas()->drawRect(SkRect::MakeWH(m_testSurface->size().width(), m_testSurface->size().height()), clearPaint);
        m_fakeImageBufferClient->fakeDraw();
        EXPECT_EQ(1, m_fakeImageBufferClient->frameCount());
        m_testSurface->getPicture();
        EXPECT_EQ(2, m_fakeImageBufferClient->frameCount());
        expectDisplayListEnabled(true);
    }

    void expectDisplayListEnabled(bool displayListEnabled)
    {
        EXPECT_EQ(displayListEnabled, (bool)m_testSurface->m_currentFrame.get());
        EXPECT_EQ(!displayListEnabled, (bool)m_testSurface->m_fallbackSurface.get());
        int expectedSurfaceCreationCount = displayListEnabled ? 0 : 1;
        EXPECT_EQ(expectedSurfaceCreationCount, m_surfaceFactory->createSurfaceCount());
    }

private:
    MockSurfaceFactory* m_surfaceFactory;
    RecordingImageBufferSurface* m_testSurface;
    std::unique_ptr<FakeImageBufferClient> m_fakeImageBufferClient;
    std::unique_ptr<ImageBuffer> m_imageBuffer;
};

namespace {

// The following test helper class installs a mock platform that provides a mock WebThread
// for the current thread. The Mock thread is capable of queuing a single non-delayed task
// and registering a single task observer. The run loop exits immediately after running
// the single task.

class CurrentThreadPlatformMock : public TestingPlatformSupport {
public:
    CurrentThreadPlatformMock() { }
    WebThread* currentThread() override { return &m_currentThread; }

    void enterRunLoop() { m_currentThread.enterRunLoop(); }
private:
    class MockWebTaskRunner : public WebTaskRunner {
    public:
        MockWebTaskRunner() : m_task(0) { }
        ~MockWebTaskRunner() override { }

        void postTask(const WebTraceLocation&, Task* task) override
        {
            EXPECT_EQ((Task*)0, m_task);
            m_task = task;
        }

        void postDelayedTask(const WebTraceLocation&, Task*, double delayMs) override { ASSERT_NOT_REACHED(); };

        WebTaskRunner* clone() override
        {
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        double virtualTimeSeconds() const override
        {
            ASSERT_NOT_REACHED();
            return 0.0;
        }

        double monotonicallyIncreasingVirtualTimeSeconds() const override
        {
            ASSERT_NOT_REACHED();
            return 0.0;
        }

        Task* m_task;
    };

    class CurrentThreadMock : public WebThread {
    public:
        CurrentThreadMock() : m_taskObserver(0) { }

        ~CurrentThreadMock() override
        {
            EXPECT_EQ((WebTaskRunner::Task*)0, m_taskRunner.m_task);
        }

        WebTaskRunner* getWebTaskRunner() override
        {
            return &m_taskRunner;
        }

        bool isCurrentThread() const override { return true; }

        PlatformThreadId threadId() const override
        {
            ASSERT_NOT_REACHED();
            return 0;
        }

        void addTaskObserver(TaskObserver* taskObserver) override
        {
            EXPECT_EQ(nullptr, m_taskObserver);
            m_taskObserver = taskObserver;
        }

        void removeTaskObserver(TaskObserver* taskObserver) override
        {
            EXPECT_EQ(m_taskObserver, taskObserver);
            m_taskObserver = 0;
        }

        WebScheduler* scheduler() const override
        {
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        void enterRunLoop()
        {
            if (m_taskObserver)
                m_taskObserver->willProcessTask();
            if (m_taskRunner.m_task) {
                m_taskRunner.m_task->run();
                delete m_taskRunner.m_task;
                m_taskRunner.m_task = 0;
            }
            if (m_taskObserver)
                m_taskObserver->didProcessTask();
        }

    private:
        MockWebTaskRunner m_taskRunner;
        TaskObserver* m_taskObserver;
    };

    CurrentThreadMock m_currentThread;
};

} // anonymous namespace

#define CALL_TEST_TASK_WRAPPER(TEST_METHOD)                                                               \
    {                                                                                                     \
        CurrentThreadPlatformMock ctpm;                                                                   \
        Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&RecordingImageBufferSurfaceTest::TEST_METHOD, WTF::unretained(this))); \
        ctpm.enterRunLoop();                                      \
    }

TEST_F(RecordingImageBufferSurfaceTest, testEmptyPicture)
{
    testEmptyPicture();
}

TEST_F(RecordingImageBufferSurfaceTest, testNoFallbackWithClear)
{
    testNoFallbackWithClear();
}

TEST_F(RecordingImageBufferSurfaceTest, testNonAnimatedCanvasUpdate)
{
    CALL_TEST_TASK_WRAPPER(testNonAnimatedCanvasUpdate)
    expectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithoutClear)
{
    CALL_TEST_TASK_WRAPPER(testAnimatedWithoutClear)
    expectDisplayListEnabled(false);
}

TEST_F(RecordingImageBufferSurfaceTest, testFrameFinalizedByTaskObserver)
{
    CALL_TEST_TASK_WRAPPER(testFrameFinalizedByTaskObserver1)
    expectDisplayListEnabled(false);
    CALL_TEST_TASK_WRAPPER(testFrameFinalizedByTaskObserver2)
    expectDisplayListEnabled(false);
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithClear)
{
    CALL_TEST_TASK_WRAPPER(testAnimatedWithClear)
    expectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testClearRect)
{
    CALL_TEST_TASK_WRAPPER(testClearRect);
    expectDisplayListEnabled(true);
}

} // namespace blink
