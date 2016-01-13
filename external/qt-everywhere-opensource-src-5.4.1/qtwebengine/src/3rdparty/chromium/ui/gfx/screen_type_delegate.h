// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_TYPE_DELEGATE_H_
#define UI_GFX_SCREEN_TYPE_DELEGATE_H_

#include "ui/gfx/native_widget_types.h"

namespace gfx {

enum GFX_EXPORT ScreenType {
  SCREEN_TYPE_NATIVE = 0,
#if defined(OS_CHROMEOS)
  SCREEN_TYPE_ALTERNATE = SCREEN_TYPE_NATIVE,
#else
  SCREEN_TYPE_ALTERNATE,
#endif
  SCREEN_TYPE_LAST = SCREEN_TYPE_ALTERNATE,
};

class GFX_EXPORT ScreenTypeDelegate {
 public:
  virtual ~ScreenTypeDelegate() {}

  // Determines which ScreenType a given |view| belongs to.
  virtual ScreenType GetScreenTypeForNativeView(NativeView view) = 0;
};

}  // namespace gfx

#endif  // UI_GFX_SCREEN_TYPE_DELEGATE_H_
