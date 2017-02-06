// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IME_EVENT_GUARD_H_
#define CONTENT_RENDERER_IME_EVENT_GUARD_H_

namespace content {
class RenderWidget;

// Simple RAII object for guarding IME updates. Calls OnImeGuardStart on
// construction and OnImeGuardFinish on destruction.
class ImeEventGuard {
 public:
  explicit ImeEventGuard(RenderWidget* widget);
  ~ImeEventGuard();

  bool show_ime() const { return show_ime_; }
  bool from_ime() const { return from_ime_; }
  void set_show_ime(bool show_ime) { show_ime_ = show_ime; }
  void set_from_ime(bool from_ime) { from_ime_ = from_ime; }

 private:
  RenderWidget* widget_;
  bool show_ime_;
  bool from_ime_;
};
}

#endif  // CONTENT_RENDERER_IME_EVENT_GUARD_H_

