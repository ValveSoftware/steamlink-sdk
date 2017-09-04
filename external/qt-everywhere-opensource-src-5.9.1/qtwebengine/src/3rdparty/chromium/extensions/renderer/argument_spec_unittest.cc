// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/argument_spec.h"
#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

class ArgumentSpecUnitTest : public gin::V8Test {
 protected:
  ArgumentSpecUnitTest() {}
  ~ArgumentSpecUnitTest() override {}
  void ExpectSuccess(const ArgumentSpec& spec,
                     const std::string& script_source,
                     const std::string& expected_json_single_quotes) {
    RunTest(spec, script_source, TestResult::PASS,
            ReplaceSingleQuotes(expected_json_single_quotes), std::string());
  }

  void ExpectFailure(const ArgumentSpec& spec,
                     const std::string& script_source) {
    RunTest(spec, script_source, TestResult::FAIL, std::string(),
            std::string());
  }

  void ExpectThrow(const ArgumentSpec& spec,
                   const std::string& script_source,
                   const std::string& expected_thrown_message) {
    RunTest(spec, script_source, TestResult::THROW, std::string(),
            expected_thrown_message);
  }

  void AddTypeRef(const std::string& id, std::unique_ptr<ArgumentSpec> spec) {
    type_refs_[id] = std::move(spec);
  }

 private:
  enum class TestResult { PASS, FAIL, THROW, };

  void RunTest(const ArgumentSpec& spec,
               const std::string& script_source,
               TestResult expected_result,
               const std::string& expected_json,
               const std::string& expected_thrown_message);

  ArgumentSpec::RefMap type_refs_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentSpecUnitTest);
};

void ArgumentSpecUnitTest::RunTest(const ArgumentSpec& spec,
                                   const std::string& script_source,
                                   TestResult expected_result,
                                   const std::string& expected_json,
                                   const std::string& expected_thrown_message) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(instance_->isolate());

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(instance_->isolate(), context_);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> val = V8ValueFromScriptSource(context, script_source);
  ASSERT_FALSE(val.IsEmpty()) << script_source;

  std::string error;
  std::unique_ptr<base::Value> out_value =
      spec.ConvertArgument(context, val, type_refs_, &error);
  bool should_succeed = expected_result == TestResult::PASS;
  ASSERT_EQ(should_succeed, !!out_value) << script_source << ", " << error;
  bool should_throw = expected_result == TestResult::THROW;
  ASSERT_EQ(should_throw, try_catch.HasCaught()) << script_source;
  if (should_succeed) {
    ASSERT_TRUE(out_value);
    EXPECT_EQ(expected_json, ValueToString(*out_value));
  } else if (should_throw) {
    EXPECT_EQ(expected_thrown_message,
              gin::V8ToString(try_catch.Message()->Get()));
  }
}

TEST_F(ArgumentSpecUnitTest, Test) {
  {
    ArgumentSpec spec(*ValueFromString("{'type': 'integer'}"));
    ExpectSuccess(spec, "1", "1");
    ExpectSuccess(spec, "-1", "-1");
    ExpectSuccess(spec, "0", "0");
    ExpectFailure(spec, "undefined");
    ExpectFailure(spec, "null");
    ExpectFailure(spec, "'foo'");
    ExpectFailure(spec, "'1'");
    ExpectFailure(spec, "{}");
    ExpectFailure(spec, "[1]");
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'integer', 'minimum': 1}"));
    ExpectSuccess(spec, "2", "2");
    ExpectSuccess(spec, "1", "1");
    ExpectFailure(spec, "0");
    ExpectFailure(spec, "-1");
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'string'}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "''", "''");
    ExpectFailure(spec, "1");
    ExpectFailure(spec, "{}");
    ExpectFailure(spec, "['foo']");
  }

  {
    ArgumentSpec spec(
        *ValueFromString("{'type': 'string', 'enum': ['foo', 'bar']}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "'bar'", "'bar'");
    ExpectFailure(spec, "['foo']");
    ExpectFailure(spec, "'fo'");
    ExpectFailure(spec, "'foobar'");
    ExpectFailure(spec, "'baz'");
    ExpectFailure(spec, "''");
  }

  {
    ArgumentSpec spec(*ValueFromString(
        "{'type': 'string', 'enum': [{'name': 'foo'}, {'name': 'bar'}]}"));
    ExpectSuccess(spec, "'foo'", "'foo'");
    ExpectSuccess(spec, "'bar'", "'bar'");
    ExpectFailure(spec, "['foo']");
    ExpectFailure(spec, "'fo'");
    ExpectFailure(spec, "'foobar'");
    ExpectFailure(spec, "'baz'");
    ExpectFailure(spec, "''");
  }

  {
    ArgumentSpec spec(*ValueFromString("{'type': 'boolean'}"));
    ExpectSuccess(spec, "true", "true");
    ExpectSuccess(spec, "false", "false");
    ExpectFailure(spec, "1");
    ExpectFailure(spec, "'true'");
    ExpectFailure(spec, "null");
  }

  {
    ArgumentSpec spec(
        *ValueFromString("{'type': 'array', 'items': {'type': 'string'}}"));
    ExpectSuccess(spec, "[]", "[]");
    ExpectSuccess(spec, "['foo']", "['foo']");
    ExpectSuccess(spec, "['foo', 'bar']", "['foo','bar']");
    ExpectSuccess(spec, "var x = new Array(); x[0] = 'foo'; x;", "['foo']");
    ExpectFailure(spec, "'foo'");
    ExpectFailure(spec, "[1, 2]");
    ExpectFailure(spec, "['foo', 1]");
    ExpectThrow(
        spec,
        "var x = [];"
        "Object.defineProperty("
        "    x, 0,"
        "    { get: () => { throw new Error('Badness'); } });"
        "x;",
        "Uncaught Error: Badness");
  }

  {
    const char kObjectSpec[] =
        "{"
        "  'type': 'object',"
        "  'properties': {"
        "    'prop1': {'type': 'string'},"
        "    'prop2': {'type': 'integer', 'optional': true}"
        "  }"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectSpec));
    ExpectSuccess(spec, "({prop1: 'foo', prop2: 2})",
                  "{'prop1':'foo','prop2':2}");
    ExpectSuccess(spec, "({prop1: 'foo', prop2: 2, prop3: 'blah'})",
                  "{'prop1':'foo','prop2':2}");
    ExpectSuccess(spec, "({prop1: 'foo'})", "{'prop1':'foo'}");
    ExpectSuccess(spec, "({prop1: 'foo', prop2: null})", "{'prop1':'foo'}");
    ExpectSuccess(spec, "x = {}; x.prop1 = 'foo'; x;", "{'prop1':'foo'}");
    ExpectSuccess(
        spec,
        "function X() {}\n"
        "X.prototype = { prop1: 'foo' };\n"
        "function Y() { this.__proto__ = X.prototype; }\n"
        "var z = new Y();\n"
        "z;",
        "{'prop1':'foo'}");
    ExpectFailure(spec, "({prop1: 'foo', prop2: 'bar'})");
    ExpectFailure(spec, "({prop2: 2})");
    // Self-referential fun. Currently we don't have to worry about these much
    // because the spec won't match at some point (and V8ValueConverter has
    // cycle detection and will fail).
    ExpectFailure(spec, "x = {}; x.prop1 = x; x;");
    ExpectThrow(
        spec,
        "({ get prop1() { throw new Error('Badness'); }});",
        "Uncaught Error: Badness");
    ExpectThrow(
        spec,
        "x = {prop1: 'foo'};\n"
        "Object.defineProperty(\n"
        "    x, 'prop2',\n"
        "    { get: () => { throw new Error('Badness'); } });\n"
        "x;",
        "Uncaught Error: Badness");
  }

  {
    const char kFunctionSpec[] = "{ 'type': 'function' }";
    // We don't allow conversion of functions (converting to a base::Value is
    // impossible), but we should still be able to parse a function
    // specification.
    ArgumentSpec spec(*ValueFromString(kFunctionSpec));
    EXPECT_EQ(ArgumentType::FUNCTION, spec.type());
  }
}

TEST_F(ArgumentSpecUnitTest, TypeRefsTest) {
  const char kObjectType[] =
      "{"
      "  'id': 'refObj',"
      "  'type': 'object',"
      "  'properties': {"
      "    'prop1': {'type': 'string'},"
      "    'prop2': {'type': 'integer', 'optional': true}"
      "  }"
      "}";
  const char kEnumType[] =
      "{'id': 'refEnum', 'type': 'string', 'enum': ['alpha', 'beta']}";
  AddTypeRef("refObj",
             base::MakeUnique<ArgumentSpec>(*ValueFromString(kObjectType)));
  AddTypeRef("refEnum",
             base::MakeUnique<ArgumentSpec>(*ValueFromString(kEnumType)));

  {
    const char kObjectWithRefEnumSpec[] =
        "{"
        "  'name': 'objWithRefEnum',"
        "  'type': 'object',"
        "  'properties': {"
        "    'e': {'$ref': 'refEnum'},"
        "    'sub': {'type': 'integer'}"
        "  }"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectWithRefEnumSpec));
    ExpectSuccess(spec, "({e: 'alpha', sub: 1})", "{'e':'alpha','sub':1}");
    ExpectSuccess(spec, "({e: 'beta', sub: 1})", "{'e':'beta','sub':1}");
    ExpectFailure(spec, "({e: 'gamma', sub: 1})");
    ExpectFailure(spec, "({e: 'alpha'})");
  }

  {
    const char kObjectWithRefObjectSpec[] =
        "{"
        "  'name': 'objWithRefObject',"
        "  'type': 'object',"
        "  'properties': {"
        "    'o': {'$ref': 'refObj'}"
        "  }"
        "}";
    ArgumentSpec spec(*ValueFromString(kObjectWithRefObjectSpec));
    ExpectSuccess(spec, "({o: {prop1: 'foo'}})", "{'o':{'prop1':'foo'}}");
    ExpectSuccess(spec, "({o: {prop1: 'foo', prop2: 2}})",
                  "{'o':{'prop1':'foo','prop2':2}}");
    ExpectFailure(spec, "({o: {prop1: 1}})");
  }

  {
    const char kRefEnumListSpec[] =
        "{'type': 'array', 'items': {'$ref': 'refEnum'}}";
    ArgumentSpec spec(*ValueFromString(kRefEnumListSpec));
    ExpectSuccess(spec, "['alpha']", "['alpha']");
    ExpectSuccess(spec, "['alpha', 'alpha']", "['alpha','alpha']");
    ExpectSuccess(spec, "['alpha', 'beta']", "['alpha','beta']");
    ExpectFailure(spec, "['alpha', 'beta', 'gamma']");
  }
}

}  // namespace extensions
