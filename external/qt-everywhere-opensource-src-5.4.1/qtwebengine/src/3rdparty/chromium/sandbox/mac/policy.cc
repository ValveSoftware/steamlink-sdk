// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/policy.h"

namespace sandbox {

Rule::Rule()
    : result(POLICY_DECISION_INVALID),
      substitute_port(MACH_PORT_NULL) {
}

Rule::Rule(PolicyDecision result)
    : result(result),
      substitute_port(MACH_PORT_NULL) {
}

Rule::Rule(mach_port_t override_port)
    : result(POLICY_SUBSTITUTE_PORT),
      substitute_port(override_port) {
}

BootstrapSandboxPolicy::BootstrapSandboxPolicy()
    : default_rule(POLICY_DENY_ERROR) {
}

BootstrapSandboxPolicy::~BootstrapSandboxPolicy() {}

static bool IsRuleValid(const Rule& rule) {
  if (!(rule.result > POLICY_DECISION_INVALID &&
        rule.result < POLICY_DECISION_LAST)) {
    return false;
  }
  if (rule.result == POLICY_SUBSTITUTE_PORT) {
    if (rule.substitute_port == MACH_PORT_NULL)
      return false;
  } else {
    if (rule.substitute_port != MACH_PORT_NULL)
      return false;
  }
  return true;
}

bool IsPolicyValid(const BootstrapSandboxPolicy& policy) {
  if (!IsRuleValid(policy.default_rule))
    return false;

  for (const auto& pair : policy.rules) {
    if (!IsRuleValid(pair.second))
      return false;
  }
  return true;
}

}  // namespace sandbox
