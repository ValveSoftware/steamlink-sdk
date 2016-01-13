// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_
#define CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/public/renderer/render_view_observer.h"
#include "ui/gfx/point.h"
#include "ui/gfx/range/range.h"

namespace blink {
class WebView;
}

namespace content {

class RenderViewImpl;

// This is the renderer-side message filter that generates the replies for the
// messages sent by the TextInputClientMac. See
// content/browser/renderer_host/text_input_client_mac.h for more information.
class TextInputClientObserver : public RenderViewObserver {
 public:
  explicit TextInputClientObserver(RenderViewImpl* render_view);
  virtual ~TextInputClientObserver();

  // RenderViewObserver overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Returns the WebView of the RenderView.
  blink::WebView* webview();

  // IPC Message handlers:
  void OnStringAtPoint(gfx::Point point);
  void OnCharacterIndexForPoint(gfx::Point point);
  void OnFirstRectForCharacterRange(gfx::Range range);
  void OnStringForRange(gfx::Range range);

  RenderViewImpl* const render_view_impl_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientObserver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_
