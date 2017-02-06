// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frontend_host_impl.h"

#include <stddef.h>

#include "content/browser/bad_message.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "grit/devtools_resources_map.h"

namespace content {

namespace {
const char kCompatibilityScript[] = "devtools.js";
}

// static
DevToolsFrontendHost* DevToolsFrontendHost::Create(
    RenderFrameHost* frontend_main_frame,
    const HandleMessageCallback& handle_message_callback) {
  return new DevToolsFrontendHostImpl(frontend_main_frame,
                                      handle_message_callback);
}

// static
base::StringPiece DevToolsFrontendHost::GetFrontendResource(
    const std::string& path) {
  for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
    if (path == kDevtoolsResources[i].name) {
      return GetContentClient()->GetDataResource(
          kDevtoolsResources[i].value, ui::SCALE_FACTOR_NONE);
    }
  }
  return std::string();
}

DevToolsFrontendHostImpl::DevToolsFrontendHostImpl(
    RenderFrameHost* frontend_main_frame,
    const HandleMessageCallback& handle_message_callback)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(frontend_main_frame)),
      handle_message_callback_(handle_message_callback) {
  frontend_main_frame->Send(new DevToolsMsg_SetupDevToolsClient(
      frontend_main_frame->GetRoutingID(),
      DevToolsFrontendHost::GetFrontendResource(
          kCompatibilityScript).as_string()));
}

DevToolsFrontendHostImpl::~DevToolsFrontendHostImpl() {
}

void DevToolsFrontendHostImpl::BadMessageRecieved() {
  bad_message::ReceivedBadMessage(web_contents()->GetRenderProcessHost(),
                                  bad_message::DFH_BAD_EMBEDDER_MESSAGE);
}

bool DevToolsFrontendHostImpl::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  if (render_frame_host != web_contents()->GetMainFrame())
    return false;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsFrontendHostImpl, message)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_DispatchOnEmbedder,
                        OnDispatchOnEmbedder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsFrontendHostImpl::OnDispatchOnEmbedder(
    const std::string& message) {
  handle_message_callback_.Run(message);
}

}  // namespace content
