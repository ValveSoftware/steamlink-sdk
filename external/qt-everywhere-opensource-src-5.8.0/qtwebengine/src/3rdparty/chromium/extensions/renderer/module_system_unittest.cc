// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/module_system.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "extensions/renderer/module_system_test.h"
#include "gin/modules/module_registry.h"

namespace extensions {

class CounterNatives : public ObjectBackedNativeHandler {
 public:
  explicit CounterNatives(ScriptContext* context)
      : ObjectBackedNativeHandler(context), counter_(0) {
    RouteFunction("Get",
                  base::Bind(&CounterNatives::Get, base::Unretained(this)));
    RouteFunction(
        "Increment",
        base::Bind(&CounterNatives::Increment, base::Unretained(this)));
  }

  void Get(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(static_cast<int32_t>(counter_));
  }

  void Increment(const v8::FunctionCallbackInfo<v8::Value>& args) {
    counter_++;
  }

 private:
  int counter_;
};

class TestExceptionHandler : public ModuleSystem::ExceptionHandler {
 public:
  TestExceptionHandler()
      : ModuleSystem::ExceptionHandler(nullptr), handled_exception_(false) {}

  void HandleUncaughtException(const v8::TryCatch& try_catch) override {
    handled_exception_ = true;
  }

  bool handled_exception() const { return handled_exception_; }

 private:
  bool handled_exception_;
};

TEST_F(ModuleSystemTest, TestExceptionHandling) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  TestExceptionHandler* handler = new TestExceptionHandler;
  std::unique_ptr<ModuleSystem::ExceptionHandler> scoped_handler(handler);
  ASSERT_FALSE(handler->handled_exception());
  env()->module_system()->SetExceptionHandlerForTest(std::move(scoped_handler));

  env()->RegisterModule("test", "throw 'hi';");
  env()->module_system()->Require("test");
  ASSERT_TRUE(handler->handled_exception());

  ExpectNoAssertionsMade();
}

TEST_F(ModuleSystemTest, TestRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("add",
                        "exports.$set('Add',"
                                     "function(x, y) { return x + y; });");
  env()->RegisterModule("test",
                        "var Add = require('add').Add;"
                        "requireNative('assert').AssertTrue(Add(3, 5) == 8);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestNestedRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("add",
                        "exports.$set('Add',"
                                     "function(x, y) { return x + y; });");
  env()->RegisterModule("double",
                        "var Add = require('add').Add;"
                        "exports.$set('Double',"
                                     "function(x) { return Add(x, x); });");
  env()->RegisterModule("test",
                        "var Double = require('double').Double;"
                        "requireNative('assert').AssertTrue(Double(3) == 6);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestModuleInsulation) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("x",
                        "var x = 10;"
                        "exports.$set('X', function() { return x; });");
  env()->RegisterModule("y",
                        "var x = 15;"
                        "require('x');"
                        "exports.$set('Y', function() { return x; });");
  env()->RegisterModule("test",
                        "var Y = require('y').Y;"
                        "var X = require('x').X;"
                        "var assert = requireNative('assert');"
                        "assert.AssertTrue(!this.hasOwnProperty('x'));"
                        "assert.AssertTrue(Y() == 15);"
                        "assert.AssertTrue(X() == 10);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestNativesAreDisabledOutsideANativesEnabledScope) {
  env()->RegisterModule("test",
                        "var assert;"
                        "try {"
                        "  assert = requireNative('assert');"
                        "} catch (e) {"
                        "  var caught = true;"
                        "}"
                        "if (assert) {"
                        "  assert.AssertTrue(true);"
                        "}");
  env()->module_system()->Require("test");
  ExpectNoAssertionsMade();
}

TEST_F(ModuleSystemTest, TestNativesAreEnabledWithinANativesEnabledScope) {
  env()->RegisterModule("test",
                        "var assert = requireNative('assert');"
                        "assert.AssertTrue(true);");

  {
    ModuleSystem::NativesEnabledScope natives_enabled(env()->module_system());
    {
      ModuleSystem::NativesEnabledScope natives_enabled_inner(
          env()->module_system());
    }
    env()->module_system()->Require("test");
  }
}

TEST_F(ModuleSystemTest, TestLazyField) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("lazy", "exports.$set('x', 5);");

  v8::Local<v8::Object> object = env()->CreateGlobal("object");

  env()->module_system()->SetLazyField(object, "blah", "lazy", "x");

  env()->RegisterModule("test",
                        "var assert = requireNative('assert');"
                        "assert.AssertTrue(object.blah == 5);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestLazyFieldYieldingObject) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule(
      "lazy",
      "var object = {};"
      "object.__defineGetter__('z', function() { return 1; });"
      "object.x = 5;"
      "object.y = function() { return 10; };"
      "exports.$set('object', object);");

  v8::Local<v8::Object> object = env()->CreateGlobal("object");

  env()->module_system()->SetLazyField(object, "thing", "lazy", "object");

  env()->RegisterModule("test",
                        "var assert = requireNative('assert');"
                        "assert.AssertTrue(object.thing.x == 5);"
                        "assert.AssertTrue(object.thing.y() == 10);"
                        "assert.AssertTrue(object.thing.z == 1);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestLazyFieldIsOnlyEvaledOnce) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->module_system()->RegisterNativeHandler(
      "counter",
      std::unique_ptr<NativeHandler>(new CounterNatives(env()->context())));
  env()->RegisterModule("lazy",
                        "requireNative('counter').Increment();"
                        "exports.$set('x', 5);");

  v8::Local<v8::Object> object = env()->CreateGlobal("object");

  env()->module_system()->SetLazyField(object, "x", "lazy", "x");

  env()->RegisterModule("test",
                        "var assert = requireNative('assert');"
                        "var counter = requireNative('counter');"
                        "assert.AssertTrue(counter.Get() == 0);"
                        "object.x;"
                        "assert.AssertTrue(counter.Get() == 1);"
                        "object.x;"
                        "assert.AssertTrue(counter.Get() == 1);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestRequireNativesAfterLazyEvaluation) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("lazy", "exports.$set('x', 5);");
  v8::Local<v8::Object> object = env()->CreateGlobal("object");

  env()->module_system()->SetLazyField(object, "x", "lazy", "x");
  env()->RegisterModule("test",
                        "object.x;"
                        "requireNative('assert').AssertTrue(true);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestTransitiveRequire) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("dependency", "exports.$set('x', 5);");
  env()->RegisterModule("lazy",
                        "exports.$set('output', require('dependency'));");

  v8::Local<v8::Object> object = env()->CreateGlobal("object");

  env()->module_system()->SetLazyField(object, "thing", "lazy", "output");

  env()->RegisterModule("test",
                        "var assert = requireNative('assert');"
                        "assert.AssertTrue(object.thing.x == 5);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestModulesOnlyGetEvaledOnce) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->module_system()->RegisterNativeHandler(
      "counter",
      std::unique_ptr<NativeHandler>(new CounterNatives(env()->context())));

  env()->RegisterModule("incrementsWhenEvaled",
                        "requireNative('counter').Increment();");
  env()->RegisterModule("test",
                        "var assert = requireNative('assert');"
                        "var counter = requireNative('counter');"
                        "assert.AssertTrue(counter.Get() == 0);"
                        "require('incrementsWhenEvaled');"
                        "assert.AssertTrue(counter.Get() == 1);"
                        "require('incrementsWhenEvaled');"
                        "assert.AssertTrue(counter.Get() == 1);");

  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestOverrideNativeHandler) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->OverrideNativeHandler("assert",
                               "exports.$set('AssertTrue', function() {});");
  env()->RegisterModule("test", "requireNative('assert').AssertTrue(true);");
  ExpectNoAssertionsMade();
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestOverrideNonExistentNativeHandler) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->OverrideNativeHandler("thing", "exports.$set('x', 5);");
  env()->RegisterModule("test",
                        "var assert = requireNative('assert');"
                        "assert.AssertTrue(requireNative('thing').x == 5);");
  env()->module_system()->Require("test");
}

TEST_F(ModuleSystemTest, TestRequireAsync) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("add",
                        "define('add', [], function() {"
                        "  return { Add: function(x, y) { return x + y; } };"
                        "});");
  env()->RegisterModule("math",
                        "define('math', ['add'], function(add) {"
                        "  return { Add: add.Add };"
                        "});");
  env()->RegisterModule(
      "test",
      "requireAsync('math').then(function(math) {"
      "  requireNative('assert').AssertTrue(math.Add(3, 5) == 8);"
      "});");
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestRequireAsyncInParallel) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("add",
                        "define('add', [], function() {"
                        "  return { Add: function(x, y) { return x + y; } };"
                        "});");
  env()->RegisterModule(
      "subtract",
      "define('subtract', [], function() {"
      "  return { Subtract: function(x, y) { return x - y; } };"
      "});");
  env()->RegisterModule(
      "math",
      "exports.$set('AddAndSubtract', function(x, y, z) {"
      "  return Promise.all([requireAsync('add'),"
      "                      requireAsync('subtract')"
      "  ]).then(function(modules) {"
      "    return modules[1].Subtract(modules[0].Add(x, y), z);"
      "  });"
      "});");
  env()->RegisterModule("test",
                        "var AddAndSubtract = require('math').AddAndSubtract;"
                        "AddAndSubtract(3, 5, 2).then(function(result) {"
                        "  requireNative('assert').AssertTrue(result == 6);"
                        "});");
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestNestedRequireAsyncs) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("first",
                        "define('first', [], function() {"
                        "  return { next: 'second' };"
                        "});");
  env()->RegisterModule("second",
                        "define('second', [], function() {"
                        "  return { next: '' };"
                        "});");
  env()->RegisterModule(
      "test",
      "requireAsync('first').then(function(module) {"
      "  return requireAsync(module.next)"
      "}).then(function(module) {"
      "  requireNative('assert').AssertTrue(module.next === '');"
      "});");
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestRequireFromAMDModule) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule(
      "add", "exports.$set('Add', function(x, y) { return x + y; });");
  env()->RegisterModule("math",
                        "define('math', [], function() {"
                        "  var add = require('add');"
                        "  return { Add: add.Add };"
                        "});");
  env()->RegisterModule(
      "test",
      "requireAsync('math').then(function(math) {"
      "  requireNative('assert').AssertTrue(math.Add(3, 5) == 8);"
      "});");
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestRequireAsyncFromAMDModule) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("add",
                        "define('add', [], function() {"
                        "  return { Add: function(x, y) { return x + y; } };"
                        "});");
  env()->RegisterModule("math",
                        "define('math', [], function() {"
                        "  function Add(x, y) {"
                        "    return requireAsync('add').then(function(add) {"
                        "      return add.Add(x, y);"
                        "    });"
                        "  }"
                        "  return { Add: Add };"
                        "});");
  env()->RegisterModule("test",
                        "requireAsync('math').then(function(math) {"
                        "  return math.Add(3, 6);"
                        "}).then(function(result) {"
                        "  requireNative('assert').AssertTrue(result == 9);"
                        "});");
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestRequireAsyncFromAnotherContext) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule(
      "test",
      "requireAsync('natives').then(function(natives) {"
      "  natives.requireAsync('ping').then(function(ping) {"
      "    return ping();"
      "  }).then(function(result) {"
      "    requireNative('assert').AssertTrue(result == 'pong');"
      "  });"
      "});");
  std::unique_ptr<ModuleSystemTestEnvironment> other_env = CreateEnvironment();
  other_env->RegisterModule("ping",
                            "define('ping', ['natives'], function(natives) {"
                            "  return function() {"
                            "    return 'pong';"
                            "  }"
                            "});");
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(
          env()->isolate(), "natives",
          other_env->module_system()->NewInstance());
  gin::ModuleRegistry::From(other_env->context()->v8_context())
      ->AddBuiltinModule(
          env()->isolate(), "natives",
          env()->module_system()->NewInstance());
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestRequireAsyncBetweenContexts) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("pong",
                        "define('pong', [], function() {"
                        "  return function() { return 'done'; };"
                        "});");
  env()->RegisterModule(
      "test",
      "requireAsync('natives').then(function(natives) {"
      "  natives.requireAsync('ping').then(function(ping) {"
      "    return ping();"
      "  }).then(function(pong) {"
      "    return pong();"
      "  }).then(function(result) {"
      "    requireNative('assert').AssertTrue(result == 'done');"
      "  });"
      "});");
  std::unique_ptr<ModuleSystemTestEnvironment> other_env = CreateEnvironment();
  other_env->RegisterModule("ping",
                            "define('ping', ['natives'], function(natives) {"
                            "  return function() {"
                            "    return natives.requireAsync('pong');"
                            "  }"
                            "});");
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(
          env()->isolate(), "natives",
          other_env->module_system()->NewInstance());
  gin::ModuleRegistry::From(other_env->context()->v8_context())
      ->AddBuiltinModule(
          env()->isolate(), "natives",
          env()->module_system()->NewInstance());
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestRequireAsyncFromContextWithNoModuleRegistry) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test",
                        "requireAsync('natives').then(function(natives) {"
                        "  var AssertTrue = requireNative('assert').AssertTrue;"
                        "  natives.requireAsync('foo').then(function() {"
                        "    AssertTrue(false);"
                        "  }).catch(function(error) {"
                        "    AssertTrue(error.message == "
                        "               'Extension view no longer exists');"
                        "  });"
                        "});");
  std::unique_ptr<ModuleSystemTestEnvironment> other_env = CreateEnvironment();
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(
          env()->isolate(), "natives",
          other_env->module_system()->NewInstance());
  other_env->ShutdownGin();
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestRequireAsyncFromContextWithNoModuleSystem) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test",
                        "requireAsync('natives').then(function(natives) {"
                        "  requireNative('assert').AssertTrue("
                        "      natives.requireAsync('foo') === undefined);"
                        "});");
  std::unique_ptr<ModuleSystemTestEnvironment> other_env = CreateEnvironment();
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(
          env()->isolate(), "natives",
          other_env->module_system()->NewInstance());
  other_env->ShutdownModuleSystem();
  env()->module_system()->Require("test");
  RunResolvedPromises();
}

TEST_F(ModuleSystemTest, TestPrivatesIsPrivate) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule(
      "test",
      "var v = privates({});"
      "requireNative('assert').AssertFalse(v instanceof Object);");
  env()->module_system()->Require("test");
}

}  // namespace extensions
