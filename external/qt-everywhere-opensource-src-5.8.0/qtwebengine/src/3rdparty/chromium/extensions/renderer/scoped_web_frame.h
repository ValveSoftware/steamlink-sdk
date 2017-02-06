// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SCOPED_WEB_FRAME_H_
#define SCOPED_WEB_FRAME_H_

#include "base/macros.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace extensions {

// ScopedWebFrame is a class to create a dummy webview and frame for testing.
// The dymmy webview and frame will be destructed when the scope exits.
class ScopedWebFrame {
public:
  ScopedWebFrame();
  ~ScopedWebFrame();

  blink::WebLocalFrame* frame() { return frame_; }

private:
  // The webview and the frame are kept alive by the ScopedWebFrame
  // because they are not destructed unless ~ScopedWebFrame explicitly
  // closes the webview and the frame.
  blink::WebView* view_;
  blink::WebLocalFrame* frame_;
  DISALLOW_COPY_AND_ASSIGN(ScopedWebFrame);
};

}  // namespace extensions

#endif  // SCOPED_WEB_FRAME_H_
