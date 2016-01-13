// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webcolorchooser_impl.h"

#include "content/common/frame_messages.h"

namespace content {

static int GenerateColorChooserIdentifier() {
  static int next = 0;
  return ++next;
}

RendererWebColorChooserImpl::RendererWebColorChooserImpl(
    RenderFrame* render_frame,
    blink::WebColorChooserClient* client)
    : RenderFrameObserver(render_frame),
      identifier_(GenerateColorChooserIdentifier()),
      client_(client) {
}

RendererWebColorChooserImpl::~RendererWebColorChooserImpl() {
}

bool RendererWebColorChooserImpl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererWebColorChooserImpl, message)
    IPC_MESSAGE_HANDLER(FrameMsg_DidChooseColorResponse,
                        OnDidChooseColorResponse)
    IPC_MESSAGE_HANDLER(FrameMsg_DidEndColorChooser, OnDidEndColorChooser)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererWebColorChooserImpl::setSelectedColor(blink::WebColor color) {
  Send(new FrameHostMsg_SetSelectedColorInColorChooser(
      routing_id(), identifier_, static_cast<SkColor>(color)));
}

void RendererWebColorChooserImpl::endChooser() {
  Send(new FrameHostMsg_EndColorChooser(routing_id(), identifier_));
}

void RendererWebColorChooserImpl::Open(
      SkColor initial_color,
      const std::vector<content::ColorSuggestion>& suggestions) {
  Send(new FrameHostMsg_OpenColorChooser(routing_id(),
                                         identifier_,
                                         initial_color,
                                         suggestions));
}

void RendererWebColorChooserImpl::OnDidChooseColorResponse(int color_chooser_id,
                                                           SkColor color) {
  DCHECK(identifier_ == color_chooser_id);

  client_->didChooseColor(static_cast<blink::WebColor>(color));
}

void RendererWebColorChooserImpl::OnDidEndColorChooser(int color_chooser_id) {
  if (identifier_ != color_chooser_id)
    return;
  client_->didEndChooser();
}

}  // namespace content
