// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SHADOW_CONTROLLER_H_
#define UI_WM_CORE_SHADOW_CONTROLLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/wm_export.h"

namespace aura {
class Window;
namespace client {
class ActivationClient;
}
}
namespace gfx {
class Rect;
}

namespace wm {

class Shadow;

// ShadowController observes changes to windows and creates and updates drop
// shadows as needed. ShadowController itself is light weight and per
// ActivationClient. ShadowController delegates to its implementation class,
// which observes all window creation.
class WM_EXPORT ShadowController :
    public aura::client::ActivationChangeObserver {
 public:
  class WM_EXPORT TestApi {
   public:
    explicit TestApi(ShadowController* controller) : controller_(controller) {}
    ~TestApi() {}

    Shadow* GetShadowForWindow(aura::Window* window);

   private:
    ShadowController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit ShadowController(aura::client::ActivationClient* activation_client);
  virtual ~ShadowController();

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

 private:
  class Impl;

  aura::client::ActivationClient* activation_client_;

  scoped_refptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namespace wm

#endif  // UI_WM_CORE_SHADOW_CONTROLLER_H_
