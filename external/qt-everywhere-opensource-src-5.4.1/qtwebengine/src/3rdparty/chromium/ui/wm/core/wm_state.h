// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WM_STATE_H_
#define UI_WM_CORE_WM_STATE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/wm/wm_export.h"

namespace wm {

class TransientWindowController;
class TransientWindowStackingClient;

// Installs state needed by the window manager.
class WM_EXPORT WMState {
 public:
  WMState();
  ~WMState();

  // WindowStackingClient:
 private:
  scoped_ptr<TransientWindowStackingClient> window_stacking_client_;
  scoped_ptr<TransientWindowController> transient_window_client_;

  DISALLOW_COPY_AND_ASSIGN(WMState);
};

}  // namespace wm

#endif  // UI_WM_CORE_WM_STATE_H_
