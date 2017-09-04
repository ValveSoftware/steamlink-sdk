// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/ReadableStreamBytesConsumer.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/dom/Document.h"
#include "core/streams/ReadableStreamOperations.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "platform/heap/Handle.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>
#include <v8.h>

namespace blink {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;
using Result = BytesConsumer::Result;
using PublicState = BytesConsumer::PublicState;

class MockClient : public GarbageCollectedFinalized<MockClient>,
                   public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(MockClient);

 public:
  static MockClient* create() { return new StrictMock<MockClient>(); }
  MOCK_METHOD0(onStateChange, void());

  DEFINE_INLINE_TRACE() {}

 protected:
  MockClient() = default;
};

class ReadableStreamBytesConsumerTest : public ::testing::Test {
 public:
  ReadableStreamBytesConsumerTest() : m_page(DummyPageHolder::create()) {}

  ScriptState* getScriptState() {
    return ScriptState::forMainWorld(m_page->document().frame());
  }
  v8::Isolate* isolate() { return getScriptState()->isolate(); }

  v8::MaybeLocal<v8::Value> eval(const char* s) {
    v8::Local<v8::String> source;
    v8::Local<v8::Script> script;
    v8::MicrotasksScope microtasks(isolate(),
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (!v8Call(
            v8::String::NewFromUtf8(isolate(), s, v8::NewStringType::kNormal),
            source)) {
      ADD_FAILURE();
      return v8::MaybeLocal<v8::Value>();
    }
    if (!v8Call(v8::Script::Compile(getScriptState()->context(), source),
                script)) {
      ADD_FAILURE() << "Compilation fails";
      return v8::MaybeLocal<v8::Value>();
    }
    return script->Run(getScriptState()->context());
  }
  v8::MaybeLocal<v8::Value> evalWithPrintingError(const char* s) {
    v8::TryCatch block(isolate());
    v8::MaybeLocal<v8::Value> r = eval(s);
    if (block.HasCaught()) {
      ADD_FAILURE()
          << toCoreString(block.Exception()->ToString(isolate())).utf8().data();
      block.ReThrow();
    }
    return r;
  }

  ReadableStreamBytesConsumer* createConsumer(ScriptValue stream) {
    NonThrowableExceptionState es;
    ScriptValue reader =
        ReadableStreamOperations::getReader(getScriptState(), stream, es);
    DCHECK(!reader.isEmpty());
    DCHECK(reader.v8Value()->IsObject());
    return new ReadableStreamBytesConsumer(getScriptState(), reader);
  }

  void gc() { V8GCController::collectAllGarbageForTesting(isolate()); }

 private:
  std::unique_ptr<DummyPageHolder> m_page;
};

TEST_F(ReadableStreamBytesConsumerTest, Create) {
  ScriptState::Scope scope(getScriptState());
  ScriptValue stream(getScriptState(),
                     evalWithPrintingError("new ReadableStream"));
  ASSERT_FALSE(stream.isEmpty());
  Persistent<BytesConsumer> consumer = createConsumer(stream);

  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
}

TEST_F(ReadableStreamBytesConsumerTest, EmptyStream) {
  ScriptState::Scope scope(getScriptState());
  ScriptValue stream(
      getScriptState(),
      evalWithPrintingError("new ReadableStream({start: c => c.close()})"));
  ASSERT_FALSE(stream.isEmpty());
  Persistent<BytesConsumer> consumer = createConsumer(stream);
  Persistent<MockClient> client = MockClient::create();
  consumer->setClient(client);

  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  const char* buffer = nullptr;
  size_t available = 0;
  checkpoint.Call(1);
  testing::runPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(3);
  testing::runPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
}

TEST_F(ReadableStreamBytesConsumerTest, ErroredStream) {
  ScriptState::Scope scope(getScriptState());
  ScriptValue stream(
      getScriptState(),
      evalWithPrintingError("new ReadableStream({start: c => c.error()})"));
  ASSERT_FALSE(stream.isEmpty());
  Persistent<BytesConsumer> consumer = createConsumer(stream);
  Persistent<MockClient> client = MockClient::create();
  consumer->setClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  const char* buffer = nullptr;
  size_t available = 0;
  checkpoint.Call(1);
  testing::runPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(3);
  testing::runPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
}

TEST_F(ReadableStreamBytesConsumerTest, TwoPhaseRead) {
  ScriptState::Scope scope(getScriptState());
  ScriptValue stream(
      getScriptState(),
      evalWithPrintingError(
          "var controller;"
          "var stream = new ReadableStream({start: c => controller = c});"
          "controller.enqueue(new Uint8Array());"
          "controller.enqueue(new Uint8Array([0x43, 0x44, 0x45, 0x46]));"
          "controller.enqueue(new Uint8Array([0x47, 0x48, 0x49, 0x4a]));"
          "controller.close();"
          "stream"));
  ASSERT_FALSE(stream.isEmpty());
  Persistent<BytesConsumer> consumer = createConsumer(stream);
  Persistent<MockClient> client = MockClient::create();
  consumer->setClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(4));
  EXPECT_CALL(checkpoint, Call(5));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(6));
  EXPECT_CALL(checkpoint, Call(7));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(8));
  EXPECT_CALL(checkpoint, Call(9));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(10));

  const char* buffer = nullptr;
  size_t available = 0;
  checkpoint.Call(1);
  testing::runPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(3);
  testing::runPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(0u, available);
  EXPECT_EQ(Result::Ok, consumer->endRead(0));
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(5);
  testing::runPendingTasks();
  checkpoint.Call(6);
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(4u, available);
  EXPECT_EQ(0x43, buffer[0]);
  EXPECT_EQ(0x44, buffer[1]);
  EXPECT_EQ(0x45, buffer[2]);
  EXPECT_EQ(0x46, buffer[3]);
  EXPECT_EQ(Result::Ok, consumer->endRead(0));
  EXPECT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(4u, available);
  EXPECT_EQ(0x43, buffer[0]);
  EXPECT_EQ(0x44, buffer[1]);
  EXPECT_EQ(0x45, buffer[2]);
  EXPECT_EQ(0x46, buffer[3]);
  EXPECT_EQ(Result::Ok, consumer->endRead(1));
  EXPECT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(3u, available);
  EXPECT_EQ(0x44, buffer[0]);
  EXPECT_EQ(0x45, buffer[1]);
  EXPECT_EQ(0x46, buffer[2]);
  EXPECT_EQ(Result::Ok, consumer->endRead(3));
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(7);
  testing::runPendingTasks();
  checkpoint.Call(8);
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
  ASSERT_EQ(4u, available);
  EXPECT_EQ(0x47, buffer[0]);
  EXPECT_EQ(0x48, buffer[1]);
  EXPECT_EQ(0x49, buffer[2]);
  EXPECT_EQ(0x4a, buffer[3]);
  EXPECT_EQ(Result::Ok, consumer->endRead(4));
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(9);
  testing::runPendingTasks();
  checkpoint.Call(10);
  EXPECT_EQ(PublicState::Closed, consumer->getPublicState());
  EXPECT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
}

TEST_F(ReadableStreamBytesConsumerTest, EnqueueUndefined) {
  ScriptState::Scope scope(getScriptState());
  ScriptValue stream(
      getScriptState(),
      evalWithPrintingError(
          "var controller;"
          "var stream = new ReadableStream({start: c => controller = c});"
          "controller.enqueue(undefined);"
          "controller.close();"
          "stream"));
  ASSERT_FALSE(stream.isEmpty());
  Persistent<BytesConsumer> consumer = createConsumer(stream);
  Persistent<MockClient> client = MockClient::create();
  consumer->setClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  const char* buffer = nullptr;
  size_t available = 0;
  checkpoint.Call(1);
  testing::runPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(3);
  testing::runPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
}

TEST_F(ReadableStreamBytesConsumerTest, EnqueueNull) {
  ScriptState::Scope scope(getScriptState());
  ScriptValue stream(
      getScriptState(),
      evalWithPrintingError(
          "var controller;"
          "var stream = new ReadableStream({start: c => controller = c});"
          "controller.enqueue(null);"
          "controller.close();"
          "stream"));

  ASSERT_FALSE(stream.isEmpty());
  Persistent<BytesConsumer> consumer = createConsumer(stream);
  Persistent<MockClient> client = MockClient::create();
  consumer->setClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  const char* buffer = nullptr;
  size_t available = 0;
  checkpoint.Call(1);
  testing::runPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(3);
  testing::runPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
}

TEST_F(ReadableStreamBytesConsumerTest, EnqueueString) {
  ScriptState::Scope scope(getScriptState());
  ScriptValue stream(
      getScriptState(),
      evalWithPrintingError(
          "var controller;"
          "var stream = new ReadableStream({start: c => controller = c});"
          "controller.enqueue('hello');"
          "controller.close();"
          "stream"));
  ASSERT_FALSE(stream.isEmpty());
  Persistent<BytesConsumer> consumer = createConsumer(stream);
  Persistent<MockClient> client = MockClient::create();
  consumer->setClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, onStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  const char* buffer = nullptr;
  size_t available = 0;
  checkpoint.Call(1);
  testing::runPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::ReadableOrWaiting, consumer->getPublicState());
  EXPECT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
  checkpoint.Call(3);
  testing::runPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::Errored, consumer->getPublicState());
  EXPECT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
}

}  // namespace

}  // namespace blink
