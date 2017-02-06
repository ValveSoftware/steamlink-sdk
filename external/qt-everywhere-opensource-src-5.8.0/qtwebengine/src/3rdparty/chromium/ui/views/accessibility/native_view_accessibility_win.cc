// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility_win.h"

#include <oleacc.h>

#include <set>
#include <vector>

#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/win/accessibility_misc_utils.h"
#include "ui/base/win/atl_module.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace views {

// static
NativeViewAccessibility* NativeViewAccessibility::Create(View* view) {
  return new NativeViewAccessibilityWin(view);
}

NativeViewAccessibilityWin::NativeViewAccessibilityWin(View* view)
    : NativeViewAccessibility(view) {
}

NativeViewAccessibilityWin::~NativeViewAccessibilityWin() {
}

gfx::NativeViewAccessible NativeViewAccessibilityWin::GetParent() {
  IAccessible* parent = NativeViewAccessibility::GetParent();
  if (parent)
    return parent;

  HWND hwnd = HWNDForView(view_);
  if (!hwnd)
    return NULL;

  HRESULT hr = ::AccessibleObjectFromWindow(
      hwnd, OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void**>(&parent));
  if (SUCCEEDED(hr))
    return parent;

  return NULL;
}

gfx::AcceleratedWidget
NativeViewAccessibilityWin::GetTargetForNativeAccessibilityEvent() {
  return HWNDForView(view_);
}

}  // namespace views
