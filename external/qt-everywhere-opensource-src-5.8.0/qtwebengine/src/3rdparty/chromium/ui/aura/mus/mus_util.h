// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_UTIL_H_
#define UI_AURA_MUS_MUS_UTIL_H_

#include "ui/aura/aura_export.h"

namespace mus {
class Window;
}

namespace aura {

class Window;

AURA_EXPORT mus::Window* GetMusWindow(Window* window);

AURA_EXPORT void SetMusWindow(Window* window, mus::Window* mus_window);

}  // namespace aura

#endif  // UI_AURA_MUS_MUS_UTIL_H_
