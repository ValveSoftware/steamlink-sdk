/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/dom/MainThreadTaskRunner.h"

#include "core/dom/ExecutionContextTask.h"
#include "core/testing/NullExecutionContext.h"
#include "core/testing/UnitTestHelpers.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class MarkingBooleanTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<MarkingBooleanTask> create(bool* toBeMarked)
    {
        return adoptPtr(new MarkingBooleanTask(toBeMarked));
    }


    virtual ~MarkingBooleanTask() { }

private:
    MarkingBooleanTask(bool* toBeMarked) : m_toBeMarked(toBeMarked) { }

    virtual void performTask(ExecutionContext* context) OVERRIDE
    {
        *m_toBeMarked = true;
    }

    bool* m_toBeMarked;
};

TEST(MainThreadTaskRunnerTest, PostTask)
{
    RefPtrWillBeRawPtr<NullExecutionContext> context = adoptRefWillBeNoop(new NullExecutionContext());
    OwnPtr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context.get());
    bool isMarked = false;

    runner->postTask(MarkingBooleanTask::create(&isMarked));
    EXPECT_FALSE(isMarked);
    WebCore::testing::runPendingTasks();
    EXPECT_TRUE(isMarked);
}

TEST(MainThreadTaskRunnerTest, SuspendTask)
{
    RefPtrWillBeRawPtr<NullExecutionContext> context = adoptRefWillBeNoop(new NullExecutionContext());
    OwnPtr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context.get());
    bool isMarked = false;

    context->setTasksNeedSuspension(true);
    runner->postTask(MarkingBooleanTask::create(&isMarked));
    runner->suspend();
    WebCore::testing::runPendingTasks();
    EXPECT_FALSE(isMarked);

    context->setTasksNeedSuspension(false);
    runner->resume();
    WebCore::testing::runPendingTasks();
    EXPECT_TRUE(isMarked);
}

TEST(MainThreadTaskRunnerTest, RemoveRunner)
{
    RefPtrWillBeRawPtr<NullExecutionContext> context = adoptRefWillBeNoop(new NullExecutionContext());
    OwnPtr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context.get());
    bool isMarked = false;

    context->setTasksNeedSuspension(true);
    runner->postTask(MarkingBooleanTask::create(&isMarked));
    runner.clear();
    WebCore::testing::runPendingTasks();
    EXPECT_FALSE(isMarked);
}

}
