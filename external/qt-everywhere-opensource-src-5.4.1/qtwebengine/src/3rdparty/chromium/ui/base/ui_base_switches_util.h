// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_SWITCHES_UTIL_H_
#define UI_BASE_UI_BASE_SWITCHES_UTIL_H_

#include "ui/base/ui_base_export.h"

namespace switches {

UI_BASE_EXPORT bool IsTextInputFocusManagerEnabled();
UI_BASE_EXPORT bool IsTouchDragDropEnabled();
UI_BASE_EXPORT bool IsTouchEditingEnabled();

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_UTIL_H_
