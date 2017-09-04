// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_UNRESPONSIVE_STATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_UNRESPONSIVE_STATE_H_

#include "content/common/content_export.h"
#include "content/public/browser/renderer_unresponsive_type.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace content {

// Contains a variety of information regarding the state
// at which a renderer was marked as unresponsive. Some of
// this information may be provided in a crash report.
struct CONTENT_EXPORT WebContentsUnresponsiveState {
  WebContentsUnresponsiveState();

  // The reason why the renderer was unresponsive.
  RendererUnresponsiveType reason;

  // TODO(dtapuska): Remove these fields once crbug.com/615090 is fixed.
  // The number of outstanding blocking input events sent to the renderer
  // that have not be acknowledged.
  size_t outstanding_ack_count;
  // The input event type that started the unresponsive state timeout.
  blink::WebInputEvent::Type outstanding_event_type;
  // The last blocking input event type sent to the renderer.
  blink::WebInputEvent::Type last_event_type;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_UNRESPONSIVE_STATE_H_
