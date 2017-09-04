// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_bindings_system.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/api_binding.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "gin/converter.h"
#include "gin/try_catch.h"

namespace extensions {

namespace {

// Fake API for testing.
const char kAlphaAPIName[] = "alpha";
const char kAlphaAPISpec[] =
    "{"
    "  'types': [{"
    "    'id': 'objRef',"
    "    'type': 'object',"
    "    'properties': {"
    "      'prop1': {'type': 'string'},"
    "      'prop2': {'type': 'integer', 'optional': true}"
    "    }"
    "  }],"
    "  'functions': [{"
    "    'name': 'functionWithCallback',"
    "    'parameters': [{"
    "      'name': 'str',"
    "      'type': 'string'"
    "    }, {"
    "      'name': 'callback',"
    "      'type': 'function'"
    "    }]"
    "  }, {"
    "    'name': 'functionWithRefAndCallback',"
    "    'parameters': [{"
    "      'name': 'ref',"
    "      '$ref': 'objRef'"
    "    }, {"
    "      'name': 'callback',"
    "      'type': 'function'"
    "    }]"
    "  }],"
    "  'events': [{"
    "    'name': 'alphaEvent'"
    "  }]"
    "}";

// Another fake API for testing.
const char kBetaAPIName[] = "beta";
const char kBetaAPISpec[] =
    "{"
    "  'functions': [{"
    "    'name': 'simpleFunc',"
    "    'parameters': [{'name': 'int', 'type': 'integer'}]"
    "  }]"
    "}";

bool AllowAllAPIs(const std::string& name) {
  return true;
}

}  // namespace

// The base class to test the APIBindingsSystem. This allows subclasses to
// retrieve API schemas differently.
class APIBindingsSystemTestBase : public APIBindingTest {
 public:
  // Returns the DictionaryValue representing the schema with the given API
  // name.
  virtual const base::DictionaryValue& GetAPISchema(
      const std::string& api_name) = 0;

  // Stores the request in |last_request_|.
  void OnAPIRequest(std::unique_ptr<APIBindingsSystem::Request> request) {
    ASSERT_FALSE(last_request_);
    last_request_ = std::move(request);
  }

 protected:
  APIBindingsSystemTestBase() {}
  void SetUp() override {
    APIBindingTest::SetUp();
    bindings_system_ = base::MakeUnique<APIBindingsSystem>(
        base::Bind(&RunFunctionOnGlobalAndIgnoreResult),
        base::Bind(&APIBindingsSystemTestBase::GetAPISchema,
                   base::Unretained(this)),
        base::Bind(&APIBindingsSystemTestBase::OnAPIRequest,
                   base::Unretained(this)));
  }

  void TearDown() override {
    bindings_system_.reset();
    APIBindingTest::TearDown();
  }

  // Checks that |last_request_| exists and was provided with the
  // |expected_name| and |expected_arguments|.
  void ValidateLastRequest(const std::string& expected_name,
                           const std::string& expected_arguments);

  const APIBindingsSystem::Request* last_request() const {
    return last_request_.get();
  }
  void reset_last_request() { last_request_.reset(); }
  APIBindingsSystem* bindings_system() { return bindings_system_.get(); }

 private:
  // The APIBindingsSystem associated with the test. Safe to use across multiple
  // contexts.
  std::unique_ptr<APIBindingsSystem> bindings_system_;

  // The last request to be received from the APIBindingsSystem, or null if
  // there is none.
  std::unique_ptr<APIBindingsSystem::Request> last_request_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingsSystemTestBase);
};

void APIBindingsSystemTestBase::ValidateLastRequest(
    const std::string& expected_name,
    const std::string& expected_arguments) {
  ASSERT_TRUE(last_request());
  // Note that even if no arguments are provided by the API call, we should have
  // an empty list.
  ASSERT_TRUE(last_request()->arguments);
  EXPECT_EQ(expected_name, last_request()->method_name);
  EXPECT_EQ(ReplaceSingleQuotes(expected_arguments),
            ValueToString(*last_request()->arguments));
}

// An implementation that works with fake/supplied APIs, for easy testability.
class APIBindingsSystemTest : public APIBindingsSystemTestBase {
 protected:
  APIBindingsSystemTest() {}

  // Calls a function constructed from |script_source| on the given |object|.
  void CallFunctionOnObject(v8::Local<v8::Context> context,
                            v8::Local<v8::Object> object,
                            const std::string& script_source);

 private:
  const base::DictionaryValue& GetAPISchema(
      const std::string& api_name) override {
    EXPECT_TRUE(base::ContainsKey(api_schemas_, api_name));
    return *api_schemas_[api_name];
  }
  void SetUp() override;

  // The API schemas for the fake APIs.
  std::map<std::string, std::unique_ptr<base::DictionaryValue>> api_schemas_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingsSystemTest);
};

void APIBindingsSystemTest::CallFunctionOnObject(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const std::string& script_source) {
  std::string wrapped_script_source =
      base::StringPrintf("(function(obj) { %s })", script_source.c_str());

  v8::Local<v8::Function> func =
      FunctionFromString(context, wrapped_script_source);
  ASSERT_FALSE(func.IsEmpty());

  v8::Local<v8::Value> argv[] = {object};
  RunFunction(func, context, 1, argv);
}

void APIBindingsSystemTest::SetUp() {
  // Create the fake API schemas.
  {
    struct APIData {
      const char* name;
      const char* spec;
    } api_data[] = {
        {kAlphaAPIName, kAlphaAPISpec}, {kBetaAPIName, kBetaAPISpec},
    };
    for (const auto& api : api_data) {
      std::unique_ptr<base::DictionaryValue> api_schema =
          DictionaryValueFromString(api.spec);
      ASSERT_TRUE(api_schema);
      api_schemas_[api.name] = std::move(api_schema);
    }
  }

  APIBindingsSystemTestBase::SetUp();
}

// Tests API object initialization, calling a method on the supplied APIs, and
// triggering the callback for the request.
TEST_F(APIBindingsSystemTest, TestInitializationAndCallbacks) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> alpha_api = bindings_system()->CreateAPIInstance(
      kAlphaAPIName, context, isolate(), base::Bind(&AllowAllAPIs));
  ASSERT_FALSE(alpha_api.IsEmpty());
  v8::Local<v8::Object> beta_api = bindings_system()->CreateAPIInstance(
      kBetaAPIName, context, isolate(), base::Bind(&AllowAllAPIs));
  ASSERT_FALSE(beta_api.IsEmpty());

  {
    // Test a simple call -> response.
    const char kTestCall[] =
        "obj.functionWithCallback('foo', function() {\n"
        "  this.callbackArguments = Array.from(arguments);\n"
        "});";
    CallFunctionOnObject(context, alpha_api, kTestCall);

    ValidateLastRequest("alpha.functionWithCallback", "['foo']");

    const char kResponseArgsJson[] = "['response',1,{'key':42}]";
    std::unique_ptr<base::ListValue> expected_args =
        ListValueFromString(kResponseArgsJson);
    bindings_system()->CompleteRequest(last_request()->request_id,
                                       *expected_args);

    std::unique_ptr<base::Value> result = GetBaseValuePropertyFromObject(
        context->Global(), context, "callbackArguments");
    ASSERT_TRUE(result);
    EXPECT_EQ(ReplaceSingleQuotes(kResponseArgsJson), ValueToString(*result));
    reset_last_request();
  }

  {
    // Test a call with references -> response.
    const char kTestCall[] =
        "obj.functionWithRefAndCallback({prop1: 'alpha', prop2: 42},\n"
        "                               function() {\n"
        "  this.callbackArguments = Array.from(arguments);\n"
        "});";

    CallFunctionOnObject(context, alpha_api, kTestCall);

    ValidateLastRequest("alpha.functionWithRefAndCallback",
                        "[{'prop1':'alpha','prop2':42}]");

    bindings_system()->CompleteRequest(last_request()->request_id,
                                       base::ListValue());

    std::unique_ptr<base::Value> result = GetBaseValuePropertyFromObject(
        context->Global(), context, "callbackArguments");
    ASSERT_TRUE(result);
    EXPECT_EQ("[]", ValueToString(*result));
    reset_last_request();
  }

  {
    // Test an event registration -> event occurrence.
    const char kTestCall[] =
        "obj.alphaEvent.addListener(function() {\n"
        "  this.eventArguments = Array.from(arguments);\n"
        "});\n";
    CallFunctionOnObject(context, alpha_api, kTestCall);

    const char kResponseArgsJson[] = "['response',1,{'key':42}]";
    std::unique_ptr<base::ListValue> expected_args =
        ListValueFromString(kResponseArgsJson);
    bindings_system()->FireEventInContext("alpha.alphaEvent", context,
                                          *expected_args);

    std::unique_ptr<base::Value> result = GetBaseValuePropertyFromObject(
        context->Global(), context, "eventArguments");
    ASSERT_TRUE(result);
    EXPECT_EQ(ReplaceSingleQuotes(kResponseArgsJson), ValueToString(*result));
  }

  {
    // Test a call -> response on the second API.
    const char kTestCall[] = "obj.simpleFunc(2)";
    CallFunctionOnObject(context, beta_api, kTestCall);
    ValidateLastRequest("beta.simpleFunc", "[2]");
    EXPECT_TRUE(last_request()->request_id.empty());
    reset_last_request();
  }
}

// An implementation using real API schemas.
class APIBindingsSystemTestWithRealAPI : public APIBindingsSystemTestBase {
 protected:
  APIBindingsSystemTestWithRealAPI() {}

  // Executes the given |script_source| in |context|, expecting no exceptions.
  void ExecuteScript(v8::Local<v8::Context> context,
                     const std::string& script_source);

  // Executes the given |script_source| in |context| and compares a caught
  // error to |expected_error|.
  void ExecuteScriptAndExpectError(v8::Local<v8::Context> context,
                                   const std::string& script_source,
                                   const std::string& expected_error);

 private:
  const base::DictionaryValue& GetAPISchema(
      const std::string& api_name) override {
    const base::DictionaryValue* schema =
        ExtensionAPI::GetSharedInstance()->GetSchema(api_name);
    EXPECT_TRUE(schema);
    return *schema;
  }

  DISALLOW_COPY_AND_ASSIGN(APIBindingsSystemTestWithRealAPI);
};

void APIBindingsSystemTestWithRealAPI::ExecuteScript(
    v8::Local<v8::Context> context,
    const std::string& script_source) {
  v8::TryCatch try_catch(isolate());
  // V8ValueFromScriptSource runs the source and returns the result; here, we
  // only care about running the source.
  V8ValueFromScriptSource(context, script_source);
  EXPECT_FALSE(try_catch.HasCaught())
      << gin::V8ToString(try_catch.Message()->Get());
}

void APIBindingsSystemTestWithRealAPI::ExecuteScriptAndExpectError(
    v8::Local<v8::Context> context,
    const std::string& script_source,
    const std::string& expected_error) {
  v8::TryCatch try_catch(isolate());
  V8ValueFromScriptSource(context, script_source);
  ASSERT_TRUE(try_catch.HasCaught()) << script_source;
  EXPECT_EQ(expected_error, gin::V8ToString(try_catch.Message()->Get()));
}

// The following test demonstrates how APIBindingsSystem can be used with "real"
// Extension APIs; that is, using the raw Extension API schemas, rather than a
// substituted test schema. This is useful to both show how the system is
// intended to be used in the future as well as to make sure that it works with
// actual APIs.
TEST_F(APIBindingsSystemTestWithRealAPI, RealAPIs) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  v8::Local<v8::Object> chrome = v8::Object::New(isolate());
  {
    v8::Maybe<bool> res = context->Global()->Set(
        context, gin::StringToV8(isolate(), "chrome"), chrome);
    ASSERT_TRUE(res.IsJust());
    ASSERT_TRUE(res.FromJust());
  }

  auto add_api_to_chrome = [this, &chrome,
                            &context](const std::string& api_name) {
    v8::Local<v8::Object> api = bindings_system()->CreateAPIInstance(
        api_name, context, context->GetIsolate(), base::Bind(&AllowAllAPIs));
    ASSERT_FALSE(api.IsEmpty()) << api_name;
    v8::Maybe<bool> res = chrome->Set(
        context, gin::StringToV8(context->GetIsolate(), api_name), api);
    ASSERT_TRUE(res.IsJust());
    ASSERT_TRUE(res.FromJust());
  };

  // Pick two relatively small APIs that don't have any custom bindings (which
  // aren't supported yet).
  add_api_to_chrome("idle");
  add_api_to_chrome("power");

  // Test passing methods.
  {
    const char kTestCall[] = "chrome.power.requestKeepAwake('display');";
    ExecuteScript(context, kTestCall);
    ValidateLastRequest("power.requestKeepAwake", "['display']");
    EXPECT_TRUE(last_request()->request_id.empty());
    reset_last_request();
  }

  {
    const char kTestCall[] = "chrome.power.releaseKeepAwake()";
    ExecuteScript(context, kTestCall);
    ValidateLastRequest("power.releaseKeepAwake", "[]");
    EXPECT_TRUE(last_request()->request_id.empty());
    reset_last_request();
  }

  {
    const char kTestCall[] = "chrome.idle.queryState(30, function() {})";
    ExecuteScript(context, kTestCall);
    ValidateLastRequest("idle.queryState", "[30]");
    EXPECT_FALSE(last_request()->request_id.empty());
    reset_last_request();
  }

  {
    const char kTestCall[] = "chrome.idle.setDetectionInterval(30);";
    ExecuteScript(context, kTestCall);
    ValidateLastRequest("idle.setDetectionInterval", "[30]");
    EXPECT_TRUE(last_request()->request_id.empty());
    reset_last_request();
  }

  // Check catching errors.
  const char kError[] = "Uncaught TypeError: Invalid invocation";
  {
    // "disp" is not an allowed enum value.
    const char kTestCall[] = "chrome.power.requestKeepAwake('disp');";
    ExecuteScriptAndExpectError(context, kTestCall, kError);
    EXPECT_FALSE(last_request());
    reset_last_request();  // Just to not pollute future results.
  }

  {
    // The queryState() param has a minimum of 15.
    const char kTestCall[] = "chrome.idle.queryState(10, function() {});";
    ExecuteScriptAndExpectError(context, kTestCall, kError);
    EXPECT_FALSE(last_request());
    reset_last_request();  // Just to not pollute future results.
  }

  {
    const char kTestCall[] =
        "chrome.idle.onStateChanged.addListener(state => {\n"
        "  this.idleState = state;\n"
        "});\n";
    ExecuteScript(context, kTestCall);
    v8::Local<v8::Value> v8_result =
        GetPropertyFromObject(context->Global(), context, "idleState");
    EXPECT_TRUE(v8_result->IsUndefined());
    bindings_system()->FireEventInContext("idle.onStateChanged", context,
                                          *ListValueFromString("['active']"));

    std::unique_ptr<base::Value> result =
        GetBaseValuePropertyFromObject(context->Global(), context, "idleState");
    ASSERT_TRUE(result);
    EXPECT_EQ("\"active\"", ValueToString(*result));
  }
}

}  // namespace extensions
