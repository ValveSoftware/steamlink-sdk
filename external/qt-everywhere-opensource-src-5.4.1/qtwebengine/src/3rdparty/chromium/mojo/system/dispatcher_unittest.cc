// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/dispatcher.h"

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "mojo/system/raw_shared_buffer.h"
#include "mojo/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

// Trivial subclass that makes the constructor public.
class TrivialDispatcher : public Dispatcher {
 public:
  TrivialDispatcher() {}

  virtual Type GetType() const OVERRIDE {
    return kTypeUnknown;
  }

 private:
  friend class base::RefCountedThreadSafe<TrivialDispatcher>;
  virtual ~TrivialDispatcher() {}

  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE {
    lock().AssertAcquired();
    return scoped_refptr<Dispatcher>(new TrivialDispatcher());
  }

  DISALLOW_COPY_AND_ASSIGN(TrivialDispatcher);
};

TEST(DispatcherTest, Basic) {
  scoped_refptr<Dispatcher> d(new TrivialDispatcher());

  EXPECT_EQ(Dispatcher::kTypeUnknown, d->GetType());

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->WriteMessage(NULL, 0, NULL, MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->ReadMessage(NULL, NULL, NULL, NULL,
                           MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->WriteData(NULL, NULL, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->BeginWriteData(NULL, NULL, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->EndWriteData(0));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->ReadData(NULL, NULL, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->BeginReadData(NULL, NULL, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->EndReadData(0));
  Waiter w;
  w.Init();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            d->AddWaiter(&w, ~MOJO_HANDLE_SIGNAL_NONE, 0));
  // Okay to remove even if it wasn't added (or was already removed).
  d->RemoveWaiter(&w);
  d->RemoveWaiter(&w);

  EXPECT_EQ(MOJO_RESULT_OK, d->Close());

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->WriteMessage(NULL, 0, NULL, MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->ReadMessage(NULL, NULL, NULL, NULL,
                           MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->WriteData(NULL, NULL, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->BeginWriteData(NULL, NULL, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->EndWriteData(0));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->ReadData(NULL, NULL, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->BeginReadData(NULL, NULL, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->EndReadData(0));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->AddWaiter(&w, ~MOJO_HANDLE_SIGNAL_NONE, 0));
  d->RemoveWaiter(&w);
}

class ThreadSafetyStressThread : public base::SimpleThread {
 public:
  enum DispatcherOp {
    CLOSE = 0,
    WRITE_MESSAGE,
    READ_MESSAGE,
    WRITE_DATA,
    BEGIN_WRITE_DATA,
    END_WRITE_DATA,
    READ_DATA,
    BEGIN_READ_DATA,
    END_READ_DATA,
    DUPLICATE_BUFFER_HANDLE,
    MAP_BUFFER,
    ADD_WAITER,
    REMOVE_WAITER,

    DISPATCHER_OP_COUNT
  };

  ThreadSafetyStressThread(base::WaitableEvent* event,
                           scoped_refptr<Dispatcher> dispatcher,
                           DispatcherOp op)
      : base::SimpleThread("thread_safety_stress_thread"),
        event_(event),
        dispatcher_(dispatcher),
        op_(op) {
    CHECK_LE(0, op_);
    CHECK_LT(op_, DISPATCHER_OP_COUNT);
  }

  virtual ~ThreadSafetyStressThread() {
    Join();
  }

 private:
  virtual void Run() OVERRIDE {
    event_->Wait();

    waiter_.Init();
    switch(op_) {
      case CLOSE: {
        MojoResult r = dispatcher_->Close();
        EXPECT_TRUE(r == MOJO_RESULT_OK || r == MOJO_RESULT_INVALID_ARGUMENT)
            << "Result: " << r;
        break;
      }
      case WRITE_MESSAGE:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->WriteMessage(NULL, 0, NULL,
                                            MOJO_WRITE_MESSAGE_FLAG_NONE));
        break;
      case READ_MESSAGE:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->ReadMessage(NULL, NULL, NULL, NULL,
                                           MOJO_WRITE_MESSAGE_FLAG_NONE));
        break;
      case WRITE_DATA:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->WriteData(NULL, NULL,
                                         MOJO_WRITE_DATA_FLAG_NONE));
        break;
      case BEGIN_WRITE_DATA:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->BeginWriteData(NULL, NULL,
                                              MOJO_WRITE_DATA_FLAG_NONE));
        break;
      case END_WRITE_DATA:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->EndWriteData(0));
        break;
      case READ_DATA:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->ReadData(NULL, NULL, MOJO_READ_DATA_FLAG_NONE));
        break;
      case BEGIN_READ_DATA:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->BeginReadData(NULL, NULL,
                                             MOJO_READ_DATA_FLAG_NONE));
        break;
      case END_READ_DATA:
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->EndReadData(0));
        break;
      case DUPLICATE_BUFFER_HANDLE: {
        scoped_refptr<Dispatcher> unused;
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->DuplicateBufferHandle(NULL, &unused));
        break;
      }
      case MAP_BUFFER: {
        scoped_ptr<RawSharedBufferMapping> unused;
        EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                  dispatcher_->MapBuffer(0u, 0u, MOJO_MAP_BUFFER_FLAG_NONE,
                                         &unused));
        break;
      }
      case ADD_WAITER: {
        MojoResult r = dispatcher_->AddWaiter(&waiter_,
                                              ~MOJO_HANDLE_SIGNAL_NONE, 0);
        EXPECT_TRUE(r == MOJO_RESULT_FAILED_PRECONDITION ||
                    r == MOJO_RESULT_INVALID_ARGUMENT);
        break;
      }
      case REMOVE_WAITER:
        dispatcher_->RemoveWaiter(&waiter_);
        break;
      default:
        NOTREACHED();
        break;
    }

    // Always try to remove the waiter, in case we added it.
    dispatcher_->RemoveWaiter(&waiter_);
  }

  base::WaitableEvent* const event_;
  const scoped_refptr<Dispatcher> dispatcher_;
  const DispatcherOp op_;

  Waiter waiter_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafetyStressThread);
};

TEST(DispatcherTest, ThreadSafetyStress) {
  static const size_t kRepeatCount = 20;
  static const size_t kNumThreads = 100;

  for (size_t i = 0; i < kRepeatCount; i++) {
    // Manual reset, not initially signalled.
    base::WaitableEvent event(true, false);
    scoped_refptr<Dispatcher> d(new TrivialDispatcher());

    {
      ScopedVector<ThreadSafetyStressThread> threads;
      for (size_t j = 0; j < kNumThreads; j++) {
        ThreadSafetyStressThread::DispatcherOp op =
            static_cast<ThreadSafetyStressThread::DispatcherOp>(
                (i+j) % ThreadSafetyStressThread::DISPATCHER_OP_COUNT);
        threads.push_back(new ThreadSafetyStressThread(&event, d, op));
        threads.back()->Start();
      }
      event.Signal();  // Kicks off real work on the threads.
    }  // Joins all the threads.

    // One of the threads should already have closed the dispatcher.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, d->Close());
  }
}

TEST(DispatcherTest, ThreadSafetyStressNoClose) {
  static const size_t kRepeatCount = 20;
  static const size_t kNumThreads = 100;

  for (size_t i = 0; i < kRepeatCount; i++) {
    // Manual reset, not initially signalled.
    base::WaitableEvent event(true, false);
    scoped_refptr<Dispatcher> d(new TrivialDispatcher());

    {
      ScopedVector<ThreadSafetyStressThread> threads;
      for (size_t j = 0; j < kNumThreads; j++) {
        ThreadSafetyStressThread::DispatcherOp op =
            static_cast<ThreadSafetyStressThread::DispatcherOp>(
                (i+j) % (ThreadSafetyStressThread::DISPATCHER_OP_COUNT-1) + 1);
        threads.push_back(new ThreadSafetyStressThread(&event, d, op));
        threads.back()->Start();
      }
      event.Signal();  // Kicks off real work on the threads.
    }  // Joins all the threads.

    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }
}

}  // namespace
}  // namespace system
}  // namespace mojo
