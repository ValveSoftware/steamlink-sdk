// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_port.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

ExtensionPort::ExtensionPort(ScriptContext* script_context, int local_id)
    : script_context_(script_context), local_id_(local_id) {}

ExtensionPort::~ExtensionPort() {}

void ExtensionPort::SetGlobalId(int id) {
  global_id_ = id;
  content::RenderFrame* render_frame = script_context_->GetRenderFrame();
  for (const auto& message : pending_messages_)
    PostMessageImpl(render_frame, *message);
  if (is_disconnected_)
    SendDisconnected(render_frame);
  pending_messages_.clear();
}

void ExtensionPort::PostExtensionMessage(std::unique_ptr<Message> message) {
  if (!initialized()) {
    pending_messages_.push_back(std::move(message));
    return;
  }
  PostMessageImpl(script_context_->GetRenderFrame(), *message);
}

void ExtensionPort::Close(bool close_channel) {
  is_disconnected_ = true;
  close_channel_ = close_channel;
  if (!initialized())
    return;

  SendDisconnected(script_context_->GetRenderFrame());
}

void ExtensionPort::PostMessageImpl(content::RenderFrame* render_frame,
                                    const Message& message) {
  // TODO(devlin): What should we do if there's no render frame? Up until now,
  // we've always just dropped the messages, but we might need to figure this
  // out for service workers.
  if (!render_frame)
    return;
  render_frame->Send(new ExtensionHostMsg_PostMessage(
      render_frame->GetRoutingID(), global_id_, message));
}

void ExtensionPort::SendDisconnected(content::RenderFrame* render_frame) {
  if (!render_frame)
    return;
  render_frame->Send(new ExtensionHostMsg_CloseMessagePort(
      render_frame->GetRoutingID(), global_id_, close_channel_));
}

}  // namespace extensions
