// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/CompositeDataConsumerHandle.h"

#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Locker.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;

const WebDataConsumerHandle::Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const WebDataConsumerHandle::Result kOk = WebDataConsumerHandle::Ok;
const WebDataConsumerHandle::Flags kNone = WebDataConsumerHandle::FlagNone;

class MockReader : public WebDataConsumerHandle::Reader {
public:
    static std::unique_ptr<StrictMock<MockReader>> create() { return wrapUnique(new StrictMock<MockReader>); }

    using Result = WebDataConsumerHandle::Result;
    using Flags = WebDataConsumerHandle::Flags;
    MOCK_METHOD4(read, Result(void*, size_t, Flags, size_t*));
    MOCK_METHOD3(beginRead, Result(const void**, Flags, size_t*));
    MOCK_METHOD1(endRead, Result(size_t));
};

class MockHandle : public WebDataConsumerHandle {
public:
    static std::unique_ptr<StrictMock<MockHandle>> create() { return wrapUnique(new StrictMock<MockHandle>); }

    MOCK_METHOD1(obtainReaderInternal, Reader*(Client*));

private:
    const char* debugName() const override { return "MockHandle in CompositeDataConsumerHandleTest"; }
};

class ThreadingRegistrationTest : public DataConsumerHandleTestUtil::ThreadingTestBase {
public:
    using Self = ThreadingRegistrationTest;
    static PassRefPtr<Self> create() { return adoptRef(new Self); }

    void run()
    {
        ThreadHolder holder(this);
        m_waitableEvent = wrapUnique(new WaitableEvent());

        postTaskToUpdatingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::createHandle, wrapPassRefPtr(this)));
        postTaskToReadingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::obtainReader, wrapPassRefPtr(this)));
    }

private:
    ThreadingRegistrationTest() = default;

    void createHandle()
    {
        m_handle = CompositeDataConsumerHandle::create(DataConsumerHandle::create("handle1", m_context), &m_updater);
        m_waitableEvent->signal();
    }
    void obtainReader()
    {
        m_reader = m_handle->obtainReader(&m_client);
        postTaskToUpdatingThread(BLINK_FROM_HERE, crossThreadBind(&Self::update, wrapPassRefPtr(this)));
    }
    void update()
    {
        m_updater->update(DataConsumerHandle::create("handle2", m_context));
        m_updater.clear();
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::resetReader, wrapPassRefPtr(this)));
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::signalDone, wrapPassRefPtr(this)));
    }

    std::unique_ptr<WebDataConsumerHandle> m_handle;
    CrossThreadPersistent<CompositeDataConsumerHandle::Updater> m_updater;
};

class ThreadingRegistrationDeleteHandleTest : public DataConsumerHandleTestUtil::ThreadingTestBase {
public:
    using Self = ThreadingRegistrationDeleteHandleTest;
    static PassRefPtr<Self> create() { return adoptRef(new Self); }

    void run()
    {
        ThreadHolder holder(this);
        m_waitableEvent = wrapUnique(new WaitableEvent());

        postTaskToUpdatingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::createHandle, wrapPassRefPtr(this)));
        postTaskToReadingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::obtainReader, wrapPassRefPtr(this)));
    }

private:
    ThreadingRegistrationDeleteHandleTest() = default;

    void createHandle()
    {
        m_handle = CompositeDataConsumerHandle::create(DataConsumerHandle::create("handle1", m_context), &m_updater);
        m_waitableEvent->signal();
    }

    void obtainReader()
    {
        m_reader = m_handle->obtainReader(&m_client);
        postTaskToUpdatingThread(BLINK_FROM_HERE, crossThreadBind(&Self::update, wrapPassRefPtr(this)));
    }
    void update()
    {
        m_updater->update(DataConsumerHandle::create("handle2", m_context));
        m_updater.clear();
        m_handle = nullptr;
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::resetReader, wrapPassRefPtr(this)));
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::signalDone, wrapPassRefPtr(this)));
    }

    std::unique_ptr<WebDataConsumerHandle> m_handle;
    CrossThreadPersistent<CompositeDataConsumerHandle::Updater> m_updater;
};

class ThreadingRegistrationDeleteReaderTest : public DataConsumerHandleTestUtil::ThreadingTestBase {
public:
    using Self = ThreadingRegistrationDeleteReaderTest;
    static PassRefPtr<Self> create() { return adoptRef(new Self); }

    void run()
    {
        ThreadHolder holder(this);
        m_waitableEvent = wrapUnique(new WaitableEvent());

        postTaskToUpdatingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::createHandle, wrapPassRefPtr(this)));
        postTaskToReadingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::obtainReader, wrapPassRefPtr(this)));
    }

private:
    ThreadingRegistrationDeleteReaderTest() = default;

    void createHandle()
    {
        m_handle = CompositeDataConsumerHandle::create(DataConsumerHandle::create("handle1", m_context), &m_updater);
        m_waitableEvent->signal();
    }

    void obtainReader()
    {
        m_reader = m_handle->obtainReader(&m_client);
        postTaskToUpdatingThread(BLINK_FROM_HERE, crossThreadBind(&Self::update, wrapPassRefPtr(this)));
    }
    void update()
    {
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::resetReader, wrapPassRefPtr(this)));
        m_updater->update(DataConsumerHandle::create("handle2", m_context));
        m_updater.clear();
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::resetReader, wrapPassRefPtr(this)));
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::signalDone, wrapPassRefPtr(this)));
    }

    std::unique_ptr<WebDataConsumerHandle> m_handle;
    CrossThreadPersistent<CompositeDataConsumerHandle::Updater> m_updater;
};

class ThreadingUpdatingReaderWhileUpdatingTest : public DataConsumerHandleTestUtil::ThreadingTestBase {
public:
    using Self = ThreadingUpdatingReaderWhileUpdatingTest;
    static PassRefPtr<Self> create() { return adoptRef(new Self); }

    void run()
    {
        ThreadHolder holder(this);
        m_waitableEvent = wrapUnique(new WaitableEvent());
        m_updateEvent = wrapUnique(new WaitableEvent());

        postTaskToUpdatingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::createHandle, wrapPassRefPtr(this)));
        postTaskToReadingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::obtainReader, wrapPassRefPtr(this)));
    }

private:
    ThreadingUpdatingReaderWhileUpdatingTest() = default;

    void createHandle()
    {
        m_handle = CompositeDataConsumerHandle::create(DataConsumerHandle::create("handle1", m_context), &m_updater);
        m_waitableEvent->signal();
    }

    void obtainReader()
    {
        m_reader = m_handle->obtainReader(&m_client);
        postTaskToUpdatingThread(BLINK_FROM_HERE, crossThreadBind(&Self::update, wrapPassRefPtr(this)));
        m_updateEvent->wait();
    }

    void update()
    {
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::reobtainReader, wrapPassRefPtr(this)));
        m_updater->update(DataConsumerHandle::create("handle2", m_context));
        m_updater.clear();
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::resetReader, wrapPassRefPtr(this)));
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::signalDone, wrapPassRefPtr(this)));
        m_updateEvent->signal();
    }

    void reobtainReader()
    {
        m_reader = nullptr;
        m_reader = m_handle->obtainReader(&m_client);
    }

    std::unique_ptr<WebDataConsumerHandle> m_handle;
    CrossThreadPersistent<CompositeDataConsumerHandle::Updater> m_updater;
    std::unique_ptr<WaitableEvent> m_updateEvent;
};

class ThreadingRegistrationUpdateTwiceAtOneTimeTest : public DataConsumerHandleTestUtil::ThreadingTestBase {
public:
    using Self = ThreadingRegistrationUpdateTwiceAtOneTimeTest;
    static PassRefPtr<Self> create() { return adoptRef(new Self); }

    void run()
    {
        ThreadHolder holder(this);
        m_waitableEvent = wrapUnique(new WaitableEvent());
        m_updateEvent = wrapUnique(new WaitableEvent());

        postTaskToUpdatingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::createHandle, wrapPassRefPtr(this)));
        postTaskToReadingThreadAndWait(BLINK_FROM_HERE, crossThreadBind(&Self::obtainReader, wrapPassRefPtr(this)));
    }

private:
    ThreadingRegistrationUpdateTwiceAtOneTimeTest() = default;

    void createHandle()
    {
        m_handle = CompositeDataConsumerHandle::create(DataConsumerHandle::create("handle1", m_context), &m_updater);
        m_waitableEvent->signal();
    }

    void obtainReader()
    {
        m_reader = m_handle->obtainReader(&m_client);
        postTaskToUpdatingThread(BLINK_FROM_HERE, crossThreadBind(&Self::update, wrapPassRefPtr(this)));
        // Stalls this thread while updating handles.
        m_updateEvent->wait();
    }
    void update()
    {
        m_updater->update(DataConsumerHandle::create("handle2", m_context));
        m_updater->update(DataConsumerHandle::create("handle3", m_context));
        m_updateEvent->signal();
        m_updater.clear();
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::resetReader, wrapPassRefPtr(this)));
        postTaskToReadingThread(BLINK_FROM_HERE, crossThreadBind(&Self::signalDone, wrapPassRefPtr(this)));
    }

    std::unique_ptr<WebDataConsumerHandle> m_handle;
    CrossThreadPersistent<CompositeDataConsumerHandle::Updater> m_updater;
    std::unique_ptr<WaitableEvent> m_updateEvent;
};

TEST(CompositeDataConsumerHandleTest, Read)
{
    char buffer[20];
    size_t size = 0;
    DataConsumerHandleTestUtil::NoopClient client;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle1 = MockHandle::create();
    std::unique_ptr<MockHandle> handle2 = MockHandle::create();
    std::unique_ptr<MockReader> reader1 = MockReader::create();
    std::unique_ptr<MockReader> reader2 = MockReader::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*handle1, obtainReaderInternal(&client)).WillOnce(Return(reader1.get()));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*reader1, read(buffer, sizeof(buffer), kNone, &size)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*handle2, obtainReaderInternal(&client)).WillOnce(Return(reader2.get()));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*reader2, read(buffer, sizeof(buffer), kNone, &size)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(4));

    // They are adopted by |obtainReader|.
    ASSERT_TRUE(reader1.release());
    ASSERT_TRUE(reader2.release());

    CompositeDataConsumerHandle::Updater* updater = nullptr;
    std::unique_ptr<WebDataConsumerHandle> handle = CompositeDataConsumerHandle::create(std::move(handle1), &updater);
    checkpoint.Call(0);
    std::unique_ptr<WebDataConsumerHandle::Reader> reader = handle->obtainReader(&client);
    checkpoint.Call(1);
    EXPECT_EQ(kOk, reader->read(buffer, sizeof(buffer), kNone, &size));
    checkpoint.Call(2);
    updater->update(std::move(handle2));
    checkpoint.Call(3);
    EXPECT_EQ(kOk, reader->read(buffer, sizeof(buffer), kNone, &size));
    checkpoint.Call(4);
}

TEST(CompositeDataConsumerHandleTest, TwoPhaseRead)
{
    const void* p = nullptr;
    size_t size = 0;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle1 = MockHandle::create();
    std::unique_ptr<MockHandle> handle2 = MockHandle::create();
    std::unique_ptr<MockReader> reader1 = MockReader::create();
    std::unique_ptr<MockReader> reader2 = MockReader::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*handle1, obtainReaderInternal(nullptr)).WillOnce(Return(reader1.get()));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*reader1, beginRead(&p, kNone, &size)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader1, endRead(0)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*handle2, obtainReaderInternal(nullptr)).WillOnce(Return(reader2.get()));
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*reader2, beginRead(&p, kNone, &size)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(5));
    EXPECT_CALL(*reader2, endRead(0)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(6));

    // They are adopted by |obtainReader|.
    ASSERT_TRUE(reader1.release());
    ASSERT_TRUE(reader2.release());

    CompositeDataConsumerHandle::Updater* updater = nullptr;
    std::unique_ptr<WebDataConsumerHandle> handle = CompositeDataConsumerHandle::create(std::move(handle1), &updater);
    checkpoint.Call(0);
    std::unique_ptr<WebDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    checkpoint.Call(1);
    EXPECT_EQ(kOk, reader->beginRead(&p, kNone, &size));
    checkpoint.Call(2);
    EXPECT_EQ(kOk, reader->endRead(0));
    checkpoint.Call(3);
    updater->update(std::move(handle2));
    checkpoint.Call(4);
    EXPECT_EQ(kOk, reader->beginRead(&p, kNone, &size));
    checkpoint.Call(5);
    EXPECT_EQ(kOk, reader->endRead(0));
    checkpoint.Call(6);
}

TEST(CompositeDataConsumerHandleTest, HangingTwoPhaseRead)
{
    const void* p = nullptr;
    size_t size = 0;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle1 = MockHandle::create();
    std::unique_ptr<MockHandle> handle2 = MockHandle::create();
    std::unique_ptr<MockHandle> handle3 = MockHandle::create();
    std::unique_ptr<MockReader> reader1 = MockReader::create();
    std::unique_ptr<MockReader> reader2 = MockReader::create();
    std::unique_ptr<MockReader> reader3 = MockReader::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*handle1, obtainReaderInternal(nullptr)).WillOnce(Return(reader1.get()));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*reader1, beginRead(&p, kNone, &size)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*reader1, endRead(0)).WillOnce(Return(kOk));
    EXPECT_CALL(*handle2, obtainReaderInternal(nullptr)).WillOnce(Return(reader2.get()));
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*reader2, beginRead(&p, kNone, &size)).WillOnce(Return(kShouldWait));
    EXPECT_CALL(checkpoint, Call(5));
    EXPECT_CALL(*handle3, obtainReaderInternal(nullptr)).WillOnce(Return(reader3.get()));
    EXPECT_CALL(checkpoint, Call(6));
    EXPECT_CALL(*reader3, beginRead(&p, kNone, &size)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(7));
    EXPECT_CALL(*reader3, endRead(0)).WillOnce(Return(kOk));
    EXPECT_CALL(checkpoint, Call(8));

    // They are adopted by |obtainReader|.
    ASSERT_TRUE(reader1.release());
    ASSERT_TRUE(reader2.release());
    ASSERT_TRUE(reader3.release());

    CompositeDataConsumerHandle::Updater* updater = nullptr;
    std::unique_ptr<WebDataConsumerHandle> handle = CompositeDataConsumerHandle::create(std::move(handle1), &updater);
    checkpoint.Call(0);
    std::unique_ptr<WebDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    checkpoint.Call(1);
    EXPECT_EQ(kOk, reader->beginRead(&p, kNone, &size));
    checkpoint.Call(2);
    updater->update(std::move(handle2));
    checkpoint.Call(3);
    EXPECT_EQ(kOk, reader->endRead(0));
    checkpoint.Call(4);
    EXPECT_EQ(kShouldWait, reader->beginRead(&p, kNone, &size));
    checkpoint.Call(5);
    updater->update(std::move(handle3));
    checkpoint.Call(6);
    EXPECT_EQ(kOk, reader->beginRead(&p, kNone, &size));
    checkpoint.Call(7);
    EXPECT_EQ(kOk, reader->endRead(0));
    checkpoint.Call(8);
}

TEST(CompositeDataConsumerHandleTest, RegisterClientOnDifferentThreads)
{
    RefPtr<ThreadingRegistrationTest> test = ThreadingRegistrationTest::create();
    test->run();

    EXPECT_EQ(
        "A reader is attached to handle1 on the reading thread.\n"
        "A reader is detached from handle1 on the reading thread.\n"
        "A reader is attached to handle2 on the reading thread.\n"
        "A reader is detached from handle2 on the reading thread.\n",
        test->result());
}

TEST(CompositeDataConsumerHandleTest, DeleteHandleWhileUpdating)
{
    RefPtr<ThreadingRegistrationDeleteHandleTest> test = ThreadingRegistrationDeleteHandleTest::create();
    test->run();

    EXPECT_EQ(
        "A reader is attached to handle1 on the reading thread.\n"
        "A reader is detached from handle1 on the reading thread.\n"
        "A reader is attached to handle2 on the reading thread.\n"
        "A reader is detached from handle2 on the reading thread.\n",
        test->result());
}

TEST(CompositeDataConsumerHandleTest, DeleteReaderWhileUpdating)
{
    RefPtr<ThreadingRegistrationDeleteReaderTest> test = ThreadingRegistrationDeleteReaderTest::create();
    test->run();

    EXPECT_EQ(
        "A reader is attached to handle1 on the reading thread.\n"
        "A reader is detached from handle1 on the reading thread.\n",
        test->result());
}

TEST(CompositeDataConsumerHandleTest, UpdateReaderWhileUpdating)
{
    RefPtr<ThreadingUpdatingReaderWhileUpdatingTest> test = ThreadingUpdatingReaderWhileUpdatingTest::create();
    test->run();

    EXPECT_EQ(
        "A reader is attached to handle1 on the reading thread.\n"
        "A reader is detached from handle1 on the reading thread.\n"
        "A reader is attached to handle2 on the reading thread.\n"
        "A reader is detached from handle2 on the reading thread.\n",
        test->result());
}

TEST(CompositeDataConsumerHandleTest, UpdateTwiceAtOnce)
{
    RefPtr<ThreadingRegistrationUpdateTwiceAtOneTimeTest> test = ThreadingRegistrationUpdateTwiceAtOneTimeTest::create();
    test->run();

    EXPECT_EQ(
        "A reader is attached to handle1 on the reading thread.\n"
        "A reader is detached from handle1 on the reading thread.\n"
        "A reader is attached to handle3 on the reading thread.\n"
        "A reader is detached from handle3 on the reading thread.\n",
        test->result());
}

TEST(CompositeDataConsumerHandleTest, DoneHandleNotification)
{
    RefPtr<DataConsumerHandleTestUtil::ThreadingHandleNotificationTest> test = DataConsumerHandleTestUtil::ThreadingHandleNotificationTest::create();
    CompositeDataConsumerHandle::Updater* updater = nullptr;
    // Test this function returns.
    test->run(CompositeDataConsumerHandle::create(createDoneDataConsumerHandle(), &updater));
}

TEST(CompositeDataConsumerHandleTest, DoneHandleNoNotification)
{
    RefPtr<DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest> test = DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest::create();
    CompositeDataConsumerHandle::Updater* updater = nullptr;
    // Test this function doesn't crash.
    test->run(CompositeDataConsumerHandle::create(createDoneDataConsumerHandle(), &updater));
}

} // namespace

} // namespace blink
