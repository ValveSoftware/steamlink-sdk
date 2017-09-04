// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_test_base.h"

namespace extensions {

class ApiTestBaseTest : public ApiTestBase {
 public:
  void SetUp() override { ApiTestBase::SetUp(); }
};

TEST_F(ApiTestBaseTest, TestEnvironment) {
  RunTest("api_test_base_unittest.js", "testEnvironment");
}

TEST_F(ApiTestBaseTest, TestPromisesRun) {
  RunTest("api_test_base_unittest.js", "testPromisesRun");
}

TEST_F(ApiTestBaseTest, TestCommonModulesAreAvailable) {
  RunTest("api_test_base_unittest.js", "testCommonModulesAreAvailable");
}

TEST_F(ApiTestBaseTest, TestMojoModulesAreAvailable) {
  RunTest("api_test_base_unittest.js", "testMojoModulesAreAvailable");
}

TEST_F(ApiTestBaseTest, TestTestBindings) {
  RunTest("api_test_base_unittest.js", "testTestBindings");
}

}  // namespace extensions
