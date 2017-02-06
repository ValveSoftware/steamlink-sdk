// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_POLICY_H_
#define SANDBOX_MAC_POLICY_H_

#include <mach/mach.h>

#include <map>
#include <string>

#include "sandbox/sandbox_export.h"

namespace sandbox {

enum PolicyDecision {
  POLICY_DECISION_INVALID,
  // Explicitly allows the real service to be looked up from launchd.
  POLICY_ALLOW,
  // Deny the look up request by replying with a MIG error. This is the
  // default behavior for servers not given an explicit rule.
  POLICY_DENY_ERROR,
  // Deny the look up request with a well-formed reply containing a
  // Mach port with a send right, messages to which will be ignored.
  POLICY_DENY_DUMMY_PORT,
  // Reply to the look up request with a send right to the substitute_port
  // specified in the Rule.
  POLICY_SUBSTITUTE_PORT,
  POLICY_DECISION_LAST,
};

// A Rule expresses the action to take when a service port is requested via
// bootstrap_look_up. If |result| is not POLICY_SUBSTITUTE_PORT, then
// |substitute_port| must be NULL. If result is POLICY_SUBSTITUTE_PORT, then
// |substitute_port| must not be NULL.
struct SANDBOX_EXPORT Rule {
  Rule();
  explicit Rule(PolicyDecision result);
  explicit Rule(mach_port_t override_port);

  PolicyDecision result;

  // The Rule does not take ownership of this port, but additional send rights
  // will be allocated to it before it is sent to a client. This name must
  // denote a send right that can duplicated with MACH_MSG_TYPE_COPY_SEND.
  mach_port_t substitute_port;
};

// A policy object manages the rules enforced on a target sandboxed process.
struct SANDBOX_EXPORT BootstrapSandboxPolicy {
  typedef std::map<std::string, Rule> NamedRules;

  BootstrapSandboxPolicy();
  BootstrapSandboxPolicy(const BootstrapSandboxPolicy& other);
  ~BootstrapSandboxPolicy();

  // The default action to take if the server name being looked up is not
  // present in |rules|.
  Rule default_rule;

  // A map of bootstrap server names to policy Rules.
  NamedRules rules;
};

// Checks that a policy is well-formed.
SANDBOX_EXPORT bool IsPolicyValid(const BootstrapSandboxPolicy& policy);

}  // namespace sandbox

#endif  // SANDBOX_MAC_POLICY_H_
