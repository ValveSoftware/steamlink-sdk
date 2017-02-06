// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "extensions/renderer/module_system_test.h"
#include "gin/dictionary.h"
#include "grit/extensions_renderer_resources.h"

namespace extensions {
namespace {

class UtilsUnittest : public ModuleSystemTest {
 private:
  void SetUp() override {
    ModuleSystemTest::SetUp();

    env()->RegisterModule("utils", IDR_UTILS_JS);
    env()->RegisterTestFile("utils_unittest", "utils_unittest.js");
    env()->OverrideNativeHandler("schema_registry",
                                 "exports.$set('GetSchema', function() {});");
    env()->OverrideNativeHandler("logging",
                                 "exports.$set('CHECK', function() {});\n"
                                 "exports.$set('DCHECK', function() {});\n"
                                 "exports.$set('WARNING', function() {});");
    env()->OverrideNativeHandler("v8_context", "");
    gin::Dictionary chrome(env()->isolate(), env()->CreateGlobal("chrome"));
    gin::Dictionary chrome_runtime(
        gin::Dictionary::CreateEmpty(env()->isolate()));
    chrome.Set("runtime", chrome_runtime);
  }
};

TEST_F(UtilsUnittest, TestNothing) {
  ExpectNoAssertionsMade();
}

TEST_F(UtilsUnittest, SuperClass) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->module_system()->CallModuleMethod("utils_unittest", "testSuperClass");
}

TEST_F(UtilsUnittest, PromiseNoResult) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->module_system()->CallModuleMethod("utils_unittest",
                                           "testPromiseNoResult");
  RunResolvedPromises();
}

TEST_F(UtilsUnittest, PromiseOneResult) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->module_system()->CallModuleMethod("utils_unittest",
                                           "testPromiseOneResult");
  RunResolvedPromises();
}

TEST_F(UtilsUnittest, PromiseTwoResults) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->module_system()->CallModuleMethod("utils_unittest",
                                           "testPromiseTwoResults");
  RunResolvedPromises();
}

TEST_F(UtilsUnittest, PromiseError) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->module_system()->CallModuleMethod("utils_unittest",
                                           "testPromiseError");
  RunResolvedPromises();
}

}  // namespace
}  // namespace extensions
