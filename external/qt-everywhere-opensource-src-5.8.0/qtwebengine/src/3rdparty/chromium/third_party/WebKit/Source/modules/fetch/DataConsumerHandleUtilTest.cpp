// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/DataConsumerHandleUtil.h"

#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include <memory>

namespace blink {

namespace {

const WebDataConsumerHandle::Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const WebDataConsumerHandle::Result kDone = WebDataConsumerHandle::Done;
const WebDataConsumerHandle::Result kUnexpectedError = WebDataConsumerHandle::UnexpectedError;
const WebDataConsumerHandle::Flags kNone = WebDataConsumerHandle::FlagNone;

TEST(DataConsumerHandleUtilTest, CreateWaitingHandle)
{
    char buffer[20];
    const void* p = nullptr;
    size_t size = 0;
    std::unique_ptr<WebDataConsumerHandle> handle = createWaitingDataConsumerHandle();
    std::unique_ptr<WebDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);

    EXPECT_EQ(kShouldWait, reader->read(buffer, sizeof(buffer), kNone, &size));
    EXPECT_EQ(kShouldWait, reader->beginRead(&p, kNone, &size));
    EXPECT_EQ(kUnexpectedError, reader->endRead(99));
}

TEST(DataConsumerHandleUtilTest, WaitingHandleNoNotification)
{
    RefPtr<DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest> test = DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest::create();
    // Test this function doesn't crash.
    test->run(createWaitingDataConsumerHandle());
}

TEST(DataConsumerHandleUtilTest, CreateDoneHandle)
{
    char buffer[20];
    const void* p = nullptr;
    size_t size = 0;
    std::unique_ptr<WebDataConsumerHandle> handle = createDoneDataConsumerHandle();
    std::unique_ptr<WebDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);

    EXPECT_EQ(kDone, reader->read(buffer, sizeof(buffer), kNone, &size));
    EXPECT_EQ(kDone, reader->beginRead(&p, kNone, &size));
    EXPECT_EQ(kUnexpectedError, reader->endRead(99));
}

TEST(DataConsumerHandleUtilTest, DoneHandleNotification)
{
    RefPtr<DataConsumerHandleTestUtil::ThreadingHandleNotificationTest> test = DataConsumerHandleTestUtil::ThreadingHandleNotificationTest::create();
    // Test this function returns.
    test->run(createDoneDataConsumerHandle());
}

TEST(DataConsumerHandleUtilTest, DoneHandleNoNotification)
{
    RefPtr<DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest> test = DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest::create();
    // Test this function doesn't crash.
    test->run(createDoneDataConsumerHandle());
}

TEST(DataConsumerHandleUtilTest, CreateUnexpectedErrorHandle)
{
    char buffer[20];
    const void* p = nullptr;
    size_t size = 0;
    std::unique_ptr<WebDataConsumerHandle> handle = createUnexpectedErrorDataConsumerHandle();
    std::unique_ptr<WebDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);

    EXPECT_EQ(kUnexpectedError, reader->read(buffer, sizeof(buffer), kNone, &size));
    EXPECT_EQ(kUnexpectedError, reader->beginRead(&p, kNone, &size));
    EXPECT_EQ(kUnexpectedError, reader->endRead(99));
}

TEST(DataConsumerHandleUtilTest, UnexpectedErrorHandleNotification)
{
    RefPtr<DataConsumerHandleTestUtil::ThreadingHandleNotificationTest> test = DataConsumerHandleTestUtil::ThreadingHandleNotificationTest::create();
    // Test this function returns.
    test->run(createUnexpectedErrorDataConsumerHandle());
}

TEST(DataConsumerHandleUtilTest, UnexpectedErrorHandleNoNotification)
{
    RefPtr<DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest> test = DataConsumerHandleTestUtil::ThreadingHandleNoNotificationTest::create();
    // Test this function doesn't crash.
    test->run(createUnexpectedErrorDataConsumerHandle());
}

} // namespace

} // namespace blink
