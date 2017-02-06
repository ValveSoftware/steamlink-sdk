// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/scoped_web_frame.h"

#include "third_party/WebKit/public/web/WebHeap.h"

namespace extensions {

ScopedWebFrame::ScopedWebFrame() : view_(nullptr), frame_(nullptr) {
  view_ = blink::WebView::create(nullptr, blink::WebPageVisibilityStateVisible);
  frame_ = blink::WebLocalFrame::create(
      blink::WebTreeScopeType::Document, nullptr);
  view_->setMainFrame(frame_);
}

ScopedWebFrame::~ScopedWebFrame() {
  view_->close();
  frame_->close();
  blink::WebHeap::collectAllGarbageForTesting();
}

}  // namespace extensions
