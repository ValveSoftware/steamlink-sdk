// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_replication_state.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"

namespace content {

FrameReplicationState::FrameReplicationState()
    : sandbox_flags(blink::WebSandboxFlags::None),
      scope(blink::WebTreeScopeType::Document),
      insecure_request_policy(blink::kLeaveInsecureRequestsAlone),
      has_potentially_trustworthy_unique_origin(false) {}

FrameReplicationState::FrameReplicationState(
    blink::WebTreeScopeType scope,
    const std::string& name,
    const std::string& unique_name,
    blink::WebSandboxFlags sandbox_flags,
    blink::WebInsecureRequestPolicy insecure_request_policy,
    bool has_potentially_trustworthy_unique_origin)
    : origin(),
      sandbox_flags(sandbox_flags),
      name(name),
      unique_name(unique_name),
      scope(scope),
      insecure_request_policy(insecure_request_policy),
      has_potentially_trustworthy_unique_origin(
          has_potentially_trustworthy_unique_origin) {}

FrameReplicationState::FrameReplicationState(
    const FrameReplicationState& other) = default;

FrameReplicationState::~FrameReplicationState() {
}

}  // namespace content
