// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_test_base.h"

#include "base/macros.h"
#include "gin/modules/module_registry.h"

// A test launcher for tests for the mojoPrivate API defined in
// extensions/test/data/mojo_private_unittest.js.

namespace extensions {

class MojoPrivateApiTest : public ApiTestBase {
 public:
  MojoPrivateApiTest() = default;

  gin::ModuleRegistry* module_registry() {
    return gin::ModuleRegistry::From(env()->context()->v8_context());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoPrivateApiTest);
};

TEST_F(MojoPrivateApiTest, Define) {
  ASSERT_NO_FATAL_FAILURE(RunTest("mojo_private_unittest.js", "testDefine"));
  EXPECT_EQ(1u, module_registry()->available_modules().count("testModule"));
}

TEST_F(MojoPrivateApiTest, DefineRegistersModule) {
  ASSERT_NO_FATAL_FAILURE(
      RunTest("mojo_private_unittest.js", "testDefineRegistersModule"));
  EXPECT_EQ(1u, module_registry()->available_modules().count("testModule"));
  EXPECT_EQ(1u, module_registry()->available_modules().count("dependency"));
}

TEST_F(MojoPrivateApiTest, DefineModuleDoesNotExist) {
  ASSERT_NO_FATAL_FAILURE(
      RunTest("mojo_private_unittest.js", "testDefineModuleDoesNotExist"));
  EXPECT_EQ(0u, module_registry()->available_modules().count("testModule"));
  EXPECT_EQ(0u,
            module_registry()->available_modules().count("does not exist!"));
  EXPECT_EQ(1u, module_registry()->unsatisfied_dependencies().count(
                    "does not exist!"));
}

TEST_F(MojoPrivateApiTest, RequireAsync) {
  ASSERT_NO_FATAL_FAILURE(
      RunTest("mojo_private_unittest.js", "testRequireAsync"));
}

TEST_F(MojoPrivateApiTest, DefineAndRequire) {
  ASSERT_NO_FATAL_FAILURE(
      RunTest("mojo_private_unittest.js", "testDefineAndRequire"));
}

}  // namespace extensions
