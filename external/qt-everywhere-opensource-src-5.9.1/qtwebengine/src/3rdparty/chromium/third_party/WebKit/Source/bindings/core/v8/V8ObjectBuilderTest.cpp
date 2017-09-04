// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8ObjectBuilder.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(V8ObjectBuilderTest, addNull) {
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  V8ObjectBuilder builder(scriptState);
  builder.addNull("null_check");
  ScriptValue jsonObject = builder.scriptValue();
  EXPECT_TRUE(jsonObject.isObject());

  String jsonString = v8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.context(),
                          jsonObject.v8Value().As<v8::Object>())
          .ToLocalChecked(),
      DoNotExternalize);

  String expected = "{\"null_check\":null}";
  EXPECT_EQ(expected, jsonString);
}

TEST(V8ObjectBuilderTest, addBoolean) {
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  V8ObjectBuilder builder(scriptState);
  builder.addBoolean("b1", true);
  builder.addBoolean("b2", false);
  ScriptValue jsonObject = builder.scriptValue();
  EXPECT_TRUE(jsonObject.isObject());

  String jsonString = v8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.context(),
                          jsonObject.v8Value().As<v8::Object>())
          .ToLocalChecked(),
      DoNotExternalize);

  String expected = "{\"b1\":true,\"b2\":false}";
  EXPECT_EQ(expected, jsonString);
}

TEST(V8ObjectBuilderTest, addNumber) {
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  V8ObjectBuilder builder(scriptState);
  builder.addNumber("n1", 123);
  builder.addNumber("n2", 123.456);
  ScriptValue jsonObject = builder.scriptValue();
  EXPECT_TRUE(jsonObject.isObject());

  String jsonString = v8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.context(),
                          jsonObject.v8Value().As<v8::Object>())
          .ToLocalChecked(),
      DoNotExternalize);

  String expected = "{\"n1\":123,\"n2\":123.456}";
  EXPECT_EQ(expected, jsonString);
}

TEST(V8ObjectBuilderTest, addString) {
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  V8ObjectBuilder builder(scriptState);

  WTF::String test1 = "test1";
  WTF::String test2;
  WTF::String test3 = "test3";
  WTF::String test4;

  builder.addString("test1", test1);
  builder.addString("test2", test2);
  builder.addStringOrNull("test3", test3);
  builder.addStringOrNull("test4", test4);
  ScriptValue jsonObject = builder.scriptValue();
  EXPECT_TRUE(jsonObject.isObject());

  String jsonString = v8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.context(),
                          jsonObject.v8Value().As<v8::Object>())
          .ToLocalChecked(),
      DoNotExternalize);

  String expected =
      "{\"test1\":\"test1\",\"test2\":\"\",\"test3\":\"test3\",\"test4\":"
      "null}";
  EXPECT_EQ(expected, jsonString);
}

TEST(V8ObjectBuilderTest, add) {
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  V8ObjectBuilder builder(scriptState);
  V8ObjectBuilder result(scriptState);
  builder.addNumber("n1", 123);
  builder.addNumber("n2", 123.456);
  result.add("builder", builder);
  ScriptValue builderJsonObject = builder.scriptValue();
  ScriptValue resultJsonObject = result.scriptValue();
  EXPECT_TRUE(builderJsonObject.isObject());
  EXPECT_TRUE(resultJsonObject.isObject());

  String jsonString = v8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.context(),
                          resultJsonObject.v8Value().As<v8::Object>())
          .ToLocalChecked(),
      DoNotExternalize);

  String expected = "{\"builder\":{\"n1\":123,\"n2\":123.456}}";
  EXPECT_EQ(expected, jsonString);
}

}  // namespace

}  // namespace blink
