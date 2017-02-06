// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
#define CONTENT_COMMON_FRAME_REPLICATION_STATE_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/content_security_policy_header.h"
#include "third_party/WebKit/public/platform/WebInsecureRequestPolicy.h"
#include "url/origin.h"

namespace blink {
enum class WebTreeScopeType;
enum class WebSandboxFlags;
}

namespace content {

// This structure holds information that needs to be replicated between a
// RenderFrame and any of its associated RenderFrameProxies.
struct CONTENT_EXPORT FrameReplicationState {
  FrameReplicationState();
  FrameReplicationState(blink::WebTreeScopeType scope,
                        const std::string& name,
                        const std::string& unique_name,
                        blink::WebSandboxFlags sandbox_flags,
                        blink::WebInsecureRequestPolicy insecure_request_policy,
                        bool has_potentially_trustworthy_unique_origin);
  FrameReplicationState(const FrameReplicationState& other);
  ~FrameReplicationState();

  // Current origin of the frame. This field is updated whenever a frame
  // navigation commits.
  //
  // TODO(alexmos): For now, |origin| updates are immediately sent to all frame
  // proxies when in --site-per-process mode. This isn't ideal, since Blink
  // typically needs a proxy's origin only when performing security checks on
  // the ancestors of a local frame.  So, as a future improvement, we could
  // delay sending origin updates to proxies until they have a local descendant
  // (if ever). This would reduce leaking a user's browsing history into a
  // compromized renderer.
  url::Origin origin;

  // Sandbox flags currently in effect for the frame.  |sandbox_flags| are
  // initialized for new child frames using the value of the <iframe> element's
  // "sandbox" attribute, combined with any sandbox flags in effect for the
  // parent frame.
  //
  // When a parent frame updates an <iframe>'s sandbox attribute via
  // JavaScript, |sandbox_flags| are updated only after the child frame commits
  // a navigation that makes the updated flags take effect.  This is also the
  // point at which updates are sent to proxies (see
  // CommitPendingSandboxFlags()). The proxies need updated flags so that they
  // can be inherited properly if a proxy ever becomes a parent of a local
  // frame.
  blink::WebSandboxFlags sandbox_flags;

  // The assigned name of the frame (see WebFrame::assignedName()).
  //
  // |name| is set when a new child frame is created using the value of the
  // <iframe> element's "name" attribute (see
  // RenderFrameHostImpl::OnCreateChildFrame), and it is updated dynamically
  // whenever a frame sets its window.name.
  //
  // |name| updates are immediately sent to all frame proxies (when in
  // --site-per-process mode), so that other frames can look up or navigate a
  // frame using its updated name (e.g., using window.open(url, frame_name)).
  std::string name;

  // Unique name of the frame (see WebFrame::uniqueName()).
  //
  // |unique_name| is used in heuristics that try to identify the same frame
  // across different, unrelated navigations (i.e. to refer to the frame
  // when going back/forward in session history OR when refering to the frame
  // in layout tests results).
  //
  // |unique_name| needs to be replicated to ensure that unique name for a given
  // frame is the same across all renderers - without replication a renderer
  // might arrive at a different value when recalculating the unique name from
  // scratch.
  std::string unique_name;

  // Accumulated CSP headers - gathered from http headers, <meta> elements,
  // parent frames (in case of about:blank frames).
  std::vector<ContentSecurityPolicyHeader> accumulated_csp_headers;

  // Whether the frame is in a document tree or a shadow tree, per the Shadow
  // DOM spec: https://w3c.github.io/webcomponents/spec/shadow/
  // Note: This should really be const, as it can never change once a frame is
  // created. However, making it const makes it a pain to embed into IPC message
  // params: having a const member implicitly deletes the copy assignment
  // operator.
  blink::WebTreeScopeType scope;

  // The insecure request policy that a frame's current document is enforcing.
  // Updates are immediately sent to all frame proxies when frames live in
  // different processes.
  blink::WebInsecureRequestPolicy insecure_request_policy;

  // True if a frame's origin is unique and should be considered potentially
  // trustworthy.
  bool has_potentially_trustworthy_unique_origin;

  // TODO(alexmos): Eventually, this structure can also hold other state that
  // needs to be replicated, such as frame sizing info.
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
