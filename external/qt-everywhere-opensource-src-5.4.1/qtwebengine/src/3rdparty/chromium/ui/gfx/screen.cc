// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"

namespace gfx {

namespace {

Screen* g_screen_[SCREEN_TYPE_LAST + 1];
ScreenTypeDelegate* g_screen_type_delegate_ = NULL;

}  // namespace

Screen::Screen() {
}

Screen::~Screen() {
}

// static
Screen* Screen::GetScreenFor(NativeView view) {
  ScreenType type = SCREEN_TYPE_NATIVE;
  if (g_screen_type_delegate_)
    type = g_screen_type_delegate_->GetScreenTypeForNativeView(view);
  if (type == SCREEN_TYPE_NATIVE)
    return GetNativeScreen();
  DCHECK(g_screen_[type]);
  return g_screen_[type];
}

// static
void Screen::SetScreenInstance(ScreenType type, Screen* instance) {
  DCHECK_LE(type, SCREEN_TYPE_LAST);
  g_screen_[type] = instance;
}

// static
Screen* Screen::GetScreenByType(ScreenType type) {
  return g_screen_[type];
}

// static
void Screen::SetScreenTypeDelegate(ScreenTypeDelegate* delegate) {
  g_screen_type_delegate_ = delegate;
}

// static
Screen* Screen::GetNativeScreen() {
  if (!g_screen_[SCREEN_TYPE_NATIVE])
    g_screen_[SCREEN_TYPE_NATIVE] = CreateNativeScreen();
  return g_screen_[SCREEN_TYPE_NATIVE];
}

}  // namespace gfx
