// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/wait_set_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/message_for_transit.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/request_context.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

class WaitSetDispatcherTest : public ::testing::Test {
 public:
  WaitSetDispatcherTest() {}
  ~WaitSetDispatcherTest() override {}

  void SetUp() override {
    CreateMessagePipe(&dispatcher0_, &dispatcher1_);
  }

  void TearDown() override {
    for (auto& d : dispatchers_to_close_)
      d->Close();
  }

  MojoResult GetOneReadyDispatcher(
      const scoped_refptr<WaitSetDispatcher>& wait_set,
      scoped_refptr<Dispatcher>* ready_dispatcher,
      uintptr_t* context) {
    uint32_t count = 1;
    MojoResult dispatcher_result = MOJO_RESULT_UNKNOWN;
    DispatcherVector dispatchers;
    MojoResult result = wait_set->GetReadyDispatchers(
        &count, &dispatchers, &dispatcher_result, context);
    if (result == MOJO_RESULT_OK) {
      CHECK_EQ(1u, dispatchers.size());
      *ready_dispatcher = dispatchers[0];
      return dispatcher_result;
    }
    return result;
  }

  void CreateMessagePipe(scoped_refptr<MessagePipeDispatcher>* d0,
                         scoped_refptr<MessagePipeDispatcher>* d1) {
    MojoHandle h0, h1;
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &h0, &h1));

    Core* core = mojo::edk::internal::g_core;
    *d0 = scoped_refptr<MessagePipeDispatcher>(
        static_cast<MessagePipeDispatcher*>(core->GetDispatcher(h0).get()));
    *d1 = scoped_refptr<MessagePipeDispatcher>(
        static_cast<MessagePipeDispatcher*>(core->GetDispatcher(h1).get()));
    pipe_id_generator_++;

    dispatchers_to_close_.push_back(*d0);
    dispatchers_to_close_.push_back(*d1);
  }

  void CloseOnShutdown(const scoped_refptr<Dispatcher>& dispatcher) {
    dispatchers_to_close_.push_back(dispatcher);
  }

  void WriteMessage(MessagePipeDispatcher* dispatcher,
                    const void* bytes,
                    size_t num_bytes) {
    Core* core = mojo::edk::internal::g_core;
    MojoMessageHandle msg;
    ASSERT_EQ(MOJO_RESULT_OK,
              core->AllocMessage(static_cast<uint32_t>(num_bytes), nullptr, 0,
                                 MOJO_ALLOC_MESSAGE_FLAG_NONE, &msg));
    void* buffer;
    ASSERT_EQ(MOJO_RESULT_OK, core->GetMessageBuffer(msg, &buffer));
    memcpy(buffer, bytes, num_bytes);

    std::unique_ptr<MessageForTransit> message(
        reinterpret_cast<MessageForTransit*>(msg));
    ASSERT_EQ(MOJO_RESULT_OK,
              dispatcher->WriteMessage(std::move(message),
                                       MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  void ReadMessage(MessagePipeDispatcher* dispatcher,
                   void* bytes,
                   uint32_t* num_bytes) {
    std::unique_ptr<MessageForTransit> message;
    ASSERT_EQ(MOJO_RESULT_OK,
              dispatcher->ReadMessage(&message, num_bytes, nullptr, 0,
                                      MOJO_READ_MESSAGE_FLAG_NONE, false));
    memcpy(bytes, message->bytes(), *num_bytes);
  }

 protected:
  scoped_refptr<MessagePipeDispatcher> dispatcher0_;
  scoped_refptr<MessagePipeDispatcher> dispatcher1_;

 private:
  // We keep an active RequestContext for the duration of each test. It's unused
  // since these tests don't rely on the MojoWatch API.
  const RequestContext request_context_;

  static uint64_t pipe_id_generator_;
  DispatcherVector dispatchers_to_close_;

  DISALLOW_COPY_AND_ASSIGN(WaitSetDispatcherTest);
};

// static
uint64_t WaitSetDispatcherTest::pipe_id_generator_ = 1;

TEST_F(WaitSetDispatcherTest, Basic) {
  scoped_refptr<WaitSetDispatcher> wait_set = new WaitSetDispatcher();
  CloseOnShutdown(wait_set);
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher0_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 1));
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher1_,
                                           MOJO_HANDLE_SIGNAL_WRITABLE, 2));

  Waiter w;
  uintptr_t context = 0;
  w.Init();
  HandleSignalsState hss;
  // |dispatcher1_| should already be writable.
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfiable_signals);

  scoped_refptr<Dispatcher> woken_dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, &context));
  EXPECT_EQ(dispatcher1_, woken_dispatcher);
  EXPECT_EQ(2u, context);
  // If a ready dispatcher isn't removed, it will continue to be returned.
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  woken_dispatcher = nullptr;
  context = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, &context));
  EXPECT_EQ(dispatcher1_, woken_dispatcher);
  EXPECT_EQ(2u, context);
  ASSERT_EQ(MOJO_RESULT_OK, wait_set->RemoveWaitingDispatcher(dispatcher1_));

  // No ready dispatcher.
  hss = HandleSignalsState();
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  EXPECT_FALSE(hss.satisfies(MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, w.Wait(0, nullptr));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));

  // Write to |dispatcher1_|, which should make |dispatcher0_| readable.
  char buffer[] = "abcd";
  w.Init();
  WriteMessage(dispatcher1_.get(), buffer, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, w.Wait(MOJO_DEADLINE_INDEFINITE, nullptr));
  woken_dispatcher = nullptr;
  context = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, &context));
  EXPECT_EQ(dispatcher0_, woken_dispatcher);
  EXPECT_EQ(1u, context);

  // Again, if a ready dispatcher isn't removed, it will continue to be
  // returned.
  woken_dispatcher = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));
  EXPECT_EQ(dispatcher0_, woken_dispatcher);

  wait_set->RemoveAwakable(&w, nullptr);
}

TEST_F(WaitSetDispatcherTest, HandleWithoutRemoving) {
  scoped_refptr<WaitSetDispatcher> wait_set = new WaitSetDispatcher();
  CloseOnShutdown(wait_set);
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher0_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 1));

  Waiter w;
  uintptr_t context = 0;
  w.Init();
  HandleSignalsState hss;
  // No ready dispatcher.
  hss = HandleSignalsState();
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  EXPECT_FALSE(hss.satisfies(MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, w.Wait(0, nullptr));
  scoped_refptr<Dispatcher> woken_dispatcher;
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));

  // The tested behaviour below should be repeatable.
  for (size_t i = 0; i < 3; i++) {
    // Write to |dispatcher1_|, which should make |dispatcher0_| readable.
    char buffer[] = "abcd";
    w.Init();
    WriteMessage(dispatcher1_.get(), buffer, sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_OK, w.Wait(MOJO_DEADLINE_INDEFINITE, nullptr));
    woken_dispatcher = nullptr;
    context = 0;
    EXPECT_EQ(MOJO_RESULT_OK,
              GetOneReadyDispatcher(wait_set, &woken_dispatcher, &context));
    EXPECT_EQ(dispatcher0_, woken_dispatcher);
    EXPECT_EQ(1u, context);

    // Read from |dispatcher0_| which should change it's state to non-readable.
    char read_buffer[sizeof(buffer) + 5];
    uint32_t num_bytes = sizeof(read_buffer);
    ReadMessage(dispatcher0_.get(), read_buffer, &num_bytes);
    EXPECT_EQ(sizeof(buffer), num_bytes);

    // No dispatchers are ready.
    w.Init();
    woken_dispatcher = nullptr;
    context = 0;
    EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
              GetOneReadyDispatcher(wait_set, &woken_dispatcher, &context));
    EXPECT_FALSE(woken_dispatcher);
    EXPECT_EQ(0u, context);
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, w.Wait(0, nullptr));
  }

  wait_set->RemoveAwakable(&w, nullptr);
}

TEST_F(WaitSetDispatcherTest, MultipleReady) {
  scoped_refptr<WaitSetDispatcher> wait_set = new WaitSetDispatcher();
  CloseOnShutdown(wait_set);

  scoped_refptr<MessagePipeDispatcher> mp1_dispatcher0;
  scoped_refptr<MessagePipeDispatcher> mp1_dispatcher1;
  CreateMessagePipe(&mp1_dispatcher0, &mp1_dispatcher1);

  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher0_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher1_,
                                           MOJO_HANDLE_SIGNAL_WRITABLE, 0));
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(mp1_dispatcher0,
                                           MOJO_HANDLE_SIGNAL_WRITABLE, 0));
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(mp1_dispatcher1,
                                           MOJO_HANDLE_SIGNAL_WRITABLE, 0));

  Waiter w;
  w.Init();
  HandleSignalsState hss;
  // The three writable dispatchers should be ready.
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfiable_signals);

  scoped_refptr<Dispatcher> woken_dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));
  // Don't know which dispatcher was returned, just that it was one of the
  // writable ones.
  EXPECT_TRUE(woken_dispatcher == dispatcher1_ ||
              woken_dispatcher == mp1_dispatcher0 ||
              woken_dispatcher == mp1_dispatcher1);

  DispatcherVector dispatchers_vector;
  uint32_t count = 4;
  MojoResult results[4];
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->GetReadyDispatchers(&count,
                                          &dispatchers_vector,
                                          results,
                                          nullptr));
  EXPECT_EQ(3u, count);
  std::sort(dispatchers_vector.begin(), dispatchers_vector.end());
  DispatcherVector expected_dispatchers;
  expected_dispatchers.push_back(dispatcher1_);
  expected_dispatchers.push_back(mp1_dispatcher0);
  expected_dispatchers.push_back(mp1_dispatcher1);
  std::sort(expected_dispatchers.begin(), expected_dispatchers.end());
  EXPECT_EQ(expected_dispatchers, dispatchers_vector);

  // If a ready dispatcher isn't removed, it will continue to be returned.
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfiable_signals);
  count = 4;
  dispatchers_vector.clear();
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->GetReadyDispatchers(&count,
                                          &dispatchers_vector,
                                          results,
                                          nullptr));
  EXPECT_EQ(3u, count);
  std::sort(dispatchers_vector.begin(), dispatchers_vector.end());
  EXPECT_EQ(expected_dispatchers, dispatchers_vector);

  // Remove one. It shouldn't be returned any longer.
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->RemoveWaitingDispatcher(expected_dispatchers.back()));
  expected_dispatchers.pop_back();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfiable_signals);
  count = 4;
  dispatchers_vector.clear();
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->GetReadyDispatchers(&count,
                                          &dispatchers_vector,
                                          results,
                                          nullptr));
  EXPECT_EQ(2u, count);
  std::sort(dispatchers_vector.begin(), dispatchers_vector.end());
  EXPECT_EQ(expected_dispatchers, dispatchers_vector);

  // Write to |dispatcher1_|, which should make |dispatcher0_| readable.
  char buffer[] = "abcd";
  w.Init();
  WriteMessage(dispatcher1_.get(), buffer, sizeof(buffer));
  {
    Waiter mp_w;
    mp_w.Init();
    // Wait for |dispatcher0_| to be readable.
    if (dispatcher0_->AddAwakable(&mp_w, MOJO_HANDLE_SIGNAL_READABLE, 0,
                                  nullptr) == MOJO_RESULT_OK) {
      EXPECT_EQ(MOJO_RESULT_OK, mp_w.Wait(MOJO_DEADLINE_INDEFINITE, 0));
      dispatcher0_->RemoveAwakable(&mp_w, nullptr);
    }
  }
  expected_dispatchers.push_back(dispatcher0_);
  std::sort(expected_dispatchers.begin(), expected_dispatchers.end());
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfiable_signals);
  count = 4;
  dispatchers_vector.clear();
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->GetReadyDispatchers(&count,
                                          &dispatchers_vector,
                                          results,
                                          nullptr));
  EXPECT_EQ(3u, count);
  std::sort(dispatchers_vector.begin(), dispatchers_vector.end());
  EXPECT_EQ(expected_dispatchers, dispatchers_vector);
}

TEST_F(WaitSetDispatcherTest, InvalidParams) {
  scoped_refptr<WaitSetDispatcher> wait_set = new WaitSetDispatcher();

  // Can't add a wait set to itself.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            wait_set->AddWaitingDispatcher(wait_set,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));

  // Can't add twice.
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher0_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            wait_set->AddWaitingDispatcher(dispatcher0_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));

  // Remove a dispatcher that wasn't added.
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            wait_set->RemoveWaitingDispatcher(dispatcher1_));

  // Add to a closed wait set.
  wait_set->Close();
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            wait_set->AddWaitingDispatcher(dispatcher0_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));
}

TEST_F(WaitSetDispatcherTest, NotSatisfiable) {
  scoped_refptr<WaitSetDispatcher> wait_set = new WaitSetDispatcher();
  CloseOnShutdown(wait_set);

  // Wait sets can only satisfy MOJO_HANDLE_SIGNAL_READABLE.
  Waiter w;
  w.Init();
  HandleSignalsState hss;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_WRITABLE, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_NONE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfiable_signals);

  hss = HandleSignalsState();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_PEER_CLOSED, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_NONE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfiable_signals);
}

TEST_F(WaitSetDispatcherTest, ClosedDispatchers) {
  scoped_refptr<WaitSetDispatcher> wait_set = new WaitSetDispatcher();
  CloseOnShutdown(wait_set);

  Waiter w;
  w.Init();
  HandleSignalsState hss;
  // A dispatcher that was added and then closed will be cancelled.
  ASSERT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher0_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, &hss));
  dispatcher0_->Close();
  EXPECT_EQ(MOJO_RESULT_OK, w.Wait(MOJO_DEADLINE_INDEFINITE, nullptr));
  EXPECT_TRUE(
      wait_set->GetHandleSignalsState().satisfies(MOJO_HANDLE_SIGNAL_READABLE));
  scoped_refptr<Dispatcher> woken_dispatcher;
  EXPECT_EQ(MOJO_RESULT_CANCELLED,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));
  EXPECT_EQ(dispatcher0_, woken_dispatcher);

  // Dispatcher will be implicitly removed because it may be impossible to
  // remove explicitly.
  woken_dispatcher = nullptr;
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            wait_set->RemoveWaitingDispatcher(dispatcher0_));

  // A dispatcher that's not satisfiable should give an error.
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(dispatcher1_,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));
  EXPECT_EQ(MOJO_RESULT_OK, w.Wait(MOJO_DEADLINE_INDEFINITE, nullptr));
  EXPECT_TRUE(
      wait_set->GetHandleSignalsState().satisfies(MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));
  EXPECT_EQ(dispatcher1_, woken_dispatcher);

  wait_set->RemoveAwakable(&w, nullptr);
}

TEST_F(WaitSetDispatcherTest, NestedSets) {
  scoped_refptr<WaitSetDispatcher> wait_set = new WaitSetDispatcher();
  CloseOnShutdown(wait_set);
  scoped_refptr<WaitSetDispatcher> nested_wait_set = new WaitSetDispatcher();
  CloseOnShutdown(nested_wait_set);

  Waiter w;
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->AddWaitingDispatcher(nested_wait_set,
                                           MOJO_HANDLE_SIGNAL_READABLE, 0));
  EXPECT_EQ(MOJO_RESULT_OK,
            wait_set->AddAwakable(&w, MOJO_HANDLE_SIGNAL_READABLE, 0, nullptr));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, w.Wait(0, nullptr));

  // Writable signal is immediately satisfied by the message pipe.
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            nested_wait_set->AddWaitingDispatcher(
                dispatcher0_, MOJO_HANDLE_SIGNAL_WRITABLE, 0));
  EXPECT_EQ(MOJO_RESULT_OK, w.Wait(0, nullptr));
  scoped_refptr<Dispatcher> woken_dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            GetOneReadyDispatcher(wait_set, &woken_dispatcher, nullptr));
  EXPECT_EQ(nested_wait_set, woken_dispatcher);

  wait_set->RemoveAwakable(&w, nullptr);
}

}  // namespace
}  // namespace edk
}  // namespace mojo
