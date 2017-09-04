// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_event_handler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"

namespace extensions {

class APIEventHandlerTest : public APIBindingTest {
 protected:
  APIEventHandlerTest() {}
  ~APIEventHandlerTest() override {}

  void CallFunctionOnObject(v8::Local<v8::Context> context,
                            v8::Local<v8::Object> object,
                            const std::string& script_source) {
    std::string wrapped_script_source =
        base::StringPrintf("(function(obj) { %s })", script_source.c_str());
    v8::Local<v8::Function> func =
        FunctionFromString(context, wrapped_script_source);
    ASSERT_FALSE(func.IsEmpty());

    v8::Local<v8::Value> argv[] = {object};
    RunFunction(func, context, object, 1, argv);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(APIEventHandlerTest);
};

// Tests adding, removing, and querying event listeners by calling the
// associated methods on the JS object.
TEST_F(APIEventHandlerTest, AddingRemovingAndQueryingEventListeners) {
  const char kEventName[] = "alpha";
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler handler(base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> event =
      handler.CreateEventInstance(kEventName, context);
  ASSERT_FALSE(event.IsEmpty());

  EXPECT_EQ(0u, handler.GetNumEventListenersForTesting(kEventName, context));

  const char kListenerFunction[] = "(function() {})";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  const char kAddListenerFunction[] =
      "(function(event, listener) { event.addListener(listener); })";
  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListenerFunction);

  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, arraysize(argv), argv);
  }
  // There should only be one listener on the event.
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kEventName, context));

  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, arraysize(argv), argv);
  }
  // Trying to add the same listener again should be a no-op.
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kEventName, context));

  // Test hasListener returns true for a listener that is present.
  const char kHasListenerFunction[] =
      "(function(event, listener) { return event.hasListener(listener); })";
  v8::Local<v8::Function> has_listener_function =
      FunctionFromString(context, kHasListenerFunction);
  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    v8::Local<v8::Value> result =
        RunFunction(has_listener_function, context, arraysize(argv), argv);
    bool has_listener = false;
    EXPECT_TRUE(gin::Converter<bool>::FromV8(isolate(), result, &has_listener));
    EXPECT_TRUE(has_listener);
  }

  // Test that hasListener returns false for a listener that isn't present.
  {
    v8::Local<v8::Function> not_a_listener =
        FunctionFromString(context, "(function() {})");
    v8::Local<v8::Value> argv[] = {event, not_a_listener};
    v8::Local<v8::Value> result =
        RunFunction(has_listener_function, context, arraysize(argv), argv);
    bool has_listener = false;
    EXPECT_TRUE(gin::Converter<bool>::FromV8(isolate(), result, &has_listener));
    EXPECT_FALSE(has_listener);
  }

  // Test hasListeners returns true
  const char kHasListenersFunction[] =
      "(function(event) { return event.hasListeners(); })";
  v8::Local<v8::Function> has_listeners_function =
      FunctionFromString(context, kHasListenersFunction);
  {
    v8::Local<v8::Value> argv[] = {event};
    v8::Local<v8::Value> result =
        RunFunction(has_listeners_function, context, arraysize(argv), argv);
    bool has_listeners = false;
    EXPECT_TRUE(
        gin::Converter<bool>::FromV8(isolate(), result, &has_listeners));
    EXPECT_TRUE(has_listeners);
  }

  const char kRemoveListenerFunction[] =
      "(function(event, listener) { event.removeListener(listener); })";
  v8::Local<v8::Function> remove_listener_function =
      FunctionFromString(context, kRemoveListenerFunction);
  {
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(remove_listener_function, context, arraysize(argv), argv);
  }
  EXPECT_EQ(0u, handler.GetNumEventListenersForTesting(kEventName, context));

  {
    v8::Local<v8::Value> argv[] = {event};
    v8::Local<v8::Value> result =
        RunFunction(has_listeners_function, context, arraysize(argv), argv);
    bool has_listeners = false;
    EXPECT_TRUE(
        gin::Converter<bool>::FromV8(isolate(), result, &has_listeners));
    EXPECT_FALSE(has_listeners);
  }
}

// Tests listening for and firing different events.
TEST_F(APIEventHandlerTest, FiringEvents) {
  const char kAlphaName[] = "alpha";
  const char kBetaName[] = "beta";
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler handler(base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> alpha_event =
      handler.CreateEventInstance(kAlphaName, context);
  v8::Local<v8::Object> beta_event =
      handler.CreateEventInstance(kBetaName, context);
  ASSERT_FALSE(alpha_event.IsEmpty());
  ASSERT_FALSE(beta_event.IsEmpty());

  const char kAlphaListenerFunction1[] =
      "(function() {\n"
      "  if (!this.alphaCount1) this.alphaCount1 = 0;\n"
      "  ++this.alphaCount1;\n"
      "});\n";
  v8::Local<v8::Function> alpha_listener1 =
      FunctionFromString(context, kAlphaListenerFunction1);
  const char kAlphaListenerFunction2[] =
      "(function() {\n"
      "  if (!this.alphaCount2) this.alphaCount2 = 0;\n"
      "  ++this.alphaCount2;\n"
      "});\n";
  v8::Local<v8::Function> alpha_listener2 =
      FunctionFromString(context, kAlphaListenerFunction2);
  const char kBetaListenerFunction[] =
      "(function() {\n"
      "  if (!this.betaCount) this.betaCount = 0;\n"
      "  ++this.betaCount;\n"
      "});\n";
  v8::Local<v8::Function> beta_listener =
      FunctionFromString(context, kBetaListenerFunction);
  ASSERT_FALSE(alpha_listener1.IsEmpty());
  ASSERT_FALSE(alpha_listener2.IsEmpty());
  ASSERT_FALSE(beta_listener.IsEmpty());

  {
    const char kAddListenerFunction[] =
        "(function(event, listener) { event.addListener(listener); })";
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    {
      v8::Local<v8::Value> argv[] = {alpha_event, alpha_listener1};
      RunFunction(add_listener_function, context, arraysize(argv), argv);
    }
    {
      v8::Local<v8::Value> argv[] = {alpha_event, alpha_listener2};
      RunFunction(add_listener_function, context, arraysize(argv), argv);
    }
    {
      v8::Local<v8::Value> argv[] = {beta_event, beta_listener};
      RunFunction(add_listener_function, context, arraysize(argv), argv);
    }
  }

  EXPECT_EQ(2u, handler.GetNumEventListenersForTesting(kAlphaName, context));
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kBetaName, context));

  auto get_fired_count = [&context](const char* name) {
    v8::Local<v8::Value> res =
        GetPropertyFromObject(context->Global(), context, name);
    if (res->IsUndefined())
      return 0;
    int32_t count = 0;
    EXPECT_TRUE(
        gin::Converter<int32_t>::FromV8(context->GetIsolate(), res, &count))
        << name;
    return count;
  };

  EXPECT_EQ(0, get_fired_count("alphaCount1"));
  EXPECT_EQ(0, get_fired_count("alphaCount2"));
  EXPECT_EQ(0, get_fired_count("betaCount"));

  handler.FireEventInContext(kAlphaName, context, base::ListValue());
  EXPECT_EQ(2u, handler.GetNumEventListenersForTesting(kAlphaName, context));
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kBetaName, context));

  EXPECT_EQ(1, get_fired_count("alphaCount1"));
  EXPECT_EQ(1, get_fired_count("alphaCount2"));
  EXPECT_EQ(0, get_fired_count("betaCount"));

  handler.FireEventInContext(kAlphaName, context, base::ListValue());
  EXPECT_EQ(2, get_fired_count("alphaCount1"));
  EXPECT_EQ(2, get_fired_count("alphaCount2"));
  EXPECT_EQ(0, get_fired_count("betaCount"));

  handler.FireEventInContext(kBetaName, context, base::ListValue());
  EXPECT_EQ(2, get_fired_count("alphaCount1"));
  EXPECT_EQ(2, get_fired_count("alphaCount2"));
  EXPECT_EQ(1, get_fired_count("betaCount"));
}

// Tests firing events with arguments.
TEST_F(APIEventHandlerTest, EventArguments) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  const char kEventName[] = "alpha";
  APIEventHandler handler(base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> event =
      handler.CreateEventInstance(kEventName, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kListenerFunction[] =
      "(function() { this.eventArgs = Array.from(arguments); })";
  v8::Local<v8::Function> listener_function =
      FunctionFromString(context, kListenerFunction);
  ASSERT_FALSE(listener_function.IsEmpty());

  {
    const char kAddListenerFunction[] =
        "(function(event, listener) { event.addListener(listener); })";
    v8::Local<v8::Function> add_listener_function =
        FunctionFromString(context, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event, listener_function};
    RunFunction(add_listener_function, context, arraysize(argv), argv);
  }

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> event_args = ListValueFromString(kArguments);
  ASSERT_TRUE(event_args);
  handler.FireEventInContext(kEventName, context, *event_args);

  std::unique_ptr<base::Value> result =
      GetBaseValuePropertyFromObject(context->Global(), context, "eventArgs");
  ASSERT_TRUE(result);
  EXPECT_EQ(ReplaceSingleQuotes(kArguments), ValueToString(*result));
}

// Test dispatching events to multiple contexts.
TEST_F(APIEventHandlerTest, MultipleContexts) {
  v8::HandleScope handle_scope(isolate());

  v8::Local<v8::Context> context_a = ContextLocal();
  v8::Local<v8::Context> context_b = v8::Context::New(isolate());
  gin::ContextHolder holder_b(isolate());
  holder_b.SetContext(context_b);

  const char kEventName[] = "onFoo";

  APIEventHandler handler(base::Bind(&RunFunctionOnGlobalAndIgnoreResult));

  v8::Local<v8::Function> listener_a = FunctionFromString(
      context_a, "(function(arg) { this.eventArgs = arg + 'alpha'; })");
  ASSERT_FALSE(listener_a.IsEmpty());
  v8::Local<v8::Function> listener_b = FunctionFromString(
      context_b, "(function(arg) { this.eventArgs = arg + 'beta'; })");
  ASSERT_FALSE(listener_b.IsEmpty());

  // Create two instances of the same event in different contexts.
  v8::Local<v8::Object> event_a =
      handler.CreateEventInstance(kEventName, context_a);
  ASSERT_FALSE(event_a.IsEmpty());
  v8::Local<v8::Object> event_b =
      handler.CreateEventInstance(kEventName, context_b);
  ASSERT_FALSE(event_b.IsEmpty());

  // Add two separate listeners to the event, one in each context.
  const char kAddListenerFunction[] =
      "(function(event, listener) { event.addListener(listener); })";
  {
    v8::Local<v8::Function> add_listener_a =
        FunctionFromString(context_a, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event_a, listener_a};
    RunFunction(add_listener_a, context_a, arraysize(argv), argv);
  }
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kEventName, context_a));
  EXPECT_EQ(0u, handler.GetNumEventListenersForTesting(kEventName, context_b));

  {
    v8::Local<v8::Function> add_listener_b =
        FunctionFromString(context_b, kAddListenerFunction);
    v8::Local<v8::Value> argv[] = {event_b, listener_b};
    RunFunction(add_listener_b, context_b, arraysize(argv), argv);
  }
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kEventName, context_a));
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kEventName, context_b));

  // Dispatch the event in context_a - the listener in context_b should not be
  // notified.
  std::unique_ptr<base::ListValue> arguments_a =
      ListValueFromString("['result_a:']");
  ASSERT_TRUE(arguments_a);

  handler.FireEventInContext(kEventName, context_a, *arguments_a);
  {
    std::unique_ptr<base::Value> result_a = GetBaseValuePropertyFromObject(
        context_a->Global(), context_a, "eventArgs");
    ASSERT_TRUE(result_a);
    EXPECT_EQ("\"result_a:alpha\"", ValueToString(*result_a));
  }
  {
    v8::Local<v8::Value> result_b =
        GetPropertyFromObject(context_b->Global(), context_b, "eventArgs");
    ASSERT_FALSE(result_b.IsEmpty());
    EXPECT_TRUE(result_b->IsUndefined());
  }

  // Dispatch the event in context_b - the listener in context_a should not be
  // notified.
  std::unique_ptr<base::ListValue> arguments_b =
      ListValueFromString("['result_b:']");
  ASSERT_TRUE(arguments_b);
  handler.FireEventInContext(kEventName, context_b, *arguments_b);
  {
    std::unique_ptr<base::Value> result_a = GetBaseValuePropertyFromObject(
        context_a->Global(), context_a, "eventArgs");
    ASSERT_TRUE(result_a);
    EXPECT_EQ("\"result_a:alpha\"", ValueToString(*result_a));
  }
  {
    std::unique_ptr<base::Value> result_b = GetBaseValuePropertyFromObject(
        context_b->Global(), context_b, "eventArgs");
    ASSERT_TRUE(result_b);
    EXPECT_EQ("\"result_b:beta\"", ValueToString(*result_b));
  }
}

TEST_F(APIEventHandlerTest, DifferentCallingMethods) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  const char kEventName[] = "alpha";
  APIEventHandler handler(base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> event =
      handler.CreateEventInstance(kEventName, context);
  ASSERT_FALSE(event.IsEmpty());

  const char kAddListenerOnNull[] =
      "(function(event) {\n"
      "  event.addListener.call(null, function() {});\n"
      "})";
  {
    v8::Local<v8::Value> args[] = {event};
    // TODO(devlin): This is the generic type error that gin throws. It's not
    // very descriptive, nor does it match the web (which would just say e.g.
    // "Illegal invocation"). Might be worth updating later.
    RunFunctionAndExpectError(
        FunctionFromString(context, kAddListenerOnNull),
        context, 1, args,
        "Uncaught TypeError: Error processing argument at index -1,"
        " conversion failure from undefined");
  }
  EXPECT_EQ(0u, handler.GetNumEventListenersForTesting(kEventName, context));

  const char kAddListenerOnEvent[] =
      "(function(event) {\n"
      "  event.addListener.call(event, function() {});\n"
      "})";
  {
    v8::Local<v8::Value> args[] = {event};
    RunFunction(FunctionFromString(context, kAddListenerOnEvent),
                context, 1, args);
  }
  EXPECT_EQ(1u, handler.GetNumEventListenersForTesting(kEventName, context));

  // Call addListener with a function that captures the event, creating a cycle.
  // If we don't properly clean up, the context will leak.
  const char kAddListenerOnEventWithCapture[] =
      "(function(event) {\n"
      "  event.addListener(function listener() {\n"
      "    event.hasListener(listener);\n"
      "  });\n"
      "})";
  {
    v8::Local<v8::Value> args[] = {event};
    RunFunction(FunctionFromString(context, kAddListenerOnEventWithCapture),
                context, 1, args);
  }
  EXPECT_EQ(2u, handler.GetNumEventListenersForTesting(kEventName, context));
}

}  // namespace extensions
