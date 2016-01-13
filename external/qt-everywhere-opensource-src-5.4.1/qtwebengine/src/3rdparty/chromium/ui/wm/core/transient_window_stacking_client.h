// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_TRANSIENT_WINDOW_STACKING_CLIENT_H_
#define UI_WM_CORE_TRANSIENT_WINDOW_STACKING_CLIENT_H_

#include "ui/aura/client/window_stacking_client.h"
#include "ui/wm/wm_export.h"

namespace wm {

class TransientWindowManager;

class WM_EXPORT TransientWindowStackingClient
    : public aura::client::WindowStackingClient {
 public:
  TransientWindowStackingClient();
  virtual ~TransientWindowStackingClient();

  // WindowStackingClient:
  virtual bool AdjustStacking(aura::Window** child,
                              aura::Window** target,
                              aura::Window::StackDirection* direction) OVERRIDE;

 private:
  // Purely for DCHECKs.
  friend class TransientWindowManager;

  static TransientWindowStackingClient* instance_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowStackingClient);
};

}  // namespace wm

#endif  // UI_WM_CORE_TRANSIENT_WINDOW_STACKING_CLIENT_H_
