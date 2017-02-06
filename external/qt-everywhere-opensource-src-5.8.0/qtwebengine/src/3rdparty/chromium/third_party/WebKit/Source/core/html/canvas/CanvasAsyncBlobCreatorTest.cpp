// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "core/html/ImageData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Functional.h"

namespace blink {

typedef CanvasAsyncBlobCreator::IdleTaskStatus IdleTaskStatus;

class MockCanvasAsyncBlobCreator : public CanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreator(DOMUint8ClampedArray* data, const IntSize& size, MimeType mimeType)
        : CanvasAsyncBlobCreator(data, mimeType, size, nullptr, 0)
    {
    }

    CanvasAsyncBlobCreator::IdleTaskStatus idleTaskStatus()
    {
        return m_idleTaskStatus;
    }

    MOCK_METHOD0(signalTaskSwitchInStartTimeoutEventForTesting, void());
    MOCK_METHOD0(signalTaskSwitchInCompleteTimeoutEventForTesting, void());

protected:
    void createBlobAndInvokeCallback() override { };
    void createNullAndInvokeCallback() override { };
    void signalAlternativeCodePathFinishedForTesting() override;
    void postDelayedTaskToMainThread(const WebTraceLocation&, std::unique_ptr<WTF::Closure>, double delayMs) override;
};

void MockCanvasAsyncBlobCreator::signalAlternativeCodePathFinishedForTesting()
{
    testing::exitRunLoop();
}

void MockCanvasAsyncBlobCreator::postDelayedTaskToMainThread(const WebTraceLocation& location, std::unique_ptr<WTF::Closure> task, double delayMs)
{
    DCHECK(isMainThread());
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(location, std::move(task));
}

//==============================================================================
//=================================PNG==========================================
//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStartPng : public MockCanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreatorWithoutStartPng(DOMUint8ClampedArray* data, const IntSize& size)
        : MockCanvasAsyncBlobCreator(data, size, MimeTypePng)
    {
    }

protected:
    void scheduleInitiatePngEncoding() override
    {
        // Deliberately make scheduleInitiatePngEncoding do nothing so that idle task never starts
    }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutCompletePng : public MockCanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreatorWithoutCompletePng(DOMUint8ClampedArray* data, const IntSize& size)
        : MockCanvasAsyncBlobCreator(data, size, MimeTypePng)
    {
    }

protected:
    void scheduleInitiatePngEncoding() override
    {
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&MockCanvasAsyncBlobCreatorWithoutCompletePng::initiatePngEncoding, wrapPersistent(this), std::numeric_limits<double>::max()));
    }

    void idleEncodeRowsPng(double deadlineSeconds) override
    {
        // Deliberately make idleEncodeRowsPng do nothing so that idle task never completes
    }
};

//==============================================================================
//=================================JPEG=========================================
//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStartJpeg : public MockCanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreatorWithoutStartJpeg(DOMUint8ClampedArray* data, const IntSize& size)
        : MockCanvasAsyncBlobCreator(data, size, MimeTypeJpeg)
    {
    }

protected:
    void scheduleInitiateJpegEncoding(const double&) override
    {
        // Deliberately make scheduleInitiateJpegEncoding do nothing so that idle task never starts
    }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutCompleteJpeg : public MockCanvasAsyncBlobCreator {
public:
    MockCanvasAsyncBlobCreatorWithoutCompleteJpeg(DOMUint8ClampedArray* data, const IntSize& size)
        : MockCanvasAsyncBlobCreator(data, size, MimeTypeJpeg)
    {
    }

protected:
    void scheduleInitiateJpegEncoding(const double& quality) override
    {
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&MockCanvasAsyncBlobCreatorWithoutCompleteJpeg::initiateJpegEncoding, wrapPersistent(this), quality, std::numeric_limits<double>::max()));
    }

    void idleEncodeRowsJpeg(double deadlineSeconds) override
    {
        // Deliberately make idleEncodeRowsJpeg do nothing so that idle task never completes
    }
};

//==============================================================================

class CanvasAsyncBlobCreatorTest : public ::testing::Test {
public:
    // Png unit tests
    void prepareMockCanvasAsyncBlobCreatorWithoutStartPng();
    void prepareMockCanvasAsyncBlobCreatorWithoutCompletePng();
    void prepareMockCanvasAsyncBlobCreatorFailPng();

    // Jpeg unit tests
    void prepareMockCanvasAsyncBlobCreatorWithoutStartJpeg();
    void prepareMockCanvasAsyncBlobCreatorWithoutCompleteJpeg();
    void prepareMockCanvasAsyncBlobCreatorFailJpeg();

protected:
    CanvasAsyncBlobCreatorTest();
    MockCanvasAsyncBlobCreator* asyncBlobCreator() { return m_asyncBlobCreator.get(); }
    void TearDown() override;

private:
    Persistent<MockCanvasAsyncBlobCreator> m_asyncBlobCreator;
};

CanvasAsyncBlobCreatorTest::CanvasAsyncBlobCreatorTest()
{
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorWithoutStartPng()
{
    IntSize testSize(20, 20);
    ImageData* imageData = ImageData::create(testSize);

    m_asyncBlobCreator = new MockCanvasAsyncBlobCreatorWithoutStartPng(imageData->data(), testSize);
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorWithoutCompletePng()
{
    IntSize testSize(20, 20);
    ImageData* imageData = ImageData::create(testSize);

    m_asyncBlobCreator = new MockCanvasAsyncBlobCreatorWithoutCompletePng(imageData->data(), testSize);
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorFailPng()
{
    IntSize testSize(0, 0);
    ImageData* imageData = ImageData::create(testSize);

    // We reuse the class MockCanvasAsyncBlobCreatorWithoutCompletePng because this
    // test case is expected to fail at initialization step before completion.
    m_asyncBlobCreator = new MockCanvasAsyncBlobCreatorWithoutCompletePng(imageData->data(), testSize);
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorWithoutStartJpeg()
{
    IntSize testSize(20, 20);
    ImageData* imageData = ImageData::create(testSize);

    m_asyncBlobCreator = new MockCanvasAsyncBlobCreatorWithoutStartJpeg(imageData->data(), testSize);
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorWithoutCompleteJpeg()
{
    IntSize testSize(20, 20);
    ImageData* imageData = ImageData::create(testSize);

    m_asyncBlobCreator = new MockCanvasAsyncBlobCreatorWithoutCompleteJpeg(imageData->data(), testSize);
}

void CanvasAsyncBlobCreatorTest::prepareMockCanvasAsyncBlobCreatorFailJpeg()
{
    IntSize testSize(0, 0);
    ImageData* imageData = ImageData::create(testSize);

    // We reuse the class MockCanvasAsyncBlobCreatorWithoutCompleteJpeg because this
    // test case is expected to fail at initialization step before completion.
    m_asyncBlobCreator = new MockCanvasAsyncBlobCreatorWithoutCompleteJpeg(imageData->data(), testSize);
}

void CanvasAsyncBlobCreatorTest::TearDown()
{
    m_asyncBlobCreator = nullptr;
}

//==============================================================================

TEST_F(CanvasAsyncBlobCreatorTest, PngIdleTaskNotStartedWhenStartTimeoutEventHappens)
{
    // This test mocks the scenario when idle task is not started when the
    // StartTimeoutEvent is inspecting the idle task status.
    // The whole image encoding process (including initialization)  will then
    // become carried out in the alternative code path instead.
    this->prepareMockCanvasAsyncBlobCreatorWithoutStartPng();
    EXPECT_CALL(*(asyncBlobCreator()), signalTaskSwitchInStartTimeoutEventForTesting());

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true);
    testing::enterRunLoop();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_EQ(IdleTaskStatus::IdleTaskSwitchedToMainThreadTask, this->asyncBlobCreator()->idleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, PngIdleTaskNotCompletedWhenCompleteTimeoutEventHappens)
{
    // This test mocks the scenario when idle task is not completed when the
    // CompleteTimeoutEvent is inspecting the idle task status.
    // The remaining image encoding process (excluding initialization)  will
    // then become carried out in the alternative code path instead.
    this->prepareMockCanvasAsyncBlobCreatorWithoutCompletePng();
    EXPECT_CALL(*(asyncBlobCreator()), signalTaskSwitchInCompleteTimeoutEventForTesting());

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true);
    testing::enterRunLoop();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_EQ(IdleTaskStatus::IdleTaskSwitchedToMainThreadTask, this->asyncBlobCreator()->idleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, PngIdleTaskFailedWhenStartTimeoutEventHappens)
{
    // This test mocks the scenario when idle task is not failed during when
    // either the StartTimeoutEvent or the CompleteTimeoutEvent is inspecting
    // the idle task status.
    this->prepareMockCanvasAsyncBlobCreatorFailPng();

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true);
    testing::enterRunLoop();

    EXPECT_EQ(IdleTaskStatus::IdleTaskFailed, this->asyncBlobCreator()->idleTaskStatus());
}

// The below 3 unit tests have exactly same workflow as the above 3 unit tests
// except that they are encoding on JPEG image formats instead of PNG.
TEST_F(CanvasAsyncBlobCreatorTest, JpegIdleTaskNotStartedWhenStartTimeoutEventHappens)
{
    this->prepareMockCanvasAsyncBlobCreatorWithoutStartJpeg();
    EXPECT_CALL(*(asyncBlobCreator()), signalTaskSwitchInStartTimeoutEventForTesting());

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true, 1.0);
    testing::enterRunLoop();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_EQ(IdleTaskStatus::IdleTaskSwitchedToMainThreadTask, this->asyncBlobCreator()->idleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, JpegIdleTaskNotCompletedWhenCompleteTimeoutEventHappens)
{
    this->prepareMockCanvasAsyncBlobCreatorWithoutCompleteJpeg();
    EXPECT_CALL(*(asyncBlobCreator()), signalTaskSwitchInCompleteTimeoutEventForTesting());

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true, 1.0);
    testing::enterRunLoop();

    ::testing::Mock::VerifyAndClearExpectations(asyncBlobCreator());
    EXPECT_EQ(IdleTaskStatus::IdleTaskSwitchedToMainThreadTask, this->asyncBlobCreator()->idleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, JpegIdleTaskFailedWhenStartTimeoutEventHappens)
{
    this->prepareMockCanvasAsyncBlobCreatorFailJpeg();

    this->asyncBlobCreator()->scheduleAsyncBlobCreation(true, 1.0);
    testing::enterRunLoop();

    EXPECT_EQ(IdleTaskStatus::IdleTaskFailed, this->asyncBlobCreator()->idleTaskStatus());
}

}
