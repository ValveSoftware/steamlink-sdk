// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CancellableTaskFactory_h
#define CancellableTaskFactory_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTaskRunner.h"
#include "wtf/Allocator.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/WeakPtr.h"
#include <memory>
#include <type_traits>

namespace blink {

class TraceLocation;

class PLATFORM_EXPORT CancellableTaskFactory {
    WTF_MAKE_NONCOPYABLE(CancellableTaskFactory);
    USING_FAST_MALLOC(CancellableTaskFactory);
public:
    // A pair of mutually exclusive factory methods are provided for constructing
    // a CancellableTaskFactory, one for when a Oilpan heap object owns a
    // CancellableTaskFactory, and one when that owning object isn't controlled
    // by Oilpan.
    //
    // In the Oilpan case, as WTF::Closure objects are off-heap, we have to construct the
    // closure in such a manner that it doesn't end up referring back to the owning heap
    // object with a strong Persistent<> GC root reference. If we do, this will create
    // a heap <-> off-heap cycle and leak, the owning object can never be GCed.
    // Instead, the closure will keep an off-heap persistent reference of the weak
    // variety, which will refer back to the owner heap object safely (but weakly.)
    //
    template<typename T>
    static std::unique_ptr<CancellableTaskFactory> create(T* thisObject, void (T::*method)(), typename std::enable_if<IsGarbageCollectedType<T>::value>::type* = nullptr)
    {
        return wrapUnique(new CancellableTaskFactory(WTF::bind(method, wrapWeakPersistent(thisObject))));
    }

    template<typename T>
    static std::unique_ptr<CancellableTaskFactory> create(T* thisObject, void (T::*method)(), typename std::enable_if<!IsGarbageCollectedType<T>::value>::type* = nullptr)
    {
        return wrapUnique(new CancellableTaskFactory(WTF::bind(method, WTF::unretained(thisObject))));
    }

    bool isPending() const
    {
        return m_weakPtrFactory.hasWeakPtrs();
    }

    void cancel();

    // Returns a task that can be disabled by calling cancel().  The user takes
    // ownership of the task.  Creating a new task cancels any previous ones.
    WebTaskRunner::Task* cancelAndCreate();

protected:
    // Only intended used by unit tests wanting to stack allocate and/or pass in a closure value.
    // Please use the create() factory method elsewhere.
    explicit CancellableTaskFactory(std::unique_ptr<WTF::Closure> closure)
        : m_closure(std::move(closure))
        , m_weakPtrFactory(this)
    {
    }

private:
    class CancellableTask : public WebTaskRunner::Task {
        USING_FAST_MALLOC(CancellableTask);
        WTF_MAKE_NONCOPYABLE(CancellableTask);

    public:
        explicit CancellableTask(WeakPtr<CancellableTaskFactory> weakPtr)
            : m_weakPtr(weakPtr) {}

        ~CancellableTask() override {}

        void run() override;

    private:
        WeakPtr<CancellableTaskFactory> m_weakPtr;
    };

    std::unique_ptr<WTF::Closure> m_closure;
    WeakPtrFactory<CancellableTaskFactory> m_weakPtrFactory;
};

} // namespace blink

#endif // CancellableTaskFactory_h
