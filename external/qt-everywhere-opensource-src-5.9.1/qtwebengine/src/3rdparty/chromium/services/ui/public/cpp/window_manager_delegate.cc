// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/window_manager_delegate.h"

namespace ui {

mojom::EventResult WindowManagerDelegate::OnAccelerator(
    uint32_t id,
    const ui::Event& event) {
  return mojom::EventResult::UNHANDLED;
}

}  // namespace ui
