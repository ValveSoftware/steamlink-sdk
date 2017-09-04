// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/api_binding.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/api_request_handler.h"
#include "gin/converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

// Function spec; we use single quotes for readability and then replace them.
const char kFunctions[] =
    "[{"
    "  'name': 'oneString',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }]"
    "}, {"
    "  'name': 'stringAndInt',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }, {"
    "     'type': 'integer',"
    "     'name': 'int'"
    "   }]"
    "}, {"
    "  'name': 'stringOptionalIntAndBool',"
    "  'parameters': [{"
    "    'type': 'string',"
    "    'name': 'str'"
    "   }, {"
    "     'type': 'integer',"
    "     'name': 'optionalint',"
    "     'optional': true"
    "   }, {"
    "     'type': 'boolean',"
    "     'name': 'bool'"
    "   }]"
    "}, {"
    "  'name': 'oneObject',"
    "  'parameters': [{"
    "    'type': 'object',"
    "    'name': 'foo',"
    "    'properties': {"
    "      'prop1': {'type': 'string'},"
    "      'prop2': {'type': 'string', 'optional': true}"
    "    }"
    "  }]"
    "}, {"
    "  'name': 'noArgs',"
    "  'parameters': []"
    "}, {"
    "  'name': 'intAndCallback',"
    "  'parameters': [{"
    "    'name': 'int',"
    "    'type': 'integer'"
    "  }, {"
    "    'name': 'callback',"
    "    'type': 'function'"
    "  }]"
    "}, {"
    "  'name': 'optionalIntAndCallback',"
    "  'parameters': [{"
    "    'name': 'int',"
    "    'type': 'integer',"
    "    'optional': true"
    "  }, {"
    "    'name': 'callback',"
    "    'type': 'function'"
    "  }]"
    "}, {"
    "  'name': 'optionalCallback',"
    "  'parameters': [{"
    "    'name': 'callback',"
    "    'type': 'function',"
    "    'optional': true"
    "  }]"
    "}]";

const char kError[] = "Uncaught TypeError: Invalid invocation";

bool AllowAllAPIs(const std::string& name) {
  return true;
}

}  // namespace

class APIBindingUnittest : public APIBindingTest {
 public:
  void OnFunctionCall(const std::string& name,
                      std::unique_ptr<base::ListValue> arguments,
                      v8::Isolate* isolate,
                      v8::Local<v8::Context> context,
                      v8::Local<v8::Function> callback) {
    last_request_id_.clear();
    arguments_ = std::move(arguments);
    if (!callback.IsEmpty()) {
      last_request_id_ =
          request_handler_->AddPendingRequest(isolate, callback, context);
    }
  }

 protected:
  APIBindingUnittest() {}
  void SetUp() override {
    APIBindingTest::SetUp();
    request_handler_ = base::MakeUnique<APIRequestHandler>(
        base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  }

  void TearDown() override {
    request_handler_.reset();
    APIBindingTest::TearDown();
  }

  void ExpectPass(v8::Local<v8::Object> object,
                  const std::string& script_source,
                  const std::string& expected_json_arguments_single_quotes) {
    RunTest(object, script_source, true,
            ReplaceSingleQuotes(expected_json_arguments_single_quotes),
            std::string());
  }

  void ExpectFailure(v8::Local<v8::Object> object,
                     const std::string& script_source,
                     const std::string& expected_error) {
    RunTest(object, script_source, false, std::string(), expected_error);
  }

  const std::string& last_request_id() const { return last_request_id_; }
  APIRequestHandler* request_handler() { return request_handler_.get(); }

 private:
  void RunTest(v8::Local<v8::Object> object,
               const std::string& script_source,
               bool should_pass,
               const std::string& expected_json_arguments,
               const std::string& expected_error);

  std::unique_ptr<base::ListValue> arguments_;
  std::unique_ptr<APIRequestHandler> request_handler_;
  std::string last_request_id_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingUnittest);
};

void APIBindingUnittest::RunTest(v8::Local<v8::Object> object,
                                 const std::string& script_source,
                                 bool should_pass,
                                 const std::string& expected_json_arguments,
                                 const std::string& expected_error) {
  EXPECT_FALSE(arguments_);
  std::string wrapped_script_source =
      base::StringPrintf("(function(obj) { %s })", script_source.c_str());

  v8::Local<v8::Context> context = ContextLocal();
  v8::Local<v8::Function> func =
      FunctionFromString(context, wrapped_script_source);
  ASSERT_FALSE(func.IsEmpty());

  v8::Local<v8::Value> argv[] = {object};

  if (should_pass) {
    RunFunction(func, context, 1, argv);
    ASSERT_TRUE(arguments_) << script_source;
    EXPECT_EQ(expected_json_arguments, ValueToString(*arguments_));
  } else {
    RunFunctionAndExpectError(func, context, 1, argv, expected_error);
    EXPECT_FALSE(arguments_);
  }

  arguments_.reset();
}

TEST_F(APIBindingUnittest, Test) {
  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", *functions, nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      &refs);
  EXPECT_TRUE(refs.empty());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  ExpectPass(binding_object, "obj.oneString('foo');", "['foo']");
  ExpectPass(binding_object, "obj.oneString('');", "['']");
  ExpectFailure(binding_object, "obj.oneString(1);", kError);
  ExpectFailure(binding_object, "obj.oneString();", kError);
  ExpectFailure(binding_object, "obj.oneString({});", kError);
  ExpectFailure(binding_object, "obj.oneString('foo', 'bar');", kError);

  ExpectPass(binding_object, "obj.stringAndInt('foo', 42);", "['foo',42]");
  ExpectPass(binding_object, "obj.stringAndInt('foo', -1);", "['foo',-1]");
  ExpectFailure(binding_object, "obj.stringAndInt(1);", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt(1, 'foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', 'foo');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', '1');", kError);
  ExpectFailure(binding_object, "obj.stringAndInt('foo', 2.3);", kError);

  ExpectPass(binding_object, "obj.stringOptionalIntAndBool('foo', 42, true);",
             "['foo',42,true]");
  ExpectPass(binding_object, "obj.stringOptionalIntAndBool('foo', true);",
             "['foo',null,true]");
  ExpectFailure(binding_object,
                "obj.stringOptionalIntAndBool('foo', 'bar', true);", kError);

  ExpectPass(binding_object,
             "obj.oneObject({prop1: 'foo'});", "[{'prop1':'foo'}]");
  ExpectFailure(
      binding_object,
      "obj.oneObject({ get prop1() { throw new Error('Badness'); } });",
      "Uncaught Error: Badness");

  ExpectPass(binding_object, "obj.noArgs()", "[]");
  ExpectFailure(binding_object, "obj.noArgs(0)", kError);
  ExpectFailure(binding_object, "obj.noArgs('')", kError);
  ExpectFailure(binding_object, "obj.noArgs(null)", kError);
  ExpectFailure(binding_object, "obj.noArgs(undefined)", kError);

  ExpectPass(binding_object, "obj.intAndCallback(1, function() {})", "[1]");
  ExpectFailure(binding_object, "obj.intAndCallback(function() {})", kError);
  ExpectFailure(binding_object, "obj.intAndCallback(1)", kError);

  ExpectPass(binding_object, "obj.optionalIntAndCallback(1, function() {})",
             "[1]");
  ExpectPass(binding_object, "obj.optionalIntAndCallback(function() {})",
             "[null]");
  ExpectFailure(binding_object, "obj.optionalIntAndCallback(1)", kError);

  ExpectPass(binding_object, "obj.optionalCallback(function() {})", "[]");
  ExpectPass(binding_object, "obj.optionalCallback()", "[]");
  ExpectPass(binding_object, "obj.optionalCallback(undefined)", "[]");
  ExpectFailure(binding_object, "obj.optionalCallback(0)", kError);
}

TEST_F(APIBindingUnittest, TypeRefsTest) {
  const char kTypes[] =
      "[{"
      "  'id': 'refObj',"
      "  'type': 'object',"
      "  'properties': {"
      "    'prop1': {'type': 'string'},"
      "    'prop2': {'type': 'integer', 'optional': true}"
      "  }"
      "}, {"
      "  'id': 'refEnum',"
      "  'type': 'string',"
      "  'enum': ['alpha', 'beta']"
      "}]";
  const char kRefFunctions[] =
      "[{"
      "  'name': 'takesRefObj',"
      "  'parameters': [{"
      "    'name': 'o',"
      "    '$ref': 'refObj'"
      "  }]"
      "}, {"
      "  'name': 'takesRefEnum',"
      "  'parameters': [{"
      "    'name': 'e',"
      "    '$ref': 'refEnum'"
      "   }]"
      "}]";

  std::unique_ptr<base::ListValue> functions =
      ListValueFromString(kRefFunctions);
  ASSERT_TRUE(functions);
  std::unique_ptr<base::ListValue> types = ListValueFromString(kTypes);
  ASSERT_TRUE(types);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", *functions, types.get(), nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      &refs);
  EXPECT_EQ(2u, refs.size());
  EXPECT_TRUE(base::ContainsKey(refs, "refObj"));
  EXPECT_TRUE(base::ContainsKey(refs, "refEnum"));

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo'})",
             "[{'prop1':'foo'}]");
  ExpectPass(binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 2})",
             "[{'prop1':'foo','prop2':2}]");
  ExpectFailure(binding_object, "obj.takesRefObj({prop1: 'foo', prop2: 'a'})",
                kError);
  ExpectPass(binding_object, "obj.takesRefEnum('alpha')", "['alpha']");
  ExpectPass(binding_object, "obj.takesRefEnum('beta')", "['beta']");
  ExpectFailure(binding_object, "obj.takesRefEnum('gamma')", kError);
}

TEST_F(APIBindingUnittest, RestrictedAPIs) {
  const char kRestrictedFunctions[] =
      "[{"
      "  'name': 'allowedOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'allowedTwo',"
      "  'parameters': []"
      "}, {"
      "  'name': 'restrictedOne',"
      "  'parameters': []"
      "}, {"
      "  'name': 'restrictedTwo',"
      "  'parameters': []"
      "}]";
  std::unique_ptr<base::ListValue> functions =
      ListValueFromString(kRestrictedFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", *functions, nullptr, nullptr,
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      &refs);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  auto is_available = [](const std::string& name) {
    std::set<std::string> functions = {"test.allowedOne", "test.allowedTwo",
                                       "test.restrictedOne",
                                       "test.restrictedTwo"};
    EXPECT_TRUE(functions.count(name));
    return name == "test.allowedOne" || name == "test.allowedTwo";
  };

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(is_available));

  auto is_defined = [&binding_object, context](const std::string& name) {
    v8::Local<v8::Value> val =
        GetPropertyFromObject(binding_object, context, name);
    EXPECT_FALSE(val.IsEmpty());
    return !val->IsUndefined() && !val->IsNull();
  };

  EXPECT_TRUE(is_defined("allowedOne"));
  EXPECT_TRUE(is_defined("allowedTwo"));
  EXPECT_FALSE(is_defined("restrictedOne"));
  EXPECT_FALSE(is_defined("restrictedTwo"));
}

// Tests that events specified in the API are created as properties of the API
// object.
TEST_F(APIBindingUnittest, TestEventCreation) {
  const char kEvents[] = "[{'name': 'onFoo'}, {'name': 'onBar'}]";
  std::unique_ptr<base::ListValue> events = ListValueFromString(kEvents);
  ASSERT_TRUE(events);
  std::unique_ptr<base::ListValue> functions = ListValueFromString(kFunctions);
  ASSERT_TRUE(functions);
  ArgumentSpec::RefMap refs;
  APIBinding binding(
      "test", *functions, nullptr, events.get(),
      base::Bind(&APIBindingUnittest::OnFunctionCall, base::Unretained(this)),
      &refs);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIEventHandler event_handler(
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));
  v8::Local<v8::Object> binding_object = binding.CreateInstance(
      context, isolate(), &event_handler, base::Bind(&AllowAllAPIs));

  // Event behavior is tested in the APIEventHandler unittests as well as the
  // APIBindingsSystem tests, so we really only need to check that the events
  // are being initialized on the object.
  v8::Maybe<bool> has_on_foo =
      binding_object->Has(context, gin::StringToV8(isolate(), "onFoo"));
  EXPECT_TRUE(has_on_foo.IsJust());
  EXPECT_TRUE(has_on_foo.FromJust());

  v8::Maybe<bool> has_on_bar =
      binding_object->Has(context, gin::StringToV8(isolate(), "onBar"));
  EXPECT_TRUE(has_on_bar.IsJust());
  EXPECT_TRUE(has_on_bar.FromJust());

  v8::Maybe<bool> has_on_baz =
      binding_object->Has(context, gin::StringToV8(isolate(), "onBaz"));
  EXPECT_TRUE(has_on_baz.IsJust());
  EXPECT_FALSE(has_on_baz.FromJust());
}

}  // namespace extensions
