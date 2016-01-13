// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility.h"

namespace views {

#if !defined(OS_WIN)
// static
NativeViewAccessibility* NativeViewAccessibility::Create(View* view) {
  return NULL;
}
#endif

NativeViewAccessibility::NativeViewAccessibility() {
}

NativeViewAccessibility::~NativeViewAccessibility() {
}

gfx::NativeViewAccessible NativeViewAccessibility::GetNativeObject() {
  return NULL;
}

void NativeViewAccessibility::Destroy() {
  delete this;
}

#if !defined(OS_WIN)
// static
void NativeViewAccessibility::RegisterWebView(View* web_view) {
}

// static
void NativeViewAccessibility::UnregisterWebView(View* web_view) {
}
#endif

}  // namespace views
