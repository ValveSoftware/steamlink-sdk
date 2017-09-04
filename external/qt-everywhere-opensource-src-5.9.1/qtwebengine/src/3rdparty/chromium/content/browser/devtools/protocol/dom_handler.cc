// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/dom_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {
namespace devtools {
namespace dom {

typedef DevToolsProtocolClient::Response Response;

DOMHandler::DOMHandler() : host_(nullptr) {
}

DOMHandler::~DOMHandler() {
}

void DOMHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
}

Response DOMHandler::SetFileInputFiles(NodeId node_id,
                                       const std::vector<std::string>& files) {
  if (host_) {
    for (const auto& file : files) {
#if defined(OS_WIN)
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
          host_->GetProcess()->GetID(),
          base::FilePath(base::UTF8ToUTF16(file)));
#else
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
          host_->GetProcess()->GetID(),
          base::FilePath(file));
#endif  // OS_WIN
    }
  }
  return Response::FallThrough();
}

}  // namespace dom
}  // namespace devtools
}  // namespace content
