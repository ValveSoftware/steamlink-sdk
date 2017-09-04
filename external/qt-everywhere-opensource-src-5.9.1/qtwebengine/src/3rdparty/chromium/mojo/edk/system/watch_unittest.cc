// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/system/request_context.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/public/c/system/functions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

void IgnoreResult(uintptr_t context,
                  MojoResult result,
                  MojoHandleSignalsState signals,
                  MojoWatchNotificationFlags flags) {
}

// A test helper class for watching a handle. The WatchHelper instance is used
// as a watch context for a single watch callback.
class WatchHelper {
 public:
  using Callback =
      std::function<void(MojoResult result, MojoHandleSignalsState state)>;

  WatchHelper() : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~WatchHelper() {
    CHECK(!watching_);
  }

  void Watch(MojoHandle handle,
             MojoHandleSignals signals,
             const Callback& callback) {
    CHECK(!watching_);

    handle_ = handle;
    callback_ = callback;
    watching_ = true;
    CHECK_EQ(MOJO_RESULT_OK, MojoWatch(handle_, signals, &WatchHelper::OnNotify,
                                       reinterpret_cast<uintptr_t>(this)));
  }

  bool is_watching() const { return watching_; }

  void Cancel() {
    CHECK_EQ(MOJO_RESULT_OK,
             MojoCancelWatch(handle_, reinterpret_cast<uintptr_t>(this)));
    CHECK(watching_);
    watching_ = false;
  }

 private:
  static void OnNotify(uintptr_t context,
                       MojoResult result,
                       MojoHandleSignalsState state,
                       MojoWatchNotificationFlags flags) {
    WatchHelper* watcher = reinterpret_cast<WatchHelper*>(context);
    watcher->task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&NotifyOnMainThread, context, result, state, flags));
  }

  static void NotifyOnMainThread(uintptr_t context,
                                 MojoResult result,
                                 MojoHandleSignalsState state,
                                 MojoWatchNotificationFlags flags) {
    WatchHelper* watcher = reinterpret_cast<WatchHelper*>(context);
    CHECK(watcher->watching_);
    if (result == MOJO_RESULT_CANCELLED)
      watcher->watching_ = false;
    watcher->callback_(result, state);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool watching_ = false;
  MojoHandle handle_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(WatchHelper);
};

class WatchTest : public test::MojoTestBase {
 public:
  WatchTest() {}
  ~WatchTest() override {}

 protected:

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WatchTest);
};

TEST_F(WatchTest, NotifyBasic) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::RunLoop loop;
  WatchHelper b_watcher;
  b_watcher.Watch(
      b, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_OK, result);
        EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
        EXPECT_TRUE(b_watcher.is_watching());
        loop.Quit();
      });

  WriteMessage(a, "Hello!");
  loop.Run();

  EXPECT_TRUE(b_watcher.is_watching());
  b_watcher.Cancel();

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, NotifyUnsatisfiable) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::RunLoop loop;
  WatchHelper b_watcher;
  b_watcher.Watch(
      b, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
        EXPECT_EQ(0u,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
        EXPECT_EQ(0u,
                  state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
        EXPECT_TRUE(b_watcher.is_watching());
        loop.Quit();
      });

  CloseHandle(a);
  loop.Run();

  b_watcher.Cancel();

  CloseHandle(b);
}

TEST_F(WatchTest, NotifyCancellation) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::RunLoop loop;
  WatchHelper b_watcher;
  b_watcher.Watch(
      b, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
        EXPECT_EQ(0u, state.satisfied_signals);
        EXPECT_EQ(0u, state.satisfiable_signals);
        EXPECT_FALSE(b_watcher.is_watching());
        loop.Quit();
      });

  CloseHandle(b);
  loop.Run();

  CloseHandle(a);
}

TEST_F(WatchTest, InvalidArguemnts) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  uintptr_t context = reinterpret_cast<uintptr_t>(this);
  EXPECT_EQ(MOJO_RESULT_OK, MojoWatch(a, MOJO_HANDLE_SIGNAL_READABLE,
                                      &IgnoreResult, context));

  // Can't cancel a watch that doesn't exist.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoCancelWatch(a, ~context));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoCancelWatch(b, context));

  CloseHandle(a);
  CloseHandle(b);

  // Can't watch a handle that doesn't exist.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoWatch(a, MOJO_HANDLE_SIGNAL_READABLE, &IgnoreResult, context));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoWatch(b, MOJO_HANDLE_SIGNAL_READABLE, &IgnoreResult, context));
}

TEST_F(WatchTest, NoDuplicateContext) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  // Try to add the same watch twice; should fail.
  uintptr_t context = reinterpret_cast<uintptr_t>(this);
  EXPECT_EQ(MOJO_RESULT_OK, MojoWatch(a, MOJO_HANDLE_SIGNAL_READABLE,
                                      &IgnoreResult, context));
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
      MojoWatch(a, MOJO_HANDLE_SIGNAL_READABLE, &IgnoreResult, context));

  // Cancel and add it again; should be OK.
  EXPECT_EQ(MOJO_RESULT_OK, MojoCancelWatch(a, context));
  EXPECT_EQ(MOJO_RESULT_OK, MojoWatch(a, MOJO_HANDLE_SIGNAL_READABLE,
                                      &IgnoreResult, context));

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, MultipleWatches) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  // Add multiple watchers to |b| and see that they are both notified by a
  // single write to |a|.
  base::RunLoop loop;
  int expected_notifications = 2;
  auto on_readable = [&] (MojoResult result, MojoHandleSignalsState state) {
    EXPECT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
              state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_GT(expected_notifications, 0);
    if (--expected_notifications == 0)
      loop.Quit();
  };
  WatchHelper watcher1;
  WatchHelper watcher2;
  watcher1.Watch(b, MOJO_HANDLE_SIGNAL_READABLE, on_readable);
  watcher2.Watch(b, MOJO_HANDLE_SIGNAL_READABLE, on_readable);

  WriteMessage(a, "Ping!");
  loop.Run();

  watcher1.Cancel();
  watcher2.Cancel();

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, WatchWhileSatisfied) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  // Write to |a| and then start watching |b|. The callback should be invoked
  // synchronously.
  WriteMessage(a, "hey");
  bool signaled = false;
  WatchHelper b_watcher;
  base::RunLoop loop;
  b_watcher.Watch(
      b, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_OK, result);
        EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
        signaled = true;
        loop.Quit();
      });
  loop.Run();
  EXPECT_TRUE(signaled);
  b_watcher.Cancel();

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, WatchWhileUnsatisfiable) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  // Close |a| and then try to watch |b|. MojoWatch() should fail.
  CloseHandle(a);
  uintptr_t context = reinterpret_cast<uintptr_t>(this);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoWatch(b, MOJO_HANDLE_SIGNAL_READABLE, &IgnoreResult, context));

  CloseHandle(b);
}

TEST_F(WatchTest, RespondFromCallback) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  // Watch |a| and |b|. Write to |a|, then write to |b| from within the callback
  // which notifies it of the available message.
  const std::string kTestMessage = "hello worlds.";
  base::RunLoop loop;
  WatchHelper b_watcher;
  b_watcher.Watch(
      b, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_OK, result);
        EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
        EXPECT_TRUE(b_watcher.is_watching());

        // Echo a's message back to it.
        WriteMessage(b, ReadMessage(b));
      });

  WatchHelper a_watcher;
  a_watcher.Watch(
      a, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_OK, result);
        EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
        EXPECT_TRUE(a_watcher.is_watching());

        // Expect to receive back the message that was originally sent to |b|.
        EXPECT_EQ(kTestMessage, ReadMessage(a));

        loop.Quit();
      });

  WriteMessage(a, kTestMessage);
  loop.Run();

  a_watcher.Cancel();
  b_watcher.Cancel();

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, WatchDataPipeConsumer) {
  MojoHandle a, b;
  CreateDataPipe(&a, &b, 64);

  base::RunLoop loop;
  WatchHelper b_watcher;
  b_watcher.Watch(
      b, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_OK, result);
        EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
        EXPECT_TRUE(b_watcher.is_watching());
        loop.Quit();
      });

  WriteData(a, "Hello!");
  loop.Run();

  EXPECT_TRUE(b_watcher.is_watching());
  b_watcher.Cancel();

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, WatchDataPipeProducer) {
  MojoHandle a, b;
  CreateDataPipe(&a, &b, 8);

  // Fill the pipe to capacity so writes will block.
  WriteData(a, "xxxxxxxx");

  base::RunLoop loop;
  WatchHelper a_watcher;
  a_watcher.Watch(
      a, MOJO_HANDLE_SIGNAL_WRITABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_OK, result);
        EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
        EXPECT_TRUE(a_watcher.is_watching());
        loop.Quit();
      });

  EXPECT_EQ("xxxxxxxx", ReadData(b, 8));
  loop.Run();

  EXPECT_TRUE(a_watcher.is_watching());
  a_watcher.Cancel();

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, WakeUpSelfWithinWatchCallback) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  int expected_notifications = 2;
  base::RunLoop loop;
  WatchHelper b_watcher;
  b_watcher.Watch(
      b, MOJO_HANDLE_SIGNAL_READABLE,
      [&] (MojoResult result, MojoHandleSignalsState state) {
        EXPECT_EQ(MOJO_RESULT_OK, result);
        EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
                  state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
        EXPECT_TRUE(b_watcher.is_watching());
        if (--expected_notifications == 0) {
          loop.Quit();
        } else {
          // Trigger b's watch again from within this callback. This should be
          // safe to do.
          WriteMessage(a, "hey");
        }
      });

  WriteMessage(a, "hey hey hey");
  loop.Run();

  b_watcher.Cancel();

  CloseHandle(a);
  CloseHandle(b);
}

TEST_F(WatchTest, NestedCancellation) {
  // Verifies that cancellations in nested system request contexts preempt
  // other notifications for the same watcher. This tests against the condition
  // hit by http://crbug.com/622298.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  base::RunLoop loop;
  bool a_watcher_run = false;
  WatchHelper a_watcher;
  a_watcher.Watch(
      a, MOJO_HANDLE_SIGNAL_READABLE,
      [&](MojoResult result, MojoHandleSignalsState state) {
        a_watcher_run = true;
      });

  WatchHelper c_watcher;
  c_watcher.Watch(
      c, MOJO_HANDLE_SIGNAL_READABLE,
      [&](MojoResult result, MojoHandleSignalsState state) {
        // This will trigger a notification on |a_watcher| above to be executed
        // once this handler finishes running...
        CloseHandle(b);

        // ...but this should prevent that notification from dispatching because
        // |a_watcher| is now cancelled.
        a_watcher.Cancel();

        loop.Quit();
      });

  {
    // Force "system" notifications for the synchronous behavior required to
    // test this case.
    mojo::edk::RequestContext request_context(
        mojo::edk::RequestContext::Source::SYSTEM);

    // Trigger the |c_watcher| callback above.
    CloseHandle(d);
  }

  loop.Run();

  EXPECT_FALSE(a_watcher.is_watching());
  EXPECT_FALSE(a_watcher_run);

  c_watcher.Cancel();

  CloseHandle(a);
  CloseHandle(c);
}

}  // namespace
}  // namespace edk
}  // namespace mojo
