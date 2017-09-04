// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_frame_utils.h"

#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"

namespace content {

int GetRoutingIdForFrameOrProxy(blink::WebFrame* web_frame) {
  if (!web_frame)
    return MSG_ROUTING_NONE;
  if (web_frame->isWebRemoteFrame())
    return RenderFrameProxy::FromWebFrame(web_frame)->routing_id();
  return RenderFrameImpl::FromWebFrame(web_frame)->GetRoutingID();
}

blink::WebFrame* GetWebFrameFromRoutingIdForFrameOrProxy(int routing_id) {
  auto* render_frame = RenderFrameImpl::FromRoutingID(routing_id);
  if (render_frame)
    return render_frame->GetWebFrame();

  auto* render_frame_proxy = RenderFrameProxy::FromRoutingID(routing_id);
  if (render_frame_proxy)
    return render_frame_proxy->web_frame();

  return nullptr;
}

}  // namespace content
