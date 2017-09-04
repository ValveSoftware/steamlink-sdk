// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_unresponsive_state.h"

namespace content {

WebContentsUnresponsiveState::WebContentsUnresponsiveState()
    : reason(RendererUnresponsiveType::RENDERER_UNRESPONSIVE_UNKNOWN),
      outstanding_ack_count(0),
      last_event_type(blink::WebInputEvent::Undefined) {}

}  // namespace content
