// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/stash_backend.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// Create a data pipe, write some data to the producer handle and return the
// consumer handle.
mojo::ScopedHandle CreateReadableHandle() {
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1, 1,
  };
  MojoResult result =
      mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
  EXPECT_EQ(MOJO_RESULT_OK, result);
  uint32_t num_bytes = 1;
  result = mojo::WriteDataRaw(producer_handle.get(), "a", &num_bytes,
                              MOJO_WRITE_DATA_FLAG_NONE);
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(1u, num_bytes);
  return mojo::ScopedHandle::From(std::move(consumer_handle));
}

}  // namespace

class StashServiceTest : public testing::Test {
 public:
  enum Event {
    EVENT_NONE,
    EVENT_STASH_RETRIEVED,
    EVENT_HANDLE_READY,
  };

  StashServiceTest() {}

  void SetUp() override {
    expecting_error_ = false;
    expected_event_ = EVENT_NONE;
    stash_backend_.reset(new StashBackend(base::Bind(
        &StashServiceTest::OnHandleReadyToRead, base::Unretained(this))));
    stash_backend_->BindToRequest(mojo::GetProxy(&stash_service_));
    stash_service_.set_connection_error_handler(base::Bind(&OnConnectionError));
    handles_ready_ = 0;
  }

  static void OnConnectionError() { FAIL() << "Unexpected connection error"; }

  mojo::Array<StashedObjectPtr> RetrieveStash() {
    mojo::Array<StashedObjectPtr> stash;
    stash_service_->RetrieveStash(base::Bind(
        &StashServiceTest::StashRetrieved, base::Unretained(this), &stash));
    WaitForEvent(EVENT_STASH_RETRIEVED);
    return stash;
  }

  void StashRetrieved(mojo::Array<StashedObjectPtr>* output,
                      mojo::Array<StashedObjectPtr> stash) {
    *output = std::move(stash);
    EventReceived(EVENT_STASH_RETRIEVED);
  }

  void WaitForEvent(Event event) {
    expected_event_ = event;
    base::RunLoop run_loop;
    stop_run_loop_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void EventReceived(Event event) {
    if (event == expected_event_ && !stop_run_loop_.is_null())
      stop_run_loop_.Run();
  }

  void OnHandleReadyToRead() {
    handles_ready_++;
    EventReceived(EVENT_HANDLE_READY);
  }

 protected:
  base::MessageLoop message_loop_;
  base::Closure stop_run_loop_;
  std::unique_ptr<StashBackend> stash_backend_;
  Event expected_event_;
  bool expecting_error_;
  mojo::InterfacePtr<StashService> stash_service_;
  int handles_ready_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StashServiceTest);
};

// Test that adding stashed objects in multiple calls can all be retrieved by a
// Retrieve call.
TEST_F(StashServiceTest, AddTwiceAndRetrieve) {
  mojo::Array<StashedObjectPtr> stashed_objects;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(1);
  stashed_object->stashed_handles = mojo::Array<mojo::ScopedHandle>();
  stashed_objects.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stashed_objects));
  stashed_object = StashedObject::New();
  stashed_object->id = "test type2";
  stashed_object->data.push_back(2);
  stashed_object->data.push_back(3);
  stashed_object->stashed_handles = mojo::Array<mojo::ScopedHandle>();
  stashed_objects.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stashed_objects));
  stashed_objects = RetrieveStash();
  ASSERT_EQ(2u, stashed_objects.size());
  EXPECT_EQ("test type", stashed_objects[0]->id);
  EXPECT_EQ(0u, stashed_objects[0]->stashed_handles.size());
  EXPECT_EQ(1u, stashed_objects[0]->data.size());
  EXPECT_EQ(1, stashed_objects[0]->data[0]);
  EXPECT_EQ("test type2", stashed_objects[1]->id);
  EXPECT_EQ(0u, stashed_objects[1]->stashed_handles.size());
  EXPECT_EQ(2u, stashed_objects[1]->data.size());
  EXPECT_EQ(2, stashed_objects[1]->data[0]);
  EXPECT_EQ(3, stashed_objects[1]->data[1]);
}

// Test that handles survive a round-trip through the stash.
TEST_F(StashServiceTest, StashAndRetrieveHandles) {
  mojo::Array<StashedObjectPtr> stashed_objects;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(1);

  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::ScopedDataPipeProducerHandle producer;
  MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1, 1,
  };
  mojo::CreateDataPipe(&options, &producer, &consumer);
  uint32_t num_bytes = 1;
  MojoResult result = mojo::WriteDataRaw(
      producer.get(), "1", &num_bytes, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(1u, num_bytes);

  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(std::move(producer)));
  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(std::move(consumer)));
  stashed_objects.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stashed_objects));
  stashed_objects = RetrieveStash();
  ASSERT_EQ(1u, stashed_objects.size());
  EXPECT_EQ("test type", stashed_objects[0]->id);
  ASSERT_EQ(2u, stashed_objects[0]->stashed_handles.size());

  consumer = mojo::ScopedDataPipeConsumerHandle::From(
      std::move(stashed_objects[0]->stashed_handles[1]));
  result = mojo::Wait(
      consumer.get(), MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE,
      nullptr);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  char data = '\0';
  result = mojo::ReadDataRaw(
      consumer.get(), &data, &num_bytes, MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(1u, num_bytes);
  EXPECT_EQ('1', data);
}

TEST_F(StashServiceTest, RetrieveWithoutStashing) {
  mojo::Array<StashedObjectPtr> stashed_objects = RetrieveStash();
  ASSERT_TRUE(!stashed_objects.is_null());
  EXPECT_EQ(0u, stashed_objects.size());
}

TEST_F(StashServiceTest, NotifyOnReadableHandle) {
  mojo::Array<StashedObjectPtr> stash_entries;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(0);
  stashed_object->monitor_handles = true;
  shell::mojom::InterfaceProviderPtr service_provider;

  // Stash the ServiceProvider request. When we make a call on
  // |service_provider|, the stashed handle will become readable.
  stashed_object->stashed_handles.push_back(mojo::ScopedHandle::From(
      mojo::GetProxy(&service_provider).PassMessagePipe()));

  stash_entries.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stash_entries));

  mojo::MessagePipe pipe;
  service_provider->GetInterface("", std::move(pipe.handle0));

  WaitForEvent(EVENT_HANDLE_READY);
  EXPECT_EQ(1, handles_ready_);
}

TEST_F(StashServiceTest, NotifyOnReadableDataPipeHandle) {
  mojo::Array<StashedObjectPtr> stash_entries;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->monitor_handles = true;

  MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1, 1,
  };
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  uint32_t num_bytes = 1;
  MojoResult result =
      mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  result = mojo::WriteDataRaw(producer_handle.get(), "a", &num_bytes,
                              MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(1u, num_bytes);
  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(std::move(producer_handle)));
  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(std::move(consumer_handle)));
  stashed_object->data.push_back(1);

  stash_entries.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stash_entries));
  WaitForEvent(EVENT_HANDLE_READY);
  EXPECT_EQ(1, handles_ready_);
}

TEST_F(StashServiceTest, NotifyOncePerStashOnReadableHandles) {
  mojo::Array<StashedObjectPtr> stash_entries;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(1);
  stashed_object->monitor_handles = true;
  stashed_object->stashed_handles.push_back(CreateReadableHandle());
  stashed_object->stashed_handles.push_back(CreateReadableHandle());
  stash_entries.push_back(std::move(stashed_object));
  stashed_object = StashedObject::New();
  stashed_object->id = "another test type";
  stashed_object->data.push_back(2);
  stashed_object->monitor_handles = true;
  stashed_object->stashed_handles.push_back(CreateReadableHandle());
  stashed_object->stashed_handles.push_back(CreateReadableHandle());
  stash_entries.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stash_entries));
  WaitForEvent(EVENT_HANDLE_READY);
  EXPECT_EQ(1, handles_ready_);

  stashed_object = StashedObject::New();
  stashed_object->id = "yet another test type";
  stashed_object->data.push_back(3);
  stashed_object->monitor_handles = true;
  stashed_object->stashed_handles.push_back(CreateReadableHandle());
  stashed_object->stashed_handles.push_back(CreateReadableHandle());
  stash_entries.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stash_entries));

  stash_service_->AddToStash(RetrieveStash());
  WaitForEvent(EVENT_HANDLE_READY);
  EXPECT_EQ(2, handles_ready_);
}

// Test that a stash service discards stashed objects when the backend no longer
// exists.
TEST_F(StashServiceTest, ServiceWithDeletedBackend) {
  stash_backend_.reset();
  stash_service_.set_connection_error_handler(base::Bind(&OnConnectionError));

  mojo::Array<StashedObjectPtr> stashed_objects;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(1);
  mojo::MessagePipe message_pipe;
  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(std::move(message_pipe.handle0)));
  stashed_objects.push_back(std::move(stashed_object));
  stash_service_->AddToStash(std::move(stashed_objects));
  stashed_objects = RetrieveStash();
  ASSERT_EQ(0u, stashed_objects.size());
  // Check that the stashed handle has been closed.
  MojoResult result =
      mojo::Wait(message_pipe.handle1.get(),
                 MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_DEADLINE_INDEFINITE, nullptr);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
}

}  // namespace extensions
