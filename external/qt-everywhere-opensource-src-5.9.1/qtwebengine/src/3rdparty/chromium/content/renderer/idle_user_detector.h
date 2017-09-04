// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IDLE_USER_DETECTOR_H_
#define CONTENT_RENDERER_IDLE_USER_DETECTOR_H_

#include "base/macros.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/public/renderer/render_view_observer.h"

namespace blink {
class WebInputEvent;
}

namespace ui {
class LatencyInfo;
}

namespace content {

// Class which observes user input events and postpones
// idle notifications if the user is active.
class IdleUserDetector : public RenderViewObserver {
 public:
  IdleUserDetector(RenderView* render_view);
  ~IdleUserDetector() override;

 private:
  // RenderViewObserver implementation:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  void OnHandleInputEvent(const blink::WebInputEvent* event,
                          const ui::LatencyInfo& latency_info,
                          InputEventDispatchType dispatch_type);

  DISALLOW_COPY_AND_ASSIGN(IdleUserDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_IDLE_USER_DETECTOR_H_
