// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestUpdateEvent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/EventTypeNames.h"
#include "modules/payments/PaymentUpdater.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {
namespace {

class MockPaymentUpdater : public GarbageCollectedFinalized<MockPaymentUpdater>, public PaymentUpdater {
    USING_GARBAGE_COLLECTED_MIXIN(MockPaymentUpdater);
    WTF_MAKE_NONCOPYABLE(MockPaymentUpdater);

public:
    MockPaymentUpdater() {}
    ~MockPaymentUpdater() override {}

    MOCK_METHOD1(onUpdatePaymentDetails, void(const ScriptValue& detailsScriptValue));
    MOCK_METHOD1(onUpdatePaymentDetailsFailure, void(const ScriptValue& error));

    DEFINE_INLINE_TRACE() {}
};

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsCalled)
{
    V8TestingScope scope;
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create();
    MockPaymentUpdater* updater = new MockPaymentUpdater;
    event->setPaymentDetailsUpdater(updater);
    event->setEventPhase(Event::CAPTURING_PHASE);
    ScriptPromiseResolver* paymentDetails = ScriptPromiseResolver::create(scope.getScriptState());
    event->updateWith(scope.getScriptState(), paymentDetails->promise(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    EXPECT_CALL(*updater, onUpdatePaymentDetails(testing::_));
    EXPECT_CALL(*updater, onUpdatePaymentDetailsFailure(testing::_)).Times(0);

    paymentDetails->resolve("foo");
}

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsFailureCalled)
{
    V8TestingScope scope;
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
    MockPaymentUpdater* updater = new MockPaymentUpdater;
    event->setPaymentDetailsUpdater(updater);
    event->setEventPhase(Event::CAPTURING_PHASE);
    ScriptPromiseResolver* paymentDetails = ScriptPromiseResolver::create(scope.getScriptState());
    event->updateWith(scope.getScriptState(), paymentDetails->promise(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    EXPECT_CALL(*updater, onUpdatePaymentDetails(testing::_)).Times(0);
    EXPECT_CALL(*updater, onUpdatePaymentDetailsFailure(testing::_));

    paymentDetails->reject("oops");
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateWithoutDispatching)
{
    V8TestingScope scope;
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
    event->setPaymentDetailsUpdater(new MockPaymentUpdater);

    event->updateWith(scope.getScriptState(), ScriptPromiseResolver::create(scope.getScriptState())->promise(), scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateTwice)
{
    V8TestingScope scope;
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
    MockPaymentUpdater* updater = new MockPaymentUpdater;
    event->setPaymentDetailsUpdater(updater);
    event->setEventPhase(Event::CAPTURING_PHASE);
    event->updateWith(scope.getScriptState(), ScriptPromiseResolver::create(scope.getScriptState())->promise(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    event->updateWith(scope.getScriptState(), ScriptPromiseResolver::create(scope.getScriptState())->promise(), scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
}

TEST(PaymentRequestUpdateEventTest, UpdaterNotRequired)
{
    V8TestingScope scope;
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);

    event->updateWith(scope.getScriptState(), ScriptPromiseResolver::create(scope.getScriptState())->promise(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
}

} // namespace
} // namespace blink
