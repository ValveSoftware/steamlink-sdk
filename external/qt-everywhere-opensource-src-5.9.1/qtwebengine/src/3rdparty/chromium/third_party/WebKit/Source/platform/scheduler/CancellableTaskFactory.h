// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CancellableTaskFactory_h
#define CancellableTaskFactory_h

#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/WeakPtr.h"
#include <memory>
#include <type_traits>

// WebTaskRunner::postCancellableTask will replace CancellableTaskFactory.
// Use postCancellableTask in new code.
// Example: For |task_runner| and |foo| below.
//   WebTaskRunner* task_runner;
//   Foo* foo;
//
//   CancellableTaskFactory factory(foo, &Foo::bar);
//   task_runner->postTask(BLINK_FROM_HERE, factory.cancelAndCreate());
//   factory.cancel();
//
// Above is equivalent to below:
//
//   std::unique_ptr<WTF::Closure> task =
//       WTF::bind(wrapPersistent(foo), &Foo::bar);
//   TaskHandle handle =
//       task_runner->postCancellableTask(BLINK_FROM_HERE, std::move(task));
//   handle.cancel();
namespace blink {

class PLATFORM_EXPORT CancellableTaskFactory {
  WTF_MAKE_NONCOPYABLE(CancellableTaskFactory);
  USING_FAST_MALLOC(CancellableTaskFactory);

 public:
  // As WTF::Closure objects are off-heap, we have to construct the closure in
  // such a manner that it doesn't end up referring back to the owning heap
  // object with a strong Persistent<> GC root reference. If we do, this will
  // create a heap <-> off-heap cycle and leak, the owning object can never be
  // GCed. Instead, the closure will keep an off-heap persistent reference of
  // the weak, which will refer back to the owner heap object safely (but
  // weakly.)
  //
  // DEPRECATED: Please use WebTaskRunner::postCancellableTask instead.
  // (https://crbug.com/665285)
  template <typename T>
  static std::unique_ptr<CancellableTaskFactory> create(
      T* thisObject,
      void (T::*method)(),
      typename std::enable_if<IsGarbageCollectedType<T>::value>::type* =
          nullptr) {
    return wrapUnique(new CancellableTaskFactory(
        WTF::bind(method, wrapWeakPersistent(thisObject))));
  }

  bool isPending() const { return m_weakPtrFactory.hasWeakPtrs(); }

  void cancel();

  // Returns a task that can be disabled by calling cancel().  The user takes
  // ownership of the task.  Creating a new task cancels any previous ones.
  WebTaskRunner::Task* cancelAndCreate();

 protected:
  // Only intended used by unit tests wanting to stack allocate and/or pass in a
  // closure value. Please use the create() factory method elsewhere.
  explicit CancellableTaskFactory(std::unique_ptr<WTF::Closure> closure)
      : m_closure(std::move(closure)), m_weakPtrFactory(this) {}

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

}  // namespace blink

#endif  // CancellableTaskFactory_h
