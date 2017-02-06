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

#include "core/dom/MainThreadTaskRunner.h"

#include "core/dom/ExecutionContextTask.h"
#include "core/testing/NullExecutionContext.h"
#include "platform/heap/Handle.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

static void markBoolean(bool* toBeMarked)
{
    *toBeMarked = true;
}

TEST(MainThreadTaskRunnerTest, PostTask)
{
    NullExecutionContext* context = new NullExecutionContext();
    std::unique_ptr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context);
    bool isMarked = false;

    runner->postTask(BLINK_FROM_HERE, createSameThreadTask(&markBoolean, WTF::unretained(&isMarked)));
    EXPECT_FALSE(isMarked);
    blink::testing::runPendingTasks();
    EXPECT_TRUE(isMarked);
}

TEST(MainThreadTaskRunnerTest, SuspendTask)
{
    NullExecutionContext* context = new NullExecutionContext();
    std::unique_ptr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context);
    bool isMarked = false;

    context->setTasksNeedSuspension(true);
    runner->postTask(BLINK_FROM_HERE, createSameThreadTask(&markBoolean, WTF::unretained(&isMarked)));
    runner->suspend();
    blink::testing::runPendingTasks();
    EXPECT_FALSE(isMarked);

    context->setTasksNeedSuspension(false);
    runner->resume();
    blink::testing::runPendingTasks();
    EXPECT_TRUE(isMarked);
}

TEST(MainThreadTaskRunnerTest, RemoveRunner)
{
    NullExecutionContext* context = new NullExecutionContext();
    std::unique_ptr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context);
    bool isMarked = false;

    context->setTasksNeedSuspension(true);
    runner->postTask(BLINK_FROM_HERE, createSameThreadTask(&markBoolean, WTF::unretained(&isMarked)));
    runner = nullptr;
    blink::testing::runPendingTasks();
    EXPECT_FALSE(isMarked);
}

} // namespace blink
