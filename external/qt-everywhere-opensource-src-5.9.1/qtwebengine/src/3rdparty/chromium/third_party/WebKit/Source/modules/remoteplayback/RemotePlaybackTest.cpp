// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlayback.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/modules/v8/RemotePlaybackAvailabilityCallback.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackState.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockFunction : public ScriptFunction {
 public:
  static ::testing::StrictMock<MockFunction>* create(ScriptState* scriptState) {
    return new ::testing::StrictMock<MockFunction>(scriptState);
  }

  v8::Local<v8::Function> bind() { return bindToV8Function(); }

  MOCK_METHOD1(call, ScriptValue(ScriptValue));

 protected:
  explicit MockFunction(ScriptState* scriptState)
      : ScriptFunction(scriptState) {}
};

class MockEventListener : public EventListener {
 public:
  MockEventListener() : EventListener(CPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext* executionContext, Event*));
};

class RemotePlaybackTest : public ::testing::Test {
 protected:
  void cancelPrompt(RemotePlayback* remotePlayback) {
    remotePlayback->promptCancelled();
  }

  void setState(RemotePlayback* remotePlayback, WebRemotePlaybackState state) {
    remotePlayback->stateChanged(state);
  }
};

TEST_F(RemotePlaybackTest, PromptCancelledRejectsWithNotAllowedError) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto resolve = MockFunction::create(scope.getScriptState());
  auto reject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*resolve, call(::testing::_)).Times(0);
  EXPECT_CALL(*reject, call(::testing::_)).Times(1);

  UserGestureIndicator indicator(DocumentUserGestureToken::create(
      &pageHolder->document(), UserGestureToken::NewGesture));
  remotePlayback->prompt(scope.getScriptState())
      .then(resolve->bind(), reject->bind());
  cancelPrompt(remotePlayback);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  ::testing::Mock::VerifyAndClear(resolve);
  ::testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, PromptConnectedRejectsWhenCancelled) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto resolve = MockFunction::create(scope.getScriptState());
  auto reject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*resolve, call(::testing::_)).Times(0);
  EXPECT_CALL(*reject, call(::testing::_)).Times(1);

  setState(remotePlayback, WebRemotePlaybackState::Connected);

  UserGestureIndicator indicator(DocumentUserGestureToken::create(
      &pageHolder->document(), UserGestureToken::NewGesture));
  remotePlayback->prompt(scope.getScriptState())
      .then(resolve->bind(), reject->bind());
  cancelPrompt(remotePlayback);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  ::testing::Mock::VerifyAndClear(resolve);
  ::testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, PromptConnectedResolvesWhenDisconnected) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto resolve = MockFunction::create(scope.getScriptState());
  auto reject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*resolve, call(::testing::_)).Times(1);
  EXPECT_CALL(*reject, call(::testing::_)).Times(0);

  setState(remotePlayback, WebRemotePlaybackState::Connected);

  UserGestureIndicator indicator(DocumentUserGestureToken::create(
      &pageHolder->document(), UserGestureToken::NewGesture));
  remotePlayback->prompt(scope.getScriptState())
      .then(resolve->bind(), reject->bind());

  setState(remotePlayback, WebRemotePlaybackState::Disconnected);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  ::testing::Mock::VerifyAndClear(resolve);
  ::testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, StateChangeEvents) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto connectingHandler = new ::testing::StrictMock<MockEventListener>();
  auto connectHandler = new ::testing::StrictMock<MockEventListener>();
  auto disconnectHandler = new ::testing::StrictMock<MockEventListener>();

  remotePlayback->addEventListener(EventTypeNames::connecting,
                                   connectingHandler);
  remotePlayback->addEventListener(EventTypeNames::connect, connectHandler);
  remotePlayback->addEventListener(EventTypeNames::disconnect,
                                   disconnectHandler);

  EXPECT_CALL(*connectingHandler, handleEvent(::testing::_, ::testing::_))
      .Times(1);
  EXPECT_CALL(*connectHandler, handleEvent(::testing::_, ::testing::_))
      .Times(1);
  EXPECT_CALL(*disconnectHandler, handleEvent(::testing::_, ::testing::_))
      .Times(1);

  setState(remotePlayback, WebRemotePlaybackState::Connecting);
  setState(remotePlayback, WebRemotePlaybackState::Connecting);
  setState(remotePlayback, WebRemotePlaybackState::Connected);
  setState(remotePlayback, WebRemotePlaybackState::Connected);
  setState(remotePlayback, WebRemotePlaybackState::Disconnected);
  setState(remotePlayback, WebRemotePlaybackState::Disconnected);

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  ::testing::Mock::VerifyAndClear(connectingHandler);
  ::testing::Mock::VerifyAndClear(connectHandler);
  ::testing::Mock::VerifyAndClear(disconnectHandler);
}

TEST_F(RemotePlaybackTest,
       DisableRemotePlaybackRejectsPromptWithInvalidStateError) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(*element);

  MockFunction* resolve = MockFunction::create(scope.getScriptState());
  MockFunction* reject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*resolve, call(::testing::_)).Times(0);
  EXPECT_CALL(*reject, call(::testing::_)).Times(1);

  UserGestureIndicator indicator(DocumentUserGestureToken::create(
      &pageHolder->document(), UserGestureToken::NewGesture));
  remotePlayback->prompt(scope.getScriptState())
      .then(resolve->bind(), reject->bind());
  HTMLMediaElementRemotePlayback::setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, *element, true);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  ::testing::Mock::VerifyAndClear(resolve);
  ::testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, DisableRemotePlaybackCancelsAvailabilityCallbacks) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(*element);

  MockFunction* callbackFunction = MockFunction::create(scope.getScriptState());
  RemotePlaybackAvailabilityCallback* availabilityCallback =
      RemotePlaybackAvailabilityCallback::create(scope.getScriptState(),
                                                 callbackFunction->bind());

  // The initial call upon registering will not happen as it's posted on the
  // message loop.
  EXPECT_CALL(*callbackFunction, call(::testing::_)).Times(0);

  MockFunction* resolve = MockFunction::create(scope.getScriptState());
  MockFunction* reject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*resolve, call(::testing::_)).Times(1);
  EXPECT_CALL(*reject, call(::testing::_)).Times(0);

  remotePlayback
      ->watchAvailability(scope.getScriptState(), availabilityCallback)
      .then(resolve->bind(), reject->bind());

  HTMLMediaElementRemotePlayback::setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, *element, true);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.isolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  ::testing::Mock::VerifyAndClear(resolve);
  ::testing::Mock::VerifyAndClear(reject);
  ::testing::Mock::VerifyAndClear(callbackFunction);
}

}  // namespace blink
