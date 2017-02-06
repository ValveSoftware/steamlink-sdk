// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/message_pump/message_pump_mojo.h"

#include "base/macros.h"
#include "base/message_loop/message_loop_test.h"
#include "base/run_loop.h"
#include "mojo/message_pump/message_pump_mojo_handler.h"
#include "mojo/public/cpp/system/core.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {

std::unique_ptr<base::MessagePump> CreateMojoMessagePump() {
  return std::unique_ptr<base::MessagePump>(new MessagePumpMojo());
}

RUN_MESSAGE_LOOP_TESTS(Mojo, &CreateMojoMessagePump);

class CountingMojoHandler : public MessagePumpMojoHandler {
 public:
  CountingMojoHandler() : success_count_(0), error_count_(0) {}

  void OnHandleReady(const Handle& handle) override {
    ReadMessageRaw(static_cast<const MessagePipeHandle&>(handle),
                   NULL,
                   NULL,
                   NULL,
                   NULL,
                   MOJO_READ_MESSAGE_FLAG_NONE);
    ++success_count_;
    if (success_count_ == success_callback_count_ &&
        !success_callback_.is_null()) {
      success_callback_.Run();
      success_callback_.Reset();
    }
  }

  void set_success_callback(const base::Closure& callback,
                            int success_count) {
    success_callback_ = callback;
    success_callback_count_ = success_count;
  }

  void OnHandleError(const Handle& handle, MojoResult result) override {
    ++error_count_;
    if (!error_callback_.is_null()) {
      error_callback_.Run();
      error_callback_.Reset();
    }
  }

  void set_error_callback(const base::Closure& callback) {
    error_callback_ = callback;
  }

  int success_count() { return success_count_; }
  int error_count() { return error_count_; }

 private:
  int success_count_;
  int error_count_;

  base::Closure error_callback_;
  int success_callback_count_;

  base::Closure success_callback_;

  DISALLOW_COPY_AND_ASSIGN(CountingMojoHandler);
};

class CountingObserver : public MessagePumpMojo::Observer {
 public:
  void WillSignalHandler() override { will_signal_handler_count++; }
  void DidSignalHandler() override { did_signal_handler_count++; }

  int will_signal_handler_count = 0;
  int did_signal_handler_count = 0;
};

TEST(MessagePumpMojo, RunUntilIdle) {
  base::MessageLoop message_loop(MessagePumpMojo::Create());
  CountingMojoHandler handler;
  base::RunLoop run_loop;
  handler.set_success_callback(run_loop.QuitClosure(), 2);
  MessagePipe handles;
  MessagePumpMojo::current()->AddHandler(&handler,
                                         handles.handle0.get(),
                                         MOJO_HANDLE_SIGNAL_READABLE,
                                         base::TimeTicks());
  WriteMessageRaw(
      handles.handle1.get(), NULL, 0, NULL, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  WriteMessageRaw(
      handles.handle1.get(), NULL, 0, NULL, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  MojoHandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(handles.handle0.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                      MOJO_DEADLINE_INDEFINITE, &hss));
  run_loop.Run();
  EXPECT_EQ(2, handler.success_count());
}

TEST(MessagePumpMojo, Observer) {
  base::MessageLoop message_loop(MessagePumpMojo::Create());

  CountingObserver observer;
  MessagePumpMojo::current()->AddObserver(&observer);

  CountingMojoHandler handler;
  base::RunLoop run_loop;
  handler.set_success_callback(run_loop.QuitClosure(), 1);
  MessagePipe handles;
  MessagePumpMojo::current()->AddHandler(&handler,
                                         handles.handle0.get(),
                                         MOJO_HANDLE_SIGNAL_READABLE,
                                         base::TimeTicks());
  WriteMessageRaw(
      handles.handle1.get(), NULL, 0, NULL, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);

  MojoHandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(handles.handle0.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                      MOJO_DEADLINE_INDEFINITE, &hss));
  run_loop.Run();
  EXPECT_EQ(1, handler.success_count());
  EXPECT_EQ(1, observer.will_signal_handler_count);
  EXPECT_EQ(1, observer.did_signal_handler_count);
  MessagePumpMojo::current()->RemoveObserver(&observer);

  base::RunLoop run_loop2;
  handler.set_success_callback(run_loop2.QuitClosure(), 2);
  WriteMessageRaw(
      handles.handle1.get(), NULL, 0, NULL, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(handles.handle0.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                      MOJO_DEADLINE_INDEFINITE, &hss));
  run_loop2.Run();
  EXPECT_EQ(2, handler.success_count());
  EXPECT_EQ(1, observer.will_signal_handler_count);
  EXPECT_EQ(1, observer.did_signal_handler_count);
}

TEST(MessagePumpMojo, UnregisterAfterDeadline) {
  base::MessageLoop message_loop(MessagePumpMojo::Create());
  CountingMojoHandler handler;
  base::RunLoop run_loop;
  handler.set_error_callback(run_loop.QuitClosure());
  MessagePipe handles;
  MessagePumpMojo::current()->AddHandler(
      &handler,
      handles.handle0.get(),
      MOJO_HANDLE_SIGNAL_READABLE,
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1));
  run_loop.Run();
  EXPECT_EQ(1, handler.error_count());
}

TEST(MessagePumpMojo, AddClosedHandle) {
  base::MessageLoop message_loop(MessagePumpMojo::Create());
  CountingMojoHandler handler;
  MessagePipe handles;
  Handle closed_handle = handles.handle0.get();
  handles.handle0.reset();
  MessagePumpMojo::current()->AddHandler(
      &handler, closed_handle, MOJO_HANDLE_SIGNAL_READABLE, base::TimeTicks());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  MessagePumpMojo::current()->RemoveHandler(closed_handle);
  EXPECT_EQ(0, handler.error_count());
  EXPECT_EQ(0, handler.success_count());
}

TEST(MessagePumpMojo, CloseAfterAdding) {
  base::MessageLoop message_loop(MessagePumpMojo::Create());
  CountingMojoHandler handler;
  MessagePipe handles;
  MessagePumpMojo::current()->AddHandler(&handler, handles.handle0.get(),
                                         MOJO_HANDLE_SIGNAL_READABLE,
                                         base::TimeTicks());
  handles.handle0.reset();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(1, handler.error_count());
  EXPECT_EQ(0, handler.success_count());
}

}  // namespace test
}  // namespace common
}  // namespace mojo
