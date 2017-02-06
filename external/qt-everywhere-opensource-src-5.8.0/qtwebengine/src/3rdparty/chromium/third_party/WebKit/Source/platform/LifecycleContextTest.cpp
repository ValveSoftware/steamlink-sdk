/*
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

#include "platform/LifecycleNotifier.h"
#include "platform/LifecycleObserver.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestingObserver;

class DummyContext final : public GarbageCollectedFinalized<DummyContext>, public LifecycleNotifier<DummyContext, TestingObserver> {
    USING_GARBAGE_COLLECTED_MIXIN(DummyContext);
public:
    static DummyContext* create()
    {
        return new DummyContext;
    }

    DEFINE_INLINE_TRACE()
    {
        LifecycleNotifier<DummyContext, TestingObserver>::trace(visitor);
    }
};

class TestingObserver final : public GarbageCollected<TestingObserver>, public LifecycleObserver<DummyContext, TestingObserver> {
    USING_GARBAGE_COLLECTED_MIXIN(TestingObserver);
public:
    static TestingObserver* create(DummyContext* context)
    {
        return new TestingObserver(context);
    }

    void contextDestroyed() override
    {
        LifecycleObserver::contextDestroyed();
        if (m_observerToRemoveOnDestruct) {
            lifecycleContext()->removeObserver(m_observerToRemoveOnDestruct);
            m_observerToRemoveOnDestruct.clear();
        }
        m_contextDestroyedCalled = true;
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_observerToRemoveOnDestruct);
        LifecycleObserver::trace(visitor);
    }

    void unobserve() { setContext(nullptr); }

    void setObserverToRemoveAndDestroy(TestingObserver* observerToRemoveOnDestruct)
    {
        ASSERT(!m_observerToRemoveOnDestruct);
        m_observerToRemoveOnDestruct = observerToRemoveOnDestruct;
    }

    TestingObserver* innerObserver() const { return m_observerToRemoveOnDestruct; }
    bool contextDestroyedCalled() const { return m_contextDestroyedCalled; }

private:
    explicit TestingObserver(DummyContext* context)
        : LifecycleObserver(context)
        , m_contextDestroyedCalled(false)
    {
    }

    Member<TestingObserver> m_observerToRemoveOnDestruct;
    bool m_contextDestroyedCalled;
};

TEST(LifecycleContextTest, shouldObserveContextDestroyed)
{
    DummyContext* context = DummyContext::create();
    Persistent<TestingObserver> observer = TestingObserver::create(context);

    EXPECT_EQ(observer->lifecycleContext(), context);
    EXPECT_FALSE(observer->contextDestroyedCalled());
    context->notifyContextDestroyed();
    context = nullptr;
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(observer->lifecycleContext(), static_cast<DummyContext*>(0));
    EXPECT_TRUE(observer->contextDestroyedCalled());
}

TEST(LifecycleContextTest, shouldNotObserveContextDestroyedIfUnobserve)
{
    DummyContext* context = DummyContext::create();
    Persistent<TestingObserver> observer = TestingObserver::create(context);
    observer->unobserve();
    context->notifyContextDestroyed();
    context = nullptr;
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(observer->lifecycleContext(), static_cast<DummyContext*>(0));
    EXPECT_FALSE(observer->contextDestroyedCalled());
}

TEST(LifecycleContextTest, observerRemovedDuringNotifyDestroyed)
{
    DummyContext* context = DummyContext::create();
    Persistent<TestingObserver> observer = TestingObserver::create(context);
    TestingObserver* innerObserver = TestingObserver::create(context);
    // Attach the observer to the other. When 'observer' is notified
    // of destruction, it will remove & destroy 'innerObserver'.
    observer->setObserverToRemoveAndDestroy(innerObserver);

    EXPECT_EQ(observer->lifecycleContext(), context);
    EXPECT_EQ(observer->innerObserver()->lifecycleContext(), context);
    EXPECT_FALSE(observer->contextDestroyedCalled());
    EXPECT_FALSE(observer->innerObserver()->contextDestroyedCalled());

    context->notifyContextDestroyed();
    EXPECT_EQ(observer->innerObserver(), nullptr);
    context = nullptr;
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(observer->lifecycleContext(), static_cast<DummyContext*>(0));
    EXPECT_TRUE(observer->contextDestroyedCalled());
}

} // namespace blink
