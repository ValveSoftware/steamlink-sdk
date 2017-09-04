// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/policy.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

TEST(PolicyTest, ValidEmptyPolicy) {
  EXPECT_TRUE(IsPolicyValid(BootstrapSandboxPolicy()));
}

TEST(PolicyTest, ValidPolicy) {
  BootstrapSandboxPolicy policy;
  policy.rules["allow"] = Rule(POLICY_ALLOW);
  policy.rules["deny_error"] = Rule(POLICY_DENY_ERROR);
  policy.rules["deny_dummy"] = Rule(POLICY_DENY_DUMMY_PORT);
  policy.rules["substitue"] = Rule(mach_task_self());
  EXPECT_TRUE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyEmptyRule) {
  Rule rule;
  BootstrapSandboxPolicy policy;
  policy.rules["test"] = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicySubstitue) {
  Rule rule(POLICY_SUBSTITUTE_PORT);
  BootstrapSandboxPolicy policy;
  policy.rules["test"] = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyWithPortAllow) {
  Rule rule(POLICY_ALLOW);
  rule.substitute_port = mach_task_self();
  BootstrapSandboxPolicy policy;
  policy.rules["allow"] = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyWithPortDenyError) {
  Rule rule(POLICY_DENY_ERROR);
  rule.substitute_port = mach_task_self();
  BootstrapSandboxPolicy policy;
  policy.rules["deny_error"] = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyWithPortDummy) {
  Rule rule(POLICY_DENY_DUMMY_PORT);
  rule.substitute_port = mach_task_self();
  BootstrapSandboxPolicy policy;
  policy.rules["deny_dummy"] = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyDefaultRule) {
  BootstrapSandboxPolicy policy;
  policy.default_rule = Rule();
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyDefaultRuleSubstitue) {
  BootstrapSandboxPolicy policy;
  policy.default_rule = Rule(POLICY_SUBSTITUTE_PORT);
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyDefaultRuleWithPortAllow) {
  Rule rule(POLICY_ALLOW);
  rule.substitute_port = mach_task_self();
  BootstrapSandboxPolicy policy;
  policy.default_rule = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyDefaultRuleWithPortDenyError) {
  Rule rule(POLICY_DENY_ERROR);
  rule.substitute_port = mach_task_self();
  BootstrapSandboxPolicy policy;
  policy.default_rule = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

TEST(PolicyTest, InvalidPolicyDefaultRuleWithPortDummy) {
  Rule rule(POLICY_DENY_DUMMY_PORT);
  rule.substitute_port = mach_task_self();
  BootstrapSandboxPolicy policy;
  policy.default_rule = rule;
  EXPECT_FALSE(IsPolicyValid(policy));
}

}  // namespace sandbox
