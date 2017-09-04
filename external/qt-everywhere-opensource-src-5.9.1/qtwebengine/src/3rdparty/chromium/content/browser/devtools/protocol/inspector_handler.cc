// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/inspector_handler.h"

#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {
namespace devtools {
namespace inspector {

using Response = DevToolsProtocolClient::Response;

InspectorHandler::InspectorHandler()
    : host_(nullptr) {
}

InspectorHandler::~InspectorHandler() {
}

void InspectorHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void InspectorHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
}

void InspectorHandler::TargetCrashed() {
  client_->TargetCrashed(TargetCrashedParams::Create());
}

void InspectorHandler::TargetDetached(const std::string& reason) {
  client_->Detached(DetachedParams::Create()->set_reason(reason));
}

Response InspectorHandler::Enable() {
  if (host_ && !host_->IsRenderFrameLive())
    client_->TargetCrashed(TargetCrashedParams::Create());
  return Response::OK();
}

Response InspectorHandler::Disable() {
  return Response::OK();
}

}  // namespace inspector
}  // namespace devtools
}  // namespace content
