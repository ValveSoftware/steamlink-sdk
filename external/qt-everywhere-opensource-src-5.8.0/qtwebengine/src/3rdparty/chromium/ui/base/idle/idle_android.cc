// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/logging.h"

namespace ui {

void CalculateIdleTime(IdleTimeCallback notify) {
  NOTIMPLEMENTED();
  notify.Run(0);
}

bool CheckIdleStateIsLocked() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ui
