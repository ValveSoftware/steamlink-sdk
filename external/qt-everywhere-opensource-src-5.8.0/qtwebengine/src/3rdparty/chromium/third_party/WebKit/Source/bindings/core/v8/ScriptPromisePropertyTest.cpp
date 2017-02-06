// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptPromiseProperty.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/GCObservation.h"
#include "core/testing/GarbageCollectedScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>
#include <v8.h>

using namespace blink;

namespace {

class NotReached : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState)
    {
        NotReached* self = new NotReached(scriptState);
        return self->bindToV8Function();
    }

private:
    explicit NotReached(ScriptState* scriptState)
        : ScriptFunction(scriptState)
    {
    }

    ScriptValue call(ScriptValue) override;
};

ScriptValue NotReached::call(ScriptValue)
{
    EXPECT_TRUE(false) << "'Unreachable' code was reached";
    return ScriptValue();
}

class StubFunction : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, ScriptValue& value, size_t& callCount)
    {
        StubFunction* self = new StubFunction(scriptState, value, callCount);
        return self->bindToV8Function();
    }

private:
    StubFunction(ScriptState* scriptState, ScriptValue& value, size_t& callCount)
        : ScriptFunction(scriptState)
        , m_value(value)
        , m_callCount(callCount)
    {
    }

    ScriptValue call(ScriptValue arg) override
    {
        m_value = arg;
        m_callCount++;
        return ScriptValue();
    }

    ScriptValue& m_value;
    size_t& m_callCount;
};

class GarbageCollectedHolder : public GarbageCollectedScriptWrappable {
public:
    typedef ScriptPromiseProperty<Member<GarbageCollectedScriptWrappable>, Member<GarbageCollectedScriptWrappable>, Member<GarbageCollectedScriptWrappable>> Property;
    GarbageCollectedHolder(ExecutionContext* executionContext)
        : GarbageCollectedScriptWrappable("holder")
        , m_property(new Property(executionContext, toGarbageCollectedScriptWrappable(), Property::Ready)) { }

    Property* getProperty() { return m_property; }
    GarbageCollectedScriptWrappable* toGarbageCollectedScriptWrappable() { return this; }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        GarbageCollectedScriptWrappable::trace(visitor);
        visitor->trace(m_property);
    }

private:
    Member<Property> m_property;
};

class ScriptPromisePropertyTestBase {
public:
    ScriptPromisePropertyTestBase()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
    {
        v8::HandleScope handleScope(isolate());
        m_otherScriptState = ScriptStateForTesting::create(v8::Context::New(isolate()), DOMWrapperWorld::ensureIsolatedWorld(isolate(), 1, -1));
    }

    virtual ~ScriptPromisePropertyTestBase()
    {
        destroyContext();
    }

    Document& document() { return m_page->document(); }
    v8::Isolate* isolate() { return toIsolate(&document()); }
    ScriptState* mainScriptState() { return ScriptState::forMainWorld(document().frame()); }
    DOMWrapperWorld& mainWorld() { return mainScriptState()->world(); }
    ScriptState* otherScriptState() { return m_otherScriptState.get(); }
    DOMWrapperWorld& otherWorld() { return m_otherScriptState->world(); }
    ScriptState* currentScriptState() { return ScriptState::current(isolate()); }

    void destroyContext()
    {
        m_page.reset();
        if (m_otherScriptState) {
            m_otherScriptState->disposePerContextData();
            m_otherScriptState = nullptr;
        }
    }

    void gc() { V8GCController::collectAllGarbageForTesting(v8::Isolate::GetCurrent()); }

    v8::Local<v8::Function> notReached(ScriptState* scriptState) { return NotReached::createFunction(scriptState); }
    v8::Local<v8::Function> stub(ScriptState* scriptState, ScriptValue& value, size_t& callCount) { return StubFunction::createFunction(scriptState, value, callCount); }

    template <typename T>
    ScriptValue wrap(DOMWrapperWorld& world, const T& value)
    {
        v8::HandleScope handleScope(isolate());
        ScriptState* scriptState = ScriptState::from(toV8Context(&document(), world));
        ScriptState::Scope scope(scriptState);
        return ScriptValue(scriptState, toV8(value, scriptState->context()->Global(), isolate()));
    }

private:
    std::unique_ptr<DummyPageHolder> m_page;
    RefPtr<ScriptState> m_otherScriptState;
};

// This is the main test class.
// If you want to examine a testcase independent of holder types, place the
// test on this class.
class ScriptPromisePropertyGarbageCollectedTest : public ScriptPromisePropertyTestBase, public ::testing::Test {
public:
    typedef GarbageCollectedHolder::Property Property;

    ScriptPromisePropertyGarbageCollectedTest()
        : m_holder(new GarbageCollectedHolder(&document()))
    {
    }

    GarbageCollectedHolder* holder() { return m_holder; }
    Property* getProperty() { return m_holder->getProperty(); }
    ScriptPromise promise(DOMWrapperWorld& world) { return getProperty()->promise(world); }

private:
    Persistent<GarbageCollectedHolder> m_holder;
};

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_IsStableObjectInMainWorld)
{
    ScriptPromise v = getProperty()->promise(DOMWrapperWorld::mainWorld());
    ScriptPromise w = getProperty()->promise(DOMWrapperWorld::mainWorld());
    EXPECT_EQ(v, w);
    ASSERT_FALSE(v.isEmpty());
    {
        ScriptState::Scope scope(mainScriptState());
        EXPECT_EQ(v.v8Value().As<v8::Object>()->CreationContext(), toV8Context(&document(), mainWorld()));
    }
    EXPECT_EQ(Property::Pending, getProperty()->getState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_IsStableObjectInVariousWorlds)
{
    ScriptPromise u = getProperty()->promise(otherWorld());
    ScriptPromise v = getProperty()->promise(DOMWrapperWorld::mainWorld());
    ScriptPromise w = getProperty()->promise(DOMWrapperWorld::mainWorld());
    EXPECT_NE(mainScriptState(), otherScriptState());
    EXPECT_NE(&mainWorld(), &otherWorld());
    EXPECT_NE(u, v);
    EXPECT_EQ(v, w);
    ASSERT_FALSE(u.isEmpty());
    ASSERT_FALSE(v.isEmpty());
    {
        ScriptState::Scope scope(otherScriptState());
        EXPECT_EQ(u.v8Value().As<v8::Object>()->CreationContext(), toV8Context(&document(), otherWorld()));
    }
    {
        ScriptState::Scope scope(mainScriptState());
        EXPECT_EQ(v.v8Value().As<v8::Object>()->CreationContext(), toV8Context(&document(), mainWorld()));
    }
    EXPECT_EQ(Property::Pending, getProperty()->getState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_IsStableObjectAfterSettling)
{
    ScriptPromise v = promise(DOMWrapperWorld::mainWorld());
    GarbageCollectedScriptWrappable* value = new GarbageCollectedScriptWrappable("value");

    getProperty()->resolve(value);
    EXPECT_EQ(Property::Resolved, getProperty()->getState());

    ScriptPromise w = promise(DOMWrapperWorld::mainWorld());
    EXPECT_EQ(v, w);
    EXPECT_FALSE(v.isEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_DoesNotImpedeGarbageCollection)
{
    ScriptValue holderWrapper = wrap(mainWorld(), holder()->toGarbageCollectedScriptWrappable());

    Persistent<GCObservation> observation;
    {
        ScriptState::Scope scope(mainScriptState());
        observation = GCObservation::create(promise(DOMWrapperWorld::mainWorld()).v8Value());
    }

    gc();
    EXPECT_FALSE(observation->wasCollected());

    holderWrapper.clear();
    gc();
    EXPECT_TRUE(observation->wasCollected());

    EXPECT_EQ(Property::Pending, getProperty()->getState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Resolve_ResolvesScriptPromise)
{
    ScriptPromise promise = getProperty()->promise(DOMWrapperWorld::mainWorld());
    ScriptPromise otherPromise = getProperty()->promise(otherWorld());
    ScriptValue actual, otherActual;
    size_t nResolveCalls = 0;
    size_t nOtherResolveCalls = 0;

    {
        ScriptState::Scope scope(mainScriptState());
        promise.then(stub(currentScriptState(), actual, nResolveCalls), notReached(currentScriptState()));
    }

    {
        ScriptState::Scope scope(otherScriptState());
        otherPromise.then(stub(currentScriptState(), otherActual, nOtherResolveCalls), notReached(currentScriptState()));
    }

    EXPECT_NE(promise, otherPromise);

    GarbageCollectedScriptWrappable* value = new GarbageCollectedScriptWrappable("value");
    getProperty()->resolve(value);
    EXPECT_EQ(Property::Resolved, getProperty()->getState());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ(1u, nResolveCalls);
    EXPECT_EQ(1u, nOtherResolveCalls);
    EXPECT_EQ(wrap(mainWorld(), value), actual);
    EXPECT_NE(actual, otherActual);
    EXPECT_EQ(wrap(otherWorld(), value), otherActual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, ResolveAndGetPromiseOnOtherWorld)
{
    ScriptPromise promise = getProperty()->promise(DOMWrapperWorld::mainWorld());
    ScriptPromise otherPromise = getProperty()->promise(otherWorld());
    ScriptValue actual, otherActual;
    size_t nResolveCalls = 0;
    size_t nOtherResolveCalls = 0;

    {
        ScriptState::Scope scope(mainScriptState());
        promise.then(stub(currentScriptState(), actual, nResolveCalls), notReached(currentScriptState()));
    }

    EXPECT_NE(promise, otherPromise);
    GarbageCollectedScriptWrappable* value = new GarbageCollectedScriptWrappable("value");
    getProperty()->resolve(value);
    EXPECT_EQ(Property::Resolved, getProperty()->getState());

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ(1u, nResolveCalls);
    EXPECT_EQ(0u, nOtherResolveCalls);

    {
        ScriptState::Scope scope(otherScriptState());
        otherPromise.then(stub(currentScriptState(), otherActual, nOtherResolveCalls), notReached(currentScriptState()));
    }

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ(1u, nResolveCalls);
    EXPECT_EQ(1u, nOtherResolveCalls);
    EXPECT_EQ(wrap(mainWorld(), value), actual);
    EXPECT_NE(actual, otherActual);
    EXPECT_EQ(wrap(otherWorld(), value), otherActual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Reject_RejectsScriptPromise)
{
    GarbageCollectedScriptWrappable* reason = new GarbageCollectedScriptWrappable("reason");
    getProperty()->reject(reason);
    EXPECT_EQ(Property::Rejected, getProperty()->getState());

    ScriptValue actual, otherActual;
    size_t nRejectCalls = 0;
    size_t nOtherRejectCalls = 0;
    {
        ScriptState::Scope scope(mainScriptState());
        getProperty()->promise(DOMWrapperWorld::mainWorld()).then(notReached(currentScriptState()), stub(currentScriptState(), actual, nRejectCalls));
    }

    {
        ScriptState::Scope scope(otherScriptState());
        getProperty()->promise(otherWorld()).then(notReached(currentScriptState()), stub(currentScriptState(), otherActual, nOtherRejectCalls));
    }

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ(1u, nRejectCalls);
    EXPECT_EQ(wrap(mainWorld(), reason), actual);
    EXPECT_EQ(1u, nOtherRejectCalls);
    EXPECT_NE(actual, otherActual);
    EXPECT_EQ(wrap(otherWorld(), reason), otherActual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_DeadContext)
{
    getProperty()->resolve(new GarbageCollectedScriptWrappable("value"));
    EXPECT_EQ(Property::Resolved, getProperty()->getState());

    destroyContext();

    EXPECT_TRUE(getProperty()->promise(DOMWrapperWorld::mainWorld()).isEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Resolve_DeadContext)
{
    {
        ScriptState::Scope scope(mainScriptState());
        getProperty()->promise(DOMWrapperWorld::mainWorld()).then(notReached(currentScriptState()), notReached(currentScriptState()));
    }

    destroyContext();
    EXPECT_TRUE(!getProperty()->getExecutionContext() || getProperty()->getExecutionContext()->activeDOMObjectsAreStopped());

    getProperty()->resolve(new GarbageCollectedScriptWrappable("value"));
    EXPECT_EQ(Property::Pending, getProperty()->getState());

    v8::MicrotasksScope::PerformCheckpoint(v8::Isolate::GetCurrent());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Reset)
{
    ScriptPromise oldPromise, newPromise;
    ScriptValue oldActual, newActual;
    GarbageCollectedScriptWrappable* oldValue = new GarbageCollectedScriptWrappable("old");
    GarbageCollectedScriptWrappable* newValue = new GarbageCollectedScriptWrappable("new");
    size_t nOldResolveCalls = 0;
    size_t nNewRejectCalls = 0;

    {
        ScriptState::Scope scope(mainScriptState());
        getProperty()->resolve(oldValue);
        oldPromise = getProperty()->promise(mainWorld());
        oldPromise.then(stub(currentScriptState(), oldActual, nOldResolveCalls), notReached(currentScriptState()));
    }

    getProperty()->reset();

    {
        ScriptState::Scope scope(mainScriptState());
        newPromise = getProperty()->promise(mainWorld());
        newPromise.then(notReached(currentScriptState()), stub(currentScriptState(), newActual, nNewRejectCalls));
        getProperty()->reject(newValue);
    }

    EXPECT_EQ(0u, nOldResolveCalls);
    EXPECT_EQ(0u, nNewRejectCalls);

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_EQ(1u, nOldResolveCalls);
    EXPECT_EQ(1u, nNewRejectCalls);
    EXPECT_NE(oldPromise, newPromise);
    EXPECT_EQ(wrap(mainWorld(), oldValue), oldActual);
    EXPECT_EQ(wrap(mainWorld(), newValue), newActual);
    EXPECT_NE(oldActual, newActual);
}

// Tests that ScriptPromiseProperty works with a non ScriptWrappable resolution
// target.
class ScriptPromisePropertyNonScriptWrappableResolutionTargetTest : public ScriptPromisePropertyTestBase, public ::testing::Test {
public:
    template <typename T>
    void test(const T& value, const char* expected, const char* file, size_t line)
    {
        typedef ScriptPromiseProperty<Member<GarbageCollectedScriptWrappable>, T, ToV8UndefinedGenerator> Property;
        Property* property = new Property(&document(), new GarbageCollectedScriptWrappable("holder"), Property::Ready);
        size_t nResolveCalls = 0;
        ScriptValue actualValue;
        String actual;
        {
            ScriptState::Scope scope(mainScriptState());
            property->promise(DOMWrapperWorld::mainWorld()).then(stub(currentScriptState(), actualValue, nResolveCalls), notReached(currentScriptState()));
        }
        property->resolve(value);
        v8::MicrotasksScope::PerformCheckpoint(isolate());
        {
            ScriptState::Scope scope(mainScriptState());
            actual = toCoreString(actualValue.v8Value()->ToString(mainScriptState()->context()).ToLocalChecked());
        }
        if (expected != actual) {
            ADD_FAILURE_AT(file, line) << "toV8 returns an incorrect value.\n  Actual: " << actual.utf8().data() << "\nExpected: " << expected;
            return;
        }
    }
};

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest, ResolveWithUndefined)
{
    test(ToV8UndefinedGenerator(), "undefined", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest, ResolveWithString)
{
    test(String("hello"), "hello", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest, ResolveWithInteger)
{
    test(-1, "-1", __FILE__, __LINE__);
}

} // namespace
