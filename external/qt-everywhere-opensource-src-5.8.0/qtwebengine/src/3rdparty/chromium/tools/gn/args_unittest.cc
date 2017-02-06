// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/args.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/test_with_scope.h"

// Assertions for VerifyAllOverridesUsed() and DeclareArgs() with multiple
// toolchains.
TEST(ArgsTest, VerifyAllOverridesUsed) {
  TestWithScope setup1, setup2;
  Args args;
  Scope::KeyValueMap key_value_map1;
  Err err;
  LiteralNode assignment1;

  setup1.scope()->SetValue("a", Value(nullptr, true), &assignment1);
  setup1.scope()->GetCurrentScopeValues(&key_value_map1);
  EXPECT_TRUE(args.DeclareArgs(key_value_map1, setup1.scope(), &err));

  LiteralNode assignment2;
  setup2.scope()->SetValue("b", Value(nullptr, true), &assignment2);
  Scope::KeyValueMap key_value_map2;
  setup2.scope()->GetCurrentScopeValues(&key_value_map2);
  EXPECT_TRUE(args.DeclareArgs(key_value_map2, setup2.scope(), &err));

  // Override "a", shouldn't see any errors as "a" was defined.
  args.AddArgOverride("a", Value(nullptr, true));
  EXPECT_TRUE(args.VerifyAllOverridesUsed(&err));

  // Override "a", & "b", shouldn't see any errors as both were defined.
  args.AddArgOverride("b", Value(nullptr, true));
  EXPECT_TRUE(args.VerifyAllOverridesUsed(&err));

  // Override "a", "b" and "c", should fail as "c" was not defined.
  args.AddArgOverride("c", Value(nullptr, true));
  EXPECT_FALSE(args.VerifyAllOverridesUsed(&err));
}
