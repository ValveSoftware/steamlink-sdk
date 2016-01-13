// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/ime_event_guard.h"

#include "content/renderer/render_widget.h"

namespace content {

ImeEventGuard::ImeEventGuard(RenderWidget* widget) : widget_(widget) {
  widget_->StartHandlingImeEvent();
}

ImeEventGuard::~ImeEventGuard() {
  widget_->FinishHandlingImeEvent();
}

} //  namespace content
