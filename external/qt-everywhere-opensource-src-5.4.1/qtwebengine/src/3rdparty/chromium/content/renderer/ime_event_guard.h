// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IME_EVENT_GUARD_H_
#define IME_EVENT_GUARD_H_

namespace content {
class RenderWidget;

// Simple RAII object for handling IME events. Calls StartHandlingImeEvent on
// construction and FinishHandlingImeEvent on destruction.
class ImeEventGuard {
 public:
  explicit ImeEventGuard(RenderWidget* widget);
  ~ImeEventGuard();
 private:
  RenderWidget* widget_;
};
}

#endif

