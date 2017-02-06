// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/Persistent.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {
namespace {

void preciselyCollectGarbage()
{
    ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);
}

class Receiver : public GarbageCollected<Receiver> {
public:
    void increment(int* counter)
    {
        ++*counter;
    }

    DEFINE_INLINE_TRACE() {}
};

TEST(PersistentTest, BindCancellation)
{
    Receiver* receiver = new Receiver;
    int counter = 0;
    std::unique_ptr<WTF::Closure> function = WTF::bind(&Receiver::increment, wrapWeakPersistent(receiver), WTF::unretained(&counter));

    (*function)();
    EXPECT_EQ(1, counter);

    receiver = nullptr;
    preciselyCollectGarbage();
    (*function)();
    EXPECT_EQ(1, counter);
}

TEST(PersistentTest, CrossThreadBindCancellation)
{
    Receiver* receiver = new Receiver;
    int counter = 0;
    std::unique_ptr<CrossThreadClosure> function = blink::crossThreadBind(&Receiver::increment, wrapCrossThreadWeakPersistent(receiver), WTF::crossThreadUnretained(&counter));

    (*function)();
    EXPECT_EQ(1, counter);

    receiver = nullptr;
    preciselyCollectGarbage();
    (*function)();
    EXPECT_EQ(1, counter);
}

} // namespace
} // namespace blink
