// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_MODULE_SYSTEM_TEST_H_
#define EXTENSIONS_RENDERER_MODULE_SYSTEM_TEST_H_

#include "base/macros.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "gin/public/context_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

class ModuleSystemTestEnvironment {
 public:
  class AssertNatives;
  class StringSourceMap;

  explicit ModuleSystemTestEnvironment(v8::Isolate* isolate);
  ~ModuleSystemTestEnvironment();

  // Register a named JS module in the module system.
  void RegisterModule(const std::string& name, const std::string& code);

  // Register a named JS module with source retrieved from a ResourceBundle.
  void RegisterModule(const std::string& name, int resource_id);

  // Register a named JS module in the module system and tell the module system
  // to use it to handle any requireNative() calls for native modules with that
  // name.
  void OverrideNativeHandler(const std::string& name, const std::string& code);

  // Registers |file_name| from chrome/test/data/extensions as a module name
  // |module_name|.
  void RegisterTestFile(const std::string& module_name,
                        const std::string& file_name);

  // Create an empty object in the global scope with name |name|.
  v8::Local<v8::Object> CreateGlobal(const std::string& name);

  void ShutdownGin();

  void ShutdownModuleSystem();

  ModuleSystem* module_system() { return context_->module_system(); }

  ScriptContext* context() { return context_.get(); }

  v8::Isolate* isolate() { return isolate_; }

  AssertNatives* assert_natives() { return assert_natives_; }

 private:
  v8::Isolate* isolate_;
  std::unique_ptr<gin::ContextHolder> context_holder_;
  v8::HandleScope handle_scope_;
  std::unique_ptr<ScriptContext> context_;
  AssertNatives* assert_natives_;
  std::unique_ptr<StringSourceMap> source_map_;

  DISALLOW_COPY_AND_ASSIGN(ModuleSystemTestEnvironment);
};

// Test fixture for testing JS that makes use of the module system.
//
// Typically tests will look like:
//
// TEST_F(MyModuleSystemTest, TestStuff) {
//   ModuleSystem::NativesEnabledScope natives_enabled(module_system_.get());
//   RegisterModule("test", "requireNative('assert').AssertTrue(true);");
//   module_system_->Require("test");
// }
//
// By default a test will fail if no method in the native module 'assert' is
// called. This behaviour can be overridden by calling ExpectNoAssertionsMade().
class ModuleSystemTest : public testing::Test {
 public:
  ModuleSystemTest();
  ~ModuleSystemTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  ModuleSystemTestEnvironment* env() { return env_.get(); }

  std::unique_ptr<ModuleSystemTestEnvironment> CreateEnvironment();

  // Make the test fail if any asserts are called. By default a test will fail
  // if no asserts are called.
  void ExpectNoAssertionsMade();

  // Runs promises that have been resolved. Resolved promises will not run
  // until this is called.
  void RunResolvedPromises();

 private:
  v8::Isolate* isolate_;
  std::unique_ptr<ModuleSystemTestEnvironment> env_;
  bool should_assertions_be_made_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModuleSystemTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_MODULE_SYSTEM_TEST_H_
