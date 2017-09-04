// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/idle_user_detector.h"

#include "base/logging.h"
#include "content/common/input_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

IdleUserDetector::IdleUserDetector(RenderView* render_view)
    : RenderViewObserver(render_view){
}

IdleUserDetector::~IdleUserDetector() {
}

bool IdleUserDetector::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(IdleUserDetector, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)
  IPC_END_MESSAGE_MAP()
  return false;
}

void IdleUserDetector::OnHandleInputEvent(
    const blink::WebInputEvent* event,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    RenderThreadImpl* render_thread = RenderThreadImpl::current();
    if (render_thread != NULL) {
      render_thread->PostponeIdleNotification();
    }
  }
}

void IdleUserDetector::OnDestruct() {
  delete this;
}

}  // namespace content
