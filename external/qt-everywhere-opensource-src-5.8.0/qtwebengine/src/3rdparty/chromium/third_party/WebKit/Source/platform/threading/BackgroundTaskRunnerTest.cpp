// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/threading/BackgroundTaskRunner.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>


namespace {

using namespace blink;

void PingPongTask(WaitableEvent* doneEvent)
{
    doneEvent->signal();
}

class BackgroundTaskRunnerTest : public testing::Test {
};

TEST_F(BackgroundTaskRunnerTest, RunShortTaskOnBackgroundThread)
{
    std::unique_ptr<WaitableEvent> doneEvent = wrapUnique(new WaitableEvent());
    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, crossThreadBind(&PingPongTask, crossThreadUnretained(doneEvent.get())), BackgroundTaskRunner::TaskSizeShortRunningTask);
    // Test passes by not hanging on the following wait().
    doneEvent->wait();
}

TEST_F(BackgroundTaskRunnerTest, RunLongTaskOnBackgroundThread)
{
    std::unique_ptr<WaitableEvent> doneEvent = wrapUnique(new WaitableEvent());
    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, crossThreadBind(&PingPongTask, crossThreadUnretained(doneEvent.get())), BackgroundTaskRunner::TaskSizeLongRunningTask);
    // Test passes by not hanging on the following wait().
    doneEvent->wait();
}

} // unnamed namespace
